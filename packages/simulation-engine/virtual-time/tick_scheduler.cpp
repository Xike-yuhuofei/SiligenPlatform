#include "sim/scheme_c/tick_scheduler.h"

#include <algorithm>
#include <utility>

namespace sim::scheme_c {

TickScheduler::TaskId TickScheduler::scheduleRecurring(TickTaskPhase phase,
                                                       TickCallback callback,
                                                       std::string label) {
    const TaskId task_id = next_task_id_++;
    recurring_tasks_.push_back(RecurringTask{
        task_id,
        phase,
        next_sequence_++,
        std::move(callback),
        std::move(label)
    });
    return task_id;
}

TickScheduler::TaskId TickScheduler::scheduleOnceAt(TickIndex tick_index,
                                                    TickTaskPhase phase,
                                                    TickCallback callback,
                                                    std::string label) {
    const TaskId task_id = next_task_id_++;
    one_shot_tasks_.push_back(OneShotTask{
        task_id,
        tick_index,
        phase,
        next_sequence_++,
        std::move(callback),
        std::move(label)
    });
    return task_id;
}

TickScheduler::TaskId TickScheduler::scheduleOnceAfter(const TickInfo& tick,
                                                       TickIndex delay_ticks,
                                                       TickTaskPhase phase,
                                                       TickCallback callback,
                                                       std::string label) {
    return scheduleOnceAt(tick.tick_index + delay_ticks, phase, std::move(callback), std::move(label));
}

bool TickScheduler::cancel(TaskId id) {
    const auto recurring_it = std::find_if(
        recurring_tasks_.begin(),
        recurring_tasks_.end(),
        [id](const RecurringTask& task) { return task.id == id; });
    if (recurring_it != recurring_tasks_.end()) {
        recurring_tasks_.erase(recurring_it);
        return true;
    }

    const auto one_shot_it = std::find_if(
        one_shot_tasks_.begin(),
        one_shot_tasks_.end(),
        [id](const OneShotTask& task) { return task.id == id; });
    if (one_shot_it != one_shot_tasks_.end()) {
        one_shot_tasks_.erase(one_shot_it);
        return true;
    }

    return false;
}

std::size_t TickScheduler::dispatch(const TickInfo& tick) {
    struct RunnableTask {
        TickIndex target_tick{0};
        TickTaskPhase phase{TickTaskPhase::Default};
        std::uint64_t sequence{0};
        TickCallback callback{};
    };

    std::vector<RunnableTask> runnable{};
    runnable.reserve(recurring_tasks_.size() + one_shot_tasks_.size());

    for (const auto& task : recurring_tasks_) {
        runnable.push_back(RunnableTask{
            tick.tick_index,
            task.phase,
            task.sequence,
            task.callback
        });
    }

    auto next_one_shot = std::remove_if(
        one_shot_tasks_.begin(),
        one_shot_tasks_.end(),
        [&runnable, &tick](const OneShotTask& task) {
            if (task.tick_index > tick.tick_index) {
                return false;
            }

            runnable.push_back(RunnableTask{
                task.tick_index,
                task.phase,
                task.sequence,
                task.callback
            });
            return true;
        });
    one_shot_tasks_.erase(next_one_shot, one_shot_tasks_.end());

    std::stable_sort(
        runnable.begin(),
        runnable.end(),
        [](const RunnableTask& lhs, const RunnableTask& rhs) {
            const auto lhs_phase = static_cast<std::uint32_t>(lhs.phase);
            const auto rhs_phase = static_cast<std::uint32_t>(rhs.phase);
            if (lhs.target_tick != rhs.target_tick) {
                return lhs.target_tick < rhs.target_tick;
            }
            if (lhs_phase != rhs_phase) {
                return lhs_phase < rhs_phase;
            }
            return lhs.sequence < rhs.sequence;
        });

    for (const auto& task : runnable) {
        task.callback(tick);
    }

    return runnable.size();
}

void TickScheduler::clear() {
    recurring_tasks_.clear();
    one_shot_tasks_.clear();
    next_task_id_ = 1;
    next_sequence_ = 1;
}

std::size_t TickScheduler::pendingTaskCount() const {
    return recurring_tasks_.size() + one_shot_tasks_.size();
}

}  // namespace sim::scheme_c
