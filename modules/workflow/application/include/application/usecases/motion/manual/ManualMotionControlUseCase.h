#pragma once

#include "domain/motion/domain-services/JogController.h"
#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IIOControlPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/motion/ports/IPositionControlPort.h"
#include "runtime_execution/application/usecases/motion/manual/ManualMotionCommand.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <functional>
#include <memory>

namespace Siligen::Application::UseCases::Motion::Manual {

/**
 * @brief 手动手轮控制命令参数
 */
struct ManualHandwheelCommand {
    enum class Action {
        ENTER_MODE,     // 进入手轮模式
        EXIT_MODE,      // 退出手轮模式
        SELECT_AXIS,    // 选择控制轴
        SET_MULTIPLIER  // 设置倍率
    } action;

    Siligen::Shared::Types::LogicalAxisId axis = Siligen::Shared::Types::LogicalAxisId::X;  // 控制轴号
    int16 multiplier = 1;  // 倍率 (1, 10, 100)
    bool enabled = false;  // 使能状态
};

/**
 * @brief 手动运动控制用例
 *
 * 这个用例为交互界面提供简化的手动运动控制接口，
 * 封装点动、步进、手轮等操作能力。
 * 点动规则统一由 Domain::Motion::DomainServices::JogController 处理。
 */
class ManualMotionControlUseCase {
   public:
    /**
     * @brief 运动状态更新回调函数类型
     * @param axis 轴号
     * @param status 运动状态
     */
    using MotionStatusCallback =
        std::function<void(Siligen::Shared::Types::LogicalAxisId, const Domain::Motion::Ports::MotionStatus&)>;

    /**
     * @brief IO状态更新回调函数类型
     * @param signal IO信号状态
     */
    using IOStatusCallback = std::function<void(const Domain::Motion::Ports::IOStatus&)>;

    /**
     * @brief 构造函数
     * @param position_control_port 位置控制端口
     * @param jog_controller JOG控制领域服务
     */
    ManualMotionControlUseCase(
        std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port,
        std::shared_ptr<Domain::Motion::DomainServices::JogController> jog_controller,
        std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port = nullptr);

    ~ManualMotionControlUseCase() = default;

    // ========== 点位运动控制 ==========

    /**
     * @brief 执行点位运动
     * @param command 运动命令
     */
    Result<void> ExecutePointToPointMotion(const ManualMotionCommand& command, bool invalidate_homing = false);

    /**
     * @brief 开始JOG运动
     * @param axis 轴号
     * @param direction 方向 (1=正向, -1=负向)
     * @param velocity 速度
     */
    Result<void> StartJogMotion(Siligen::Shared::Types::LogicalAxisId axis, int16 direction, float32 velocity);

    /**
     * @brief 停止JOG运动
     * @param axis 轴号
     */
    Result<void> StopJogMotion(Siligen::Shared::Types::LogicalAxisId axis);

    /**
     * @brief 开始JOG步进运动
     * @param axis 轴号
     * @param direction 方向 (1=正向, -1=负向)
     * @param distance 移动距离
     * @param velocity 速度
     */
    Result<void> StartJogMotionStep(Siligen::Shared::Types::LogicalAxisId axis,
                                    int16 direction,
                                    float32 distance,
                                    float32 velocity);

    // ========== 手轮控制 ==========

    /**
     * @brief 执行手轮控制命令
     * @param command 手轮控制命令
     */
    Result<void> ExecuteHandwheelCommand(const ManualHandwheelCommand& command);

    /**
     * @brief 选择手轮控制轴
     * @param axis 轴号
     */
    Result<void> SelectHandwheelAxis(Siligen::Shared::Types::LogicalAxisId axis);

    /**
     * @brief 设置手轮倍率
     * @param multiplier 倍率 (1, 10, 100)
     */
    Result<void> SetHandwheelMultiplier(int16 multiplier);

   private:
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port_;
    std::shared_ptr<Domain::Motion::DomainServices::JogController> jog_controller_;
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port_;

    void InvalidateHomingState(Siligen::Shared::Types::LogicalAxisId axis_id);
};

}  // namespace Siligen::Application::UseCases::Motion::Manual






