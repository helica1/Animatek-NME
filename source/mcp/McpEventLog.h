#pragma once

#include <juce_core/juce_core.h>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

// What the synth and the connection did, in order, for an MCP client that
// cannot see the screen. A tool call only answers for itself: without this, an
// assistant testing against a real G1 sends an edit and never learns that the
// synth answered with an error, dropped the connection, or had a knob turned
// on its front panel in the meantime.
//
// A fixed-size ring: the oldest events fall off, and a reader that fell behind
// is told so rather than handed a history with a silent hole in it. Sequence
// numbers only ever grow, so "everything after the last one I saw" is exact.
//
// Thread-safe, although today every writer runs on the message thread: the
// connection callbacks it listens to are documented as message-thread, but one
// of them already says otherwise, and a log is the wrong place to find out.
class McpEventLog
{
public:
    struct Event
    {
        std::int64_t seq = 0;
        std::int64_t timeMs = 0;     // juce::Time::currentTimeMillis() when recorded
        juce::String type;
        int slot = -1;               // -1 when the event is not about one slot
        juce::var data;
    };

    struct Page
    {
        std::vector<Event> events;   // oldest first
        std::int64_t latestSeq = 0;  // pass this back as `after` next time
        bool truncated = false;      // events after `after` were dropped before this read
        bool hasMore = false;        // the limit cut the page short
    };

    explicit McpEventLog(size_t capacity = 512);

    void record(const juce::String& type, int slot = -1, juce::var data = {});

    // Events with seq > after, oldest first, at most `limit` of them.
    Page since(std::int64_t after, size_t limit) const;

    std::int64_t latestSeq() const;

private:
    size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<Event> events_;
    std::int64_t nextSeq_ = 1;
};
