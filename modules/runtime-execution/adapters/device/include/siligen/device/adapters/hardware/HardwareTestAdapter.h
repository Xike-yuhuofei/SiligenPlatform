#pragma once

#include "siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "coordinate_alignment/contracts/IHardwareTestPort.h"
#include "trace_diagnostics/contracts/DiagnosticTypes.h"
#include "shared/types/HardwareConfiguration.h"

#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Siligen::Infrastructure::Adapters::Hardware {

using Siligen::Domain::Machine::Ports::IHardwareTestPort;
using Siligen::Domain::Machine::Ports::JogDirection;
using Siligen::Domain::Machine::Ports::HomingStage;
using Siligen::Domain::Machine::Ports::HomingTestStatus;
using Siligen::Domain::Motion::ValueObjects::HomeInputState;
using Siligen::Domain::Machine::Ports::LimitSwitchState;
using Siligen::Domain::Machine::Ports::TriggerAction;
using Siligen::Domain::Machine::Ports::TriggerEvent;
using Siligen::Domain::Machine::Ports::TrajectoryDefinition;
using Siligen::Domain::Machine::Ports::TrajectoryInterpolationType;
using Siligen::Domain::Machine::Ports::InterpolationParameters;
using Siligen::Domain::Machine::Ports::CMPParameters;
using Siligen::Domain::Machine::Ports::HardwareCheckResult;
using Siligen::Domain::Machine::Ports::CommunicationCheckResult;
using Siligen::Domain::Machine::Ports::AccuracyCheckResult;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Domain::Configuration::Ports::HomingConfig;

/**
 * @brief 硬件测试适配器 - MultiCard真实硬件实现
 *
 * 继续同时实现 machine-test port 与 machine health port，
 * 以承接尚未清空的 trigger controller live consumer。
 * 线程安全: 使用互斥锁保护硬件访问。
 */
