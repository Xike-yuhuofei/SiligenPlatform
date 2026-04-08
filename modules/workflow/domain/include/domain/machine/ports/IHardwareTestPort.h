#pragma once

#include "domain/diagnostics/value-objects/DiagnosticTypes.h"
#include "domain/motion/value-objects/HardwareTestTypes.h"
#include "domain/motion/value-objects/TrajectoryTypes.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <map>
#include <vector>

namespace Siligen::Domain::Machine::Ports {

using Shared::Types::Result;
using Shared::Types::LogicalAxisId;
using Shared::Types::Point2D;
using Siligen::Domain::Diagnostics::ValueObjects::AccuracyCheckResult;
using Siligen::Domain::Diagnostics::ValueObjects::CommunicationCheckResult;
using Siligen::Domain::Diagnostics::ValueObjects::HardwareCheckResult;
using Siligen::Domain::Diagnostics::ValueObjects::ResponseTimeCheckResult;
using Siligen::Domain::Motion::ValueObjects::CMPParameters;
using Siligen::Domain::Motion::ValueObjects::HomingStage;
using Siligen::Domain::Motion::ValueObjects::HomingTestStatus;
using Siligen::Domain::Motion::ValueObjects::InterpolationParameters;
using Siligen::Domain::Motion::ValueObjects::JogDirection;
using Siligen::Domain::Motion::ValueObjects::LimitSwitchState;
using Siligen::Domain::Motion::ValueObjects::TrajectoryDefinition;
using Siligen::Domain::Motion::ValueObjects::TrajectoryInterpolationType;
using Siligen::Domain::Motion::ValueObjects::TriggerAction;
using Siligen::Domain::Motion::ValueObjects::TriggerEvent;

/**
 * @brief 硬件测试端口接口
 *
 * 定义应用层到硬件层的所有测试操作,遵循六边形架构原则。
 *
 * 实现者:
 * - HardwareTestAdapter (modules/device-hal/src/adapters/diagnostics/health/testing/) - 真实硬件实现
 * - MockHardwareTestPort (tests/mocks/) - 单元测试Mock实现
 *
 * 消费者:
 * - 各测试用例类 (application/usecases/)
 */
class IHardwareTestPort {
   public:
    virtual ~IHardwareTestPort() = default;

    // ============ 连接管理 ============

    /**
     * @brief 检查硬件连接状态
     * @return true=已连接, false=未连接
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief 获取所有轴的使能状态
     * @return 轴号到使能状态的映射
     */
    virtual std::map<LogicalAxisId, bool> getAxisEnableStates() const = 0;


    // ============ 点动测试 ============

    /**
     * @brief 启动点动运动
     * @param axis 轴号(0=X, 1=Y)
     * @param direction 移动方向(Positive/Negative)
     * @param speed 移动速度(mm/s)
     * @return Success或错误信息
     */
    virtual Result<void> startJog(LogicalAxisId axis, JogDirection direction, double speed) = 0;

    /**
     * @brief 停止点动运动
     * @param axis 轴号
     * @return Success或错误信息
     */
    virtual Result<void> stopJog(LogicalAxisId axis) = 0;

    /**
     * @brief 单步点动运动（固定距离）
     * @param axis 轴号(0=X, 1=Y)
     * @param direction 移动方向(Positive/Negative)
     * @param distance 移动距离(mm)
     * @param speed 移动速度(mm/s)
     * @return Success或错误信息
     */
    virtual Result<void> startJogStep(LogicalAxisId axis, JogDirection direction, double distance, double speed) = 0;

    /**
     * @brief 获取轴的当前位置
     * @param axis 轴号
     * @return 位置(mm)或错误信息
     */
    virtual Result<double> getAxisPosition(LogicalAxisId axis) const = 0;

    /**
     * @brief 获取所有轴的当前位置
     * @return 轴号到位置(mm)的映射
     */
    virtual std::map<LogicalAxisId, double> getAllAxisPositions() const = 0;


    // ============ 回零测试 ============

    /**
     * @brief 启动回零操作
     * @param axes 需要回零的轴号列表
     * @note 回零参数由配置提供
     * @return Success或错误信息
     */
    virtual Result<void> startHoming(const std::vector<LogicalAxisId>& axes) = 0;

    /**
     * @brief 停止回零操作
     * @param axes 需要停止的轴号列表
     * @return Success或错误信息
     */
    virtual Result<void> stopHoming(const std::vector<LogicalAxisId>& axes) = 0;

    /**
     * @brief 获取回零状态
     * @param axis 轴号
     * @return 回零状态(未开始/进行中/已完成/失败)
     */
    virtual HomingTestStatus getHomingStatus(LogicalAxisId axis) const = 0;

    /**
     * @brief 获取回零阶段(用于诊断)
     * @param axis 轴号
     * @return 回零阶段
     */
    virtual HomingStage getHomingStage(LogicalAxisId axis) const = 0;

    /**
     * @brief 获取限位开关状态
     * @param axis 轴号
     * @return 限位开关状态(正/负限位是否触发)
     */
    virtual LimitSwitchState getLimitSwitchState(LogicalAxisId axis) const = 0;


    // ============ I/O测试 ============

