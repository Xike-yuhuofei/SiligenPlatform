#include "AutoPathSourceAdapter.h"
#include "DXFMigrationConfig.h"

#define MODULE_NAME "AutoPathSourceAdapter"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace Siligen::Infrastructure::Adapters::Parsing {

namespace {
std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
    return value;
}

bool EndsWith(const std::string& value, const std::string& suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }
    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// 模拟DXF文件分析 - 未来将迁移到dxf-pipeline
DXFValidationResult AnalyzeDXFInternal(const std::string& filepath) {
    DXFValidationResult result;
    
    // 基本文件检查
    if (!std::filesystem::exists(filepath)) {
        result.error_details = "文件不存在: " + filepath;
        return result;
    }
    
    // 简单格式检测（基于文件扩展名）
    std::string ext = std::filesystem::path(filepath).extension().string();
    if (EndsWith(ToLower(ext), ".dxf")) {
        result.is_valid = true;
        result.format_version = "检测为DXF格式";
        result.entity_count = 100; // 模拟值
        
        // 简单判断是否需要预处理（基于文件大小或特定条件）
        auto file_size = std::filesystem::file_size(filepath);
        result.requires_preprocessing = file_size > 1024 * 1024; // 大于1MB需要预处理
    } else {
        result.error_details = "不支持的文件格式: " + ext;
    }
    
    return result;
}
} // namespace

AutoPathSourceAdapter::AutoPathSourceAdapter()
    : pb_adapter_(std::make_shared<PbPathSourceAdapter>()) {
}

// 实现通用接口 - 保持向后兼容
Result<PathSourceResult> AutoPathSourceAdapter::LoadFromFile(const std::string& filepath) {
    const std::string lower = ToLower(filepath);
    
    if (EndsWith(lower, ".pb")) {
        return pb_adapter_->LoadFromFile(filepath);
    }
    
    if (EndsWith(lower, ".dxf")) {
        // 对于DXF文件，现在使用专用接口处理
        auto dxf_result = LoadDXFFile(filepath);
        if (!dxf_result.IsSuccess()) {
            return Result<PathSourceResult>::Failure(dxf_result.GetError());
        }
        
        // 转换结果格式以保持兼容性
        PathSourceResult result;
        const auto& dxf_value = dxf_result.Value();
        result.success = dxf_value.success;
        result.error_message = dxf_value.error_message;
        result.primitives = dxf_value.primitives;
        
        return Result<PathSourceResult>::Success(result);
    }
    
    return Result<PathSourceResult>::Failure(
        Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::FILE_FORMAT_INVALID,
                                      "不支持的路径源类型: " + filepath,
                                      "AutoPathSourceAdapter"));
}

