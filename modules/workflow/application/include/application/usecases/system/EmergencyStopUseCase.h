// EmergencyStopUseCase.h
// 版本: 1.0.0
// 描述: 紧急停止用例 - 应用层业务流程编排
//
// 六边形架构 - 应用层用例
// 依赖方向: 依赖领域服务,协调多个领域对象完成紧急停止业务流程
// Task: T047 - Phase 4 建立清晰的模块边界

#pragma once

#include "domain/machine/aggregates/DispenserModel.h"
#include "domain/dispensing/domain-services/CMPTriggerService.h"
#include "domain/motion/domain-services/MotionControlService.h"
#include "domain/motion/domain-services/MotionStatusService.h"
#include "domain/safety/domain-services/EmergencyStopService.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::System {
using Siligen::Domain::Machine::Aggregates::Legacy::DispenserModel;

/// @brief 紧急停止的原因枚举
/// @details 记录触发紧急停止的具体原因
enum class EmergencyStopReason : int32_t {
    USER_REQUEST = 0,      // 用户请求 (User request)
    POSITION_ERROR = 1,    // 位置误差过大 (Position error exceeded)
    HARDWARE_ERROR = 2,    // 硬件错误 (Hardware error)
    SAFETY_VIOLATION = 3,  // 安全违规 (Safety violation)
    TIMEOUT = 4,           // 操作超时 (Operation timeout)
    EXTERNAL_SIGNAL = 5,   // 外部信号 (External signal)
    UNKNOWN_CAUSE = 6      // 未知原因（已确认无法归类）
    // 注意: 未初始化状态应使用 std::optional<EmergencyStopReason> 表示
};

/// @brief 紧急停止原因转字符串
inline const char* EmergencyStopReasonToString(EmergencyStopReason reason) {
    switch (reason) {
        case EmergencyStopReason::USER_REQUEST:
            return "USER_REQUEST";
        case EmergencyStopReason::POSITION_ERROR:
            return "POSITION_ERROR";
        case EmergencyStopReason::HARDWARE_ERROR:
            return "HARDWARE_ERROR";
        case EmergencyStopReason::SAFETY_VIOLATION:
            return "SAFETY_VIOLATION";
        case EmergencyStopReason::TIMEOUT:
            return "TIMEOUT";
        case EmergencyStopReason::EXTERNAL_SIGNAL:
            return "EXTERNAL_SIGNAL";
        case EmergencyStopReason::UNKNOWN_CAUSE:
            return "UNKNOWN_CAUSE";
        default:
            return "INVALID";
    }
}

/// @brief 紧急停止的请求参数
/// @details 封装用例输入参数
struct EmergencyStopRequest {
    EmergencyStopReason reason;                       // 停止原因 (Stop reason)
    std::string detail_message;                       // 详细信息 (Detail message)
    bool disable_hardware;                            // 是否禁用硬件 (Disable hardware)
    bool clear_task_queue;                            // 是否清空任务队列 (Clear task queue)
    bool disable_cmp;                                 // 是否禁用CMP触发 (Disable CMP trigger)
    std::chrono::system_clock::time_point timestamp;  // 时间戳 (Timestamp)

    EmergencyStopRequest()
        : reason(EmergencyStopReason::USER_REQUEST),
          detail_message(""),
          disable_hardware(true),
          clear_task_queue(true),
          disable_cmp(true),
          timestamp(std::chrono::system_clock::now()) {}

    // 验证请求参数 (Validate request parameters)
    bool Validate() const {
        return true;  // 紧急停止请求总是有效
    }
};

/// @brief 紧急停止的响应结果
/// @details 封装用例输出结果
struct EmergencyStopResponse {
    bool motion_stopped;                            // 运动是否已停止 (Motion stopped)
    bool cmp_disabled;                              // CMP是否已禁用 (CMP disabled)
    bool hardware_disabled;                         // 硬件是否已禁用 (Hardware disabled)
    int32_t tasks_cleared;                          // 清除的任务数量 (Tasks cleared count)
    Siligen::Shared::Types::Point2D stop_position;  // 停止位置 (Stop position)
    std::chrono::milliseconds response_time;        // 响应时间 (Response time)
    std::vector<std::string> actions_taken;         // 执行的动作列表 (Actions taken)
    std::string status_message;                     // 状态消息 (Status message)

