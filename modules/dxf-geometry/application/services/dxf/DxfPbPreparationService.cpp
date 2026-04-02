#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "DxfPbPreparationService"

namespace Siligen::Application::Services::DXF {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {
constexpr const char* kDxFProjectRootEnv = "SILIGEN_DXF_PROJECT_ROOT";
constexpr const char* kDxFProjectLauncherRelative = "scripts/dxf_core_launcher.py";
constexpr const char* kCanonicalDxFToPbCommand = "engineering-data-dxf-to-pb";
constexpr const char* kForbiddenMetaChars = "&|;<>`";
constexpr const char* kEngineeringDataPythonEnv = "SILIGEN_ENGINEERING_DATA_PYTHON";
constexpr const char* kLegacyPbPythonEnv = "SILIGEN_DXF_PB_PYTHON";
constexpr std::array<const char*, 1> kDefaultPbScriptRelativePaths = {
    "scripts/engineering-data/dxf_to_pb.py",
};

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

void AppendBoolArg(std::vector<std::string>& args, const std::string& flag, bool enabled) {
    if (enabled) {
        args.push_back(flag);
        return;
    }
    if (flag.rfind("--", 0) == 0) {
        args.push_back("--no-" + flag.substr(2));
        return;
    }
    args.push_back("no-" + flag);
}

template <typename T>
void AppendValueArg(std::vector<std::string>& args, const std::string& flag, T value) {
    args.push_back(flag);
    args.push_back(std::to_string(value));
}

std::string ReplaceAll(std::string value, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return value;
    }

    size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
    return value;
}

std::string NormalizeExtension(std::filesystem::path path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

std::filesystem::path FindAncestorContainingRelativePath(const std::filesystem::path& start,
                                                         const std::filesystem::path& relative_path) {
    namespace fs = std::filesystem;

    if (start.empty()) {
        return {};
    }

    std::error_code ec;
    fs::path candidate = start;
    if (!fs::is_directory(candidate, ec)) {
        candidate = candidate.parent_path();
    }
    ec.clear();

    constexpr int kMaxSearchLevels = 12;
    for (int level = 0; level < kMaxSearchLevels && !candidate.empty(); ++level) {
        const auto script_path = candidate / relative_path;
        if (fs::exists(script_path, ec) && !ec) {
            return script_path;
        }
        ec.clear();

        auto parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }

    return {};
}

std::filesystem::path ResolveDefaultPbScriptPath(const std::filesystem::path& dxf_path) {
    namespace fs = std::filesystem;

    std::error_code ec;
    std::vector<fs::path> anchors;
    anchors.push_back(dxf_path.parent_path());
    anchors.push_back(fs::current_path(ec));
    ec.clear();

    for (const auto& anchor : anchors) {
        for (const auto* relative_path_text : kDefaultPbScriptRelativePaths) {
            auto resolved = FindAncestorContainingRelativePath(anchor, fs::path(relative_path_text));
            if (!resolved.empty()) {
                return resolved;
            }
        }
    }

    const auto current_path = fs::current_path(ec);
    if (ec) {
        return fs::path(kDefaultPbScriptRelativePaths.front());
    }
    return current_path / fs::path(kDefaultPbScriptRelativePaths.front());
}

std::string DescribeDefaultPbScriptLocations() {
    std::ostringstream out;
    out << "未找到本地 DXF->PB 脚本(已搜索: ";
    for (size_t i = 0; i < kDefaultPbScriptRelativePaths.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << kDefaultPbScriptRelativePaths[i];
    }
    out << ")";
    return out.str();
}

Result<void> ValidatePbOutput(const std::filesystem::path& pb_path) {
    std::error_code ec;
    if (!std::filesystem::exists(pb_path, ec)) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, "PB output not found: " + pb_path.string(), MODULE_NAME));
    }

    const auto pb_size = std::filesystem::file_size(pb_path, ec);
    if (ec || pb_size == 0) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_FORMAT_INVALID, "PB output is empty: " + pb_path.string(), MODULE_NAME));
    }

    return Result<void>::Success();
}

