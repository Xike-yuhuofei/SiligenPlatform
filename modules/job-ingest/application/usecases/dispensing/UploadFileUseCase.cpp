#include "job_ingest/application/usecases/dispensing/UploadFileUseCase.h"
#include "shared/interfaces/ILoggingService.h"

// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "UploadFileUseCase"
#include "shared/types/Error.h"
#include "shared/hashing/Sha256.h"
#include "shared/types/Result.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <random>
#include <sstream>
#include <stdexcept>

namespace Siligen::JobIngest::Application::UseCases::Dispensing {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Hashing::ComputeSha256Hex;
using Siligen::Shared::Types::Result;

namespace {

bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle) {
    auto it = std::search(
        haystack.begin(),
        haystack.end(),
        needle.begin(),
        needle.end(),
        [](const char left, const char right) {
            return std::toupper(static_cast<unsigned char>(left)) ==
                std::toupper(static_cast<unsigned char>(right));
        });
    return it != haystack.end();
}

}  // namespace

UploadFileUseCase::UploadFileUseCase(std::shared_ptr<IUploadStoragePort> storage_port,
                                     size_t max_file_size_mb)
    : storage_port_(std::move(storage_port)),
      max_file_size_mb_(max_file_size_mb) {
    if (!storage_port_) {
        throw std::invalid_argument("UploadFileUseCase: storage_port cannot be null");
    }
}

Result<SourceDrawing> UploadFileUseCase::Execute(const UploadRequest& request) {
    SILIGEN_LOG_INFO("Starting file upload: " + request.original_filename);

    // 1. 验证请求参数
    if (!request.Validate()) {
        return Result<SourceDrawing>::Failure(Error(ErrorCode::INVALID_PARAMETER,
                                                    "Invalid upload request: empty file content or filename",
                                                    "UploadFileUseCase"));
    }

    // 2. 生成安全文件名
    std::string safe_filename = GenerateSafeFilename(request.original_filename);
    SILIGEN_LOG_INFO("Generated safe filename: " + safe_filename);

    // 3. 验证文件（大小、类型）
    std::vector<std::string> allowed_extensions = {"dxf", "DXF"};
    auto validation_result = storage_port_->Validate(request, max_file_size_mb_, allowed_extensions);

    if (!validation_result.IsSuccess()) {
        return Result<SourceDrawing>::Failure(validation_result.GetError());
    }

    // 4. 存储文件
    auto store_result = storage_port_->Store(request, safe_filename);

    if (!store_result.IsSuccess()) {
        return Result<SourceDrawing>::Failure(store_result.GetError());
    }
    const auto& stored_path = store_result.Value();

    const std::string source_hash = ComputeSourceHash(request);
    const std::string source_drawing_ref = "sha256:" + source_hash;
    auto report_summary_result = ValidateStoredDxfArtifact(request, source_drawing_ref, source_hash);
    if (report_summary_result.IsError()) {
        CleanupStoredArtifact(stored_path);
        return Result<SourceDrawing>::Failure(report_summary_result.GetError());
    }

    SourceDrawing response;
    response.source_drawing_ref = source_drawing_ref;
    response.filepath = stored_path;
    response.original_name = request.original_filename;
    response.size = request.file_size;
    response.generated_filename = safe_filename;
    response.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    response.source_hash = source_hash;
    response.validation_report.schema_version = "DXFValidationReport.v1";
    response.validation_report.stage_id = "S1";
    response.validation_report.owner_module = "M1";
    response.validation_report.source_ref = source_drawing_ref;
    response.validation_report.source_hash = source_hash;
    response.validation_report.gate_result = "PASS";
    response.validation_report.result_classification = "source_drawing_accepted";
    response.validation_report.preview_ready = false;
    response.validation_report.production_ready = false;
    response.validation_report.summary = report_summary_result.Value();
    response.validation_report.resolved_units.clear();
    response.validation_report.resolved_unit_scale = 0.0;

    SILIGEN_LOG_INFO("File uploaded successfully: " + response.filepath);

    return Result<SourceDrawing>::Success(response);
}

std::string UploadFileUseCase::GenerateSafeFilename(const std::string& original_filename) {
    // 1. 提取文件扩展名
    std::string extension;
    size_t dot_pos = original_filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        extension = original_filename.substr(dot_pos);
    }

    // 2. 生成 UUID（简化版：使用随机数）
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream uuid_ss;
    uuid_ss << std::hex;
    for (int i = 0; i < 8; i++) uuid_ss << dis(gen);
    uuid_ss << "-";
    for (int i = 0; i < 4; i++) uuid_ss << dis(gen);
    uuid_ss << "-4";  // UUID version 4
    for (int i = 0; i < 3; i++) uuid_ss << dis(gen);
    uuid_ss << "-";
    for (int i = 0; i < 4; i++) uuid_ss << dis(gen);
    uuid_ss << "-";
    for (int i = 0; i < 12; i++) uuid_ss << dis(gen);
    std::string uuid = uuid_ss.str();

    // 3. 获取时间戳
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    // 4. 提取基础文件名（移除特殊字符）
    std::string base_name = original_filename.substr(0, dot_pos);

    // 移除危险字符
    // 使用 unsigned char 避免 UTF-8 字符导致的负值传入 std::isalnum
    base_name.erase(std::remove_if(base_name.begin(),
                                   base_name.end(),
                                   [](unsigned char c) { return !std::isalnum(c) && c != '_' && c != '-'; }),
                    base_name.end());

    if (base_name.empty()) {
        base_name = "unnamed";
    }

    // 5. 组合安全文件名: UUID_timestamp_base.extension
    std::stringstream filename_ss;
    filename_ss << uuid << "_" << timestamp << "_" << base_name << extension;

    return filename_ss.str();
}

Result<std::string> UploadFileUseCase::ValidateStoredDxfArtifact(
    const UploadRequest& request,
    const std::string& source_drawing_ref,
    const std::string& source_hash) const {
    if (request.file_content.size() < 16U) {
        return Result<std::string>::Failure(
            Error(ErrorCode::FILE_FORMAT_INVALID, "DXF file is too small", "UploadFileUseCase"));
    }

    const std::string payload(
        reinterpret_cast<const char*>(request.file_content.data()),
        request.file_content.size());
    if (!ContainsCaseInsensitive(payload, "SECTION") ||
        !ContainsCaseInsensitive(payload, "EOF")) {
        return Result<std::string>::Failure(
            Error(ErrorCode::FILE_FORMAT_INVALID, "Uploaded file is not a readable DXF artifact", "UploadFileUseCase"));
    }

    std::ostringstream oss;
    oss << "Source drawing accepted and archived for downstream processing."
        << " source_ref=" << source_drawing_ref
        << " source_hash=" << source_hash;
    return Result<std::string>::Success(oss.str());
}

std::string UploadFileUseCase::ComputeSourceHash(const UploadRequest& request) const {
    return ComputeSha256Hex(request.file_content);
}

void UploadFileUseCase::CleanupStoredArtifact(const std::string& stored_path) const noexcept {
    auto delete_result = storage_port_->Delete(stored_path);
    if (delete_result.IsError()) {
        SILIGEN_LOG_WARNING("清理上传失败DXF文件失败: " + delete_result.GetError().ToString());
    }
}

}  // namespace Siligen::JobIngest::Application::UseCases::Dispensing
