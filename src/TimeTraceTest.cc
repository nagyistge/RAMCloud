/* Copyright (c) 2014-2016 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright
 * notice and this permission notice appear in all copies.xx
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "TestUtil.h"
#include "Dispatch.h"
#include "Logger.h"
#include "TimeTrace.h"

namespace RAMCloud {
class TimeTraceTest : public ::testing::Test {
  public:
    TestLog::Enable logEnabler;
    TimeTrace trace;

    TimeTraceTest()
        : logEnabler()
        , trace()
    {
        Cycles::mockCyclesPerSec = 2e09;
    }

    ~TimeTraceTest()
    {
        Cycles::mockCyclesPerSec = 0;
    }

  private:
    DISALLOW_COPY_AND_ASSIGN(TimeTraceTest);
};

TEST_F(TimeTraceTest, constructor) {
    EXPECT_EQ(0, trace.events[0].format);
    EXPECT_EQ(0, trace.events[0].format);
}

TEST_F(TimeTraceTest, record_basics) {
    trace.record(100, "point a");
    trace.record(200, "point b %u %u %d %u", 1, 2, -3, 4);
    trace.record(350, "point c");
    EXPECT_EQ("     0.0 ns (+   0.0 ns): point a\n"
            "    50.0 ns (+  50.0 ns): point b 1 2 -3 4\n"
            "   125.0 ns (+  75.0 ns): point c",
            trace.getTrace());
}

TEST_F(TimeTraceTest, record_readersActive) {
    trace.activeReaders = 1;
    trace.record(100, "point a");
    trace.record(200, "point b");
    trace.activeReaders = 0;
    trace.record(350, "point c");
    EXPECT_EQ("     0.0 ns (+   0.0 ns): point c",
            trace.getTrace());
}

TEST_F(TimeTraceTest, record_wrapAround) {
    trace.nextIndex = TimeTrace::BUFFER_SIZE - 2;
    trace.record(100, "near the end");
    trace.record(200, "at the end");
    trace.record(350, "beginning");
    EXPECT_EQ(1, trace.nextIndex.load());
    trace.nextIndex = TimeTrace::BUFFER_SIZE - 2;
    EXPECT_EQ("     0.0 ns (+   0.0 ns): near the end\n"
            "    50.0 ns (+  50.0 ns): at the end\n"
            "   125.0 ns (+  75.0 ns): beginning",
            trace.getTrace());
}

TEST_F(TimeTraceTest, getTrace) {
    trace.record(100, "point a");
    EXPECT_EQ("     0.0 ns (+   0.0 ns): point a",
            trace.getTrace());
}

TEST_F(TimeTraceTest, printToLog) {
    trace.record(100, "point a");
    trace.printToLog();
    EXPECT_EQ("printInternal:      0.0 ns (+   0.0 ns): point a",
            TestLog::get());
}

TEST_F(TimeTraceTest, printToLogBackground) {
    Dispatch dispatch(false);
    TimeTrace t;
    t.record(100, "point a");
    t.printToLogBackground(&dispatch);
    EXPECT_EQ("", TestLog::get());
    dispatch.poll();
    for (int i = 0; i < 1000; i++) {
        if (TestLog::get().size() != 0) {
            break;
        }
        usleep(1000);
    }
    EXPECT_EQ("printInternal:      0.0 ns (+   0.0 ns): point a",
            TestLog::get());
}

TEST_F(TimeTraceTest, reset) {
    trace.record("first", 100);
    trace.record("second", 200);
    trace.record("third", 200);
    trace.events[20].format = "sneaky";
    trace.reset();
    EXPECT_TRUE(trace.events[2].format == NULL);
    EXPECT_FALSE(trace.events[20].format == NULL);
    EXPECT_EQ(0, trace.nextIndex.load());
}

TEST_F(TimeTraceTest, printInternal_emptyTrace_stringVersion) {
    trace.nextIndex = 0;
    EXPECT_EQ("No time trace events to print", trace.getTrace());
}
TEST_F(TimeTraceTest, printInternal_emptyTrace_logVersion) {
    trace.nextIndex = 0;
    trace.printInternal(NULL);
    EXPECT_EQ("printInternal: No time trace events to print", TestLog::get());
    EXPECT_EQ(0, trace.activeReaders);
}
TEST_F(TimeTraceTest, printInternal_startAtBeginning) {
    trace.record(100, "point a");
    trace.record(200, "point b");
    trace.record(350, "point c");
    EXPECT_EQ("     0.0 ns (+   0.0 ns): point a\n"
            "    50.0 ns (+  50.0 ns): point b\n"
            "   125.0 ns (+  75.0 ns): point c",
            trace.getTrace());
    EXPECT_EQ(0, trace.activeReaders);
}
TEST_F(TimeTraceTest, printInternal_startAtNextIndex) {
    trace.record(100, "point a");
    trace.record(200, "point b");
    trace.record(350, "point c");
    trace.nextIndex = 1;
    EXPECT_EQ("     0.0 ns (+   0.0 ns): point b\n"
            "    75.0 ns (+  75.0 ns): point c",
            trace.getTrace());
}
TEST_F(TimeTraceTest, printInternal_wrapAround) {
    trace.nextIndex = TimeTrace::BUFFER_SIZE - 2;
    trace.record(100, "point a");
    trace.record(200, "point b");
    trace.record(350, "point c");
    trace.nextIndex = TimeTrace::BUFFER_SIZE - 2;
    EXPECT_EQ("     0.0 ns (+   0.0 ns): point a\n"
            "    50.0 ns (+  50.0 ns): point b\n"
            "   125.0 ns (+  75.0 ns): point c",
            trace.getTrace());
}
TEST_F(TimeTraceTest, printInternal_printfArguments) {
    trace.record(200, "point b %d %d", 99, 101);
    EXPECT_EQ("     0.0 ns (+   0.0 ns): point b 99 101",
            trace.getTrace());
}
TEST_F(TimeTraceTest, printInternal_printToLog) {
    trace.record(100, "point a");
    trace.record(200, "point b %d %d", 99, 101);
    trace.record(350, "point c");
    trace.printInternal(NULL);
    EXPECT_EQ("printInternal:      0.0 ns (+   0.0 ns): point a | "
            "printInternal:     50.0 ns (+  50.0 ns): point b 99 101 | "
            "printInternal:    125.0 ns (+  75.0 ns): point c",
            TestLog::get());
}


}  // namespace RAMCloud
