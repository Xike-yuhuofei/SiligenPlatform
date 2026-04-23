#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime_process_bootstrap/WorkspaceAssetPaths.h"

namespace Siligen::Application {

enum class RunMode {
    NONE,
    BASIC,
    HOME,
    STATUS,
    DXF_DISPENSE
};

enum class CommandType {
    NONE,
    BOOTSTRAP_CHECK,
    INTERACTIVE,
    CONNECT,
    DISCONNECT,
    STATUS,
    HOME,
    JOG,
    MOVE,
    STOP_ALL,
    EMERGENCY_STOP,
    DISPENSER_START,
    DISPENSER_PURGE,
    DISPENSER_STOP,
    DISPENSER_PAUSE,
    DISPENSER_RESUME,
    SUPPLY_OPEN,
    SUPPLY_CLOSE,
    DXF_PLAN,
    DXF_DISPENSE,
    DXF_PREVIEW,
    DXF_PREVIEW_SNAPSHOT,
    DXF_AUGMENT
};

struct CommandLineConfig {
    RunMode mode = RunMode::NONE;
    CommandType command = CommandType::NONE;
    std::string config_path = Siligen::Infrastructure::Configuration::CanonicalMachineConfigRelativePath();

    bool help = false;
    bool version = false;
    bool no_interactive = false;

    double velocity = 0.0;
    double acceleration = 0.0;

    std::string control_card_ip = "192.168.0.1";
    std::string local_ip = "192.168.0.200";
    int port = 0;
    int connect_timeout_ms = 0;

    std::string dxf_file_path;
    std::string dxf_output_path;
    bool dxf_output_r12 = true;
    bool dry_run = false;
    double dxf_speed = 0.0;
    double dxf_dry_run_speed = 0.0;
    std::string preview_output_dir;
    std::string preview_title;
    bool preview_json = false;

    bool show_motion = false;
    bool show_io = false;
    bool show_valves = false;

    short axis = 1;
    int direction = 1;
    double position = 0.0;
    double distance = 0.0;
    bool relative_move = false;
    bool wait_for_completion = true;
    int timeout_ms = 30000;
    bool immediate_stop = false;
    bool home_all_axes = true;

    bool auto_connect = true;
    bool start_heartbeat = true;
    bool start_status_monitoring = false;
    bool auto_home_axes = false;
    int status_monitor_interval_ms = 1000;
    int heartbeat_interval_ms = 3000;
    int heartbeat_timeout_ms = 5000;
    int heartbeat_failure_threshold = 3;

    uint32_t dispenser_count = 10;
    uint32_t dispenser_interval_ms = 200;
    uint32_t dispenser_duration_ms = 100;
    bool wait_purge_key = false;
    int purge_key_channel = 2;

    bool optimize_path = true;
    double start_x = 0.0;
    double start_y = 0.0;
    bool approximate_splines = false;
    int two_opt_iterations = 0;
    double spline_step_mm = 0.0;
    double spline_error_mm = 0.0;
    double continuity_tolerance_mm = 0.0;
    double curve_chain_angle_deg = 0.0;
    double curve_chain_max_segment_mm = 0.0;
    double max_velocity = 0.0;
    double max_acceleration = 0.0;
    double max_jerk = 0.0;
    double time_step = 0.001;
    double arc_tolerance = 0.01;
    double dispensing_interval = 3.0;
    int trigger_pulse_us = 100;
    bool use_interpolation_planner = false;
    std::string interpolation_algorithm;
    double spacing_tol_ratio = 0.0;
    double spacing_min_mm = 0.0;
    double spacing_max_mm = 0.0;
    bool velocity_trace = false;
    int velocity_trace_interval_ms = 50;
    std::string velocity_trace_path;
    bool planning_report = false;
    std::string planning_report_path;
    size_t preview_max_points = 0;
};

class CommandLineParser {
   public:
    static CommandLineConfig Parse(int argc, char* argv[]);

   private:
    static RunMode ParseMode(const std::string& mode_str);
};

}  // namespace Siligen::Application