bool NeedsGeneration(const std::filesystem::path& dxf_path, const std::filesystem::path& pb_path) {
    std::error_code pb_exists_ec;
    if (!std::filesystem::exists(pb_path, pb_exists_ec)) {
        return true;
    }

    std::error_code dxf_time_ec;
    std::error_code pb_time_ec;
    const auto dxf_time = std::filesystem::last_write_time(dxf_path, dxf_time_ec);
    const auto pb_time = std::filesystem::last_write_time(pb_path, pb_time_ec);
    if (!dxf_time_ec && !pb_time_ec && dxf_time > pb_time) {
        return true;
    }

    return false;
}

bool HasDxFLauncher(const std::filesystem::path& root) {
    std::error_code ec;
    return std::filesystem::exists(root / kDxFProjectLauncherRelative, ec);
}

bool HasForbiddenMetaChar(const std::string& value) {
    return value.find_first_of(kForbiddenMetaChars) != std::string::npos;
}

void WarnDeprecatedPbPythonEnvOnce() {
    static bool warned = false;
    if (warned) {
        return;
    }
    warned = true;
    SILIGEN_LOG_WARNING(std::string(kLegacyPbPythonEnv) + " 已弃用，请改用 " + kEngineeringDataPythonEnv);
}

Result<std::vector<std::string>> TokenizeCommandLine(const std::string& command_text) {
    std::vector<std::string> tokens;
    std::string current;
    current.reserve(command_text.size());

    char active_quote = '\0';
    for (char c : command_text) {
        if (active_quote != '\0') {
            if (c == active_quote) {
                active_quote = '\0';
            } else {
                current.push_back(c);
            }
            continue;
        }

        if (c == '"' || c == '\'') {
            active_quote = c;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(c);
    }

    if (active_quote != '\0') {
        return Result<std::vector<std::string>>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR,
                  "SILIGEN_DXF_PB_COMMAND 存在未闭合引号",
                  MODULE_NAME));
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    if (tokens.empty()) {
        return Result<std::vector<std::string>>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR,
                  "SILIGEN_DXF_PB_COMMAND 为空或无法解析为命令",
                  MODULE_NAME));
    }
    return Result<std::vector<std::string>>::Success(tokens);
}

Result<std::filesystem::path> ResolveExternalDxFProjectRoot() {
    namespace fs = std::filesystem;

    const char* explicit_root = std::getenv(kDxFProjectRootEnv);
    if (!(explicit_root && *explicit_root)) {
        return Result<fs::path>::Success({});
    }

    fs::path root = fs::path(explicit_root);
    if (!HasDxFLauncher(root)) {
        return Result<fs::path>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND,
                  std::string(kDxFProjectRootEnv) + " 指向的外部DXF仓库无效（缺少 " +
                      kDxFProjectLauncherRelative + "）: " + root.string(),
                  MODULE_NAME));
    }
    return Result<fs::path>::Success(root);
}

