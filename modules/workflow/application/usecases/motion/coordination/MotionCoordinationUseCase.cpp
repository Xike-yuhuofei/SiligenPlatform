#include "MotionCoordinationUseCase.h"

#include <chrono>
#include <thread>
#include <utility>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Motion::Coordination {

namespace {
constexpr auto kDefaultTriggerAxis = Siligen::Shared::Types::LogicalAxisId::X;

Result<void> MissingPort(const char* method, const char* dependency) {
    return Result<void>::Failure(Error(
        ErrorCode::PORT_NOT_INITIALIZED,
        std::string(dependency) + " not initialized",
        method));
}

Result<std::string> MissingPortString(const char* method, const char* dependency) {
    return Result<std::string>::Failure(Error(
        ErrorCode::PORT_NOT_INITIALIZED,
        std::string(dependency) + " not initialized",
        method));
}
}  // namespace

MotionCoordinationUseCase::MotionCoordinationUseCase(
    std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port,
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort> io_port,
    std::shared_ptr<Domain::Motion::Ports::IAxisControlPort> axis_control_port,
    std::shared_ptr<Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port,
    std::shared_ptr<Domain::Motion::Ports::IAdvancedMotionPort> advanced_motion_port)
    : interpolation_port_(std::move(interpolation_port))
    , io_port_(std::move(io_port))
    , axis_control_port_(std::move(axis_control_port))
    , trigger_port_(std::move(trigger_port))
    , advanced_motion_port_(std::move(advanced_motion_port)) {
}

Result<void> MotionCoordinationUseCase::ConfigureCoordinateSystem(int16 coord_sys,
                                                                  const std::vector<LogicalAxisId>& axis_map,
                                                                  float32 max_velocity) {
    if (!interpolation_port_) {
        return MissingPort("MotionCoordinationUseCase::ConfigureCoordinateSystem", "Interpolation port");
    }
    if (coord_sys <= 0 || axis_map.empty() || max_velocity <= 0.0f) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "Invalid coordinate system parameters",
            "MotionCoordinationUseCase::ConfigureCoordinateSystem"));
    }

    Domain::Motion::Ports::CoordinateSystemConfig config;
    config.dimension = static_cast<int16>(axis_map.size());
    config.axis_map = axis_map;
    config.max_velocity = max_velocity;
    config.max_acceleration = max_velocity;
    return interpolation_port_->ConfigureCoordinateSystem(coord_sys, config);
}

Result<void> MotionCoordinationUseCase::AddInterpolationSegment(const InterpolationConfig& command) {
    (void)command;
    return Result<void>::Failure(Error(
        ErrorCode::NOT_IMPLEMENTED,
        "InterpolationConfig does not contain interpolation segment payload",
        "MotionCoordinationUseCase::AddInterpolationSegment"));
}

Result<void> MotionCoordinationUseCase::DispatchCoordinateSystemSegment(
    int16 coord_sys,
    const Domain::Motion::Ports::InterpolationData& segment) {
    if (!interpolation_port_) {
        return MissingPort("MotionCoordinationUseCase::DispatchCoordinateSystemSegment", "Interpolation port");
    }
    if (coord_sys <= 0) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "Coordinate system must be greater than zero",
            "MotionCoordinationUseCase::DispatchCoordinateSystemSegment"));
    }

    const auto status_result = interpolation_port_->GetCoordinateSystemStatus(coord_sys);
    if (status_result.IsError()) {
        return Result<void>::Failure(status_result.GetError());
    }

    const auto status = status_result.Value();
    const bool buffer_empty = !status.is_moving && status.remaining_segments == 0U;

    if (buffer_empty) {
        auto result = interpolation_port_->ClearInterpolationBuffer(coord_sys);
        if (result.IsError()) {
            return result;
        }
    }

    auto result = interpolation_port_->AddInterpolationData(coord_sys, segment);
    if (result.IsError()) {
        return result;
    }
    result = interpolation_port_->FlushInterpolationData(coord_sys);
    if (result.IsError()) {
        return result;
    }

    if (buffer_empty) {
        return interpolation_port_->StartCoordinateSystemMotion(1U << static_cast<uint32>(coord_sys - 1));
    }

    return Result<void>::Success();
}

Result<void> MotionCoordinationUseCase::StartCoordinateSystemMotion(uint32 coord_sys_mask) {
    if (!interpolation_port_) {
        return MissingPort("MotionCoordinationUseCase::StartCoordinateSystemMotion", "Interpolation port");
    }
    return interpolation_port_->StartCoordinateSystemMotion(coord_sys_mask);
}

Result<void> MotionCoordinationUseCase::StopCoordinateSystemMotion(uint32 coord_sys_mask) {
    if (!interpolation_port_) {
        return MissingPort("MotionCoordinationUseCase::StopCoordinateSystemMotion", "Interpolation port");
    }
    return interpolation_port_->StopCoordinateSystemMotion(coord_sys_mask);
}

Result<void> MotionCoordinationUseCase::SetCoordinateSystemVelocityOverride(int16 coord_sys,
                                                                           float32 override_percent) {
    if (!interpolation_port_) {
        return MissingPort("MotionCoordinationUseCase::SetCoordinateSystemVelocityOverride", "Interpolation port");
    }
    return interpolation_port_->SetCoordinateSystemVelocityOverride(coord_sys, override_percent);
}

