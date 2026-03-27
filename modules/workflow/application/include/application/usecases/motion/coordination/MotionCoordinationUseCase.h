#pragma once

#include "domain/dispensing/ports/ITriggerControllerPort.h"
#include "domain/motion/ports/IAdvancedMotionPort.h"
#include "domain/motion/ports/IAxisControlPort.h"
#include "domain/motion/ports/IIOControlPort.h"
#include "domain/motion/ports/IInterpolationPort.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Motion::Coordination {

/**
 * @brief IO控制命令参数
 */
struct MotionIOCommand {
    int16 channel = 0;  // IO通道号
    enum class Type {
        DIGITAL_OUTPUT,  // 数字输出控制
        LIMIT_ENABLE,    // 限位使能
        ENCODER_ENABLE,  // 编码器使能
        COMPARE_OUTPUT   // 比较输出
    } type = Type::DIGITAL_OUTPUT;

    bool value = false;           // 输出值或使能状态
    float32 position = 0.0f;      // 比较位置
    float32 tolerance = 0.0f;     // 比较容差
    uint32 pulse_width_us = 100;  // 脉冲宽度
};

/**
 * @brief 运动协调控制用例
 */
class MotionCoordinationUseCase {
   public:
    MotionCoordinationUseCase(std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port,
                              std::shared_ptr<Domain::Motion::Ports::IIOControlPort> io_port,
                              std::shared_ptr<Domain::Motion::Ports::IAxisControlPort> axis_control_port = nullptr,
                              std::shared_ptr<Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port = nullptr,
                              std::shared_ptr<Domain::Motion::Ports::IAdvancedMotionPort> advanced_motion_port = nullptr);

    ~MotionCoordinationUseCase() = default;

    Result<void> ConfigureCoordinateSystem(int16 coord_sys,
                                           const std::vector<Siligen::Shared::Types::LogicalAxisId>& axis_map,
                                           float32 max_velocity);
    Result<void> AddInterpolationSegment(const InterpolationConfig& command);
    Result<void> DispatchCoordinateSystemSegment(
        int16 coord_sys,
        const Domain::Motion::Ports::InterpolationData& segment);
    Result<void> StartCoordinateSystemMotion(uint32 coord_sys_mask);
    Result<void> StopCoordinateSystemMotion(uint32 coord_sys_mask);
    Result<void> SetCoordinateSystemVelocityOverride(int16 coord_sys, float32 override_percent);

    Result<void> ControlDigitalOutput(const MotionIOCommand& command);
    Result<void> ControlMultipleDigitalOutputs(const std::vector<int16>& channels,
                                               const std::vector<bool>& values);
    Result<void> OutputPulse(int16 channel, uint32 pulse_width_us);
    Result<void> ConfigureLimitEnable(Siligen::Shared::Types::LogicalAxisId axis, bool positive, bool enabled);
    Result<void> ConfigureEncoderEnable(Siligen::Shared::Types::LogicalAxisId axis, bool enabled);

    Result<void> ConfigureCompareOutput(const MotionIOCommand& command);
    Result<void> ForceCompareOutput(int16 channel, bool value);

    Result<void> ConfigureUART(int16 uart_id, int32 baud_rate);
    Result<void> SendUARTData(int16 uart_id, const std::string& data);
    Result<std::string> ReceiveUARTData(int16 uart_id, int16 max_length = 256);

   private:
    std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port_;
    std::shared_ptr<Domain::Motion::Ports::IIOControlPort> io_port_;
    std::shared_ptr<Domain::Motion::Ports::IAxisControlPort> axis_control_port_;
    std::shared_ptr<Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port_;
    std::shared_ptr<Domain::Motion::Ports::IAdvancedMotionPort> advanced_motion_port_;
};

}  // namespace Siligen::Application::UseCases::Motion::Coordination






