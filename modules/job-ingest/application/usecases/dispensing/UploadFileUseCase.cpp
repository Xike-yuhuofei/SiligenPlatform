#include "job_ingest/application/usecases/dispensing/UploadFileUseCase.h"
#include "shared/interfaces/ILoggingService.h"

// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "UploadFileUseCase"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace Siligen::JobIngest::Application::UseCases::Dispensing {

using Siligen::Engineering::Contracts::DxfValidationReport;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

std::string HexEncodeUint64(std::uint64_t value) {
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << value;
    return oss.str();
}

std::uint64_t Fnv1a64(const std::vector<std::uint8_t>& payload) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::uint8_t byte : payload) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
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

    SourceDrawing response;
    response.source_drawing_ref = "source-drawing:" + safe_filename;
    response.filepath = stored_path;
    response.original_name = request.original_filename;
    response.size = request.file_size;
    response.generated_filename = safe_filename;
    response.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    response.source_hash = ComputeSourceHash(request);
    response.validation_report.schema_version = "DXFValidationReport.v1";
    response.validation_report.stage_id = "S1";
    response.validation_report.owner_module = "M1";
    response.validation_report.source_ref = response.source_drawing_ref;
    response.validation_report.source_hash = response.source_hash;
    response.validation_report.gate_result = "PASS";
    response.validation_report.result_classification = "source_drawing_accepted";
    response.validation_report.preview_ready = false;
    response.validation_report.production_ready = false;
    response.validation_report.summary = "Source drawing accepted and archived for downstream geometry parsing.";

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

std::string UploadFileUseCase::ComputeSourceHash(const UploadRequest& request) const {
    return HexEncodeUint64(Fnv1a64(request.file_content));
}

}  // namespace Siligen::JobIngest::Application::UseCases::Dispensing
