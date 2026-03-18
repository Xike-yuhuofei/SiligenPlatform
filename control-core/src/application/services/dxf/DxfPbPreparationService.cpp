#include "DxfPbPreparationService.h"

#include "domain/configuration/ports/IConfigurationPort.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>

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

void AppendBoolArg(std::string& command, const std::string& flag, bool enabled) {
    command += " ";
    if (enabled) {
        command += flag;
        return;
    }
    if (flag.rfind("--", 0) == 0) {
        command += "--no-";
        command += flag.substr(2);
        return;
    }
    command += "no-";
    command += flag;
}

template <typename T>
void AppendValueArg(std::string& command, const std::string& flag, T value) {
    command += " ";
    command += flag;
    command += " ";
    command += std::to_string(value);
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

std::filesystem::path FindExternalDxFProjectRoot() {
    namespace fs = std::filesystem;

    const char* explicit_root = std::getenv(kDxFProjectRootEnv);
    if (explicit_root && *explicit_root) {
        fs::path root = fs::path(explicit_root);
        if (HasDxFLauncher(root)) {
            return root;
        }
    }

    std::error_code ec;
    fs::path current = fs::current_path(ec);
    if (ec) {
        return {};
    }

    for (fs::path cursor = current; !cursor.empty(); cursor = cursor.parent_path()) {
        if (HasDxFLauncher(cursor)) {
            return cursor;
        }

        const fs::path sibling = cursor.parent_path() / "DXF";
        if (!sibling.empty() && HasDxFLauncher(sibling)) {
            return sibling;
        }

        if (cursor == cursor.root_path()) {
            break;
        }
    }

    return {};
}

std::string BuildPbCommand(const std::filesystem::path& dxf_path,
                           const std::filesystem::path& pb_path,
                           const std::string& python,
                           const Siligen::Domain::Configuration::Ports::DxfPreprocessConfig& preprocess_config) {
    namespace fs = std::filesystem;

    const std::filesystem::path external_root = FindExternalDxFProjectRoot();
    if (!external_root.empty()) {
        const fs::path launcher_path = external_root / kDxFProjectLauncherRelative;
        std::string command = QuoteArg(python) + " " + QuoteArg(launcher_path.string()) +
                              " dxf-to-pb --input " + QuoteArg(dxf_path.string()) +
                              " --output " + QuoteArg(pb_path.string());

        AppendBoolArg(command, "--normalize-units", preprocess_config.normalize_units);
        AppendBoolArg(command, "--approx-splines", preprocess_config.approx_splines);
        AppendBoolArg(command, "--snap-enabled", preprocess_config.snap_enabled);
        AppendBoolArg(command, "--densify-enabled", preprocess_config.densify_enabled);
        AppendBoolArg(command, "--min-seg-enabled", preprocess_config.min_seg_enabled);
        AppendValueArg(command, "--spline-samples", preprocess_config.spline_samples);
        AppendValueArg(command, "--spline-max-step", preprocess_config.spline_max_step);
        AppendValueArg(command, "--chordal", preprocess_config.chordal);
        AppendValueArg(command, "--max-seg", preprocess_config.max_seg);
        AppendValueArg(command, "--snap", preprocess_config.snap);
        AppendValueArg(command, "--angular", preprocess_config.angular);
        AppendValueArg(command, "--min-seg", preprocess_config.min_seg);

        SILIGEN_LOG_INFO("使用外部DXF算法仓库生成PB: root=" + external_root.string());
        return command;
    }

    const char* script_env = std::getenv("SILIGEN_DXF_PB_SCRIPT");
    fs::path script_path = script_env && *script_env
                               ? fs::path(script_env)
                               : fs::current_path() / "packages" / "engineering-data" / "scripts" / "dxf_to_pb.py";
    if (!fs::exists(script_path)) {
        return {};
    }

    std::string command = QuoteArg(python) + " " + QuoteArg(script_path.string()) + " --input " +
                          QuoteArg(dxf_path.string()) + " --output " + QuoteArg(pb_path.string());

    AppendBoolArg(command, "--normalize-units", preprocess_config.normalize_units);
    AppendBoolArg(command, "--approx-splines", preprocess_config.approx_splines);
    AppendBoolArg(command, "--snap-enabled", preprocess_config.snap_enabled);
    AppendBoolArg(command, "--densify-enabled", preprocess_config.densify_enabled);
    AppendBoolArg(command, "--min-seg-enabled", preprocess_config.min_seg_enabled);
    AppendValueArg(command, "--spline-samples", preprocess_config.spline_samples);
    AppendValueArg(command, "--spline-max-step", preprocess_config.spline_max_step);
    AppendValueArg(command, "--chordal", preprocess_config.chordal);
    AppendValueArg(command, "--max-seg", preprocess_config.max_seg);
    AppendValueArg(command, "--snap", preprocess_config.snap);
    AppendValueArg(command, "--angular", preprocess_config.angular);
    AppendValueArg(command, "--min-seg", preprocess_config.min_seg);
    return command;
}

Result<void> GeneratePbFile(const std::filesystem::path& dxf_path,
                            const std::filesystem::path& pb_path,
                            const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    namespace fs = std::filesystem;

    std::string command;
    const char* cmd_override = std::getenv("SILIGEN_DXF_PB_COMMAND");
    if (cmd_override && *cmd_override) {
        command = cmd_override;
        command = ReplaceAll(command, "{input}", QuoteArg(dxf_path.string()));
        command = ReplaceAll(command, "{output}", QuoteArg(pb_path.string()));
    } else {
        Siligen::Domain::Configuration::Ports::DxfPreprocessConfig preprocess_config;
        if (config_port) {
            auto config_result = config_port->GetDxfPreprocessConfig();
            if (config_result.IsSuccess()) {
                preprocess_config = config_result.Value();
            } else {
                SILIGEN_LOG_WARNING("读取DXF预处理配置失败，使用默认值: " + config_result.GetError().ToString());
            }
        }

        const char* python_env = std::getenv("SILIGEN_DXF_PB_PYTHON");
        const std::string python = (python_env && *python_env) ? python_env : "python";
        command = BuildPbCommand(dxf_path, pb_path, python, preprocess_config);
        if (command.empty()) {
            const char* script_env = std::getenv("SILIGEN_DXF_PB_SCRIPT");
            std::string detail = script_env && *script_env
                                     ? ("指定脚本不存在: " + std::string(script_env))
                                     : "未找到外部DXF仓库启动脚本，也未找到本地 packages/engineering-data/scripts/dxf_to_pb.py";
            return Result<void>::Failure(
                Error(ErrorCode::FILE_NOT_FOUND, detail, MODULE_NAME));
        }
    }

    std::error_code remove_ec;
    std::filesystem::remove(pb_path, remove_ec);

    const int exit_code = std::system(command.c_str());
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
