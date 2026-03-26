#include "CommandHandlers.h"
#include "CommandHandlersInternal.h"

#include "infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h"
#include "application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "application/usecases/dispensing/PlanningUseCase.h"
#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h"
#include "domain/trajectory/value-objects/PlanningReport.h"
#include "shared/types/TrajectoryTypes.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Siligen::Adapters::CLI {

using Siligen::Application::CommandLineConfig;
using Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using Siligen::Application::UseCases::Dispensing::DispensingMVPRequest;
using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Application::UseCases::Dispensing::PlanningUseCase;
using Siligen::Infrastructure::Adapters::Planning::Geometry::ContourAugmentConfig;
using Siligen::Infrastructure::Adapters::Planning::Geometry::ContourAugmenterAdapter;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase;
using Siligen::Domain::Motion::InterpolationAlgorithm;
using Siligen::Domain::Trajectory::ValueObjects::PlanningReport;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::TrajectoryConfig;

namespace {

constexpr float32 kMaxVelocityUpper = 1000.0f;
constexpr float32 kMaxAccelerationUpper = 10000.0f;
constexpr float32 kMaxTimeStep = 0.1f;
constexpr float32 kEpsilon = 1e-6f;
constexpr const char* kForbiddenOverrideMetaChars = "&|;<>`";

bool HasForbiddenMetaChar(const std::string& value) {
    return value.find_first_of(kForbiddenOverrideMetaChars) != std::string::npos;
}

std::string QuoteArg(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) {
        return value;
    }
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (char c : value) {
        if (c == '"') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

bool HasLineBreak(const std::string& value) {
    return value.find('\n') != std::string::npos || value.find('\r') != std::string::npos;
}

void WarnDeprecatedPreviewPythonEnvOnce() {
    static bool warned = false;
    if (warned) {
        return;
    }
    warned = true;
    std::cerr << "[WARNING] 环境变量 SILIGEN_DXF_PREVIEW_PYTHON 已弃用，请改用 "
                 "SILIGEN_ENGINEERING_DATA_PYTHON。"
              << std::endl;
}

struct ProcessRunResult {
    bool ok = false;
    int exit_code = 1;
    std::string error;
};

#ifdef _WIN32
ProcessRunResult RunProcess(const std::vector<std::string>& args) {
    if (args.empty()) {
        return ProcessRunResult{false, 1, "外部命令参数为空"};
    }

    std::ostringstream command_stream;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            command_stream << ' ';
        }
        command_stream << QuoteArg(args[i]);
    }

    std::string command = command_stream.str();
    std::vector<char> mutable_command(command.begin(), command.end());
    mutable_command.push_back('\0');

    STARTUPINFOA startup_info{};
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags |= STARTF_USESTDHANDLES;
    startup_info.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
    startup_info.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION process_info{};
    const BOOL created = ::CreateProcessA(
        nullptr,
        mutable_command.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup_info,
        &process_info);
    if (!created) {
        const DWORD err = ::GetLastError();
        return ProcessRunResult{
            false,
            1,
            "启动预览进程失败: winerr=" + std::to_string(static_cast<unsigned long>(err))};
    }

    const DWORD wait_result = ::WaitForSingleObject(process_info.hProcess, INFINITE);
    if (wait_result == WAIT_FAILED) {
        const DWORD err = ::GetLastError();
        ::CloseHandle(process_info.hThread);
        ::CloseHandle(process_info.hProcess);
        return ProcessRunResult{
            false, 1, "等待预览进程失败: winerr=" + std::to_string(static_cast<unsigned long>(err))};
    }

    DWORD exit_code = 1;
    if (!::GetExitCodeProcess(process_info.hProcess, &exit_code)) {
        const DWORD err = ::GetLastError();
        ::CloseHandle(process_info.hThread);
        ::CloseHandle(process_info.hProcess);
        return ProcessRunResult{
            false, 1, "读取预览进程退出码失败: winerr=" + std::to_string(static_cast<unsigned long>(err))};
    }