class HardwareTestAdapter : public IHardwareTestPort,
                            public Siligen::Device::Contracts::Ports::MachineHealthPort {
   public:
    /**
     * @brief 构造函数
     * @param multicard IMultiCardWrapper对象指针
     * @param config 硬件配置（包含脉冲当量等参数）
     * @param homing_configs 回零配置数组（当前使用前2轴配置）
     */
    HardwareTestAdapter(std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> multicard,
                        const Shared::Types::HardwareConfiguration& config,
                        const std::array<HomingConfig, 4>& homing_configs);

    /**
     * @brief 析构函数
     */
    ~HardwareTestAdapter() override = default;

    // 禁止拷贝
    HardwareTestAdapter(const HardwareTestAdapter&) = delete;
    HardwareTestAdapter& operator=(const HardwareTestAdapter&) = delete;

    // ============ 连接管理 ============

    bool isConnected() const override;
    std::map<LogicalAxisId, bool> getAxisEnableStates() const override;

    // ============ 点动测试 ============

    Result<void> startJog(LogicalAxisId axis, JogDirection direction, double speed) override;
    Result<void> stopJog(LogicalAxisId axis) override;
    Result<void> startJogStep(LogicalAxisId axis, JogDirection direction, double distance, double speed) override;
    Result<double> getAxisPosition(LogicalAxisId axis) const override;
    std::map<LogicalAxisId, double> getAllAxisPositions() const override;

    // ============ 回零测试 ============

    Result<void> startHoming(const std::vector<LogicalAxisId>& axes) override;
    Result<void> stopHoming(const std::vector<LogicalAxisId>& axes) override;
    HomingTestStatus getHomingStatus(LogicalAxisId axis) const override;
    HomingStage getHomingStage(LogicalAxisId axis) const override;
    LimitSwitchState getLimitSwitchState(LogicalAxisId axis) const override;

    // ============ I/O测试 ============

    Result<void> setDigitalOutput(int port, bool value) override;
    Result<bool> getDigitalInput(int port) const override;
    std::map<int, bool> getAllDigitalInputs() const override;
    std::map<int, bool> getAllDigitalOutputs() const override;

    // ============ 位置触发测试 ============

    Result<int> configureTriggerPoint(LogicalAxisId axis, double position, int outputPort, TriggerAction action) override;
    Result<void> enableTrigger(int triggerPointId) override;
    Result<void> disableTrigger(int triggerPointId) override;
    Result<void> clearAllTriggers() override;
    std::vector<TriggerEvent> getTriggerEvents() const override;

    // ============ CMP测试 ============

    Result<void> startCMPTest(const TrajectoryDefinition& trajectory, const CMPParameters& cmpParams) override;
    Result<void> stopCMPTest() override;
    double getCMPTestProgress() const override;
    std::vector<Point2D> getCMPActualPath() const override;

    // ============ 插补测试 ============

    Result<void> executeInterpolationTest(TrajectoryInterpolationType interpolationType,
                                          const std::vector<Point2D>& controlPoints,
                                          const InterpolationParameters& interpParams) override;
    std::vector<Point2D> getInterpolatedPath() const override;

    // ============ 系统诊断 ============

    HardwareCheckResult checkHardwareConnection() override;
    CommunicationCheckResult testCommunicationQuality(int testDurationMs) override;
    Result<double> measureAxisResponseTime(LogicalAxisId axis) override;
    Result<AccuracyCheckResult> testPositioningAccuracy(LogicalAxisId axis, int testCycles) override;
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MachineHealthSnapshot> ReadHealth()
        const override;

    // ============ 安全控制 ============

    Result<void> emergencyStop() override;
    Result<void> resetEmergencyStop() override;
    bool isEmergencyStopActive() const override;
    Result<void> validateMotionParameters(LogicalAxisId axis, double targetPosition, double speed) const override;

   private:
    struct Units {
        static constexpr double PULSE_PER_SEC_TO_MS = 1000.0;
    };
    static constexpr bool kUseHomeAsHardLimit = false;
    static int AxisIndex(LogicalAxisId axis) {
        return static_cast<int>(axis);
    }

    std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> multicard_;
    Shared::Types::HardwareConfiguration hardware_config_;
    Shared::Types::UnitConverter unit_converter_;  // 单位转换器（mm/s ↔ pulse/s）
    std::array<HomingConfig, 4> homing_configs_;  ///< 回零配置（仅使用前2轴）
    int axis_count_;
    mutable std::recursive_mutex m_mutex;

    // 触发点管理
    struct TriggerPointConfig {
        int id;
        LogicalAxisId axis;
        double position;
        int outputPort;
        TriggerAction action;
        bool enabled;
    };
    std::vector<TriggerPointConfig> m_triggerPoints;
    std::vector<TriggerEvent> m_triggerEvents;
    int m_nextTriggerPointId;

    // CMP测试状态
    bool m_cmpTestRunning;
    double m_cmpTestProgress;
    std::vector<Point2D> m_cmpActualPath;

    // 插补测试状态
    std::vector<Point2D> m_interpolatedPath;

    // 回零结果缓存（用于处理硬件状态位短暂或被清除的情况）
    mutable std::array<bool, 4> homing_success_latch_{};
    mutable std::array<bool, 4> homing_failed_latch_{};
    mutable std::array<bool, 4> homing_active_latch_{};
    mutable std::array<HomingStage, 4> homing_stage_{};
    mutable std::array<std::chrono::steady_clock::time_point, 4> homing_start_time_{};
    mutable std::array<bool, 4> homing_timer_active_{};

    /**
     * @brief 读取硬限位开关原始状态（使用MC_GetDiRaw，当前接Home输入）
     */
    Result<LimitSwitchState> readLimitSwitchStateRaw(int axis) const;
    Result<HomeInputState> readHomeInputStateRaw(int axis) const;
    Result<bool> IsHomeInputTriggeredStable(int axis,
                                            int sample_count,
                                            int sample_interval_ms,
                                            bool allow_blocking = true) const;

    /**
     * @brief 已触发限位时先退离开关，避免回零继续向限位方向运动
     */
    Result<void> EscapeFromHomeLimit(int axis, short axisNum, const HomingConfig& config) const;

    /**
     * @brief 验证轴号有效性
     */
    Result<void> validateAxisNumber(int axis) const;

    /**
     * @brief 获取MultiCard API错误信息
     */
    std::string getMultiCardErrorMessage(short errorCode) const;

    /**
     * @brief 执行直线插补
     */
    short executeLinearInterpolation(short crdNum,
                                     const std::vector<Point2D>& controlPoints,
                                     const InterpolationParameters& interpParams);

    /**
     * @brief 执行圆弧插补
     */
    short executeArcInterpolation(short crdNum,
                                  const std::vector<Point2D>& controlPoints,
                                  const InterpolationParameters& interpParams);

    /**
     * @brief 执行样条插补（BSpline或Bezier）
     */
    short executeSplineInterpolation(short crdNum,
                                     const std::vector<Point2D>& controlPoints,
                                     const InterpolationParameters& interpParams);

    /**
     * @brief 采样插补路径
     */
    Result<void> sampleInterpolationPath(short crdNum, int sampleIntervalMs);
};

}  // namespace Siligen::Infrastructure::Adapters::Hardware
