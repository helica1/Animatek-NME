#include <doctest.h>
#include "mcp/McpEventLog.h"

// The MCP bridge's event log is how an assistant driving a real G1 learns what
// the synth did between two of its calls. What matters is that a reader can
// always tell whether it has seen everything: nothing silently skipped, nothing
// handed over twice.

TEST_CASE("an empty event log has nothing to hand out and nothing lost")
{
    McpEventLog log(4);
    const auto page = log.since(0, 10);
    CHECK(page.events.empty());
    CHECK(page.latestSeq == 0);
    CHECK_FALSE(page.truncated);
    CHECK_FALSE(page.hasMore);
}

TEST_CASE("events come back oldest first, and resuming from latestSeq gives only new ones")
{
    McpEventLog log(8);
    log.record("connection", -1);
    log.record("synth_error", 2);

    auto page = log.since(0, 10);
    REQUIRE(page.events.size() == 2);
    CHECK(page.events[0].type == "connection");
    CHECK(page.events[1].type == "synth_error");
    CHECK(page.events[1].slot == 2);
    CHECK(page.events[0].seq < page.events[1].seq);
    CHECK(page.latestSeq == page.events[1].seq);

    log.record("slot_focus", 1);
    const auto next = log.since(page.latestSeq, 10);
    REQUIRE(next.events.size() == 1);
    CHECK(next.events[0].type == "slot_focus");
    CHECK_FALSE(next.truncated);

    CHECK(log.since(next.latestSeq, 10).events.empty());
}

TEST_CASE("a reader that fell behind the ring is told, not handed a gap")
{
    McpEventLog log(3);
    for (int i = 0; i < 5; ++i)
        log.record("hardware_parameter", 0);   // seqs 1..5, only 3..5 kept

    const auto fromStart = log.since(0, 10);
    CHECK(fromStart.truncated);
    REQUIRE(fromStart.events.size() == 3);
    CHECK(fromStart.events.front().seq == 3);

    // Having seen 2, the next one wanted is 3, which is still there.
    CHECK_FALSE(log.since(2, 10).truncated);
    // Having seen 1, event 2 is gone.
    CHECK(log.since(1, 10).truncated);
}

TEST_CASE("a page cut short by the limit resumes exactly where it stopped")
{
    McpEventLog log(10);
    for (int i = 0; i < 5; ++i)
        log.record("patch_received", i % 4);

    const auto first = log.since(0, 2);
    REQUIRE(first.events.size() == 2);
    CHECK(first.hasMore);
    CHECK(first.latestSeq == first.events.back().seq);

    const auto rest = log.since(first.latestSeq, 10);
    REQUIRE(rest.events.size() == 3);
    CHECK_FALSE(rest.hasMore);
    CHECK(rest.events.front().seq == first.events.back().seq + 1);
    CHECK(rest.latestSeq == log.latestSeq());
}
