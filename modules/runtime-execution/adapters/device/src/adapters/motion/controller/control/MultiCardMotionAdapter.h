#pragma once

#include "domain/motion/ports/IAxisControlPort.h"
#include "domain/motion/ports/IJogControlPort.h"
#include "domain/motion/ports/IMotionConnectionPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/motion/ports/IPositionControlPort.h"
#include "runtime_execution/contracts/motion/IIOControlPort.h"
#include "shared/types/HardwareConfiguration.h"
#include "shared/types/DiagnosticsConfig.h"
#include "siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h"
#include "siligen/device/adapters/drivers/multicard/MultiCardCPP.h"
#include "../internal/UnitConverter.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Infrastructure::Adapters {

using Siligen::Shared::Types::LogicalAxisId;

/**
 * @brief MultiCard硬件运动控制适配器
 *
 * 实现细粒度运动端口接口（位置/状态/JOG/轴控制/IO/连接）。
 * 实际实现的功能：
 * - 位置控制: MoveToPosition, MoveAxisToPosition, RelativeMove, SynchronizedMove
 * - 状态查询: GetCurrentPosition, GetAxisPosition, GetAxisVelocity, GetAxisStatus, IsAxisMoving, IsAxisInPosition
 * - 运动控制: StopAxis, StopAllAxes, EmergencyStop, WaitForMotionComplete
 * - 参数设置: SetAxisVelocity, SetAxisAcceleration, SetSoftLimits
 * - 硬限位配置: SetHardLimits, EnableHardLimits, SetHardLimitPolarity
 */
