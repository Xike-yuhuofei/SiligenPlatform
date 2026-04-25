#pragma once

#include "shared/types/CMPTypes.h"

#include <algorithm>
#include <numeric>
#include <vector>

namespace Siligen::Domain::Motion {

/// Sort a TriggerTimeline by trigger_times in ascending order.
inline TriggerTimeline SortTimelineByTime(const TriggerTimeline& timeline) {
    std::vector<size_t> indices(timeline.trigger_times.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return timeline.trigger_times[a] < timeline.trigger_times[b];
    });

    TriggerTimeline sorted;
    for (size_t idx : indices) {
        sorted.trigger_times.push_back(timeline.trigger_times[idx]);
        sorted.trigger_positions.push_back(timeline.trigger_positions[idx]);
        sorted.pulse_widths.push_back(timeline.pulse_widths[idx]);
        sorted.cmp_channels.push_back(timeline.cmp_channels[idx]);
    }
    return sorted;
}

}  // namespace Siligen::Domain::Motion