// 实现专用DXF接口
Result<DXFPathSourceResult> AutoPathSourceAdapter::LoadDXFFile(const std::string& filepath) {
    DXFPathSourceResult result;
    
    // 首先验证文件
    auto validation = ValidateDXFFile(filepath);
    if (!validation.IsSuccess()) {
        result.error_message = "文件验证失败: " + validation.GetError().ToString();
        return Result<DXFPathSourceResult>::Success(result);
    }
    
    const auto& validation_result = validation.Value();
    if (!validation_result.is_valid) {
        result.error_message = "无效的DXF文件: " + validation_result.error_details;
        return Result<DXFPathSourceResult>::Success(result);
    }
    
    // 检查是否需要预处理
    auto needs_preprocessing = RequiresPreprocessing(filepath);
    if (!needs_preprocessing.IsSuccess()) {
        result.error_message = "预处理检查失败: " + needs_preprocessing.GetError().ToString();
        return Result<DXFPathSourceResult>::Success(result);
    }
    
    if (needs_preprocessing.Value()) {
        // 需要预处理 - 生成PB文件
        auto pb_path_result = PreprocessDXFFile(filepath);
        if (!pb_path_result.IsSuccess()) {
            result.error_message = "DXF预处理失败: " + pb_path_result.GetError().ToString();
            return Result<DXFPathSourceResult>::Success(result);
        }
        
        const std::string& pb_path = pb_path_result.Value();
        result.was_preprocessed = true;
        result.preprocessing_log = "已生成预处理文件: " + pb_path;
        
        // 加载预处理后的PB文件
        auto pb_result = pb_adapter_->LoadFromFile(pb_path);
        if (!pb_result.IsSuccess()) {
            result.error_message = "加载预处理文件失败: " + pb_result.GetError().ToString();
            return Result<DXFPathSourceResult>::Success(result);
        }
        
        const auto& pb_value = pb_result.Value();
        result.success = pb_value.success;
        result.primitives = pb_value.primitives;
        result.source_format = "DXF(R12) -> PB";
        
    } else {
        // 直接加载PB文件（如果存在）
        std::filesystem::path pb_path(filepath);
        pb_path.replace_extension(".pb");
        
        if (std::filesystem::exists(pb_path)) {
            auto pb_result = pb_adapter_->LoadFromFile(pb_path.string());
            if (pb_result.IsSuccess()) {
                const auto& pb_value = pb_result.Value();
                result.success = pb_value.success;
                result.primitives = pb_value.primitives;
                result.source_format = "PB";
            }
        }
        
        if (!result.success) {
            result.error_message = "需要预处理但未找到PB文件，请先生成预处理文件";
        }
    }
    
    return Result<DXFPathSourceResult>::Success(result);
}

Result<DXFValidationResult> AutoPathSourceAdapter::ValidateDXFFile(const std::string& filepath) {
    return Result<DXFValidationResult>::Success(AnalyzeDXFInternal(filepath));
}

std::vector<std::string> AutoPathSourceAdapter::GetSupportedFormats() {
    return {"DXF R12", "DXF R14", "DXF 2000", "DXF 2004", "DXF 2007", "DXF 2010", "PB"};
}

Result<bool> AutoPathSourceAdapter::RequiresPreprocessing(const std::string& filepath) {
    auto validation = AnalyzeDXFInternal(filepath);
    if (!validation.is_valid) {
        return Result<bool>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::FILE_FORMAT_INVALID,
                                          "无效的DXF文件: " + validation.error_details,
                                          "AutoPathSourceAdapter"));
    }
    
    // 检查PB文件是否存在
    std::filesystem::path pb_path(filepath);
    pb_path.replace_extension(".pb");
    bool pb_exists = std::filesystem::exists(pb_path);
    
    // 如果需要预处理且PB文件不存在，则需要预处理
    bool needs_preprocessing = validation.requires_preprocessing && !pb_exists;
    
    return Result<bool>::Success(needs_preprocessing);
}

// 私有方法实现
Result<std::string> AutoPathSourceAdapter::PreprocessDXFFile(const std::string& dxf_path) {
    // 生成PB文件路径
    std::filesystem::path pb_path(dxf_path);
    pb_path.replace_extension(".pb");
    
    // 检查dxf-pipeline服务是否可用
    bool use_service = CheckDXFPipelineServiceAvailable();
    
    if (use_service) {
        // 调用dxf-pipeline服务进行预处理
        auto service_result = CallDXFPipelineService(dxf_path, pb_path.string());
        if (service_result.IsSuccess()) {
            return Result<std::string>::Success(pb_path.string());
        }
        // 服务调用失败，回退到本地处理
        SILIGEN_LOG_WARNING("DXF pipeline服务调用失败，使用本地处理: " + service_result.GetError().GetMessage());
    }
    
    // 本地预处理实现 - 未来将迁移到dxf-pipeline
    // 这里只是创建空文件作为占位符
    std::ofstream pb_file(pb_path);
    if (!pb_file) {
        return Result<std::string>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::FILE_IO_ERROR,
                                          "无法创建PB文件: " + pb_path.string(),
                                          "AutoPathSourceAdapter"));
    }
    
    pb_file << "# 预处理占位符 - 未来将由dxf-pipeline生成实际内容" << std::endl;
    pb_file.close();
    
    return Result<std::string>::Success(pb_path.string());
}

