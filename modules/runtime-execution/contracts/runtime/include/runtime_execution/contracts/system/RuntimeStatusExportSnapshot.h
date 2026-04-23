#pragma once

#include "dispense_packaging/contracts/ExecutionPackage.h"
#include "runtime_execution/contracts/system/RuntimeSupervisionSnapshot.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

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

struct RuntimeIdentityExportSnapshot {
    std::string executable_path;
    std::string working_directory;
    std::string protocol_version;
    std::string preview_snapshot_contract;
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
    double execution_budget_s = 0.0;
    Siligen::Domain::Dispensing::Contracts::ExecutionBudgetBreakdown execution_budget_breakdown;
    std::string error_message;
    bool dry_run = false;
};

struct RuntimeSafetyBoundaryExportSnapshot {
    std::string state = "unknown";
    bool motion_permitted = false;
    bool process_output_permitted = false;
    bool estop_active = false;
    bool estop_known = false;
    bool door_open_active = false;
    bool door_open_known = false;
    bool interlock_latched = false;
    std::vector<std::string> blocking_reasons;
};

struct RuntimeActionCapabilitiesExportSnapshot {
    bool motion_commands_permitted = false;
    bool manual_output_commands_permitted = false;
    bool manual_dispenser_pause_permitted = false;
    bool manual_dispenser_resume_permitted = false;
    bool active_job_present = false;
    bool estop_reset_permitted = false;
};

struct RuntimeStatusExportSnapshot {
    bool connected = false;
    std::string connection_state = "disconnected";
    std::string device_mode = "production";
    std::string machine_state = "Unknown";
    std::string machine_state_reason = "unknown";
    RuntimeIdentityExportSnapshot runtime_identity;
    bool interlock_latched = false;
    RawIoStatus io;
    EffectiveInterlocksStatus effective_interlocks;
    RuntimeSafetyBoundaryExportSnapshot safety_boundary;
    RuntimeActionCapabilitiesExportSnapshot action_capabilities;
    SupervisionStatusSnapshot supervision;
    std::map<std::string, RuntimeAxisStatusExportSnapshot> axes;
    bool has_position = false;
    RuntimePositionExportSnapshot position;
    RuntimeDispenserStatusExportSnapshot dispenser;
    RuntimeJobExecutionExportSnapshot job_execution;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