    /**
     * @brief 设置数字输出端口状态
     * @param port 端口号
     * @param value true=高电平, false=低电平
     * @return Success或错误信息
     */
    virtual Result<void> setDigitalOutput(int port, bool value) = 0;

    /**
     * @brief 获取数字输入端口状态
     * @param port 端口号
     * @return 端口状态(true=高电平, false=低电平)或错误信息
     */
    virtual Result<bool> getDigitalInput(int port) const = 0;

    /**
     * @brief 获取所有数字输入端口状态
     * @return 端口号到状态的映射
     */
    virtual std::map<int, bool> getAllDigitalInputs() const = 0;

    /**
     * @brief 获取所有数字输出端口状态
     * @return 端口号到状态的映射
     */
    virtual std::map<int, bool> getAllDigitalOutputs() const = 0;


    // ============ 位置触发测试 ============

    /**
     * @brief 配置位置触发点
     * @param axis 触发轴
     * @param position 触发位置(mm)
     * @param outputPort 输出端口号
     * @param action 触发动作(TurnOn/TurnOff)
     * @return 触发点ID或错误信息
     */
    virtual Result<int> configureTriggerPoint(LogicalAxisId axis, double position, int outputPort, TriggerAction action) = 0;

    /**
     * @brief 启用位置触发
     * @param triggerPointId 触发点ID
     * @return Success或错误信息
     */
    virtual Result<void> enableTrigger(int triggerPointId) = 0;

    /**
     * @brief 禁用位置触发
     * @param triggerPointId 触发点ID
     * @return Success或错误信息
     */
    virtual Result<void> disableTrigger(int triggerPointId) = 0;

    /**
     * @brief 清除所有触发点配置
     * @return Success或错误信息
     */
    virtual Result<void> clearAllTriggers() = 0;

    /**
     * @brief 获取触发事件历史
     * @return 触发事件列表
     */
    virtual std::vector<TriggerEvent> getTriggerEvents() const = 0;


    // ============ CMP测试 ============

    /**
     * @brief 启动CMP测试
     * @param trajectory 轨迹定义(类型+控制点)
     * @param cmpParams CMP参数配置
     * @return Success或错误信息
     */
    virtual Result<void> startCMPTest(const TrajectoryDefinition& trajectory, const CMPParameters& cmpParams) = 0;

    /**
     * @brief 停止CMP测试
     * @return Success或错误信息
     */
    virtual Result<void> stopCMPTest() = 0;

    /**
     * @brief 获取CMP测试进度
     * @return 进度百分比(0-100)
     */
    virtual double getCMPTestProgress() const = 0;

    /**
     * @brief 获取实际轨迹采样数据
     * @return 实际轨迹点列表
     */
    virtual std::vector<Point2D> getCMPActualPath() const = 0;


    // ============ 插补测试 ============

    /**
     * @brief 执行插补测试
     * @param interpolationType 插补类型
     * @param controlPoints 控制点
     * @param interpParams 插补参数
     * @return Success或错误信息
     */
    virtual Result<void> executeInterpolationTest(TrajectoryInterpolationType interpolationType,
                                                  const std::vector<Point2D>& controlPoints,
                                                  const InterpolationParameters& interpParams) = 0;

    /**
     * @brief 获取插补生成的轨迹点
     * @return 轨迹点列表
     */
    virtual std::vector<Point2D> getInterpolatedPath() const = 0;


    // ============ 系统诊断 ============

    /**
     * @brief 执行硬件连接检查
     * @return 硬件检查结果
     */
    virtual HardwareCheckResult checkHardwareConnection() = 0;

    /**
     * @brief 执行通信质量测试
     * @param testDurationMs 测试时长(毫秒)
     * @return 通信检查结果
     */
    virtual CommunicationCheckResult testCommunicationQuality(int testDurationMs) = 0;

    /**
     * @brief 测量轴响应时间
     * @param axis 轴号
     * @return 响应时间(毫秒)或错误信息
     */
    virtual Result<double> measureAxisResponseTime(LogicalAxisId axis) = 0;

    /**
     * @brief 执行定位精度测试
     * @param axis 轴号
     * @param testCycles 测试往返次数
     * @return 精度检查结果或错误信息
     */
    virtual Result<AccuracyCheckResult> testPositioningAccuracy(LogicalAxisId axis, int testCycles) = 0;


    // ============ 安全控制 ============

    /**
     * @brief 急停所有轴
     * @return Success或错误信息
     */
    virtual Result<void> emergencyStop() = 0;

    /**
     * @brief 复位急停状态
     * @return Success或错误信息
     */
    virtual Result<void> resetEmergencyStop() = 0;

    /**
     * @brief 检查是否处于急停状态
     * @return true=急停状态, false=正常状态
     */
    virtual bool isEmergencyStopActive() const = 0;

    /**
     * @brief 验证运动参数安全性
     * @param axis 轴号
     * @param targetPosition 目标位置(mm)
     * @param speed 速度(mm/s)
     * @return Success或错误信息(参数超限)
     */
    virtual Result<void> validateMotionParameters(LogicalAxisId axis, double targetPosition, double speed) const = 0;
};

}  // namespace Siligen::Domain::Machine::Ports



