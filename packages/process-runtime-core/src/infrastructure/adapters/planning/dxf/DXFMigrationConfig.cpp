#include "DXFMigrationConfig.h"

#define MODULE_NAME "DXFMigrationConfig"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include <filesystem>
#include <system_error>

namespace Siligen::Infrastructure::Adapters::Parsing {
namespace {

std::filesystem::path CanonicalizeOrKeep(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    const auto canonical_path = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return path;
    }
    return canonical_path;
}

std::filesystem::path FindWorkspaceRoot() noexcept {
    std::error_code ec;
    auto current = std::filesystem::current_path(ec);
    if (ec) {
        return std::filesystem::path(".");
    }

    auto candidate = current;
    constexpr int max_levels = 12;
    for (int level = 0; level < max_levels; ++level) {
        if (std::filesystem::exists(candidate / "WORKSPACE.md", ec) && !ec) {
            return candidate;
        }
        ec.clear();

        const auto parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }

    return current;
}

std::filesystem::path ResolvePipelineRoot(const std::string& configured_path) noexcept {
    std::filesystem::path configured(configured_path);
    if (configured.is_absolute()) {
        return CanonicalizeOrKeep(configured);
    }
    return CanonicalizeOrKeep(FindWorkspaceRoot() / configured);
}

std::string ResolvePipelineRootString(const std::string& configured_path) {
    return ResolvePipelineRoot(configured_path).string();
}

}  // namespace

// 静态成员初始化
DXFMigrationConfig DXFMigrationConfigManager::config_;

DXFMigrationConfig& DXFMigrationConfigManager::GetConfig() {
    return config_;
}

bool DXFMigrationConfigManager::ShouldUseService() {
    switch (config_.current_phase) {
        case DXFMigrationConfig::MigrationPhase::SERVICE_REQUIRED:
            return true;
        case DXFMigrationConfig::MigrationPhase::SERVICE_AVAILABLE:
            return IsServiceAvailable() && config_.enable_fallback;
        case DXFMigrationConfig::MigrationPhase::LOCAL_ONLY:
        default:
            return false;
    }
}

bool DXFMigrationConfigManager::IsServiceAvailable() {
    if (!config_.enable_service_discovery) {
        return false;
    }
    
    // 检查服务健康状态
    if (config_.enable_health_check) {
        return CheckServiceHealth();
    }
    
    // 基础检查：engineering-data目录是否存在
    return std::filesystem::exists(ResolvePipelineRoot(config_.dxf_pipeline_path));
}

void DXFMigrationConfigManager::UpdateMigrationPhase(DXFMigrationConfig::MigrationPhase new_phase) {
    config_.current_phase = new_phase;
    
    // 记录迁移事件
    if (config_.log_migration_events) {
        const char* phase_names[] = {"LOCAL_ONLY", "SERVICE_AVAILABLE", "SERVICE_REQUIRED"};
        SILIGEN_LOG_INFO("DXF迁移阶段更新: " + std::string(phase_names[static_cast<int>(new_phase)]));
    }
}

std::string DXFMigrationConfigManager::GetServiceCommand(const std::string& dxf_path, const std::string& output_path) {
    switch (config_.service_protocol) {
        case DXFMigrationConfig::ServiceProtocol::COMMAND_LINE:
            // 构建命令行调用
            return "python " + ResolvePipelineRootString(config_.dxf_pipeline_path) + "/scripts/dxf_to_pb.py " +
                   dxf_path + " " + output_path;
        
        case DXFMigrationConfig::ServiceProtocol::HTTP_REST:
            // 构建HTTP请求（未来实现）
            return config_.service_endpoint + "/api/dxf/convert?input=" + dxf_path + "&output=" + output_path;
        
        case DXFMigrationConfig::ServiceProtocol::GRPC:
        default:
            // 暂不支持
            return "";
    }
}

bool DXFMigrationConfigManager::ValidateConfig() {
    // 检查 engineering-data 路径
    const auto pipeline_root = ResolvePipelineRoot(config_.dxf_pipeline_path);
    if (!std::filesystem::exists(pipeline_root)) {
        SILIGEN_LOG_WARNING("engineering-data路径不存在: " + pipeline_root.string());
        return false;
    }
    
    // 检查服务端点格式
    if (config_.service_protocol == DXFMigrationConfig::ServiceProtocol::HTTP_REST) {
        if (config_.service_endpoint.empty()) {
            SILIGEN_LOG_WARNING("HTTP服务端点未配置");
            return false;
        }
    }
    
    // 验证命令行接口
    if (config_.service_protocol == DXFMigrationConfig::ServiceProtocol::COMMAND_LINE) {
        return ValidateCommandLineInterface();
    }
    
    return true;
}

bool DXFMigrationConfigManager::CheckServiceHealth() {
    // 未来实现：检查服务健康状态
    // 当前仅检查目录存在性
    return std::filesystem::exists(ResolvePipelineRoot(config_.dxf_pipeline_path));
}

bool DXFMigrationConfigManager::ValidateCommandLineInterface() {
    // 检查脚本文件是否存在
    std::string script_path = ResolvePipelineRootString(config_.dxf_pipeline_path) + "/scripts/dxf_to_pb.py";
    
    if (!std::filesystem::exists(script_path)) {
        SILIGEN_LOG_WARNING("DXF转换脚本不存在: " + script_path);
        return false;
    }
    
    // 检查Python环境（基础验证）
    // 这里可以添加更详细的Python环境检查
    
    return true;
}

} // namespace Siligen::Infrastructure::Adapters::Parsing
