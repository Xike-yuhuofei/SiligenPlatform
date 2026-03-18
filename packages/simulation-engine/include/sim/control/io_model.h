#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "sim/simulation_engine.h"

namespace sim {

namespace channels {
inline constexpr const char* kValve = "DO_VALVE";
inline constexpr const char* kCameraTrigger = "DO_CAMERA_TRIGGER";
}  // namespace channels

class IOModel {
public:
    void reset();
    void setDelay(const std::string& channel, double delay_seconds);
    void commandOutput(const std::string& channel,
                       bool state,
                       double now,
                       const std::string& source);
    void update(double now);

    bool getOutput(const std::string& channel) const;
    bool hasPendingEvents() const;
    const std::vector<TimelineEvent>& events() const;

private:
    struct PendingCommand {
        std::string channel;
        bool state{false};
        double execute_at{0.0};
        std::string source;
    };

    std::unordered_map<std::string, double> delays_{};
    std::unordered_map<std::string, bool> commanded_states_{};
    std::unordered_map<std::string, bool> actual_states_{};
    std::deque<PendingCommand> pending_{};
    std::vector<TimelineEvent> events_{};
};

}  // namespace sim
