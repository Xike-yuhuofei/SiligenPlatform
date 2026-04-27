#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"

#include "application/services/dxf/PbCommandResolver.h"
#include "application/services/dxf/PbProcessRunner.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

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

InputQualitySummary DefaultInputQualitySummary() {
    InputQualitySummary summary;
    return summary;
}

std::filesystem::path ResolveValidationReportPath(const std::filesystem::path& prepared_path) {
    auto report_path = prepared_path;
    report_path.replace_extension(".validation.json");
    return report_path;
}

std::string ReadJsonString(const nlohmann::json& payload, const char* key) {
    if (!payload.contains(key) || !payload.at(key).is_string()) {
        return {};
    }
    return payload.at(key).get<std::string>();
}

Result<std::string> RequireJsonString(const nlohmann::json& payload,
                                      const char* key,
                                      const std::filesystem::path& report_path,
                                      const char* owner_name = MODULE_NAME) {
    if (!payload.contains(key) || !payload.at(key).is_string()) {
        return Result<std::string>::Failure(
            Error(
                ErrorCode::FILE_FORMAT_INVALID,
                "validation report missing required string field: " + std::string(key) + " path=" + report_path.string(),
                owner_name));
    }

    const auto value = payload.at(key).get<std::string>();
    if (value.empty()) {
        return Result<std::string>::Failure(
            Error(
                ErrorCode::FILE_FORMAT_INVALID,
                "validation report empty required string field: " + std::string(key) + " path=" + report_path.string(),
                owner_name));
    }
    return Result<std::string>::Success(value);
}

std::vector<std::string> ReadJsonStringArray(const nlohmann::json& payload, const char* key) {
    std::vector<std::string> values;
    if (!payload.contains(key) || !payload.at(key).is_array()) {
        return values;
    }
    for (const auto& item : payload.at(key)) {
        if (item.is_string()) {
            values.push_back(item.get<std::string>());
        }
    }
    return values;
}

bool ReadJsonBool(const nlohmann::json& payload, const char* key, bool fallback = false) {
    if (!payload.contains(key) || !payload.at(key).is_boolean()) {
        return fallback;
    }
    return payload.at(key).get<bool>();
}

Result<bool> RequireJsonBool(const nlohmann::json& payload,
                             const char* key,
                             const std::filesystem::path& report_path,
                             const char* owner_name = MODULE_NAME) {
    if (!payload.contains(key) || !payload.at(key).is_boolean()) {
        return Result<bool>::Failure(
            Error(
                ErrorCode::FILE_FORMAT_INVALID,
                "validation report missing required bool field: " + std::string(key) + " path=" + report_path.string(),
                owner_name));
    }
    return Result<bool>::Success(payload.at(key).get<bool>());
}

double ReadJsonDouble(const nlohmann::json& payload, const char* key, double fallback = 0.0) {
    if (!payload.contains(key) || !payload.at(key).is_number()) {
        return fallback;
    }
    return payload.at(key).get<double>();
}

Result<double> RequireJsonDouble(const nlohmann::json& payload,
                                 const char* key,
                                 const std::filesystem::path& report_path,
                                 const char* owner_name = MODULE_NAME) {
    if (!payload.contains(key) || !payload.at(key).is_number()) {
        return Result<double>::Failure(
            Error(
                ErrorCode::FILE_FORMAT_INVALID,
                "validation report missing required number field: " + std::string(key) + " path=" + report_path.string(),
                owner_name));
    }
    return Result<double>::Success(payload.at(key).get<double>());
}

Result<std::vector<std::string>> RequireJsonStringArray(const nlohmann::json& payload,
                                                        const char* key,
                                                        const std::filesystem::path& report_path,
                                                        const char* owner_name = MODULE_NAME) {
    if (!payload.contains(key) || !payload.at(key).is_array()) {
        return Result<std::vector<std::string>>::Failure(
            Error(
                ErrorCode::FILE_FORMAT_INVALID,
                "validation report missing required string array field: " + std::string(key) +
                    " path=" + report_path.string(),
                owner_name));
    }

    std::vector<std::string> values;
    for (const auto& item : payload.at(key)) {
        if (!item.is_string()) {
            return Result<std::vector<std::string>>::Failure(
                Error(
                    ErrorCode::FILE_FORMAT_INVALID,
                    "validation report string array contains non-string item: " + std::string(key) +
                        " path=" + report_path.string(),
                    owner_name));
        }
        values.push_back(item.get<std::string>());
    }
    return Result<std::vector<std::string>>::Success(std::move(values));
}

