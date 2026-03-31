#pragma once

#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <cstdint>

#include "shared/types/DiagnosticsConfig.h"

namespace Siligen::Application::Facades::Tcp {
class TcpSystemFacade;
class TcpMotionFacade;
class TcpDispensingFacade;
class TcpRecipeFacade;
}

namespace Siligen::Domain::Configuration::Ports {
class IConfigurationPort;
}

namespace Siligen::Adapters::Tcp {
class MockIoControlService;

/**
 * @brief TCP命令分发器
 *
 * 职责：
 * - 解析JSON请求中的method字段
 * - 将请求分发到对应的应用门面
 * - 将门面响应转换为JSON
 *
 * 依赖规则：
 * - 只依赖TCP应用门面，不依赖ApplicationContainer
 * - JSON转换严格限制在此类内部
 */
class TcpCommandDispatcher {
public:
    /**
     * @brief 构造函数 - 通过依赖注入接收TCP应用门面
     *
     * 应用门面由组合根统一构造后注入，协议层不再直接拼装大量 use case。
     */
    TcpCommandDispatcher(
        std::shared_ptr<Application::Facades::Tcp::TcpSystemFacade> systemFacade,
        std::shared_ptr<Application::Facades::Tcp::TcpMotionFacade> motionFacade,
        std::shared_ptr<Application::Facades::Tcp::TcpDispensingFacade> dispensingFacade,
        std::shared_ptr<Application::Facades::Tcp::TcpRecipeFacade> recipeFacade,
        std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> configPort,
        std::shared_ptr<MockIoControlService> mockIoControl
    );

    ~TcpCommandDispatcher() = default;

    /**
     * @brief 分发请求到对应的应用门面
     * @param message JSON格式的请求字符串
     * @return JSON格式的响应字符串
     */
    std::string Dispatch(const std::string& message);

private:
    // 应用门面（由main.cpp聚合构造后注入）
    std::shared_ptr<Application::Facades::Tcp::TcpSystemFacade> systemFacade_;
    std::shared_ptr<Application::Facades::Tcp::TcpMotionFacade> motionFacade_;
    std::shared_ptr<Application::Facades::Tcp::TcpDispensingFacade> dispensingFacade_;
    std::shared_ptr<Application::Facades::Tcp::TcpRecipeFacade> recipeFacade_;
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> configPort_;
    std::shared_ptr<MockIoControlService> mockIoControl_;
    Shared::Types::DiagnosticsConfig diagnostics_config_;

    struct DxfCache {
        bool loaded = false;
        std::string artifact_id;
        std::string filepath;
        uint32_t segment_count = 0;
        double total_length = 0.0;
        double x_min = 0.0;
        double x_max = 0.0;
        double y_min = 0.0;
        double y_max = 0.0;
        std::string plan_id;
        std::string plan_fingerprint;
        std::string preview_snapshot_id;
        std::string preview_snapshot_hash;
        std::string preview_request_signature;
        std::string preview_generated_at;
        std::string preview_confirmed_at;
        std::string preview_state;
        std::string preview_source;
        double preview_speed_mm_s = 0.0;
    };

    mutable std::mutex dxf_mutex_;
    DxfCache dxf_cache_;
    std::string active_dxf_job_id_;

    mutable std::mutex alarms_mutex_;
    std::unordered_set<std::string> acknowledged_alarm_ids_;

    void LoadDiagnosticsConfig();
    bool IsDeepTcpLoggingEnabled() const;
    bool IsDeepMotionLoggingEnabled() const;
    std::string TruncatePayload(const std::string& payload) const;

    // 命令处理方法
    std::string HandlePing(const std::string& id, const nlohmann::json& params);
    std::string HandleConnect(const std::string& id, const nlohmann::json& params);
    std::string HandleDisconnect(const std::string& id, const nlohmann::json& params);
    std::string HandleMockIoSet(const std::string& id, const nlohmann::json& params);
    std::string HandleStatus(const std::string& id, const nlohmann::json& params);
    std::string HandleMotionCoordStatus(const std::string& id, const nlohmann::json& params);
    std::string HandleHome(const std::string& id, const nlohmann::json& params);
    std::string HandleHomeAuto(const std::string& id, const nlohmann::json& params);
    std::string HandleHomeGo(const std::string& id, const nlohmann::json& params);
    std::string HandleJog(const std::string& id, const nlohmann::json& params);
    std::string HandleMove(const std::string& id, const nlohmann::json& params);
    std::string HandleStop(const std::string& id, const nlohmann::json& params);
    std::string HandleEstop(const std::string& id, const nlohmann::json& params);
    std::string HandleEstopReset(const std::string& id, const nlohmann::json& params);
    std::string HandleDispenserStart(const std::string& id, const nlohmann::json& params);
    std::string HandleDispenserStop(const std::string& id, const nlohmann::json& params);
    std::string HandleDispenserPause(const std::string& id, const nlohmann::json& params);
    std::string HandleDispenserResume(const std::string& id, const nlohmann::json& params);
    std::string HandlePurge(const std::string& id, const nlohmann::json& params);
    std::string HandleSupplyOpen(const std::string& id, const nlohmann::json& params);
    std::string HandleSupplyClose(const std::string& id, const nlohmann::json& params);
    std::string HandleDxfLoad(const std::string& id, const nlohmann::json& params);
    std::string HandleDxfArtifactCreate(const std::string& id, const nlohmann::json& params);
    std::string HandleDxfPlanPrepare(const std::string& id, const nlohmann::json& params);
    std::string HandleDxfJobStart(const std::string& id, const nlohmann::json& params);
    std::string HandleDxfJobStatus(const std::string& id, const nlohmann::json& params);
    std::string HandleDxfJobPause(const std::string& id, const nlohmann::json& params);
    std::string HandleDxfJobResume(const std::string& id, const nlohmann::json& params);
    std::string HandleDxfJobStop(const std::string& id, const nlohmann::json& params);
    std::string HandleDxfInfo(const std::string& id, const nlohmann::json& params);
    std::string HandleDxfPreviewSnapshot(const std::string& id, const nlohmann::json& params);
    std::string HandleDxfPreviewConfirm(const std::string& id, const nlohmann::json& params);
    std::string HandleAlarmsList(const std::string& id, const nlohmann::json& params);
    std::string HandleAlarmsClear(const std::string& id, const nlohmann::json& params);
    std::string HandleAlarmsAcknowledge(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeList(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeGet(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeCreate(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeUpdate(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeArchive(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeDraftCreate(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeDraftUpdate(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipePublish(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeVersions(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeVersionCreate(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeCompare(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeActivate(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeTemplates(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeSchemaDefault(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeAudit(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeExport(const std::string& id, const nlohmann::json& params);
    std::string HandleRecipeImport(const std::string& id, const nlohmann::json& params);

    // 命令路由表
    using CommandHandler = std::function<std::string(const std::string&, const nlohmann::json&)>;
    std::unordered_map<std::string, CommandHandler> commandHandlers_;

    void RegisterCommand(const std::string& name, CommandHandler handler);
    void RegisterCoreCommands();
    void RegisterMotionCommands();
    void RegisterDispensingCommands();
    void RegisterDxfCommands();
    void RegisterAlarmCommands();
    void RegisterRecipeCommands();
    void RegisterCommands();
};

} // namespace Siligen::Adapters::Tcp