    EmergencyStopResponse()
        : motion_stopped(false),
          cmp_disabled(false),
          hardware_disabled(false),
          tasks_cleared(0),
          stop_position(),
          response_time(0),
          actions_taken(),
          status_message("") {}
};

/// @brief 紧急停止用例
/// @details 应用层用例,协调多个领域服务完成紧急停止业务流程
///
/// 业务流程:
/// 1. 记录紧急停止请求和时间戳
/// 2. 立即执行运动紧急停止
/// 3. 禁用CMP触发输出
/// 4. (可选)清空任务队列
/// 5. (可选)禁用硬件
/// 6. 更新点胶机状态为EMERGENCY_STOP
/// 7. 记录停止位置和响应时间
///
/// 性能约束:
/// - 响应时间: <100ms (关键安全要求)
/// - 所有操作必须串行执行,确保可靠性
/// - 错误不影响紧急停止的执行
///
/// 错误处理:
/// - 紧急停止操作不会因为任何错误而中断
/// - 所有错误都记录但不传播,确保尽可能多的停止动作被执行
/// - 返回结果包含所有执行的动作和状态
///
/// 使用示例:
/// @code
/// auto useCase = std::make_shared<EmergencyStopUseCase>(
///     motionControlService, motionStatusService, cmpService, dispenserModel, logger);
/// EmergencyStopRequest request;
/// request.reason = EmergencyStopReason::USER_REQUEST;
/// request.detail_message = "User pressed emergency stop button";
/// auto result = useCase->Execute(request);
/// if (result.IsSuccess()) {
///     auto response = result.GetValue();
///     // 检查停止结果
/// }
/// @endcode
class EmergencyStopUseCase {
   public:
    /// @brief 构造函数
    /// @param motion_control_service 运动控制领域服务
    /// @param motion_status_service 运动状态查询服务
    /// @param cmp_service CMP触发控制领域服务
    /// @param dispenser_model 点胶机领域模型
    /// @param logging_service 日志服务
    explicit EmergencyStopUseCase(
        std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionControlService> motion_control_service,
        std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionStatusService> motion_status_service,
        std::shared_ptr<Siligen::Domain::Dispensing::DomainServices::CMPService> cmp_service,
        std::shared_ptr<DispenserModel> dispenser_model,
        std::shared_ptr<Siligen::Shared::Interfaces::ILoggingService> logging_service);

    ~EmergencyStopUseCase() = default;

    // 禁止拷贝和移动
    EmergencyStopUseCase(const EmergencyStopUseCase&) = delete;
    EmergencyStopUseCase& operator=(const EmergencyStopUseCase&) = delete;
    EmergencyStopUseCase(EmergencyStopUseCase&&) = delete;
    EmergencyStopUseCase& operator=(EmergencyStopUseCase&&) = delete;

    /// @brief 执行紧急停止用例
    /// @param request 紧急停止请求参数
    /// @return Result<EmergencyStopResponse> 执行结果
    /// @note 这是用例的主要入口点,协调所有领域对象
    /// @note 紧急停止总是返回成功,响应中包含详细执行状态
    Siligen::Shared::Types::Result<EmergencyStopResponse> Execute(const EmergencyStopRequest& request);

    /// @brief 检查系统是否处于紧急停止状态
    /// @return Result<bool> 是否处于紧急停止状态
    Siligen::Shared::Types::Result<bool> IsInEmergencyStop() const;

    /// @brief 从紧急停止状态恢复
    /// @return Result<void> 恢复结果
    /// @note 需要用户确认后才能恢复,恢复后系统状态转换为UNINITIALIZED
    Siligen::Shared::Types::Result<void> RecoverFromEmergencyStop();

   private:
    // 领域服务依赖
    std::shared_ptr<Siligen::Domain::Safety::DomainServices::EmergencyStopService> emergency_stop_service_;
    std::shared_ptr<Siligen::Shared::Interfaces::ILoggingService> logging_service_;

    void LogEmergencyStop(const EmergencyStopRequest& request, const EmergencyStopResponse& response);

    // 辅助方法
    void LogInfo(const std::string& message);
    void LogError(const std::string& message);
    void LogCritical(const std::string& message);
};

}  // namespace Siligen::Application::UseCases::System


