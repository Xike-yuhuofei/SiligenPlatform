#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"

#include "application/services/dxf/PbCommandResolver.h"
#include "application/services/dxf/PbProcessRunner.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <utility>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "DxfPbPreparationService"

namespace Siligen::Application::Services::DXF {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

std::string NormalizeExtension(std::filesystem::path path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
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
    return !dxf_time_ec && !pb_time_ec && dxf_time > pb_time;
}

Result<std::filesystem::path> ResolvePreparedArtifactPath(const std::string& source_path) {
    const std::filesystem::path input_path(source_path);
    const std::string ext = NormalizeExtension(input_path);

    if (ext == ".pb") {
        return Result<std::filesystem::path>::Success(input_path);
    }
    if (ext == ".dxf") {
        auto pb_path = input_path;
        pb_path.replace_extension(".pb");
        return Result<std::filesystem::path>::Success(pb_path);
    }

    return Result<std::filesystem::path>::Failure(
        Error(ErrorCode::FILE_FORMAT_INVALID, "仅支持DXF/PB文件: " + source_path, MODULE_NAME));
}

}  // namespace

DxfPbPreparationService::DxfPbPreparationService(
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port)
    : config_port_(std::move(config_port)) {}

Result<std::string> DxfPbPreparationService::EnsurePbReady(const std::string& filepath) const {
    namespace fs = std::filesystem;

    detail::PbProcessRunner process_runner;
    const fs::path input_path(filepath);
    const std::string ext = NormalizeExtension(input_path);

    if (ext == ".pb") {
        auto validation_result = process_runner.ValidateOutput(input_path);
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
        auto validation_result = process_runner.ValidateOutput(pb_path);
        if (validation_result.IsSuccess()) {
            return Result<std::string>::Success(pb_path.string());
        }

        SILIGEN_LOG_WARNING("检测到现有PB不可用，准备重新生成: " + validation_result.GetError().ToString());
    }

    detail::PbCommandResolver command_resolver(config_port_);
    auto command_result = command_resolver.Resolve(input_path, pb_path);
    if (command_result.IsError()) {
        return Result<std::string>::Failure(command_result.GetError());
    }

    auto generation_result = process_runner.Run(command_result.Value(), pb_path);
    if (generation_result.IsError()) {
        return Result<std::string>::Failure(generation_result.GetError());
    }

    return Result<std::string>::Success(pb_path.string());
}

Result<void> DxfPbPreparationService::CleanupPreparedInput(const std::string& source_path) const {
    auto prepared_path_result = ResolvePreparedArtifactPath(source_path);
    if (prepared_path_result.IsError()) {
        return Result<void>::Failure(prepared_path_result.GetError());
    }

    std::error_code exists_ec;
    const bool prepared_exists = std::filesystem::exists(prepared_path_result.Value(), exists_ec);
    if (exists_ec) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR,
                  "failed to inspect prepared artifact: " + prepared_path_result.Value().string(),
                  MODULE_NAME));
    }
    if (!prepared_exists) {
        return Result<void>::Success();
    }

    std::error_code type_ec;
    if (std::filesystem::is_directory(prepared_path_result.Value(), type_ec)) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR,
                  "prepared artifact path is a directory: " + prepared_path_result.Value().string(),
                  MODULE_NAME));
    }
    if (type_ec) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR,
                  "failed to inspect prepared artifact: " + prepared_path_result.Value().string(),
                  MODULE_NAME));
    }

    std::error_code remove_ec;
    std::filesystem::remove(prepared_path_result.Value(), remove_ec);
    if (remove_ec) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR,
                  "failed to cleanup pb artifact: " + prepared_path_result.Value().string(),
                  MODULE_NAME));
    }

    return Result<void>::Success();
}

}  // namespace Siligen::Application::Services::DXF