Result<std::vector<std::string>> BuildPbCommandArgs(
    const std::filesystem::path& dxf_path,
    const std::filesystem::path& pb_path,
    const std::string& python,
    const std::filesystem::path& external_root,
    const Siligen::Domain::Configuration::Ports::DxfPreprocessConfig& preprocess_config) {
    namespace fs = std::filesystem;

    std::vector<std::string> args;
    if (!external_root.empty()) {
        const fs::path launcher_path = external_root / kDxFProjectLauncherRelative;
        args = {
            python,
            launcher_path.string(),
            kCanonicalDxFToPbCommand,
            "--input",
            dxf_path.string(),
            "--output",
            pb_path.string(),
        };

        AppendBoolArg(args, "--normalize-units", preprocess_config.normalize_units);
        AppendBoolArg(args, "--strict-r12", preprocess_config.strict_r12);
        AppendBoolArg(args, "--approx-splines", preprocess_config.approx_splines);
        AppendBoolArg(args, "--snap-enabled", preprocess_config.snap_enabled);
        AppendBoolArg(args, "--densify-enabled", preprocess_config.densify_enabled);
        AppendBoolArg(args, "--min-seg-enabled", preprocess_config.min_seg_enabled);
        AppendValueArg(args, "--spline-samples", preprocess_config.spline_samples);
        AppendValueArg(args, "--spline-max-step", preprocess_config.spline_max_step);
        AppendValueArg(args, "--chordal", preprocess_config.chordal);
        AppendValueArg(args, "--max-seg", preprocess_config.max_seg);
        AppendValueArg(args, "--snap", preprocess_config.snap);
        AppendValueArg(args, "--angular", preprocess_config.angular);
        AppendValueArg(args, "--min-seg", preprocess_config.min_seg);

        SILIGEN_LOG_INFO("使用外部DXF算法仓库生成PB: root=" + external_root.string());
        return Result<std::vector<std::string>>::Success(args);
    }

    const char* script_env = std::getenv("SILIGEN_DXF_PB_SCRIPT");
    fs::path script_path = script_env && *script_env
                               ? fs::path(script_env)
                               : ResolveDefaultPbScriptPath(dxf_path);
    if (!fs::exists(script_path)) {
        std::string detail = script_env && *script_env
                                 ? ("指定脚本不存在: " + std::string(script_env))
                                 : DescribeDefaultPbScriptLocations();
        return Result<std::vector<std::string>>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, detail, MODULE_NAME));
    }

    args = {
        python,
        script_path.string(),
        "--input",
        dxf_path.string(),
        "--output",
        pb_path.string(),
    };

    AppendBoolArg(args, "--normalize-units", preprocess_config.normalize_units);
    AppendBoolArg(args, "--strict-r12", preprocess_config.strict_r12);
    AppendBoolArg(args, "--approx-splines", preprocess_config.approx_splines);
    AppendBoolArg(args, "--snap-enabled", preprocess_config.snap_enabled);
    AppendBoolArg(args, "--densify-enabled", preprocess_config.densify_enabled);
    AppendBoolArg(args, "--min-seg-enabled", preprocess_config.min_seg_enabled);
    AppendValueArg(args, "--spline-samples", preprocess_config.spline_samples);
    AppendValueArg(args, "--spline-max-step", preprocess_config.spline_max_step);
    AppendValueArg(args, "--chordal", preprocess_config.chordal);
    AppendValueArg(args, "--max-seg", preprocess_config.max_seg);
    AppendValueArg(args, "--snap", preprocess_config.snap);
    AppendValueArg(args, "--angular", preprocess_config.angular);
    AppendValueArg(args, "--min-seg", preprocess_config.min_seg);
    return Result<std::vector<std::string>>::Success(args);
}

std::string BuildCommandSummary(const std::vector<std::string>& args) {
    if (args.empty()) {
        return "<empty>";
    }

    std::ostringstream out;
    const size_t limit = std::min<size_t>(args.size(), 8);
    for (size_t i = 0; i < limit; ++i) {
        if (i > 0) {
            out << ' ';
        }
        out << QuoteArg(args[i]);
    }
    if (args.size() > limit) {
        out << " ...(+" << (args.size() - limit) << " args)";
    }
    return out.str();
}

