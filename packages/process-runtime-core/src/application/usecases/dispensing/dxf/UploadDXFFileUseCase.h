#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
namespace Siligen::Domain::Configuration::Ports {
class IFileStoragePort;
class IConfigurationPort;
}
namespace Siligen::Application::Services::DXF {
class DxfPbPreparationService;
}
namespace Siligen::Shared::Types {
template <typename T>
class Result;
}

namespace Siligen::Application::UseCases::Dispensing::DXF {

using Siligen::Shared::Types::Result;

/**
 * @brief DXF 文件上传请求
 */
struct DXFUploadRequest {
    std::vector<uint8_t> file_content;  ///< 文件内容
    std::string original_filename;      ///< 原始文件名
    size_t file_size;                   ///< 文件大小（字节）
    std::string content_type;           ///< MIME 类型

    /**
     * @brief 验证请求参数
     * @return bool 是否有效
     */
    bool Validate() const {
        return !file_content.empty() && !original_filename.empty() && file_size > 0;
    }
};

/**
 * @brief DXF 文件上传响应
 */
struct DXFUploadResponse {
    bool success;                    ///< 是否成功
    std::string filepath;            ///< 存储文件路径
    std::string original_name;       ///< 原始文件名
    size_t size;                     ///< 文件大小（字节）
    std::string generated_filename;  ///< 生成的安全文件名
    int64_t timestamp;               ///< 上传时间戳
};

/**
 * @brief DXF 文件上传用例
 *
 * 业务流程:
 * 1. 验证请求参数
 * 2. 生成安全文件名（UUID + 时间戳 + 原始名称）
 * 3. 验证文件（大小、类型、DXF 格式）
 * 4. 存储文件
 * 5. 返回文件路径
 *
 * 架构合规性:
 * - 调用 IFileStoragePort 接口
 * - 不依赖具体实现
 * - 使用 Result<T> 错误处理
 */
class UploadDXFFileUseCase {
   public:
    /**
     * @brief 构造函数
     * @param file_storage_port 文件存储端口
     * @param max_file_size_mb 最大文件大小（MB）
     */
    UploadDXFFileUseCase(std::shared_ptr<Domain::Configuration::Ports::IFileStoragePort> file_storage_port,
                         size_t max_file_size_mb = 10,
                         std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port = nullptr,
                         std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService>
                             pb_preparation_service = nullptr);

    ~UploadDXFFileUseCase() = default;

    // 禁止拷贝和移动
    UploadDXFFileUseCase(const UploadDXFFileUseCase&) = delete;
    UploadDXFFileUseCase& operator=(const UploadDXFFileUseCase&) = delete;
    UploadDXFFileUseCase(UploadDXFFileUseCase&&) = delete;
    UploadDXFFileUseCase& operator=(UploadDXFFileUseCase&&) = delete;

    /**
     * @brief 执行文件上传
     * @param request 上传请求
     * @return Result<DXFUploadResponse> 上传结果
     */
    Result<DXFUploadResponse> Execute(const DXFUploadRequest& request);

   private:
    std::shared_ptr<Domain::Configuration::Ports::IFileStoragePort> file_storage_port_;
    size_t max_file_size_mb_;
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> pb_preparation_service_;

    /**
     * @brief 生成安全文件名
     * @param original_filename 原始文件名
     * @return std::string 安全文件名 (UUID_timestamp_original.dxf)
     */
    std::string GenerateSafeFilename(const std::string& original_filename);

    /**
     * @brief 验证 DXF 文件格式
     * @param file_content 文件内容
     * @return Result<void> 验证结果
     */
    Result<void> ValidateDXFFormat(const std::vector<uint8_t>& file_content);

    /**
     * @brief 清理上传失败后遗留的 DXF/PB 文件
     */
    void CleanupGeneratedArtifacts(const std::string& filepath) noexcept;
};

}  // namespace Siligen::Application::UseCases::Dispensing::DXF

