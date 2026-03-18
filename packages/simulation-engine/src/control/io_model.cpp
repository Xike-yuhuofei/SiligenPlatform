#include "sim/control/io_model.h"

#include <algorithm>

namespace sim {

void IOModel::reset() {
    delays_.clear();
    commanded_states_.clear();
    actual_states_.clear();
    pending_.clear();
    events_.clear();
}

void IOModel::setDelay(const std::string& channel, double delay_seconds) {
    delays_[channel] = std::max(0.0, delay_seconds);
}

void IOModel::commandOutput(const std::string& channel,
                            bool state,
                            double now,
                            const std::string& source) {
    const auto current = commanded_states_.find(channel);
    if (current != commanded_states_.end() && current->second == state) {
        return;
    }

    commanded_states_[channel] = state;
    pending_.push_back(PendingCommand{
        channel,
        state,
        now + delays_[channel],
        source
    });
}

void IOModel::update(double now) {
    while (!pending_.empty() && pending_.front().execute_at <= now + 1e-12) {
        const auto command = pending_.front();
        pending_.pop_front();

        const auto actual = actual_states_.find(command.channel);
        if (actual != actual_states_.end() && actual->second == command.state) {
            continue;
        }

        actual_states_[command.channel] = command.state;
        events_.push_back(TimelineEvent{
            now,
            command.channel,
            command.state,
            command.source
        });
    }
}

bool IOModel::getOutput(const std::string& channel) const {
    const auto iterator = actual_states_.find(channel);
    return iterator != actual_states_.end() ? iterator->second : false;
}

bool IOModel::hasPendingEvents() const {
    return !pending_.empty();
}

const std::vector<TimelineEvent>& IOModel::events() const {
    return events_;
}

}  // namespace sim