Result<std::vector<std::string>> BuildOverrideCommandArgs(const std::string& command_template,
                                                          const std::filesystem::path& dxf_path,
                                                          const std::filesystem::path& pb_path) {
    if (command_template.find('\n') != std::string::npos || command_template.find('\r') != std::string::npos) {
        return Result<std::vector<std::string>>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR,
                  "SILIGEN_DXF_PB_COMMAND 不允许包含换行符",
                  MODULE_NAME));
    }
    if (command_template.find("{input}") == std::string::npos ||
        command_template.find("{output}") == std::string::npos) {
        return Result<std::vector<std::string>>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR,
                  "SILIGEN_DXF_PB_COMMAND 必须包含 {input} 和 {output} 占位符",
                  MODULE_NAME));
    }

    auto tokenized = TokenizeCommandLine(command_template);
    if (tokenized.IsError()) {
        return tokenized;
    }

    std::vector<std::string> args;
    args.reserve(tokenized.Value().size());
    for (const auto& raw : tokenized.Value()) {
        if (HasForbiddenMetaChar(raw)) {
            return Result<std::vector<std::string>>::Failure(
                Error(ErrorCode::CONFIGURATION_ERROR,
                      "SILIGEN_DXF_PB_COMMAND 禁止包含 shell 元字符(&|;<>`)",
                      MODULE_NAME));
        }
        std::string token = ReplaceAll(raw, "{input}", dxf_path.string());
        token = ReplaceAll(token, "{output}", pb_path.string());
        if (token.empty()) {
            return Result<std::vector<std::string>>::Failure(
                Error(ErrorCode::CONFIGURATION_ERROR,
                      "SILIGEN_DXF_PB_COMMAND 含空参数",
                      MODULE_NAME));
        }
        args.push_back(std::move(token));
    }

    return Result<std::vector<std::string>>::Success(args);
}

#ifdef _WIN32
Result<int> RunProcess(const std::vector<std::string>& args) {
    if (args.empty()) {
        return Result<int>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "外部命令参数为空",
                  MODULE_NAME));
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
    PROCESS_INFORMATION process_info{};
    const BOOL created = ::CreateProcessA(
        nullptr,
        mutable_command.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup_info,
        &process_info);
    if (!created) {
        const DWORD err = ::GetLastError();
        return Result<int>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "启动PB生成进程失败: winerr=" + std::to_string(static_cast<unsigned long>(err)),
                  MODULE_NAME));
    }

    const DWORD wait_result = ::WaitForSingleObject(process_info.hProcess, INFINITE);
    if (wait_result == WAIT_FAILED) {
        const DWORD err = ::GetLastError();
        ::CloseHandle(process_info.hThread);
        ::CloseHandle(process_info.hProcess);
        return Result<int>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "等待PB生成进程失败: winerr=" + std::to_string(static_cast<unsigned long>(err)),
                  MODULE_NAME));
    }

    DWORD exit_code = 1;
    if (!::GetExitCodeProcess(process_info.hProcess, &exit_code)) {
        const DWORD err = ::GetLastError();
        ::CloseHandle(process_info.hThread);
        ::CloseHandle(process_info.hProcess);
        return Result<int>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "读取PB生成进程退出码失败: winerr=" + std::to_string(static_cast<unsigned long>(err)),
                  MODULE_NAME));
    }

    ::CloseHandle(process_info.hThread);
    ::CloseHandle(process_info.hProcess);
    return Result<int>::Success(static_cast<int>(exit_code));
}
#else
Result<int> RunProcess(const std::vector<std::string>& args) {
    if (args.empty()) {
        return Result<int>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "外部命令参数为空",
                  MODULE_NAME));
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        return Result<int>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "fork 失败",
                  MODULE_NAME));
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
        return Result<int>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "waitpid 失败",
                  MODULE_NAME));
    }

    if (WIFEXITED(status)) {
        return Result<int>::Success(WEXITSTATUS(status));
    }
    if (WIFSIGNALED(status)) {
        return Result<int>::Success(128 + WTERMSIG(status));
    }
    return Result<int>::Success(status);
}
#endif

