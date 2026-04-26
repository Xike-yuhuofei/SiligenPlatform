#include "application/services/dxf/PbCommandResolver.h"

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Siligen::Application::Services::DXF::detail {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

constexpr const char* kModuleName = "DxfPbPreparationService";
constexpr const char* kDxFProjectRootEnv = "SILIGEN_DXF_PROJECT_ROOT";
constexpr const char* kDxFProjectLauncherRelative = "scripts/dxf_core_launcher.py";
constexpr const char* kCanonicalDxFToPbCommand = "engineering-data-dxf-to-pb";
constexpr const char* kPbCommandOverrideEnv = "SILIGEN_DXF_PB_COMMAND";
constexpr const char* kPbScriptEnv = "SILIGEN_DXF_PB_SCRIPT";
constexpr const char* kEngineeringDataPythonEnv = "SILIGEN_ENGINEERING_DATA_PYTHON";
constexpr const char* kLegacyPbPythonEnv = "SILIGEN_DXF_PB_PYTHON";
constexpr const char* kCanonicalEngineeringDataPythonRelativePath = "build/engineering-data-venv/Scripts/python.exe";
constexpr const char* kForbiddenMetaChars = "&|;<>`";
constexpr std::array<const char*, 1> kDefaultPbScriptRelativePaths = {
    "scripts/engineering-data/dxf_to_pb.py",
};

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

std::filesystem::path ResolveCanonicalEngineeringDataPythonPath(const std::filesystem::path& dxf_path) {
    namespace fs = std::filesystem;

    std::error_code ec;
    std::vector<fs::path> anchors;
    anchors.push_back(dxf_path.parent_path());
    anchors.push_back(fs::current_path(ec));
    ec.clear();

    for (const auto& anchor : anchors) {
        auto resolved = FindAncestorContainingRelativePath(anchor, fs::path(kCanonicalEngineeringDataPythonRelativePath));
        if (!resolved.empty()) {
            return resolved;
        }
    }

    return {};
}

std::string DescribeDefaultPbScriptLocations() {
    std::ostringstream out;
    out << "未找到本地 DXF->PB 脚本(已搜索: ";
    for (size_t index = 0; index < kDefaultPbScriptRelativePaths.size(); ++index) {
        if (index > 0) {
            out << ", ";
        }
        out << kDefaultPbScriptRelativePaths[index];
    }
    out << ")";
    return out.str();
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
                  std::string(kPbCommandOverrideEnv) + " 存在未闭合引号",
                  kModuleName));
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    if (tokens.empty()) {
        return Result<std::vector<std::string>>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR,
                  std::string(kPbCommandOverrideEnv) + " 为空或无法解析为命令",
                  kModuleName));
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
                  kModuleName));
    }
    return Result<fs::path>::Success(root);
}

Result<std::vector<std::string>> BuildPbCommandArgs(
    const std::filesystem::path& dxf_path,
    const std::filesystem::path& pb_path,
    const std::string& python,
    const std::filesystem::path& external_root,
    const Siligen::Domain::Configuration::Ports::DxfImportConfig& preprocess_config) {
    namespace fs = std::filesystem;

    std::vector<std::string> args;
    auto validation_report_path = pb_path;
    validation_report_path.replace_extension(".validation.json");
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
            "--validation-report",
            validation_report_path.string(),
        };

        AppendBoolArg(args, "--normalize-units", preprocess_config.normalize_units);
        AppendBoolArg(args, "--snap-enabled", preprocess_config.snap_enabled);
        AppendBoolArg(args, "--densify-enabled", preprocess_config.densify_enabled);
        AppendBoolArg(args, "--min-seg-enabled", preprocess_config.min_seg_enabled);
        AppendValueArg(args, "--chordal", preprocess_config.chordal);
        AppendValueArg(args, "--max-seg", preprocess_config.max_seg);
        AppendValueArg(args, "--snap", preprocess_config.snap);
        AppendValueArg(args, "--angular", preprocess_config.angular);
        AppendValueArg(args, "--min-seg", preprocess_config.min_seg);

        SILIGEN_LOG_INFO("使用外部DXF算法仓库生成PB: root=" + external_root.string());
        return Result<std::vector<std::string>>::Success(args);
    }

    const char* script_env = std::getenv(kPbScriptEnv);
    fs::path script_path = (script_env && *script_env) ? fs::path(script_env) : ResolveDefaultPbScriptPath(dxf_path);
    if (!fs::exists(script_path)) {
        const std::string detail = (script_env && *script_env)
                                       ? ("指定脚本不存在: " + std::string(script_env))
                                       : DescribeDefaultPbScriptLocations();
        return Result<std::vector<std::string>>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, detail, kModuleName));
    }

    args = {
        python,
        script_path.string(),
        "--input",
        dxf_path.string(),
        "--output",
        pb_path.string(),
        "--validation-report",
        validation_report_path.string(),
    };

    AppendBoolArg(args, "--normalize-units", preprocess_config.normalize_units);
    AppendBoolArg(args, "--snap-enabled", preprocess_config.snap_enabled);
    AppendBoolArg(args, "--densify-enabled", preprocess_config.densify_enabled);
    AppendBoolArg(args, "--min-seg-enabled", preprocess_config.min_seg_enabled);
    AppendValueArg(args, "--chordal", preprocess_config.chordal);
    AppendValueArg(args, "--max-seg", preprocess_config.max_seg);
    AppendValueArg(args, "--snap", preprocess_config.snap);
    AppendValueArg(args, "--angular", preprocess_config.angular);
    AppendValueArg(args, "--min-seg", preprocess_config.min_seg);
    return Result<std::vector<std::string>>::Success(args);
}

