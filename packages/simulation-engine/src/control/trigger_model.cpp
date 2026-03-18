#include "sim/control/trigger_model.h"

#include <algorithm>
#include <utility>

namespace sim {

void TriggerModel::initialize(std::vector<TriggerSpec> triggers) {
    triggers_ = std::move(triggers);
    std::sort(triggers_.begin(),
              triggers_.end(),
              [](const TriggerSpec& lhs, const TriggerSpec& rhs) {
                  return lhs.trigger_position < rhs.trigger_position;
              });
    next_trigger_index_ = 0;
}

void TriggerModel::update(double axis_position, double now, IOModel& io_model) {
    while (next_trigger_index_ < triggers_.size() &&
           axis_position >= triggers_[next_trigger_index_].trigger_position) {
        const auto& trigger = triggers_[next_trigger_index_];
        io_model.commandOutput(trigger.channel, trigger.state, now, "trigger");
        ++next_trigger_index_;
    }
}

}  // namespace sim