Result<void> MotionCoordinationUseCase::ControlDigitalOutput(const MotionIOCommand& command) {
    if (!io_port_) {
        return MissingPort("MotionCoordinationUseCase::ControlDigitalOutput", "IO port");
    }
    if (command.type != MotionIOCommand::Type::DIGITAL_OUTPUT) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "MotionIOCommand type must be DIGITAL_OUTPUT",
            "MotionCoordinationUseCase::ControlDigitalOutput"));
    }
    return io_port_->WriteDigitalOutput(command.channel, command.value);
}

Result<void> MotionCoordinationUseCase::ControlMultipleDigitalOutputs(const std::vector<int16>& channels,
                                                                      const std::vector<bool>& values) {
    if (channels.size() != values.size()) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "Channels and values size mismatch",
            "MotionCoordinationUseCase::ControlMultipleDigitalOutputs"));
    }

    for (size_t i = 0; i < channels.size(); ++i) {
        MotionIOCommand command;
        command.channel = channels[i];
        command.type = MotionIOCommand::Type::DIGITAL_OUTPUT;
        command.value = values[i];
        auto result = ControlDigitalOutput(command);
        if (result.IsError()) {
            return result;
        }
    }

    return Result<void>::Success();
}

Result<void> MotionCoordinationUseCase::OutputPulse(int16 channel, uint32 pulse_width_us) {
    if (!io_port_) {
        return MissingPort("MotionCoordinationUseCase::OutputPulse", "IO port");
    }
    if (pulse_width_us == 0) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "Pulse width must be greater than zero",
            "MotionCoordinationUseCase::OutputPulse"));
    }

    auto enable_result = io_port_->WriteDigitalOutput(channel, true);
    if (enable_result.IsError()) {
        return enable_result;
    }

    std::this_thread::sleep_for(std::chrono::microseconds(pulse_width_us));
    return io_port_->WriteDigitalOutput(channel, false);
}

Result<void> MotionCoordinationUseCase::ConfigureLimitEnable(LogicalAxisId axis, bool positive, bool enabled) {
    if (!axis_control_port_) {
        return MissingPort("MotionCoordinationUseCase::ConfigureLimitEnable", "Axis control port");
    }
    return axis_control_port_->EnableHardLimits(axis, enabled, positive ? 0 : 1);
}

Result<void> MotionCoordinationUseCase::ConfigureEncoderEnable(LogicalAxisId axis, bool enabled) {
    if (!advanced_motion_port_) {
        return MissingPort("MotionCoordinationUseCase::ConfigureEncoderEnable", "Advanced motion port");
    }
    return enabled ? advanced_motion_port_->EnableEncoder(axis)
                   : advanced_motion_port_->DisableEncoder(axis);
}

Result<void> MotionCoordinationUseCase::ConfigureCompareOutput(const MotionIOCommand& command) {
    if (!trigger_port_) {
        return MissingPort("MotionCoordinationUseCase::ConfigureCompareOutput", "Trigger port");
    }
    if (command.type != MotionIOCommand::Type::COMPARE_OUTPUT) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "MotionIOCommand type must be COMPARE_OUTPUT",
            "MotionCoordinationUseCase::ConfigureCompareOutput"));
    }

    Domain::Dispensing::Ports::TriggerConfig trigger_config;
    trigger_config.channel = command.channel;
    trigger_config.mode = Domain::Dispensing::Ports::TriggerMode::SINGLE_POINT;
    trigger_config.signal_type = Domain::Dispensing::Ports::SignalType::PULSE;
    trigger_config.pulse_width_us = static_cast<int32>(command.pulse_width_us);
    auto configure_result = trigger_port_->ConfigureTrigger(trigger_config);
    if (configure_result.IsError()) {
        return configure_result;
    }

    return trigger_port_->SetSingleTrigger(
        kDefaultTriggerAxis,
        command.position,
        static_cast<int32>(command.pulse_width_us));
}

Result<void> MotionCoordinationUseCase::ForceCompareOutput(int16 channel, bool value) {
    if (!io_port_) {
        return MissingPort("MotionCoordinationUseCase::ForceCompareOutput", "IO port");
    }
    return io_port_->WriteDigitalOutput(channel, value);
}

Result<void> MotionCoordinationUseCase::ConfigureUART(int16 uart_id, int32 baud_rate) {
    if (!advanced_motion_port_) {
        return MissingPort("MotionCoordinationUseCase::ConfigureUART", "Advanced motion port");
    }
    return advanced_motion_port_->ConfigureUART(uart_id, baud_rate);
}

Result<void> MotionCoordinationUseCase::SendUARTData(int16 uart_id, const std::string& data) {
    if (!advanced_motion_port_) {
        return MissingPort("MotionCoordinationUseCase::SendUARTData", "Advanced motion port");
    }
    return advanced_motion_port_->SendUARTData(uart_id, data);
}

Result<std::string> MotionCoordinationUseCase::ReceiveUARTData(int16 uart_id, int16 max_length) {
    if (!advanced_motion_port_) {
        return MissingPortString("MotionCoordinationUseCase::ReceiveUARTData", "Advanced motion port");
    }
    return advanced_motion_port_->ReceiveUARTData(uart_id, max_length);
}

}  // namespace Siligen::Application::UseCases::Motion::Coordination