    ::CloseHandle(process_info.hThread);
    ::CloseHandle(process_info.hProcess);
    return ProcessRunResult{true, static_cast<int>(exit_code), ""};
}
#else
ProcessRunResult RunProcess(const std::vector<std::string>& args) {
    if (args.empty()) {
        return ProcessRunResult{false, 1, "外部命令参数为空"};
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        return ProcessRunResult{false, 1, "fork 失败"};
    }
    if (pid == 0) {
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        ::execvp(argv.front(), argv.data());
        _exit(127);
    }

    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) {
        return ProcessRunResult{false, 1, "waitpid 失败"};
    }
    if (WIFEXITED(status)) {
        return ProcessRunResult{true, WEXITSTATUS(status), ""};
    }
    if (WIFSIGNALED(status)) {
        return ProcessRunResult{true, 128 + WTERMSIG(status), ""};
    }
    return ProcessRunResult{true, status, ""};
}
#endif

std::filesystem::path ResolvePreviewScriptPath() {
    const char* script_env = std::getenv("SILIGEN_DXF_PREVIEW_SCRIPT");
    if (script_env && *script_env) {
        return std::filesystem::path(script_env);
    }
    return Siligen::Infrastructure::Configuration::ResolveWorkspaceRelativePath(
        kDefaultPreviewScriptRelativePath);
}

std::string ResolvePreviewPythonExecutable() {
    const char* python_env = std::getenv("SILIGEN_ENGINEERING_DATA_PYTHON");
    if (python_env && *python_env) {
        return python_env;
    }
    python_env = std::getenv("SILIGEN_DXF_PREVIEW_PYTHON");
    if (python_env && *python_env) {
        WarnDeprecatedPreviewPythonEnvOnce();
        return python_env;
    }
    return "python";
}

double ResolvePreviewSpeed(const CommandLineConfig& config) {
    if (config.dxf_speed > 0.0) {
        return config.dxf_speed;
    }
    return 10.0;
}

size_t ResolvePreviewMaxPoints(const CommandLineConfig& config) {
    if (config.preview_max_points > 0) {
        return config.preview_max_points;
    }
    return 12000;
}

std::filesystem::path ResolvePreviewOutputDir(const CommandLineConfig& config, const std::filesystem::path& dxf_path) {
    if (!config.preview_output_dir.empty()) {
        return std::filesystem::path(config.preview_output_dir);
    }
    if (!config.dxf_output_path.empty()) {
        return std::filesystem::path(config.dxf_output_path);
    }

    if (dxf_path.has_parent_path()) {
        return dxf_path.parent_path();
    }
    return Siligen::Infrastructure::Configuration::ResolveWorkspaceRelativePath(
        kDefaultPreviewOutputDirRelativePath);
}

std::string ResolvePreviewTitle(const CommandLineConfig& config, const std::string& dxf_path) {
    if (!config.preview_title.empty()) {
        return config.preview_title;
    }
    return BuildPreviewTitle(dxf_path);
}

void WarnPreviewMaxPointsIgnored(const CommandLineConfig& config) {
    if (config.preview_max_points == 0) {
        return;
    }
    std::cout << "[WARNING] 参数 --preview-max-points 已迁移到 dxf-preview；当前命令不会消费该参数。"
              << std::endl;
}

