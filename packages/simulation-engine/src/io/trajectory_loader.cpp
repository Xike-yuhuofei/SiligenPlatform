#include "sim/io/trajectory_loader.h"

#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "sim/control/io_model.h"
#include "sim/trajectory/arc_segment.h"
#include "sim/trajectory/line_segment.h"

namespace {

sim::Point2d parsePoint(const nlohmann::json& node) {
    return sim::Point2d{node.at("x").get<double>(), node.at("y").get<double>()};
}

sim::Trajectory loadTrajectory(const nlohmann::json& root) {
    sim::Trajectory trajectory;

    for (const auto& segment_node : root.at("segments")) {
        const auto type = segment_node.at("type").get<std::string>();
        if (type == "LINE") {
            trajectory.addSegment(std::make_unique<sim::LineSegment>(
                parsePoint(segment_node.at("start")),
                parsePoint(segment_node.at("end"))));
        } else if (type == "ARC") {
            trajectory.addSegment(std::make_unique<sim::ArcSegment>(
                parsePoint(segment_node.at("center")),
                segment_node.at("radius").get<double>(),
                segment_node.at("start_angle").get<double>(),
                segment_node.at("end_angle").get<double>()));
        } else {
            throw std::runtime_error("Unsupported segment type: " + type);
        }
    }

    return trajectory;
}

void loadIoDelays(const nlohmann::json& root, sim::SimulationContext& context) {
    if (!root.contains("io_delays")) {
        context.io_delays.push_back(sim::OutputDelaySpec{sim::channels::kValve, 0.003});
        context.io_delays.push_back(sim::OutputDelaySpec{sim::channels::kCameraTrigger, 0.001});
        return;
    }

    for (const auto& delay_node : root.at("io_delays")) {
        context.io_delays.push_back(sim::OutputDelaySpec{
            delay_node.at("channel").get<std::string>(),
            delay_node.value("delay_seconds", 0.0)
        });
    }
}

void loadTriggers(const nlohmann::json& root, sim::SimulationContext& context) {
    if (!root.contains("triggers")) {
        context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 40.0, true});
        context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 180.0, false});
        context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 240.0, true});
        context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 320.0, false});
        return;
    }

    for (const auto& trigger_node : root.at("triggers")) {
        context.triggers.push_back(sim::TriggerSpec{
            trigger_node.at("channel").get<std::string>(),
            trigger_node.at("trigger_position").get<double>(),
            trigger_node.value("state", true)
        });
    }
}

void loadValveConfig(const nlohmann::json& root, sim::SimulationContext& context) {
    if (!root.contains("valve")) {
        context.valve = sim::ValveConfig{0.012, 0.008};
        return;
    }

    const auto& valve_node = root.at("valve");
    context.valve = sim::ValveConfig{
        valve_node.value("open_delay_seconds", 0.012),
        valve_node.value("close_delay_seconds", 0.008)
    };
}

}  // namespace

namespace sim {

SimulationContext TrajectoryLoader::loadFromJsonFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open trajectory file: " + path);
    }

    nlohmann::json root;
    input >> root;

    SimulationContext context;
    context.trajectory = loadTrajectory(root);
    context.motion_limits.max_velocity = root.value("max_velocity", 220.0);
    context.motion_limits.max_acceleration = root.value("max_acceleration", 600.0);
    context.timestep_seconds = root.value("timestep_seconds", 0.001);
    context.max_simulation_time_seconds = root.value("max_simulation_time_seconds", 600.0);

    loadIoDelays(root, context);
    loadTriggers(root, context);
    loadValveConfig(root, context);

    return context;
}

}  // namespace sim
