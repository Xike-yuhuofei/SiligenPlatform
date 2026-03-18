#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "sim/control/io_model.h"
#include "sim/io/result_exporter.h"
#include "sim/io/trajectory_loader.h"

namespace {

std::filesystem::path writeTempFile(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Failed to create temporary test file: " + path.string());
    }

    output << content;
    return path;
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testTrajectoryLoaderParsesExplicitConfig() {
    const auto path = writeTempFile(
        "simulation_engine_loader_explicit.json",
        R"({
  "timestep_seconds": 0.002,
  "max_simulation_time_seconds": 10.0,
  "max_velocity": 123.0,
  "max_acceleration": 456.0,
  "segments": [
    {
      "type": "LINE",
      "start": { "x": 0.0, "y": 0.0 },
      "end": { "x": 10.0, "y": 0.0 }
    },
    {
      "type": "ARC",
      "center": { "x": 10.0, "y": 5.0 },
      "radius": 5.0,
      "start_angle": -1.57079632679,
      "end_angle": 0.0
    }
  ],
  "io_delays": [
    { "channel": "DO_VALVE", "delay_seconds": 0.003 },
    { "channel": "DO_CAMERA_TRIGGER", "delay_seconds": 0.004 }
  ],
  "triggers": [
    { "channel": "DO_VALVE", "trigger_position": 4.5, "state": true },
    { "channel": "DO_VALVE", "trigger_position": 8.5, "state": false }
  ],
  "valve": {
    "open_delay_seconds": 0.015,
    "close_delay_seconds": 0.009
  }
})");

    const auto context = sim::TrajectoryLoader::loadFromJsonFile(path.string());

    require(context.timestep_seconds == 0.002, "explicit config: timestep mismatch");
    require(context.max_simulation_time_seconds == 10.0, "explicit config: max time mismatch");
    require(context.motion_limits.max_velocity == 123.0, "explicit config: max velocity mismatch");
    require(context.motion_limits.max_acceleration == 456.0, "explicit config: max acceleration mismatch");
    require(context.trajectory.segmentCount() == 2U, "explicit config: segment count mismatch");
    require(context.io_delays.size() == 2U, "explicit config: io delay count mismatch");
    require(context.io_delays[1].channel == sim::channels::kCameraTrigger,
            "explicit config: io delay channel mismatch");
    require(context.io_delays[1].delay_seconds == 0.004, "explicit config: io delay value mismatch");
    require(context.triggers.size() == 2U, "explicit config: trigger count mismatch");
    require(context.triggers[0].trigger_position == 4.5, "explicit config: trigger position mismatch");
    require(context.triggers[1].state == false, "explicit config: trigger state mismatch");
    require(context.valve.open_delay_seconds == 0.015, "explicit config: valve open delay mismatch");
    require(context.valve.close_delay_seconds == 0.009, "explicit config: valve close delay mismatch");

    std::filesystem::remove(path);
}

void testTrajectoryLoaderUsesDefaults() {
    const auto path = writeTempFile(
        "simulation_engine_loader_defaults.json",
        R"({
  "segments": [
    {
      "type": "LINE",
      "start": { "x": 0.0, "y": 0.0 },
      "end": { "x": 20.0, "y": 0.0 }
    }
  ]
})");

    const auto context = sim::TrajectoryLoader::loadFromJsonFile(path.string());

    require(context.timestep_seconds == 0.001, "defaults: timestep mismatch");
    require(context.max_simulation_time_seconds == 600.0, "defaults: max time mismatch");
    require(context.motion_limits.max_velocity == 220.0, "defaults: max velocity mismatch");
    require(context.motion_limits.max_acceleration == 600.0, "defaults: max acceleration mismatch");
    require(context.io_delays.size() == 2U, "defaults: io delay count mismatch");
    require(context.io_delays[0].channel == sim::channels::kValve, "defaults: valve channel mismatch");
    require(context.io_delays[0].delay_seconds == 0.003, "defaults: valve delay mismatch");
    require(context.triggers.size() == 4U, "defaults: trigger count mismatch");
    require(context.valve.open_delay_seconds == 0.012, "defaults: valve open delay mismatch");
    require(context.valve.close_delay_seconds == 0.008, "defaults: valve close delay mismatch");

    std::filesystem::remove(path);
}

void testTrajectoryLoaderRejectsUnsupportedSegment() {
    const auto path = writeTempFile(
        "simulation_engine_loader_invalid.json",
        R"({
  "segments": [
    {
      "type": "SPLINE",
      "start": { "x": 0.0, "y": 0.0 },
      "end": { "x": 20.0, "y": 0.0 }
    }
  ]
})");

    bool threw = false;
    try {
        static_cast<void>(sim::TrajectoryLoader::loadFromJsonFile(path.string()));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    require(threw, "invalid segment: expected runtime_error");
    std::filesystem::remove(path);
}

void testResultExporterSerializesExpectedFields() {
    sim::SimulationResult result;
    result.total_time = 1.25;
    result.motion_distance = 42.0;
    result.valve_open_time = 0.5;
    result.motion_profile.push_back(sim::MotionSample{0.001, 1.0, 2.0, 3.0, 4U, 0.25});
    result.io_events.push_back(sim::TimelineEvent{0.01, sim::channels::kValve, true, "trigger"});
    result.valve_events.push_back(sim::TimelineEvent{0.02, sim::channels::kValve, true, "valve-delay"});

    const auto json_text = sim::ResultExporter::toJson(result, false);
    const auto json = nlohmann::json::parse(json_text);

    require(json.at("total_time").get<double>() == 1.25, "exporter: total_time mismatch");
    require(json.at("motion_distance").get<double>() == 42.0, "exporter: motion_distance mismatch");
    require(json.at("valve_open_time").get<double>() == 0.5, "exporter: valve_open_time mismatch");
    require(json.at("motion_profile").size() == 1U, "exporter: motion_profile size mismatch");
    require(json.at("motion_profile").at(0).at("segment_index").get<std::size_t>() == 4U,
            "exporter: segment_index mismatch");
    require(json.at("io_events").size() == 1U, "exporter: io_events size mismatch");
    require(json.at("io_events").at(0).at("source").get<std::string>() == "trigger",
            "exporter: io event source mismatch");
    require(json.at("valve_events").size() == 1U, "exporter: valve_events size mismatch");

    const auto path = std::filesystem::temp_directory_path() / "simulation_engine_result_export.json";
    sim::ResultExporter::writeJsonFile(result, path.string(), true);
    require(std::filesystem::exists(path), "exporter: result file missing");

    std::ifstream input(path);
    nlohmann::json file_json;
    input >> file_json;
    require(file_json.at("motion_profile").size() == 1U, "exporter: persisted motion_profile mismatch");
    input.close();

    std::filesystem::remove(path);
}

}  // namespace

int main() {
    try {
        testTrajectoryLoaderParsesExplicitConfig();
        testTrajectoryLoaderUsesDefaults();
        testTrajectoryLoaderRejectsUnsupportedSegment();
        testResultExporterSerializesExpectedFields();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
