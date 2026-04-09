// MoveToPositionUseCase.h
// 版本: 1.0.0
// 描述: 移动到指定位置用例 - 应用层业务流程编排
//
// 六边形架构 - 应用层用例
// 依赖方向: 依赖领域服务,协调多个领域对象完成业务流程
// Task: T046 - Phase 4 建立清晰的模块边界

#pragma once

#include "runtime_execution/contracts/motion/MotionControlService.h"
#include "runtime_execution/contracts/motion/MotionStatusService.h"
#include "runtime_execution/contracts/motion/MotionValidationService.h"
#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"
#include "runtime_execution/contracts/system/MachineExecutionSnapshot.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"
#include "shared/util/ModuleDependencyCheck.h"

#include <chrono>
#include <memory>
#include <string>

namespace Siligen::Application::UseCases::Motion::PTP {

/// @brief 移动到指定位置的请求参数
/// @details 封装用例输入参数
struct MoveToPositionRequest {
    Siligen::Shared::Types::Point2D target_position;  // 目标位置 (Target position)
    float movement_speed;                             // 移动速度(mm/s) (Movement speed)
    bool validate_state;                              // 是否验证系统状态 (Validate system state)
    bool wait_for_completion;                         // 是否等待运动完成 (Wait for completion)
    int32_t timeout_ms;                               // 超时时间(毫秒) (Timeout in milliseconds)

    MoveToPositionRequest()
        : target_position(),
          movement_speed(-1.0f)  // -1表示使用配置的默认速度
          ,
          validate_state(true),
          wait_for_completion(true),
          timeout_ms(30000)  // 默认30秒超时
    {}

    // 验证请求参数 (Validate request parameters)
    bool Validate() const {
        // 验证位置有效性 (Validate position validity)
        if (!target_position.IsValid()) {
            return false;
        }
        // 验证速度范围 (Validate speed range)
        if (movement_speed != -1.0f && movement_speed <= 0.0f) {
            return false;
        }
        // 验证超时时间 (Validate timeout)
        if (timeout_ms <= 0 || timeout_ms > 300000) {  // 最大5分钟
            return false;
        }
        return true;
    }
};

/// @brief 移动到指定位置的响应结果
/// @details 封装用例输出结果
struct MoveToPositionResponse {
    Siligen::Shared::Types::Point2D actual_position;  // 实际到达位置 (Actual position reached)
    float position_error;                             // 位置误差(mm) (Position error)
    std::chrono::milliseconds execution_time;         // 执行时间 (Execution time)
    bool motion_completed;                            // 运动是否完成 (Motion completed)
    std::string status_message;                       // 状态消息 (Status message)

    MoveToPositionResponse()
        : actual_position(), position_error(0.0f), execution_time(0), motion_completed(false), status_message("") {}
};

/// @brief 移动到指定位置用例
/// @details 应用层用例,协调运动控制/状态/校验服务与运行时状态端口完成位置移动业务流程
///
/// 业务流程:
/// 1. 验证输入参数有效性
/// 2. 检查运行时执行状态是否允许移动
/// 3. 验证目标位置在安全范围内
/// 4. 执行运动命令
/// 5. (可选)等待运动完成并验证到达精度
/// 6. 返回执行结果并保留领域层状态演化
///
/// 错误处理:
/// - 参数无效: 返回INVALID_PARAMETER错误
/// - 状态不允许: 返回INVALID_STATE错误
/// - 运动失败: 返回MOTION_ERROR错误
/// - 超时: 返回TIMEOUT错误
///
/// 使用示例:
/// @code
/// auto useCase = std::make_shared<MoveToPositionUseCase>(
///     motionControlService, motionStatusService, motionValidationService, machineExecutionStatePort, logger);
/// MoveToPositionRequest request;
/// request.target_position = Point2D(100.0f, 200.0f);
/// request.movement_speed = 5.0f;
/// auto result = useCase->Execute(request);
/// if (result.IsSuccess()) {
///     auto response = result.GetValue();
///     // 处理成功结果
/// }
/// @endcode
class MoveToPositionUseCase : public Siligen::Shared::Util::ApplicationModuleTag {
   public:
    // 编译时依赖检查: Application层依赖Domain层和Shared层
    static constexpr bool dependency_check_ =
        Siligen::Shared::Util::CheckDependencies<MoveToPositionUseCase,
                                                 Siligen::Domain::Motion::DomainServices::MotionControlService,
                                                 Siligen::Domain::Motion::DomainServices::MotionStatusService,
                                                 Siligen::Domain::Motion::DomainServices::MotionValidationService,
                                                 Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort,
                                                 Siligen::Shared::Interfaces::ILoggingService>();

    /// @brief 构造函数
    /// @param motion_control_service 运动控制领域服务
    /// @param motion_status_service 运动状态查询服务
    /// @param motion_validation_service 运动参数校验服务
    /// @param machine_execution_state_port 运行时执行状态读取端口
    /// @param logging_service 日志服务
    explicit MoveToPositionUseCase(
        std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionControlService> motion_control_service,
        std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionStatusService> motion_status_service,
        std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionValidationService> motion_validation_service,
        std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort> machine_execution_state_port,
        std::shared_ptr<Siligen::Shared::Interfaces::ILoggingService> logging_service);

    ~MoveToPositionUseCase() = default;

    // 禁止拷贝和移动
    MoveToPositionUseCase(const MoveToPositionUseCase&) = delete;
    MoveToPositionUseCase& operator=(const MoveToPositionUseCase&) = delete;
    MoveToPositionUseCase(MoveToPositionUseCase&&) = delete;
    MoveToPositionUseCase& operator=(MoveToPositionUseCase&&) = delete;

    /// @brief 执行移动到指定位置用例
    /// @param request 移动请求参数
    /// @return Result<MoveToPositionResponse> 执行结果
    /// @note 这是用例的主要入口点,协调所有领域对象
    Siligen::Shared::Types::Result<MoveToPositionResponse> Execute(const MoveToPositionRequest& request);

    /// @brief 取消当前运动
    /// @return Result<void> 取消结果
    /// @note 调用运动服务的停止方法
    Siligen::Shared::Types::Result<void> Cancel();

   private:
    // 领域服务依赖
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionControlService> motion_control_service_;
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionStatusService> motion_status_service_;
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionValidationService> motion_validation_service_;
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort> machine_execution_state_port_;
    std::shared_ptr<Siligen::Shared::Interfaces::ILoggingService> logging_service_;

    // 业务流程步骤
    Siligen::Shared::Types::Result<void> ValidateRequest(const MoveToPositionRequest& request);
    Siligen::Shared::Types::Result<void> CheckMachineExecutionState();
    Siligen::Shared::Types::Result<void> ExecuteMotion(const Siligen::Shared::Types::Point2D& target_position,
                                                       float speed);
    Siligen::Shared::Types::Result<void> WaitForMotionCompletion(int32_t timeout_ms);
    Siligen::Shared::Types::Result<void> VerifyPositionAccuracy(const Siligen::Shared::Types::Point2D& target_position,
                                                                float tolerance);

    // 辅助方法
    void LogInfo(const std::string& message);
    void LogError(const std::string& message);
    void LogDebug(const std::string& message);
};

}  // namespace Siligen::Application::UseCases::Motion::PTP
