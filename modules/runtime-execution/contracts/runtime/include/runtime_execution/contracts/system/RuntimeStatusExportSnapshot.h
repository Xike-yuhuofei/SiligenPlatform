#pragma once

#include "runtime_execution/contracts/system/RuntimeSupervisionSnapshot.h"

#include <cstdint>
#include <map>
#include <string>

namespace Siligen::RuntimeExecution::Contracts::System {

struct RuntimeAxisStatusExportSnapshot {
    double position = 0.0;
    double velocity = 0.0;
    bool enabled = false;
    bool homed = false;
    std::string homing_state = "unknown";
};

struct RuntimePositionExportSnapshot {
    double x = 0.0;
    double y = 0.0;
};

struct RuntimeDispenserStatusExportSnapshot {
    bool valve_open = false;
    bool supply_open = false;
    std::uint32_t completedCount = 0;
    std::uint32_t totalCount = 0;
    double progress = 0.0;
};

struct RuntimeJobExecutionExportSnapshot {
    std::string job_id;
    std::string plan_id;
    std::string plan_fingerprint;
    std::string state = "idle";
    std::uint32_t target_count = 0;
    std::uint32_t completed_count = 0;
    std::uint32_t current_cycle = 0;
    std::uint32_t current_segment = 0;
    std::uint32_t total_segments = 0;
    std::uint32_t cycle_progress_percent = 0;
    std::uint32_t overall_progress_percent = 0;
    double elapsed_seconds = 0.0;
    std::string error_message;
    bool dry_run = false;
};

struct RuntimeStatusExportSnapshot {
    bool connected = false;
    std::string connection_state = "disconnected";
    std::string machine_state = "Unknown";
    std::string machine_state_reason = "unknown";
    bool interlock_latched = false;
    RawIoStatus io;
    EffectiveInterlocksStatus effective_interlocks;
    SupervisionStatusSnapshot supervision;
    std::map<std::string, RuntimeAxisStatusExportSnapshot> axes;
    bool has_position = false;
    RuntimePositionExportSnapshot position;
    RuntimeDispenserStatusExportSnapshot dispenser;
    RuntimeJobExecutionExportSnapshot job_execution;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
