#include "UploadFileUseCase.h"
#include "application/services/dxf/DxfPbPreparationService.h"
#include "shared/interfaces/ILoggingService.h"

#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/configuration/ports/IFileStoragePort.h"

// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "UploadFileUseCase"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>

namespace Siligen::Application::UseCases::Dispensing {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

UploadFileUseCase::UploadFileUseCase(std::shared_ptr<Domain::Configuration::Ports::IFileStoragePort> file_storage_port,
                                           size_t max_file_size_mb,
                                           std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
                                           std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService>
                                               pb_preparation_service)
    : file_storage_port_(std::move(file_storage_port)),
      max_file_size_mb_(max_file_size_mb),
      config_port_(std::move(config_port)),
      pb_preparation_service_(pb_preparation_service
                                  ? std::move(pb_preparation_service)
                                  : std::make_shared<Siligen::Application::Services::DXF::DxfPbPreparationService>(
                                        config_port_)) {
    if (!file_storage_port_) {
        throw std::invalid_argument("UploadFileUseCase: file_storage_port cannot be null");
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

    // 4. 构造文件数据
    Domain::Configuration::Ports::FileData file_data{
        request.file_content,       // content
        safe_filename,              // original_name (使用生成的安全文件名)
        request.file_size,          // size
        request.content_type        // content_type
    };

    // 5. 验证文件（大小、类型）
    std::vector<std::string> allowed_extensions = {"dxf", "DXF"};
    auto validation_result = file_storage_port_->ValidateFile(file_data, max_file_size_mb_, allowed_extensions);

    if (!validation_result.IsSuccess()) {
        return Result<UploadResponse>::Failure(validation_result.GetError());
    }

    // 6. 存储文件
    auto store_result = file_storage_port_->StoreFile(file_data, safe_filename);

    if (!store_result.IsSuccess()) {
        return Result<UploadResponse>::Failure(store_result.GetError());
    }

    // 6.1 生成对应的 PB 文件
    auto pb_result = pb_preparation_service_->EnsurePbReady(store_result.Value());
    if (!pb_result.IsSuccess()) {
        CleanupGeneratedArtifacts(store_result.Value());
        return Result<UploadResponse>::Failure(pb_result.GetError());
    }

    // 7. 构建响应
    UploadResponse response;
    response.success = true;
    response.filepath = store_result.Value();
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

void UploadFileUseCase::CleanupGeneratedArtifacts(const std::string& filepath) noexcept {
    auto delete_result = file_storage_port_->DeleteFile(filepath);
    if (delete_result.IsError()) {
        SILIGEN_LOG_WARNING("清理上传失败DXF文件失败: " + delete_result.GetError().ToString());
    }

    std::filesystem::path pb_path(filepath);
    pb_path.replace_extension(".pb");

    std::error_code ec;
    std::filesystem::remove(pb_path, ec);
    if (ec) {
        SILIGEN_LOG_WARNING("清理上传失败PB文件失败: " + pb_path.string() + ", error=" + ec.message());
    }
}

}  // namespace Siligen::Application::UseCases::Dispensing