Result<InputQualitySummary> LoadValidationReportSummary(const std::filesystem::path& report_path) {
    std::ifstream input(report_path);
    if (!input.good()) {
        return Result<InputQualitySummary>::Failure(
            Error(
                ErrorCode::FILE_NOT_FOUND,
                "validation report not found: " + report_path.string(),
                MODULE_NAME));
    }

    nlohmann::json payload;
    try {
        input >> payload;
    } catch (const std::exception& ex) {
        return Result<InputQualitySummary>::Failure(
            Error(
                ErrorCode::FILE_FORMAT_INVALID,
                "validation report parse failed: " + report_path.string() + " reason=" + ex.what(),
                MODULE_NAME));
    }

    const auto file = payload.value("file", nlohmann::json::object());
    const auto summary_payload = payload.value("summary", nlohmann::json::object());

    InputQualitySummary summary;
    summary.report_path = report_path.string();
    auto report_id_result = RequireJsonString(payload, "report_id", report_path);
    if (report_id_result.IsError()) {
        return Result<InputQualitySummary>::Failure(report_id_result.GetError());
    }
    summary.report_id = report_id_result.Value();

    auto schema_version_result = RequireJsonString(payload, "schema_version", report_path);
    if (schema_version_result.IsError()) {
        return Result<InputQualitySummary>::Failure(schema_version_result.GetError());
    }
    summary.schema_version = schema_version_result.Value();

    auto dxf_hash_result = RequireJsonString(file, "file_hash", report_path);
    if (dxf_hash_result.IsError()) {
        return Result<InputQualitySummary>::Failure(dxf_hash_result.GetError());
    }
    summary.dxf_hash = dxf_hash_result.Value();

    auto source_drawing_ref_result = RequireJsonString(file, "source_drawing_ref", report_path);
    if (source_drawing_ref_result.IsError()) {
        return Result<InputQualitySummary>::Failure(source_drawing_ref_result.GetError());
    }
    summary.source_drawing_ref = source_drawing_ref_result.Value();

    auto gate_result_result = RequireJsonString(summary_payload, "gate_result", report_path);
    if (gate_result_result.IsError()) {
        return Result<InputQualitySummary>::Failure(gate_result_result.GetError());
    }
    summary.gate_result = gate_result_result.Value();

    auto preview_ready_result = RequireJsonBool(payload, "preview_ready", report_path);
    if (preview_ready_result.IsError()) {
        return Result<InputQualitySummary>::Failure(preview_ready_result.GetError());
    }
    summary.preview_ready = preview_ready_result.Value();

    auto production_ready_result = RequireJsonBool(payload, "production_ready", report_path);
    if (production_ready_result.IsError()) {
        return Result<InputQualitySummary>::Failure(production_ready_result.GetError());
    }
    summary.production_ready = production_ready_result.Value();

    auto classification_result = RequireJsonString(payload, "classification", report_path);
    if (classification_result.IsError()) {
        return Result<InputQualitySummary>::Failure(classification_result.GetError());
    }
    summary.classification = classification_result.Value();

    auto summary_result = RequireJsonString(payload, "operator_summary", report_path);
    if (summary_result.IsError()) {
        return Result<InputQualitySummary>::Failure(summary_result.GetError());
    }
    summary.summary = summary_result.Value();

    if (!payload.contains("primary_code") || !payload.at("primary_code").is_string()) {
        return Result<InputQualitySummary>::Failure(
            Error(
                ErrorCode::FILE_FORMAT_INVALID,
                "validation report missing required string field: primary_code path=" + report_path.string(),
                MODULE_NAME));
    }
    summary.primary_code = payload.at("primary_code").get<std::string>();

    auto warning_codes_result = RequireJsonStringArray(payload, "warning_codes", report_path);
    if (warning_codes_result.IsError()) {
        return Result<InputQualitySummary>::Failure(warning_codes_result.GetError());
    }
    summary.warning_codes = warning_codes_result.Value();

    auto error_codes_result = RequireJsonStringArray(payload, "error_codes", report_path);
    if (error_codes_result.IsError()) {
        return Result<InputQualitySummary>::Failure(error_codes_result.GetError());
    }
    summary.error_codes = error_codes_result.Value();

    auto resolved_units_result = RequireJsonString(payload, "resolved_units", report_path);
    if (resolved_units_result.IsError()) {
        return Result<InputQualitySummary>::Failure(resolved_units_result.GetError());
    }
    summary.resolved_units = resolved_units_result.Value();

    auto resolved_unit_scale_result = RequireJsonDouble(payload, "resolved_unit_scale", report_path);
    if (resolved_unit_scale_result.IsError()) {
        return Result<InputQualitySummary>::Failure(resolved_unit_scale_result.GetError());
    }
    summary.resolved_unit_scale = resolved_unit_scale_result.Value();

    return Result<InputQualitySummary>::Success(std::move(summary));
}

Result<PreparedInputArtifact> LoadPreparedInputArtifact(const std::filesystem::path& pb_path) {
    PreparedInputArtifact artifact;
    artifact.prepared_path = pb_path.string();
    auto report_result = LoadValidationReportSummary(ResolveValidationReportPath(pb_path));
    if (report_result.IsError()) {
        return Result<PreparedInputArtifact>::Failure(report_result.GetError());
    }
    artifact.input_quality = report_result.Value();
    return Result<PreparedInputArtifact>::Success(std::move(artifact));
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

Result<PreparedInputArtifact> DxfPbPreparationService::PrepareInputArtifact(const std::string& filepath) const {
    auto ready_result = EnsurePbReady(filepath);
    if (ready_result.IsError()) {
        return Result<PreparedInputArtifact>::Failure(ready_result.GetError());
    }

    return LoadPreparedInputArtifact(std::filesystem::path(ready_result.Value()));
}

Result<void> DxfPbPreparationService::CleanupPreparedInput(const std::string& source_path) {
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
