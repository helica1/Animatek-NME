#include "McpEventLog.h"
#include <algorithm>

McpEventLog::McpEventLog(size_t capacity)
    : capacity_(std::max<size_t>(1, capacity))
{
}

void McpEventLog::record(const juce::String& type, int slot, juce::var data)
{
    Event event;
    event.timeMs = juce::Time::currentTimeMillis();
    event.type = type;
    event.slot = slot;
    event.data = std::move(data);

    const std::lock_guard<std::mutex> lock(mutex_);
    event.seq = nextSeq_++;
    events_.push_back(std::move(event));
    while (events_.size() > capacity_)
        events_.pop_front();
}

McpEventLog::Page McpEventLog::since(std::int64_t after, size_t limit) const
{
    const std::lock_guard<std::mutex> lock(mutex_);

    Page page;
    page.latestSeq = nextSeq_ - 1;
    if (events_.empty())
        return page;

    // Something the reader has not seen yet was pushed out of the ring.
    page.truncated = after + 1 < events_.front().seq;

    const auto first = std::find_if(events_.begin(), events_.end(),
                                    [after](const Event& e) { return e.seq > after; });
    for (auto it = first; it != events_.end(); ++it)
    {
        if (page.events.size() >= limit)
        {
            page.hasMore = true;
            // The caller resumes from the last event it was actually given.
            page.latestSeq = page.events.empty() ? after : page.events.back().seq;
            break;
        }
        page.events.push_back(*it);
    }
    return page;
}

std::int64_t McpEventLog::latestSeq() const
{
    const std::lock_guard<std::mutex> lock(mutex_);
    return nextSeq_ - 1;
}
