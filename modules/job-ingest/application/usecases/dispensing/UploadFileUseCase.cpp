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

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

UploadFileUseCase::UploadFileUseCase(std::shared_ptr<IUploadStoragePort> storage_port,
                                     std::shared_ptr<IUploadPreparationPort> preparation_port,
                                     size_t max_file_size_mb)
    : storage_port_(std::move(storage_port)),
      preparation_port_(std::move(preparation_port)),
      max_file_size_mb_(max_file_size_mb) {
    if (!storage_port_) {
        throw std::invalid_argument("UploadFileUseCase: storage_port cannot be null");
    }
    if (!preparation_port_) {
        throw std::invalid_argument("UploadFileUseCase: preparation_port cannot be null");
    }
}

Result<UploadResponse> UploadFileUseCase::Execute(const UploadRequest& request) {
    SILIGEN_LOG_INFO("Starting file upload: " + request.original_filename);

    // 1. 验证请求参数
    if (!request.Validate()) {
        return Result<UploadResponse>::Failure(Error(ErrorCode::INVALID_PARAMETER,
                                                     "Invalid upload request: empty file content or filename",
                                                     "UploadFileUseCase"));
    }

    // 2. 验证 DXF 格式
    auto dxf_validation = ValidateFileFormat(request.file_content);
    if (!dxf_validation.IsSuccess()) {
        return Result<UploadResponse>::Failure(dxf_validation.GetError());
    }

    // 3. 生成安全文件名
    std::string safe_filename = GenerateSafeFilename(request.original_filename);
    SILIGEN_LOG_INFO("Generated safe filename: " + safe_filename);

    // 4. 验证文件（大小、类型）
    std::vector<std::string> allowed_extensions = {"dxf", "DXF"};
    auto validation_result = storage_port_->Validate(request, max_file_size_mb_, allowed_extensions);

    if (!validation_result.IsSuccess()) {
        return Result<UploadResponse>::Failure(validation_result.GetError());
    }

    // 5. 存储文件
    auto store_result = storage_port_->Store(request, safe_filename);

    if (!store_result.IsSuccess()) {
        return Result<UploadResponse>::Failure(store_result.GetError());
    }
    const auto& stored_path = store_result.Value();

    // 6. 生成对应的准备产物
    auto prepared_result = preparation_port_->EnsurePreparedInput(stored_path);
    if (!prepared_result.IsSuccess()) {
        CleanupGeneratedArtifacts(stored_path);
        return Result<UploadResponse>::Failure(prepared_result.GetError());
    }

    // 7. 构建响应
    UploadResponse response;
    response.success = true;
    response.filepath = stored_path;
    response.original_name = request.original_filename;
    response.size = request.file_size;
    response.generated_filename = safe_filename;
    response.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    SILIGEN_LOG_INFO("File uploaded successfully: " + response.filepath);

    return Result<UploadResponse>::Success(response);
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

Result<void> UploadFileUseCase::ValidateFileFormat(const std::vector<uint8_t>& file_content) {
    // DXF 文件格式验证（简化版）
    // 检查文件头是否包含 DXF 标识

    if (file_content.size() < 20) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_FORMAT_INVALID, "File too small to be a valid DXF file", "UploadFileUseCase"));
    }

    // DXF 文件通常以文本开头，检查前几个字节
    std::string header(file_content.begin(), file_content.begin() + std::min(size_t(100), file_content.size()));

    // 检查是否包含 DXF 标识（ASCII DXF 以 0 开始，然后是 SECTION）
    // 这是一个简化的检查，实际 DXF 格式更复杂
    bool is_valid_dxf = false;

    // 检查 ASCII DXF 格式
    if (header.find("SECTION") != std::string::npos || header.find("0") == 0 ||  // DXF 组码
        header.find("$ACADVER") != std::string::npos) {
        is_valid_dxf = true;
    }

    // 检查二进制 DXF 格式（以 AutoCAD Binary DXF 开头）
    if (file_content.size() >= 22) {
        std::string binary_sig(file_content.begin(), file_content.begin() + 22);
        if (binary_sig.find("AutoCAD Binary DXF") != std::string::npos) {
            is_valid_dxf = true;
        }
    }

    if (!is_valid_dxf) {
        return Result<void>::Failure(Error(
            ErrorCode::FILE_FORMAT_INVALID, "File does not appear to be a valid DXF file", "UploadFileUseCase"));
    }

    return Result<void>::Success();
}

void UploadFileUseCase::CleanupGeneratedArtifacts(const std::string& stored_path) const noexcept {
    auto delete_result = storage_port_->Delete(stored_path);
    if (delete_result.IsError()) {
        SILIGEN_LOG_WARNING("清理上传失败DXF文件失败: " + delete_result.GetError().ToString());
    }

    auto cleanup_result = preparation_port_->CleanupPreparedInput(stored_path);
    if (cleanup_result.IsError()) {
        SILIGEN_LOG_WARNING("清理上传失败准备产物失败: " + cleanup_result.GetError().ToString());
    }
}

}  // namespace Siligen::JobIngest::Application::UseCases::Dispensing