// 检查dxf-pipeline服务是否可用
bool AutoPathSourceAdapter::CheckDXFPipelineServiceAvailable() {
    return DXFMigrationConfigManager::ShouldUseService() && 
           DXFMigrationConfigManager::ValidateConfig();
}

// 调用dxf-pipeline服务
Result<bool> AutoPathSourceAdapter::CallDXFPipelineService(const std::string& dxf_path, const std::string& pb_path) {
    auto& config = DXFMigrationConfigManager::GetConfig();
    
    if (config.log_service_calls) {
        SILIGEN_LOG_INFO("调用DXF pipeline服务处理文件: " + dxf_path);
    }
    
    // 根据配置选择调用方式
    switch (config.service_protocol) {
        case DXFMigrationConfig::ServiceProtocol::COMMAND_LINE:
            return CallCommandLineService(dxf_path, pb_path);
        case DXFMigrationConfig::ServiceProtocol::HTTP_REST:
            return CallHttpService(dxf_path, pb_path);
        case DXFMigrationConfig::ServiceProtocol::GRPC:
        default:
            return Result<bool>::Failure(
                Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED,
                                              "不支持的协议类型",
                                              "AutoPathSourceAdapter"));
    }
}

// 命令行服务调用
Result<bool> AutoPathSourceAdapter::CallCommandLineService(const std::string& dxf_path, const std::string& pb_path) {
    auto& config = DXFMigrationConfigManager::GetConfig();
    
    // 构建命令
    std::string command = DXFMigrationConfigManager::GetServiceCommand(dxf_path, pb_path);
    
    if (command.empty()) {
        return Result<bool>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::COMMAND_FAILED,
                                          "无法构建服务命令",
                                          "AutoPathSourceAdapter"));
    }
    
    if (config.log_service_calls) {
        SILIGEN_LOG_INFO("执行命令行服务调用: " + command);
    }
    
    // 执行命令
    int result = std::system(command.c_str());
    
    if (result != 0) {
        std::string error_msg = "命令行服务调用失败，退出码: " + std::to_string(result);
        
        if (config.log_service_calls) {
            SILIGEN_LOG_ERROR(error_msg);
        }
        
        return Result<bool>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::COMMAND_FAILED,
                                          error_msg,
                                          "AutoPathSourceAdapter"));
    }
    
    // 检查输出文件是否生成
    if (!std::filesystem::exists(pb_path)) {
        std::string error_msg = "服务调用成功但未生成输出文件: " + pb_path;
        
        if (config.log_service_calls) {
            SILIGEN_LOG_ERROR(error_msg);
        }
        
        return Result<bool>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::FILE_NOT_FOUND,
                                          error_msg,
                                          "AutoPathSourceAdapter"));
    }
    
    if (config.log_service_calls) {
        SILIGEN_LOG_INFO("命令行服务调用成功，生成文件: " + pb_path);
    }
    
    return Result<bool>::Success(true);
}

// HTTP服务调用
Result<bool> AutoPathSourceAdapter::CallHttpService(const std::string& dxf_path, const std::string& pb_path) {
    // 未来实现：HTTP REST API调用
    // 当前返回失败，使用本地处理
    return Result<bool>::Failure(
        Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED,
                                      "HTTP服务调用暂未实现",
                                      "AutoPathSourceAdapter"));
}

Result<bool> AutoPathSourceAdapter::CheckPBFileExists(const std::string& dxf_path) {
    std::filesystem::path pb_path(dxf_path);
    pb_path.replace_extension(".pb");
    return Result<bool>::Success(std::filesystem::exists(pb_path));
}

Result<DXFValidationResult> AutoPathSourceAdapter::AnalyzeDXFFile(const std::string& filepath) {
    return Result<DXFValidationResult>::Success(AnalyzeDXFInternal(filepath));
}

} // namespace Siligen::Infrastructure::Adapters::Parsing