bool TryParseInterpolationAlgorithm(const std::string& raw, InterpolationAlgorithm& out) {
    if (raw.empty()) {
        return false;
    }

    const std::string value = StringManipulator::ToLower(StringManipulator::Trim(raw));
    if (value == "0" || value == "linear" || value == "line") {
        out = InterpolationAlgorithm::LINEAR;
        return true;
    }
    if (value == "1" || value == "arc") {
        out = InterpolationAlgorithm::ARC;
        return true;
    }
    if (value == "2" || value == "spline") {
        out = InterpolationAlgorithm::SPLINE;
        return true;
    }
    if (value == "3" || value == "transition") {
        out = InterpolationAlgorithm::TRANSITION;
        return true;
    }
    if (value == "4" || value == "cmp" || value == "cmp_coordinated" || value == "cmp-coordinated") {
        out = InterpolationAlgorithm::CMP_COORDINATED;
        return true;
    }
    if (value == "5" || value == "seal_bead" || value == "seal-bead") {
        out = InterpolationAlgorithm::SEAL_BEAD;
        return true;
    }
    if (value == "6" || value == "grid_filling" || value == "grid-filling") {
        out = InterpolationAlgorithm::GRID_FILLING;
        return true;
    }
    if (value == "7" || value == "circular_array" || value == "circular-array") {
        out = InterpolationAlgorithm::CIRCULAR_ARRAY;
        return true;
    }
    return false;
}

struct VelocityTraceResult {
    bool ok = false;
    std::string path;
    std::string error;
    size_t point_count = 0;
};

struct PlanningReportCsvResult {
    bool ok = false;
    std::string path;
    std::string error;
};

std::string ResolveVelocityTracePath(const std::string& dxf_path, const CommandLineConfig& config) {
    std::string output_path = config.velocity_trace_path;
    const auto config_path = ResolveCliConfigPath(config.config_path);
    if (output_path.empty()) {
        std::string ini_value;
        if (ReadIniValue(config_path, "VelocityTrace", "output_path", ini_value)) {
            output_path = ini_value;
        }
    }
    if (output_path.empty()) {
        output_path = "logs/velocity_trace";
    }

    std::filesystem::path path(output_path);
    if (path.extension().empty()) {
        std::filesystem::path dxf(dxf_path);
        std::string stem = dxf.stem().string();
        if (stem.empty()) {
            stem = "dxf_velocity_trace";
        }
        path /= (stem + "_velocity_trace.csv");
    }
    return path.string();
}

std::string ResolvePlanningReportPath(const std::string& dxf_path, const CommandLineConfig& config) {
    std::string output_path = config.planning_report_path;
    const auto config_path = ResolveCliConfigPath(config.config_path);
    if (output_path.empty()) {
        std::string ini_value;
        if (ReadIniValue(config_path, "PlanningReport", "output_path", ini_value)) {
            output_path = ini_value;
        }
    }
    if (output_path.empty()) {
        output_path = "logs/planning_report";
    }

    std::filesystem::path path(output_path);
    if (path.extension().empty()) {
        std::filesystem::path dxf(dxf_path);
        std::string stem = dxf.stem().string();
        if (stem.empty()) {
            stem = "dxf_planning_report";
        }
        path /= (stem + "_planning_report.csv");
    }
    return path.string();
}