Result<std::vector<std::string>> ResolvePbCommandArgs(
    const std::filesystem::path& dxf_path,
    const std::filesystem::path& pb_path,
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    const char* cmd_override = std::getenv("SILIGEN_DXF_PB_COMMAND");
    if (cmd_override && *cmd_override) {
        return BuildOverrideCommandArgs(cmd_override, dxf_path, pb_path);
    }

    Siligen::Domain::Configuration::Ports::DxfPreprocessConfig preprocess_config;
    if (config_port) {
        auto config_result = config_port->GetDxfPreprocessConfig();
        if (config_result.IsSuccess()) {
            preprocess_config = config_result.Value();
        } else {
            SILIGEN_LOG_WARNING("读取DXF预处理配置失败，使用默认值: " + config_result.GetError().ToString());
        }
    }

    auto external_root_result = ResolveExternalDxFProjectRoot();
    if (external_root_result.IsError()) {
        return Result<std::vector<std::string>>::Failure(external_root_result.GetError());
    }

    const char* python_env = std::getenv(kEngineeringDataPythonEnv);
    if (!(python_env && *python_env)) {
        python_env = std::getenv(kLegacyPbPythonEnv);
        if (python_env && *python_env) {
            WarnDeprecatedPbPythonEnvOnce();
        }
    }
    const std::string python = (python_env && *python_env) ? python_env : "python";
    return BuildPbCommandArgs(dxf_path,
                              pb_path,
                              python,
                              external_root_result.Value(),
                              preprocess_config);
}

Result<void> GeneratePbFile(const std::filesystem::path& dxf_path,
                            const std::filesystem::path& pb_path,
                            const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    auto args_result = ResolvePbCommandArgs(dxf_path, pb_path, config_port);
    if (args_result.IsError()) {
        return Result<void>::Failure(args_result.GetError());
    }
    const auto& command_args = args_result.Value();

    std::error_code remove_ec;
    std::filesystem::remove(pb_path, remove_ec);

    SILIGEN_LOG_INFO("执行PB生成命令: " + BuildCommandSummary(command_args));
    auto run_result = RunProcess(command_args);
    if (run_result.IsError()) {
        return Result<void>::Failure(run_result.GetError());
    }
    const int exit_code = run_result.Value();
    if (exit_code != 0) {
        return Result<void>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "PB generator failed: exit_code=" + std::to_string(exit_code),
                  MODULE_NAME));
    }

    auto validation_result = ValidatePbOutput(pb_path);
    if (validation_result.IsError()) {
        return validation_result;
    }

    SILIGEN_LOG_INFO("PB generated: " + pb_path.string());
    return Result<void>::Success();
}

}  // namespace

DxfPbPreparationService::DxfPbPreparationService(
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port)
    : config_port_(std::move(config_port)) {}

Result<std::string> DxfPbPreparationService::EnsurePbReady(const std::string& filepath) const {
    namespace fs = std::filesystem;

    fs::path input_path(filepath);
    const std::string ext = NormalizeExtension(input_path);

    if (ext == ".pb") {
        auto validation_result = ValidatePbOutput(input_path);
        if (validation_result.IsError()) {
            return Result<std::string>::Failure(validation_result.GetError());
        }
        return Result<std::string>::Success(input_path.string());
    }

    if (ext != ".dxf") {
        return Result<std::string>::Failure(
            Error(ErrorCode::FILE_FORMAT_INVALID, "仅支持DXF/PB文件: " + filepath, MODULE_NAME));
    }

    if (!fs::exists(input_path)) {
        return Result<std::string>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, "DXF文件不存在: " + input_path.string(), MODULE_NAME));
    }

    fs::path pb_path = input_path;
    pb_path.replace_extension(".pb");

    if (!NeedsGeneration(input_path, pb_path)) {
        auto validation_result = ValidatePbOutput(pb_path);
        if (validation_result.IsSuccess()) {
            return Result<std::string>::Success(pb_path.string());
        }

        SILIGEN_LOG_WARNING("检测到现有PB不可用，准备重新生成: " + validation_result.GetError().ToString());
    }

    auto generation_result = GeneratePbFile(input_path, pb_path, config_port_);
    if (generation_result.IsError()) {
        return Result<std::string>::Failure(generation_result.GetError());
    }

    return Result<std::string>::Success(pb_path.string());
}

}  // namespace Siligen::Application::Services::DXF

