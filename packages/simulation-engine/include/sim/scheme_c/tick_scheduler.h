#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "sim/scheme_c/types.h"

namespace sim::scheme_c {

enum class TickTaskPhase : std::uint32_t {
    RuntimeBridge = 0,
    VirtualAxisGroup = 10,
    VirtualIo = 20,
    Recording = 30,
    Default = 100
};

class TickScheduler {
public:
    using TaskId = std::uint64_t;
    using TickCallback = std::function<void(const TickInfo&)>;

    TickScheduler() = default;

    TaskId scheduleRecurring(TickTaskPhase phase,
                             TickCallback callback,
                             std::string label = {});
    TaskId scheduleOnceAt(TickIndex tick_index,
                          TickTaskPhase phase,
                          TickCallback callback,
                          std::string label = {});
    TaskId scheduleOnceAfter(const TickInfo& tick,
                             TickIndex delay_ticks,
                             TickTaskPhase phase,
                             TickCallback callback,
                             std::string label = {});
    bool cancel(TaskId id);
    std::size_t dispatch(const TickInfo& tick);
    void clear();
    std::size_t pendingTaskCount() const;

private:
    struct RecurringTask {
        TaskId id{0};
        TickTaskPhase phase{TickTaskPhase::Default};
        std::uint64_t sequence{0};
        TickCallback callback{};
        std::string label{};
    };

    struct OneShotTask {
        TaskId id{0};
        TickIndex tick_index{0};
        TickTaskPhase phase{TickTaskPhase::Default};
        std::uint64_t sequence{0};
        TickCallback callback{};
        std::string label{};
    };

    TaskId next_task_id_{1};
    std::uint64_t next_sequence_{1};
    std::vector<RecurringTask> recurring_tasks_{};
    std::vector<OneShotTask> one_shot_tasks_{};
};

}  // namespace sim::scheme_c