VelocityTraceResult WriteVelocityTraceCsv(
    const std::vector<Siligen::TrajectoryPoint>& points,
    int interval_ms,
    const std::string& output_path) {
    VelocityTraceResult result;
    if (points.empty()) {
        result.error = "轨迹点为空，无法生成速度曲线";
        return result;
    }

    const int clamped_interval = std::max(1, interval_ms);
    const float32 interval_s = static_cast<float32>(clamped_interval) / 1000.0f;

    std::filesystem::path path(output_path);
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream file(output_path, std::ios::binary);
    if (!file) {
        result.error = "无法写入速度曲线文件: " + output_path;
        return result;
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "timestamp_s,x_mm,y_mm,velocity_mm_s,dispense_on\n";

    float32 next_time = points.front().timestamp;
    size_t count = 0;
    for (const auto& pt : points) {
        if (pt.timestamp + kEpsilon < next_time) {
            continue;
        }
        file << pt.timestamp << ','
             << pt.position.x << ','
             << pt.position.y << ','
             << pt.velocity << ','
             << (pt.enable_position_trigger ? 1 : 0) << '\n';
        ++count;
        next_time += interval_s;
    }

    result.ok = true;
    result.path = output_path;
    result.point_count = count;
    return result;
}

PlanningReportCsvResult WritePlanningReportCsv(const PlanningReport& report, const std::string& output_path) {
    PlanningReportCsvResult result;
    if (output_path.empty()) {
        result.error = "规划报告输出路径为空";
        return result;
    }

    std::filesystem::path path(output_path);
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream file(output_path, std::ios::binary);
    if (!file) {
        result.error = "无法写入规划报告文件: " + output_path;
        return result;
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "total_length_mm,total_time_s,max_velocity,max_acceleration,max_jerk,"
            "constraint_violations,time_integration_error_s,time_integration_fallbacks,"
            "jerk_limit_enforced,jerk_plan_failed,segment_count,rapid_segment_count,"
            "rapid_length_mm,corner_segment_count,discontinuity_count\n";
    file << report.total_length_mm << ','
         << report.total_time_s << ','
         << report.max_velocity_observed << ','
         << report.max_acceleration_observed << ','
         << report.max_jerk_observed << ','
         << report.constraint_violations << ','
         << report.time_integration_error_s << ','
         << report.time_integration_fallbacks << ','
         << (report.jerk_limit_enforced ? 1 : 0) << ','
         << (report.jerk_plan_failed ? 1 : 0) << ','
         << report.segment_count << ','
         << report.rapid_segment_count << ','
         << report.rapid_length_mm << ','
         << report.corner_segment_count << ','
         << report.discontinuity_count << '\n';

    result.ok = true;
    result.path = output_path;
    return result;
}

bool ResolveTrajectoryConfig(const CommandLineConfig& config, TrajectoryConfig& trajectory, std::string& error_message) {
    trajectory = TrajectoryConfig();
    const auto config_path = ResolveCliConfigPath(config.config_path);

    double velocity = config.max_velocity;
    if (velocity < 0.0) {
        error_message = "最大速度必须为正数 (mm/s)";
        return false;
    }
    if (velocity <= 0.0) {
        double fallback = 0.0;
        if (ReadIniDoubleValue(config_path, "Machine", "max_speed", fallback) && fallback > 0.0) {
            velocity = fallback;
        } else if (ReadIniDoubleValue(config_path, "Hardware", "max_velocity_mm_s", fallback) && fallback > 0.0) {
            velocity = fallback;
        } else {
            velocity = trajectory.max_velocity;
        }
    }
    trajectory.max_velocity = static_cast<float32>(velocity);

    double acceleration = config.max_acceleration;
    if (acceleration < 0.0) {
        error_message = "最大加速度必须为正数 (mm/s^2)";
        return false;
    }
    if (acceleration <= 0.0) {
        double fallback = 0.0;
        if (ReadIniDoubleValue(config_path, "Machine", "max_acceleration", fallback) && fallback > 0.0) {
            acceleration = fallback;
        } else if (ReadIniDoubleValue(config_path, "Hardware", "max_acceleration_mm_s2", fallback) &&
                   fallback > 0.0) {
            acceleration = fallback;
        } else {
            acceleration = trajectory.max_acceleration;
        }
    }
    trajectory.max_acceleration = static_cast<float32>(acceleration);

    if (config.max_jerk < 0.0) {
        error_message = "最大加加速度必须为正数 (mm/s^3)";
        return false;
    }
    if (config.max_jerk > 0.0) {
        trajectory.max_jerk = static_cast<float32>(config.max_jerk);
    }

    trajectory.time_step = static_cast<float32>(config.time_step);
    trajectory.arc_tolerance = static_cast<float32>(config.arc_tolerance);
    trajectory.dispensing_interval = static_cast<float32>(config.dispensing_interval);
    trajectory.trigger_pulse_us = static_cast<uint16>(config.trigger_pulse_us);

    if (trajectory.max_velocity <= 0.0f || trajectory.max_velocity > kMaxVelocityUpper) {
        error_message = "最大速度超出范围 (0, 1000] mm/s";
        return false;
    }
    if (trajectory.max_acceleration <= 0.0f || trajectory.max_acceleration > kMaxAccelerationUpper) {
        error_message = "最大加速度超出范围 (0, 10000] mm/s^2";
        return false;
    }
    if (trajectory.time_step <= 0.0f || trajectory.time_step > kMaxTimeStep) {
        error_message = "采样周期超出范围 (0, 0.1] s";
        return false;
    }

    return true;
}

}  // namespace

int CLICommandHandlers::HandleDXFPlan(const CommandLineConfig& config) {
    WarnPreviewMaxPointsIgnored(config);
    const auto resolved = ApplyDxfDefaults(config);
    if (resolved.dxf_file_path.empty()) {
        std::cout << "请提供 DXF 文件路径 (--file)" << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(resolved.dxf_file_path)) {
        std::cout << "文件不存在或无法访问: " << resolved.dxf_file_path << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<PlanningUseCase>();
    if (!usecase) {
        std::cout << "无法解析 DXF 规划用例" << std::endl;
        return 1;
    }

    TrajectoryConfig trajectory;
    std::string trajectory_error;
    if (!ResolveTrajectoryConfig(resolved, trajectory, trajectory_error)) {
        std::cout << "轨迹参数无效: " << trajectory_error << std::endl;
        return 1;
    }

    PlanningRequest request;
    request.dxf_filepath = resolved.dxf_file_path;
    request.trajectory_config = trajectory;
    request.optimize_path = resolved.optimize_path;
    request.start_x = static_cast<float32>(resolved.start_x);
    request.start_y = static_cast<float32>(resolved.start_y);
    request.approximate_splines = resolved.approximate_splines;
    request.two_opt_iterations = resolved.two_opt_iterations;
    request.spline_max_step_mm = static_cast<float32>(resolved.spline_step_mm);
    request.spline_max_error_mm = static_cast<float32>(resolved.spline_error_mm);
    request.continuity_tolerance_mm = static_cast<float32>(resolved.continuity_tolerance_mm);
    request.curve_chain_angle_deg = static_cast<float32>(resolved.curve_chain_angle_deg);
    request.curve_chain_max_segment_mm = static_cast<float32>(resolved.curve_chain_max_segment_mm);
    request.use_hardware_trigger = resolved.use_hardware_trigger;
    request.use_interpolation_planner = resolved.use_interpolation_planner;
    request.spacing_tol_ratio = static_cast<float32>(resolved.spacing_tol_ratio);
    request.spacing_min_mm = static_cast<float32>(resolved.spacing_min_mm);
    request.spacing_max_mm = static_cast<float32>(resolved.spacing_max_mm);
    if (!resolved.interpolation_algorithm.empty()) {
        InterpolationAlgorithm algorithm;
        if (!TryParseInterpolationAlgorithm(resolved.interpolation_algorithm, algorithm)) {
            std::cout << "插补算法无效: " << resolved.interpolation_algorithm << std::endl;
            return 1;
        }
        request.interpolation_algorithm = algorithm;
    }

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    const auto& response = result.Value();
    std::cout << "DXF 规划成功" << std::endl;
    std::cout << "段数: " << response.segment_count << std::endl;
    std::cout << "总长度: " << response.total_length << " mm" << std::endl;
    std::cout << "预计时间: " << response.estimated_time << " s" << std::endl;
    std::cout << "触发点数: " << response.trigger_count << std::endl;
    std::cout << "轨迹点数: " << response.trajectory_points.size() << std::endl;

    if (resolved.velocity_trace) {
        const std::string trace_path = ResolveVelocityTracePath(resolved.dxf_file_path, resolved);
        auto trace_result = WriteVelocityTraceCsv(
            response.trajectory_points,
            resolved.velocity_trace_interval_ms,
            trace_path);
        if (trace_result.ok) {
            std::cout << "速度曲线已导出: " << trace_result.path
                      << " (点数=" << trace_result.point_count << ")" << std::endl;
        } else {
            std::cout << "速度曲线导出失败: " << trace_result.error << std::endl;
        }
    }

    if (resolved.planning_report) {
        const auto& report = response.planning_report;
        std::cout << "规划报告: "
                  << "总长度=" << report.total_length_mm << " mm, "
                  << "总时间=" << report.total_time_s << " s, "
                  << "最大速度=" << report.max_velocity_observed << " mm/s, "
                  << "最大加速度=" << report.max_acceleration_observed << " mm/s^2, "
                  << "最大加加速度=" << report.max_jerk_observed << " mm/s^3, "
                  << "约束违例=" << report.constraint_violations << std::endl;

        const std::string report_path = ResolvePlanningReportPath(resolved.dxf_file_path, resolved);
        auto report_result = WritePlanningReportCsv(report, report_path);
        if (report_result.ok) {
            std::cout << "规划报告已导出: " << report_result.path << std::endl;
        } else {
            std::cout << "规划报告导出失败: " << report_result.error << std::endl;
        }
    }

    return 0;
}

int CLICommandHandlers::HandleDXFAugment(const CommandLineConfig& config) {
    WarnPreviewMaxPointsIgnored(config);
    const auto resolved = ApplyDxfDefaults(config);
    if (resolved.dxf_file_path.empty()) {
        std::cout << "请提供 DXF 文件路径 (--file)" << std::endl;
        return 1;
    }

    std::filesystem::path input_path(resolved.dxf_file_path);
    if (!std::filesystem::exists(input_path)) {
        std::cout << "文件不存在或无法访问: " << resolved.dxf_file_path << std::endl;
        return 1;
    }

    std::filesystem::path output_path;
    if (!resolved.dxf_output_path.empty()) {
        output_path = resolved.dxf_output_path;
    } else {
        const std::string stem = input_path.stem().string().empty() ? "dxf_augmented"
                                                                    : (input_path.stem().string() + "_aug");
        output_path = input_path.parent_path() / (stem + ".dxf");
    }
    if (output_path.extension().empty()) {
        output_path += ".dxf";
    }
    if (output_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(output_path.parent_path(), ec);
    }

    ContourAugmenterAdapter augmenter;
    ContourAugmentConfig augment_config;
    augment_config.output_r12 = resolved.dxf_output_r12;
    auto result = augmenter.ConvertFile(input_path.string(), output_path.string(), augment_config);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << "DXF 轮廓增强完成: " << output_path.string() << std::endl;
    return 0;
}

int CLICommandHandlers::HandleDXFPreview(const CommandLineConfig& config) {
    const auto resolved = ApplyDxfDefaults(config);
    if (resolved.dxf_file_path.empty()) {
        std::cout << "请提供 DXF 文件路径 (--file)" << std::endl;
        return 1;
    }

    std::filesystem::path input_path(resolved.dxf_file_path);
    if (!std::filesystem::exists(input_path)) {
        std::cout << "文件不存在或无法访问: " << resolved.dxf_file_path << std::endl;
        return 1;
    }

    const auto script_path = ResolvePreviewScriptPath();
    if (!std::filesystem::exists(script_path)) {
        std::cout << "未找到预览脚本: " << script_path.string() << std::endl;
        return 1;
    }

    std::filesystem::path output_dir = ResolvePreviewOutputDir(resolved, input_path);
    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        std::cout << "无法创建预览输出目录: " << output_dir.string() << std::endl;
        return 1;
    }

    const std::string python = ResolvePreviewPythonExecutable();
    if (HasLineBreak(python) || HasForbiddenMetaChar(python)) {
        std::cout << "SILIGEN_ENGINEERING_DATA_PYTHON/SILIGEN_DXF_PREVIEW_PYTHON 含非法字符" << std::endl;
        return 1;
    }
    if (HasLineBreak(script_path.string()) || HasForbiddenMetaChar(script_path.string())) {
        std::cout << "SILIGEN_DXF_PREVIEW_SCRIPT 含非法字符" << std::endl;
        return 1;
    }
    const double speed = ResolvePreviewSpeed(resolved);
    const size_t max_points = ResolvePreviewMaxPoints(resolved);
    const std::string title = ResolvePreviewTitle(resolved, input_path.string());

    std::vector<std::string> command_args = {
        python,
        script_path.string(),
        "--input",
        input_path.string(),
        "--output-dir",
        output_dir.string(),
        "--speed",
        std::to_string(speed),
        "--max-points",
        std::to_string(max_points),
        "--deterministic",
    };
    if (resolved.preview_json) {
        command_args.emplace_back("--json");
    }
    if (!title.empty()) {
        command_args.emplace_back("--title");
        command_args.emplace_back(title);
    }

    const auto run_result = RunProcess(command_args);
    if (!run_result.ok) {
        std::cout << "DXF 预览命令执行失败: " << run_result.error << std::endl;
        return 1;
    }
    if (run_result.exit_code != 0) {
        std::cout << "DXF 预览生成失败，退出码: " << run_result.exit_code << std::endl;
        return 1;
    }

    if (!resolved.preview_json) {
        std::cout << "DXF 预览生成完成，输出目录: " << output_dir.string() << std::endl;
    }
    return 0;
}

int CLICommandHandlers::HandleDXFDispense(const CommandLineConfig& config) {
    WarnPreviewMaxPointsIgnored(config);
    const auto resolved = ApplyDxfDefaults(config);
    if (resolved.dry_run) {
        std::cout << "启用预览模式，不执行硬件点胶" << std::endl;
        return HandleDXFPlan(resolved);
    }

    auto ensure = EnsureConnected(resolved);
    if (ensure.IsError()) {
        PrintError(ensure.GetError());
        return 1;
    }

    if (resolved.dxf_file_path.empty()) {
        std::cout << "请提供 DXF 文件路径 (--file)" << std::endl;
        return 1;
    }

    auto not_homed_result = GetNotHomedAxes({LogicalAxisId::X, LogicalAxisId::Y});
    if (not_homed_result.IsError()) {
        PrintError(not_homed_result.GetError());
        return 1;
    }

    auto not_homed_axes = not_homed_result.Value();
    if (!not_homed_axes.empty()) {
        if (!resolved.auto_home_axes) {
            PrintError(BuildAxesNotHomedError(not_homed_axes));
            PrintHomingStatusDiagnostics(not_homed_axes);
            return 1;
        }

        std::cout << "检测到轴未回零，开始自动回零..." << std::endl;
        auto home_usecase = container_->Resolve<HomeAxesUseCase>();
        if (!home_usecase) {
            std::cout << "无法解析回零用例" << std::endl;
            return 1;
        }

        HomeAxesRequest request;
        request.home_all_axes = false;
        request.wait_for_completion = true;
        request.timeout_ms = resolved.timeout_ms;
        request.axes = not_homed_axes;

        auto home_result = home_usecase->Execute(request);
        if (home_result.IsError()) {
            PrintError(home_result.GetError());
            PrintHomingStatusDiagnostics(not_homed_axes);
            return 1;
        }

        if (!home_result.Value().failed_axes.empty()) {
            PrintHomingStatusDiagnostics(home_result.Value().failed_axes);
            return 1;
        }
    }

    auto usecase = container_->Resolve<DispensingExecutionUseCase>();
    if (!usecase) {
        std::cout << "无法解析 DXF 点胶执行用例" << std::endl;
        return 1;
    }

    DispensingMVPRequest request;
    request.dxf_filepath = resolved.dxf_file_path;
    request.optimize_path = resolved.optimize_path;
    request.start_x = static_cast<float32>(resolved.start_x);
    request.start_y = static_cast<float32>(resolved.start_y);
    request.approximate_splines = resolved.approximate_splines;
    request.two_opt_iterations = resolved.two_opt_iterations;
    request.spline_max_step_mm = static_cast<float32>(resolved.spline_step_mm);
    request.spline_max_error_mm = static_cast<float32>(resolved.spline_error_mm);
    request.continuity_tolerance_mm = static_cast<float32>(resolved.continuity_tolerance_mm);
    request.curve_chain_angle_deg = static_cast<float32>(resolved.curve_chain_angle_deg);
    request.curve_chain_max_segment_mm = static_cast<float32>(resolved.curve_chain_max_segment_mm);
    request.use_hardware_trigger = resolved.use_hardware_trigger;
    request.use_interpolation_planner = resolved.use_interpolation_planner;
    request.velocity_trace_enabled = resolved.velocity_trace;
    request.velocity_trace_interval_ms = resolved.velocity_trace_interval_ms;
    request.velocity_trace_path = resolved.velocity_trace_path;
    if (!resolved.interpolation_algorithm.empty()) {
        InterpolationAlgorithm algorithm;
        if (!TryParseInterpolationAlgorithm(resolved.interpolation_algorithm, algorithm)) {
            std::cout << "插补算法无效: " << resolved.interpolation_algorithm << std::endl;
            return 1;
        }
        request.interpolation_algorithm = algorithm;
    }
    if (resolved.max_jerk > 0.0) {
        request.max_jerk = static_cast<float32>(resolved.max_jerk);
    }
    if (resolved.arc_tolerance >= 0.0) {
        request.arc_tolerance_mm = static_cast<float32>(resolved.arc_tolerance);
    }
    if (resolved.dxf_speed > 0.0) {
        request.dispensing_speed_mm_s = static_cast<float32>(resolved.dxf_speed);
    }
    if (resolved.dxf_dry_run_speed > 0.0) {
        request.dry_run_speed_mm_s = static_cast<float32>(resolved.dxf_dry_run_speed);
    }

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    const auto& response = result.Value();
    std::cout << "\n执行结果: " << (response.success ? "成功" : "失败") << std::endl;
    if (!response.message.empty()) {
        std::cout << "消息: " << response.message << std::endl;
    }
    std::cout << "总段数: " << response.total_segments << std::endl;
    std::cout << "执行段数: " << response.executed_segments << std::endl;
    std::cout << "失败段数: " << response.failed_segments << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "总距离: " << response.total_distance << " mm" << std::endl;
    std::cout << "执行时间: " << response.execution_time_seconds << " 秒" << std::endl;
    std::cout << std::defaultfloat;

    if (response.quality_metrics_available) {
        const auto& qm = response.quality_metrics;
        std::cout << "质量指标摘要:" << std::endl;
        std::cout << "  线宽 CV: " << qm.line_width_cv << std::endl;
        std::cout << "  拐角偏差(%): " << qm.corner_deviation_pct << std::endl;
        std::cout << "  堆胶改善(%): " << qm.glue_pile_reduction_pct << std::endl;
        std::cout << "  触发间距误差(%): " << qm.trigger_interval_error_pct << std::endl;
        std::cout << "  运行次数: " << qm.run_count << std::endl;
        std::cout << "  中断次数: " << qm.interruption_count << std::endl;
    } else {
        std::cout << "质量指标摘要: 暂无" << std::endl;
    }

    if (!response.error_details.empty()) {
        std::cout << "错误详情: " << response.error_details << std::endl;
    }

    return response.success ? 0 : 1;
}

}  // namespace Siligen::Adapters::CLI


