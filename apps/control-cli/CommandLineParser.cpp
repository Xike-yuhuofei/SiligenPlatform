#include "CommandLineParser.h"

#include "shared/Strings/StringManipulator.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

namespace Siligen::Application {

namespace {

using Siligen::StringManipulator;

[[noreturn]] void ThrowUsage(const std::string& message) {
    throw std::runtime_error(message);
}

std::string ConsumeValue(const std::string& arg, const std::string& flag, int argc, char* argv[], int& index) {
    const std::string prefix = flag + "=";
    if (arg == flag) {
        if (index + 1 >= argc) {
            ThrowUsage("缺少参数值: " + flag);
        }
        return argv[++index];
    }
    if (arg.rfind(prefix, 0) == 0) {
        return arg.substr(prefix.size());
    }
    return {};
}

template <typename T>
bool TryParseNumber(const std::string& raw, T& value) {
    std::istringstream iss(raw);
    return (iss >> value) && iss.eof();
}

template <typename T>
void AssignNumber(const std::string& flag, const std::string& raw, T& value) {
    if (!TryParseNumber(raw, value)) {
        ThrowUsage("参数无效: " + flag + "=" + raw);
    }
}

CommandType ParseRecipeAction(const std::string& action) {
    if (action == "create") return CommandType::RECIPE_CREATE;
    if (action == "update") return CommandType::RECIPE_UPDATE;
    if (action == "draft") return CommandType::RECIPE_DRAFT_CREATE;
    if (action == "draft-update") return CommandType::RECIPE_DRAFT_UPDATE;
    if (action == "publish") return CommandType::RECIPE_PUBLISH;
    if (action == "list") return CommandType::RECIPE_LIST;
    if (action == "get") return CommandType::RECIPE_GET;
    if (action == "versions") return CommandType::RECIPE_LIST_VERSIONS;
    if (action == "archive") return CommandType::RECIPE_ARCHIVE;
    if (action == "version-create") return CommandType::RECIPE_VERSION_CREATE;
    if (action == "compare") return CommandType::RECIPE_VERSION_COMPARE;
    if (action == "rollback" || action == "activate") return CommandType::RECIPE_VERSION_ACTIVATE;
    if (action == "audit") return CommandType::RECIPE_AUDIT;
    if (action == "export") return CommandType::RECIPE_EXPORT;
    if (action == "import") return CommandType::RECIPE_IMPORT;
    ThrowUsage("recipe 不支持的操作: " + action);
}

void ParsePrimaryCommand(CommandLineConfig& config, int argc, char* argv[], int& index, bool& command_set) {
    if (index != 1) {
        return;
    }

    const std::string arg = argv[index];
    if (arg == "bootstrap-check") {
        config.command = CommandType::BOOTSTRAP_CHECK;
        config.command_string = arg;
        command_set = true;
        return;
    }
    if (arg == "connect") {
        config.command = CommandType::CONNECT;
        config.command_string = arg;
        command_set = true;
        return;
    }
    if (arg == "disconnect") {
        config.command = CommandType::DISCONNECT;
        config.command_string = arg;
        command_set = true;
        return;
    }
    if (arg == "status") {
        config.command = CommandType::STATUS;
        config.command_string = arg;
        command_set = true;
        return;
    }
    if (arg == "home") {
        config.command = CommandType::HOME;
        config.command_string = arg;
        command_set = true;
        return;
    }
    if (arg == "jog") {
        config.command = CommandType::JOG;
        config.command_string = arg;
        command_set = true;
        return;
    }
    if (arg == "move") {
        config.command = CommandType::MOVE;
        config.command_string = arg;
        command_set = true;
        return;
    }
    if (arg == "stop-all") {
        config.command = CommandType::STOP_ALL;
        config.command_string = arg;
        command_set = true;
        return;
    }
    if (arg == "estop") {
        config.command = CommandType::EMERGENCY_STOP;
        config.command_string = arg;
        command_set = true;
        return;
    }
    if (arg == "dxf-plan") {
        config.command = CommandType::DXF_PLAN;
        config.command_string = arg;
        command_set = true;
        return;
    }
    if (arg == "dxf-dispense") {
        config.command = CommandType::DXF_DISPENSE;
        config.command_string = arg;
        config.mode = RunMode::DXF_DISPENSE;
        config.mode_string = arg;
        command_set = true;
        return;
    }
    if (arg == "dxf-augment") {
        config.command = CommandType::DXF_AUGMENT;
        config.command_string = arg;
        command_set = true;
        return;
    }
    if (arg == "dispenser") {
        if (index + 1 >= argc) {
            ThrowUsage("dispenser 需要指定 start|purge|stop|pause|resume");
        }
        const std::string action = argv[++index];
        if (action == "start") config.command = CommandType::DISPENSER_START;
        else if (action == "purge") config.command = CommandType::DISPENSER_PURGE;
        else if (action == "stop") config.command = CommandType::DISPENSER_STOP;
        else if (action == "pause") config.command = CommandType::DISPENSER_PAUSE;
        else if (action == "resume") config.command = CommandType::DISPENSER_RESUME;
        else ThrowUsage("dispenser 不支持的操作: " + action);
        config.command_string = "dispenser " + action;
        command_set = true;
        return;
    }
    if (arg == "supply") {
        if (index + 1 >= argc) {
            ThrowUsage("supply 需要指定 open|close");
        }
        const std::string action = argv[++index];
        if (action == "open") config.command = CommandType::SUPPLY_OPEN;
        else if (action == "close") config.command = CommandType::SUPPLY_CLOSE;
        else ThrowUsage("supply 不支持的操作: " + action);
        config.command_string = "supply " + action;
        command_set = true;
        return;
    }
    if (arg == "recipe" || arg == "recipes") {
        if (index + 1 >= argc) {
            ThrowUsage("recipe 需要指定子命令");
        }
        const std::string action = argv[++index];
        config.command = ParseRecipeAction(action);
        config.command_string = "recipe " + action;
        command_set = true;
        return;
    }
}

}  // namespace

RunMode CommandLineParser::ParseMode(const std::string& mode_str) {
    if (mode_str == "basic") return RunMode::BASIC;
    if (mode_str == "home") return RunMode::HOME;
    if (mode_str == "status") return RunMode::STATUS;
    if (mode_str == "dxf-dispense") return RunMode::DXF_DISPENSE;
    return RunMode::NONE;
}

bool CommandLineParser::ParseStringList(const std::string& raw, std::vector<std::string>& values) {
    std::istringstream stream(raw);
    std::string token;
    values.clear();
    while (std::getline(stream, token, ',')) {
        token = StringManipulator::Trim(token);
        if (!token.empty()) {
            values.push_back(token);
        }
    }
    return !values.empty();
}

CommandLineConfig CommandLineParser::Parse(int argc, char* argv[]) {
    CommandLineConfig config;
    bool command_set = false;

    if (argc <= 1) {
        config.command = CommandType::INTERACTIVE;
        config.command_string = "interactive";
        return config;
    }

    for (int i = 1; i < argc; ++i) {
        const int original_index = i;
        ParsePrimaryCommand(config, argc, argv, i, command_set);
        if (command_set && original_index == 1) {
            continue;
        }

        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            config.help = true;
            return config;
        }
        if (arg == "-v" || arg == "--version") {
            config.version = true;
            return config;
        }

        if (arg == "--verbose") {
            config.verbose = true;
            continue;
        }
        if (arg == "--no-interactive") {
            config.no_interactive = true;
            continue;
        }
        if (arg == "--all") {
            config.home_all_axes = true;
            continue;
        }
        if (arg == "--relative") {
            config.relative_move = true;
            continue;
        }
        if (arg == "--absolute") {
            config.relative_move = false;
            continue;
        }
        if (arg == "--wait") {
            config.wait_for_completion = true;
            continue;
        }
        if (arg == "--no-wait") {
            config.wait_for_completion = false;
            continue;
        }
        if (arg == "--wait-purge-key") {
            config.wait_purge_key = true;
            continue;
        }
        if (arg == "--immediate") {
            config.immediate_stop = true;
            continue;
        }
        if (arg == "--disable-safety-checks") {
            config.safety_checks = false;
            continue;
        }
        if (arg == "--no-auto-enable") {
            config.auto_enable = false;
            continue;
        }
        if (arg == "--show-status") {
            config.show_status = true;
            continue;
        }
        if (arg == "--show-motion") {
            config.show_motion = true;
            continue;
        }
        if (arg == "--show-io") {
            config.show_io = true;
            continue;
        }
        if (arg == "--show-valves") {
            config.show_valves = true;
            continue;
        }
        if (arg == "--no-heartbeat") {
            config.start_heartbeat = false;
            continue;
        }
        if (arg == "--status-monitor") {
            config.start_status_monitoring = true;
            continue;
        }
        if (arg == "--auto-home") {
            config.auto_home_axes = true;
            continue;
        }
        if (arg == "--optimize-path") {
            config.optimize_path = true;
            continue;
        }
        if (arg == "--no-optimize-path") {
            config.optimize_path = false;
            continue;
        }
        if (arg == "--approximate-splines") {
            config.approximate_splines = true;
            continue;
        }
        if (arg == "--dxf-r12") {
            config.dxf_output_r12 = true;
            continue;
        }
        if (arg == "--dry-run") {
            config.dry_run = true;
            continue;
        }
        if (arg == "--use-hardware-trigger") {
            config.use_hardware_trigger = true;
            continue;
        }
        if (arg == "--no-hardware-trigger") {
            config.use_hardware_trigger = false;
            continue;
        }
        if (arg == "--use-interpolation-planner" || arg == "--interpolation-planner") {
            config.use_interpolation_planner = true;
            continue;
        }
        if (arg == "--no-interpolation-planner") {
            config.use_interpolation_planner = false;
            continue;
        }
        if (arg == "--velocity-trace") {
            config.velocity_trace = true;
            continue;
        }
        if (arg == "--planning-report") {
            config.planning_report = true;
            continue;
        }

        if (const auto value = ConsumeValue(arg, "--mode", argc, argv, i); !value.empty()) {
            config.mode = ParseMode(value);
            config.mode_string = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--config", argc, argv, i); !value.empty()) {
            config.config_path = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--axis", argc, argv, i); !value.empty()) {
            AssignNumber("--axis", value, config.axis);
            config.home_all_axes = false;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--direction", argc, argv, i); !value.empty()) {
            AssignNumber("--direction", value, config.direction);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--position", argc, argv, i); !value.empty()) {
            AssignNumber("--position", value, config.position);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--distance", argc, argv, i); !value.empty()) {
            AssignNumber("--distance", value, config.distance);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--timeout-ms", argc, argv, i); !value.empty()) {
            AssignNumber("--timeout-ms", value, config.timeout_ms);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--velocity", argc, argv, i); !value.empty()) {
            AssignNumber("--velocity", value, config.velocity);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--acceleration", argc, argv, i); !value.empty()) {
            AssignNumber("--acceleration", value, config.acceleration);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--count", argc, argv, i); !value.empty()) {
            AssignNumber("--count", value, config.dispenser_count);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--interval-ms", argc, argv, i); !value.empty()) {
            AssignNumber("--interval-ms", value, config.dispenser_interval_ms);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--duration-ms", argc, argv, i); !value.empty()) {
            AssignNumber("--duration-ms", value, config.dispenser_duration_ms);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--dispensing-time", argc, argv, i); !value.empty()) {
            AssignNumber("--dispensing-time", value, config.dispensing_time);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--pulse-width", argc, argv, i); !value.empty()) {
            AssignNumber("--pulse-width", value, config.pulse_width);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--cmp-channel", argc, argv, i); !value.empty()) {
            AssignNumber("--cmp-channel", value, config.cmp_channel);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--segments", argc, argv, i); !value.empty()) {
            AssignNumber("--segments", value, config.segments);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--status-monitor-interval", argc, argv, i); !value.empty()) {
            AssignNumber("--status-monitor-interval", value, config.status_monitor_interval_ms);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--heartbeat-interval", argc, argv, i); !value.empty()) {
            AssignNumber("--heartbeat-interval", value, config.heartbeat_interval_ms);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--heartbeat-timeout", argc, argv, i); !value.empty()) {
            AssignNumber("--heartbeat-timeout", value, config.heartbeat_timeout_ms);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--heartbeat-failure-threshold", argc, argv, i); !value.empty()) {
            AssignNumber("--heartbeat-failure-threshold", value, config.heartbeat_failure_threshold);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--control-card-ip", argc, argv, i); !value.empty()) {
            config.control_card_ip = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--local-ip", argc, argv, i); !value.empty()) {
            config.local_ip = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--port", argc, argv, i); !value.empty()) {
            AssignNumber("--port", value, config.port);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--connect-timeout-ms", argc, argv, i); !value.empty()) {
            AssignNumber("--connect-timeout-ms", value, config.connect_timeout_ms);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--file", argc, argv, i); !value.empty()) {
            config.dxf_file_path = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--output", argc, argv, i); !value.empty()) {
            config.dxf_output_path = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--two-opt-iterations", argc, argv, i); !value.empty()) {
            AssignNumber("--two-opt-iterations", value, config.two_opt_iterations);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--spline-step-mm", argc, argv, i); !value.empty()) {
            AssignNumber("--spline-step-mm", value, config.spline_step_mm);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--spline-error-mm", argc, argv, i); !value.empty()) {
            AssignNumber("--spline-error-mm", value, config.spline_error_mm);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--continuity-tolerance", argc, argv, i); !value.empty()) {
            AssignNumber("--continuity-tolerance", value, config.continuity_tolerance_mm);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--curve-chain-angle", argc, argv, i); !value.empty()) {
            AssignNumber("--curve-chain-angle", value, config.curve_chain_angle_deg);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--curve-chain-max-seg", argc, argv, i); !value.empty()) {
            AssignNumber("--curve-chain-max-seg", value, config.curve_chain_max_segment_mm);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--start-x", argc, argv, i); !value.empty()) {
            AssignNumber("--start-x", value, config.start_x);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--start-y", argc, argv, i); !value.empty()) {
            AssignNumber("--start-y", value, config.start_y);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--dxf-speed", argc, argv, i); !value.empty()) {
            AssignNumber("--dxf-speed", value, config.dxf_speed);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--dry-run-speed", argc, argv, i); !value.empty()) {
            AssignNumber("--dry-run-speed", value, config.dxf_dry_run_speed);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--max-vel", argc, argv, i); !value.empty()) {
            AssignNumber("--max-vel", value, config.max_velocity);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--max-acc", argc, argv, i); !value.empty()) {
            AssignNumber("--max-acc", value, config.max_acceleration);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--max-jerk", argc, argv, i); !value.empty()) {
            AssignNumber("--max-jerk", value, config.max_jerk);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--time-step", argc, argv, i); !value.empty()) {
            AssignNumber("--time-step", value, config.time_step);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--arc-tolerance", argc, argv, i); !value.empty()) {
            AssignNumber("--arc-tolerance", value, config.arc_tolerance);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--dispensing-interval", argc, argv, i); !value.empty()) {
            AssignNumber("--dispensing-interval", value, config.dispensing_interval);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--trigger-pulse-us", argc, argv, i); !value.empty()) {
            AssignNumber("--trigger-pulse-us", value, config.trigger_pulse_us);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--interpolation-algorithm", argc, argv, i); !value.empty()) {
            config.interpolation_algorithm = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--spacing-tol-ratio", argc, argv, i); !value.empty()) {
            AssignNumber("--spacing-tol-ratio", value, config.spacing_tol_ratio);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--spacing-min", argc, argv, i); !value.empty()) {
            AssignNumber("--spacing-min", value, config.spacing_min_mm);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--spacing-max", argc, argv, i); !value.empty()) {
            AssignNumber("--spacing-max", value, config.spacing_max_mm);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--velocity-trace-interval", argc, argv, i); !value.empty()) {
            AssignNumber("--velocity-trace-interval", value, config.velocity_trace_interval_ms);
            config.velocity_trace = true;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--velocity-trace-path", argc, argv, i); !value.empty()) {
            config.velocity_trace_path = value;
            config.velocity_trace = true;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--planning-report-path", argc, argv, i); !value.empty()) {
            config.planning_report_path = value;
            config.planning_report = true;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--preview-max-points", argc, argv, i); !value.empty()) {
            AssignNumber("--preview-max-points", value, config.preview_max_points);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--purge-key-channel", argc, argv, i); !value.empty()) {
            AssignNumber("--purge-key-channel", value, config.purge_key_channel);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--recipe-id", argc, argv, i); !value.empty()) {
            config.recipe_id = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--name", argc, argv, i); !value.empty()) {
            config.recipe_name = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--description", argc, argv, i); !value.empty()) {
            config.recipe_description = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--tags", argc, argv, i); !value.empty()) {
            if (!ParseStringList(value, config.recipe_tags)) {
                ThrowUsage("参数无效: --tags=" + value);
            }
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--recipe-status", argc, argv, i); !value.empty()) {
            config.recipe_status = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--recipe-query", argc, argv, i); !value.empty()) {
            config.recipe_query = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--recipe-tag", argc, argv, i); !value.empty()) {
            config.recipe_tag = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--template-id", argc, argv, i); !value.empty()) {
            config.template_id = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--version-id", argc, argv, i); !value.empty()) {
            config.version_id = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--base-version-id", argc, argv, i); !value.empty()) {
            config.base_version_id = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--version-label", argc, argv, i); !value.empty()) {
            config.version_label = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--change-note", argc, argv, i); !value.empty()) {
            config.change_note = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--actor", argc, argv, i); !value.empty()) {
            config.actor = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--param", argc, argv, i); !value.empty()) {
            config.recipe_params.push_back(value);
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--export-file", argc, argv, i); !value.empty()) {
            config.export_path = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--import-file", argc, argv, i); !value.empty()) {
            config.import_path = value;
            continue;
        }
        if (const auto value = ConsumeValue(arg, "--resolution", argc, argv, i); !value.empty()) {
            config.conflict_resolution = value;
            continue;
        }

        ThrowUsage("未知参数: " + arg);
    }

    if (config.mode == RunMode::NONE) {
        config.mode = RunMode::BASIC;
        config.mode_string = "basic";
    }

    if (!command_set) {
        if (config.mode == RunMode::DXF_DISPENSE) {
            config.command = CommandType::DXF_DISPENSE;
            config.command_string = "dxf-dispense";
        } else if (config.mode == RunMode::HOME) {
            config.command = CommandType::HOME;
            config.command_string = "home";
        } else if (config.mode == RunMode::STATUS) {
            config.command = CommandType::STATUS;
            config.command_string = "status";
        } else {
            config.command = CommandType::INTERACTIVE;
            config.command_string = "interactive";
        }
    }

    return config;
}

}  // namespace Siligen::Application
