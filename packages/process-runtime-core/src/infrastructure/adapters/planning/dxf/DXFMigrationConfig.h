#pragma once

#include <string>

namespace Siligen::Infrastructure::Adapters::Parsing {

/**
 * @brief DXF迁移配置管理
 * 
 * 管理 DXF 运行时适配器到 engineering-data 预处理管线的配置。
 * 支持渐进式迁移和配置切换。
 */
struct DXFMigrationConfig {
    // 迁移阶段配置
    enum class MigrationPhase {
        LOCAL_ONLY,         // 仅使用本地实现
        SERVICE_AVAILABLE,   // 服务可用，可切换
        SERVICE_REQUIRED     // 必须使用服务
    };

    // 服务调用方式
    enum class ServiceProtocol {
        HTTP_REST,          // HTTP REST API
        COMMAND_LINE,       // 命令行调用
        GRPC               // gRPC调用（未来）
    };

    MigrationPhase current_phase = MigrationPhase::LOCAL_ONLY;
    ServiceProtocol service_protocol = ServiceProtocol::COMMAND_LINE;
    
    // 服务配置
    std::string service_endpoint = "http://localhost:5000";
    std::string dxf_pipeline_path = "packages/engineering-data";  // 保留旧字段名以兼容历史配置
    int service_timeout_ms = 30000;
    int max_retries = 3;
    
    // 功能开关
    bool enable_service_discovery = false;
    bool enable_fallback = true;
    bool enable_health_check = true;
    
    // 日志配置
    bool log_service_calls = true;
    bool log_migration_events = true;
};

/**
 * @brief 迁移配置管理器
 */
class DXFMigrationConfigManager {
public:
    static DXFMigrationConfig& GetConfig();
    
    /**
     * @brief 检查是否应该使用服务
     */
    static bool ShouldUseService();
    
    /**
     * @brief 检查服务是否可用
     */
    static bool IsServiceAvailable();
    
    /**
     * @brief 更新迁移阶段
     */
    static void UpdateMigrationPhase(DXFMigrationConfig::MigrationPhase new_phase);
    
    /**
     * @brief 获取服务调用命令
     */
    static std::string GetServiceCommand(const std::string& dxf_path, const std::string& output_path);
    
    /**
     * @brief 验证配置有效性
     */
    static bool ValidateConfig();
    
private:
    static DXFMigrationConfig config_;
    
    // 服务健康检查
    static bool CheckServiceHealth();
    
    // 命令行调用验证
    static bool ValidateCommandLineInterface();
};

} // namespace Siligen::Infrastructure::Adapters::Parsing