class MultiCardMotionAdapter : public Siligen::Domain::Motion::Ports::IMotionConnectionPort,
                               public Siligen::Domain::Motion::Ports::IAxisControlPort,
                               public Siligen::Domain::Motion::Ports::IPositionControlPort,
                               public Siligen::Domain::Motion::Ports::IMotionStatePort,
                               public Siligen::Domain::Motion::Ports::IJogControlPort,
                               public Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort {
   public:
    /**
     * @brief 构造函数
     * @param hardware_wrapper MultiCard硬件包装器接口
     * @param config 硬件配置（包含单位转换系数）
     */
    static Shared::Types::Result<std::shared_ptr<MultiCardMotionAdapter>> Create(
        std::shared_ptr<Infrastructure::Hardware::IMultiCardWrapper> hardware_wrapper,
        const Shared::Types::HardwareConfiguration& config = Shared::Types::HardwareConfiguration(),
        const Shared::Types::DiagnosticsConfig& diagnostics = Shared::Types::DiagnosticsConfig());

    ~MultiCardMotionAdapter() override = default;

    // 禁止拷贝和移动
    MultiCardMotionAdapter(const MultiCardMotionAdapter&) = delete;
    MultiCardMotionAdapter& operator=(const MultiCardMotionAdapter&) = delete;
    MultiCardMotionAdapter(MultiCardMotionAdapter&&) = delete;
    MultiCardMotionAdapter& operator=(MultiCardMotionAdapter&&) = delete;

    // === IMotionConnectionPort 接口实现 ===
    Result<void> Connect(const std::string& card_ip, const std::string& pc_ip, int16 port = 0) override;
    Result<void> Disconnect() override;
    Result<bool> IsConnected() const override;
    Result<void> Reset() override;
    Result<std::string> GetCardInfo() const override;

    // === IAxisControlPort 接口实现 ===
    Result<void> EnableAxis(LogicalAxisId axis) override;
    Result<void> DisableAxis(LogicalAxisId axis) override;
    Result<bool> IsAxisEnabled(LogicalAxisId axis) const override;
    Result<void> ClearAxisStatus(LogicalAxisId axis) override;
    Result<void> ClearPosition(LogicalAxisId axis) override;

    // 位置控制 (IPositionControlPort)
    Result<void> MoveToPosition(const Point2D& position, float32 velocity) override;
    Result<void> MoveAxisToPosition(LogicalAxisId axis, float32 position, float32 velocity) override;
    Result<void> RelativeMove(LogicalAxisId axis, float32 distance, float32 velocity) override;
    Result<void> SynchronizedMove(const std::vector<Siligen::Domain::Motion::Ports::MotionCommand>& commands) override;

    // 状态查询 (IMotionStatePort)
    Result<Point2D> GetCurrentPosition() const override;
    Result<float32> GetAxisPosition(LogicalAxisId axis) const override;
    Result<float32> GetAxisVelocity(LogicalAxisId axis) const override;
    Result<Siligen::Domain::Motion::Ports::MotionStatus> GetAxisStatus(LogicalAxisId axis) const override;
    Result<bool> IsAxisMoving(LogicalAxisId axis) const override;
    Result<bool> IsAxisInPosition(LogicalAxisId axis) const override;
    Result<std::vector<Siligen::Domain::Motion::Ports::MotionStatus>> GetAllAxesStatus() const override;

    // 运动控制 (IPositionControlPort)
    Result<void> StopAxis(LogicalAxisId axis, bool immediate = false) override;
    Result<void> StopAllAxes(bool immediate = false) override;
    Result<void> EmergencyStop() override;
    Result<void> RecoverFromEmergencyStop() override;
    
    // JOG连续运动 (IJogControlPort)
    Result<void> StartJog(LogicalAxisId axis, int16 direction, float32 velocity) override;
    Result<void> StartJogStep(LogicalAxisId axis, int16 direction, float32 distance, float32 velocity) override;
    Result<void> StopJog(LogicalAxisId axis) override;
    Result<void> SetJogParameters(LogicalAxisId axis,
                                  const Siligen::Domain::Motion::Ports::JogParameters& params) override;
    
    // 等待操作完成 (IPositionControlPort)
    Result<void> WaitForMotionComplete(LogicalAxisId axis, int32 timeout_ms = 60000) override;

    // 参数设置 (IAxisControlPort)
    Result<void> SetAxisVelocity(LogicalAxisId axis, float32 velocity) override;
    Result<void> SetAxisAcceleration(LogicalAxisId axis, float32 acceleration) override;
    Result<void> SetSoftLimits(LogicalAxisId axis, float32 negative_limit, float32 positive_limit) override;
    Result<void> ConfigureAxis(LogicalAxisId axis, const Siligen::Domain::Motion::Ports::AxisConfiguration& config) override;

    // 硬限位配置 (IAxisControlPort)
    Result<void> SetHardLimits(LogicalAxisId axis,
                               short positive_io_index,
                               short negative_io_index,
                               short card_index = 0,
                               short signal_type = 0) override;
    Result<void> EnableHardLimits(LogicalAxisId axis, bool enable, short limit_type = -1) override;
    Result<void> SetHardLimitPolarity(LogicalAxisId axis, short positive_polarity, short negative_polarity) override;

    // IO控制 (IIOControlPort)
    Result<Siligen::RuntimeExecution::Contracts::Motion::IOStatus> ReadDigitalInput(int16 channel) override;
    Result<Siligen::RuntimeExecution::Contracts::Motion::IOStatus> ReadDigitalOutput(int16 channel) override;
    Result<void> WriteDigitalOutput(int16 channel, bool value) override;
    Result<bool> ReadLimitStatus(LogicalAxisId axis, bool positive) override;
    Result<bool> ReadServoAlarm(LogicalAxisId axis) override;

    /**
     * @brief 验证硬限位是否已启用
     * @param axis 轴号（0-based）
     * @return 成功时返回true（限位已启用），失败时返回错误
     */
    Result<bool> VerifyLimitsEnabled(LogicalAxisId axis);

    /**
     * @brief 检测硬限位是否触发（通过直接读取IO状态）
     * @param axis 轴号（0-based）
     * @param positive true=检测正限位，false=检测负限位（Home输入时不区分方向）
     * @return 成功时返回true（限位已触发），失败时返回错误
     * @note 此方法绕过状态字，直接通过MC_GetDiRaw读取IO状态，
     *       当前硬件限位接在Home输入上
     */
    Result<bool> IsHardLimitTriggered(LogicalAxisId axis, bool positive) const;

   private:
   explicit MultiCardMotionAdapter(
        std::shared_ptr<Infrastructure::Hardware::IMultiCardWrapper> hardware_wrapper,
        const Shared::Types::HardwareConfiguration& config = Shared::Types::HardwareConfiguration(),
        const Shared::Types::DiagnosticsConfig& diagnostics = Shared::Types::DiagnosticsConfig());

    std::shared_ptr<Infrastructure::Hardware::IMultiCardWrapper> hardware_wrapper_;

    // 硬件配置(用于JOG参数、速度/加速度限制等)
    Shared::Types::HardwareConfiguration hardware_config_;

    // 诊断配置（日志开关/采样参数）
    Shared::Types::DiagnosticsConfig diagnostics_config_;

    // 单位转换器 (统一管理单位转换，替代硬编码DEFAULT_PULSE_PER_MM)
    UnitConverter unit_converter_;

    // 硬限位IO配置存储
    struct LimitConfig {
        short pos_io_index = -1;
        short neg_io_index = -1;
        short card_index = 0;
        bool configured = false;
    };
    struct AxisFeedbackSnapshot {
        float profile_position_mm = 0.0f;
        float encoder_position_mm = 0.0f;
        float profile_velocity_mm_s = 0.0f;
        float encoder_velocity_mm_s = 0.0f;
        int profile_position_ret = 0;
        int encoder_position_ret = 0;
        int profile_velocity_ret = 0;
        int encoder_velocity_ret = 0;
        bool encoder_enabled = true;
    };
    std::array<LimitConfig, 4> limit_configs_{};

    // 默认配置
    // DEFAULT_PULSE_PER_MM已删除 - 使用unit_converter_统一管理单位转换
    static constexpr size_t kAxisCount = 2;
    static constexpr short X_AXIS = 0;
    static constexpr short Y_AXIS = 1;

    // 辅助方法
    bool ValidateAxis(short axis) const;
    Result<short> ResolveAxis(LogicalAxisId axis_id, const char* operation) const;
    Result<short> ResolveSdkAxis(LogicalAxisId axis_id, const char* operation) const;
    Result<void> EnsureAxisEnabled(short axis, short sdk_axis, const char* operation);
    Result<void> PrepareAxisForJog(short axis,
                                   short sdk_axis,
                                   LogicalAxisId axis_id,
                                   int16 direction,
                                   const char* operation,
                                   bool stop_before_clear);
    TJogPrm BuildJogParameters(short axis) const;
    short ToSdkAxis(short axis) const;  // 内部轴号(0-based)转SDK轴号(1-based)
    Siligen::Domain::Motion::Ports::MotionState MapHardwareState(int32 hardware_state) const;
    std::string FormatErrorMessage(const std::string& operation, short axis, short error_code) const;
    Result<bool> IsLimitTriggeredRaw(short axis, bool positive) const;
    Result<std::array<bool, kAxisCount>> ReadHomeLimitState() const;
    Result<bool> IsHomeSensorTriggeredRaw(short axis) const;
    Result<void> ValidateHomeBoundaryForDirection(short axis, int16 direction, const char* operation) const;
    bool IsEncoderFeedbackEnabled(short axis) const noexcept;
    AxisFeedbackSnapshot ReadAxisFeedbackSnapshot(short axis, short sdk_axis) const noexcept;
    Result<float32> ResolveAxisPositionFromSnapshot(short axis,
                                                    const AxisFeedbackSnapshot& snapshot,
                                                    const std::string& operation) const;
    Result<float32> ResolveAxisVelocityFromSnapshot(short axis,
                                                    const AxisFeedbackSnapshot& snapshot,
                                                    const std::string& operation) const;
    std::string DescribeSelectedFeedbackSource(short axis) const;

    // 诊断辅助
    void LogAxisSnapshot(const std::string& tag, short axis, short sdk_axis) const;
};

}  // namespace Siligen::Infrastructure::Adapters