Result<std::vector<std::string>> BuildOverrideCommandArgs(const std::string& command_template,
                                                          const std::filesystem::path& dxf_path,
                                                          const std::filesystem::path& pb_path) {
    if (command_template.find('\n') != std::string::npos || command_template.find('\r') != std::string::npos) {
        return Result<std::vector<std::string>>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR,
                  std::string(kPbCommandOverrideEnv) + " 不允许包含换行符",
                  kModuleName));
    }
    if (command_template.find("{input}") == std::string::npos ||
        command_template.find("{output}") == std::string::npos) {
        return Result<std::vector<std::string>>::Failure(
            Error(ErrorCode::CONFIGURATION_ERROR,
                  std::string(kPbCommandOverrideEnv) + " 必须包含 {input} 和 {output} 占位符",
                  kModuleName));
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
                      std::string(kPbCommandOverrideEnv) + " 禁止包含 shell 元字符(&|;<>`)",
                      kModuleName));
        }

        std::string token = ReplaceAll(raw, "{input}", dxf_path.string());
        token = ReplaceAll(token, "{output}", pb_path.string());
        if (token.empty()) {
            return Result<std::vector<std::string>>::Failure(
                Error(ErrorCode::CONFIGURATION_ERROR,
                      std::string(kPbCommandOverrideEnv) + " 含空参数",
                      kModuleName));
        }
        args.push_back(std::move(token));
    }

    return Result<std::vector<std::string>>::Success(args);
}

std::string ResolvePythonExecutable(const std::filesystem::path& dxf_path) {
    const char* python_env = std::getenv(kEngineeringDataPythonEnv);
    if (!(python_env && *python_env)) {
        python_env = std::getenv(kLegacyPbPythonEnv);
        if (python_env && *python_env) {
            WarnDeprecatedPbPythonEnvOnce();
        }
    }

    if (python_env && *python_env) {
        return python_env;
    }

    const auto canonical_python = ResolveCanonicalEngineeringDataPythonPath(dxf_path);
    if (!canonical_python.empty()) {
        return canonical_python.string();
    }

    return "python";
}

Siligen::Domain::Configuration::Ports::DxfImportConfig ResolvePreprocessConfig(
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    Siligen::Domain::Configuration::Ports::DxfImportConfig preprocess_config;
    if (!config_port) {
        return preprocess_config;
    }

    auto config_result = config_port->GetDxfImportConfig();
    if (config_result.IsSuccess()) {
        return config_result.Value();
    }

    SILIGEN_LOG_WARNING("读取DXF预处理配置失败，使用默认值: " + config_result.GetError().ToString());
    return preprocess_config;
}

}  // namespace

PbCommandResolver::PbCommandResolver(
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port)
    : config_port_(std::move(config_port)) {}

Result<std::vector<std::string>> PbCommandResolver::Resolve(const std::filesystem::path& dxf_path,
                                                            const std::filesystem::path& pb_path) const {
    const char* command_override = std::getenv(kPbCommandOverrideEnv);
    if (command_override && *command_override) {
        return BuildOverrideCommandArgs(command_override, dxf_path, pb_path);
    }

    auto external_root_result = ResolveExternalDxFProjectRoot();
    if (external_root_result.IsError()) {
        return Result<std::vector<std::string>>::Failure(external_root_result.GetError());
    }

    return BuildPbCommandArgs(dxf_path,
                              pb_path,
                              ResolvePythonExecutable(dxf_path),
                              external_root_result.Value(),
                              ResolvePreprocessConfig(config_port_));
}

}  // namespace Siligen::Application::Services::DXF::detail
