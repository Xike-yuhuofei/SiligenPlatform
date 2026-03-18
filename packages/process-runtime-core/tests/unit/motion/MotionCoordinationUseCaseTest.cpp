#include "application/usecases/motion/coordination/MotionCoordinationUseCase.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using Siligen::Application::UseCases::Motion::Coordination::MotionCoordinationUseCase;
using Siligen::Application::UseCases::Motion::Coordination::MotionIOCommand;
using Siligen::Domain::Dispensing::Ports::ITriggerControllerPort;
using Siligen::Domain::Dispensing::Ports::TriggerConfig;
using Siligen::Domain::Dispensing::Ports::TriggerMode;
using Siligen::Domain::Dispensing::Ports::TriggerStatus;
using Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using Siligen::Domain::Motion::Ports::CoordinateSystemStatus;
using Siligen::Domain::Motion::Ports::IAdvancedMotionPort;
using Siligen::Domain::Motion::Ports::IAxisControlPort;
using Siligen::Domain::Motion::Ports::IIOControlPort;
using Siligen::Domain::Motion::Ports::IInterpolationPort;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::Ports::IOStatus;
using Siligen::Domain::Motion::Ports::MotionMode;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;

Result<void> NotImplementedVoid(const char* method) {
    return Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "MotionCoordinationUseCaseTest"));
}

template <typename T>
Result<T> NotImplemented(const char* method) {
    return Result<T>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "MotionCoordinationUseCaseTest"));
}

class FakeInterpolationPort final : public IInterpolationPort {
   public:
    Result<void> ConfigureCoordinateSystem(int16 coord_sys, const CoordinateSystemConfig& config) override {
        configure_called = true;
        last_coord_sys = coord_sys;
        last_config = config;
        return Result<void>::Success();
    }

    Result<void> AddInterpolationData(int16 /*coord_sys*/, const InterpolationData& /*data*/) override {
        return NotImplementedVoid("AddInterpolationData");
    }

    Result<void> ClearInterpolationBuffer(int16 /*coord_sys*/) override {
        return Result<void>::Success();
    }

    Result<void> FlushInterpolationData(int16 /*coord_sys*/) override {
        return Result<void>::Success();
    }

    Result<void> StartCoordinateSystemMotion(uint32 coord_sys_mask) override {
        start_mask = coord_sys_mask;
        return Result<void>::Success();
    }

    Result<void> StopCoordinateSystemMotion(uint32 coord_sys_mask) override {
        stop_mask = coord_sys_mask;
        return Result<void>::Success();
    }

    Result<void> SetCoordinateSystemVelocityOverride(int16 coord_sys, float32 override_percent) override {
        override_coord_sys = coord_sys;
        override_percent_value = override_percent;
        return Result<void>::Success();
    }

    Result<void> EnableCoordinateSystemSCurve(int16 /*coord_sys*/, float32 /*jerk*/) override {
        return NotImplementedVoid("EnableCoordinateSystemSCurve");
    }

    Result<void> DisableCoordinateSystemSCurve(int16 /*coord_sys*/) override {
        return NotImplementedVoid("DisableCoordinateSystemSCurve");
    }

    Result<void> SetConstLinearVelocityMode(int16 /*coord_sys*/, bool /*enabled*/, uint32 /*rotate_axis_mask*/) override {
        return NotImplementedVoid("SetConstLinearVelocityMode");
    }

    Result<uint32> GetInterpolationBufferSpace(int16 /*coord_sys*/) const override {
        return Result<uint32>::Success(0);
    }

    Result<uint32> GetLookAheadBufferSpace(int16 /*coord_sys*/) const override {
        return Result<uint32>::Success(0);
    }

    Result<CoordinateSystemStatus> GetCoordinateSystemStatus(int16 /*coord_sys*/) const override {
        return Result<CoordinateSystemStatus>::Success(CoordinateSystemStatus{});
    }

    bool configure_called = false;
    int16 last_coord_sys = 0;
    CoordinateSystemConfig last_config{};
    uint32 start_mask = 0;
    uint32 stop_mask = 0;
    int16 override_coord_sys = 0;
    float32 override_percent_value = 0.0f;
};

class FakeIOControlPort final : public IIOControlPort {
   public:
    Result<IOStatus> ReadDigitalInput(int16 channel) override {
        IOStatus status;
        status.channel = channel;
        return Result<IOStatus>::Success(status);
    }

    Result<IOStatus> ReadDigitalOutput(int16 channel) override {
        IOStatus status;
        status.channel = channel;
        return Result<IOStatus>::Success(status);
    }

    Result<void> WriteDigitalOutput(int16 channel, bool value) override {
        writes.emplace_back(channel, value);
        return Result<void>::Success();
    }

    Result<bool> ReadLimitStatus(LogicalAxisId /*axis*/, bool /*positive*/) override {
        return Result<bool>::Success(false);
    }

    Result<bool> ReadServoAlarm(LogicalAxisId /*axis*/) override {
        return Result<bool>::Success(false);
    }

    std::vector<std::pair<int16, bool>> writes;
};

class FakeAxisControlPort final : public IAxisControlPort {
   public:
    Result<void> EnableAxis(LogicalAxisId /*axis*/) override {
        return NotImplementedVoid("EnableAxis");
    }

    Result<void> DisableAxis(LogicalAxisId /*axis*/) override {
        return NotImplementedVoid("DisableAxis");
    }

    Result<bool> IsAxisEnabled(LogicalAxisId /*axis*/) const override {
        return NotImplemented<bool>("IsAxisEnabled");
    }

    Result<void> ClearAxisStatus(LogicalAxisId /*axis*/) override {
        return NotImplementedVoid("ClearAxisStatus");
    }

    Result<void> ClearPosition(LogicalAxisId /*axis*/) override {
        return NotImplementedVoid("ClearPosition");
    }

    Result<void> SetAxisVelocity(LogicalAxisId /*axis*/, float32 /*velocity*/) override {
        return NotImplementedVoid("SetAxisVelocity");
    }

    Result<void> SetAxisAcceleration(LogicalAxisId /*axis*/, float32 /*acceleration*/) override {
        return NotImplementedVoid("SetAxisAcceleration");
    }

    Result<void> SetSoftLimits(LogicalAxisId /*axis*/, float32 /*negative_limit*/, float32 /*positive_limit*/) override {
        return NotImplementedVoid("SetSoftLimits");
    }

    Result<void> ConfigureAxis(LogicalAxisId /*axis*/, const Siligen::Domain::Motion::Ports::AxisConfiguration& /*config*/) override {
        return NotImplementedVoid("ConfigureAxis");
    }

    Result<void> SetHardLimits(LogicalAxisId /*axis*/,
                               short /*positive_io_index*/,
                               short /*negative_io_index*/,
                               short /*card_index*/,
                               short /*signal_type*/) override {
        return NotImplementedVoid("SetHardLimits");
    }

    Result<void> EnableHardLimits(LogicalAxisId axis, bool enable, short limit_type) override {
        last_axis = axis;
        last_enable = enable;
        last_limit_type = limit_type;
        return Result<void>::Success();
    }

    Result<void> SetHardLimitPolarity(LogicalAxisId /*axis*/, short /*positive_polarity*/, short /*negative_polarity*/) override {
        return NotImplementedVoid("SetHardLimitPolarity");
    }

    LogicalAxisId last_axis = LogicalAxisId::INVALID;
    bool last_enable = false;
    short last_limit_type = -1;
};

class FakeTriggerControllerPort final : public ITriggerControllerPort {
   public:
    Result<void> ConfigureTrigger(const TriggerConfig& config) override {
        configure_called = true;
        last_config = config;
        return Result<void>::Success();
    }

    Result<TriggerConfig> GetTriggerConfig() const override {
        return Result<TriggerConfig>::Success(last_config);
    }

    Result<void> SetSingleTrigger(LogicalAxisId axis, float32 position, int32 pulse_width_us) override {
        single_trigger_called = true;
        last_axis = axis;
        last_position = position;
        last_pulse_width_us = pulse_width_us;
        return Result<void>::Success();
    }

    Result<void> SetContinuousTrigger(LogicalAxisId /*axis*/,
                                      float32 /*start_pos*/,
                                      float32 /*end_pos*/,
                                      float32 /*interval*/,
                                      int32 /*pulse_width_us*/) override {
        return NotImplementedVoid("SetContinuousTrigger");
    }

    Result<void> SetRangeTrigger(LogicalAxisId /*axis*/, float32 /*start_pos*/, float32 /*end_pos*/, int32 /*pulse_width_us*/) override {
        return NotImplementedVoid("SetRangeTrigger");
    }

    Result<void> SetSequenceTrigger(LogicalAxisId /*axis*/,
                                    const std::vector<float32>& /*positions*/,
                                    int32 /*pulse_width_us*/) override {
        return NotImplementedVoid("SetSequenceTrigger");
    }

    Result<void> EnableTrigger(LogicalAxisId /*axis*/) override {
        return Result<void>::Success();
    }

    Result<void> DisableTrigger(LogicalAxisId /*axis*/) override {
        return Result<void>::Success();
    }

    Result<void> ClearTrigger(LogicalAxisId /*axis*/) override {
        return Result<void>::Success();
    }

    Result<TriggerStatus> GetTriggerStatus(LogicalAxisId /*axis*/) const override {
        return Result<TriggerStatus>::Success(TriggerStatus{});
    }

    Result<bool> IsTriggerEnabled(LogicalAxisId /*axis*/) const override {
        return Result<bool>::Success(false);
    }

    Result<int32> GetTriggerCount(LogicalAxisId /*axis*/) const override {
        return Result<int32>::Success(0);
    }

    bool configure_called = false;
    bool single_trigger_called = false;
    TriggerConfig last_config{};
    LogicalAxisId last_axis = LogicalAxisId::INVALID;
    float32 last_position = 0.0f;
    int32 last_pulse_width_us = 0;
};

TEST(MotionCoordinationUseCaseTest, ConfigureCoordinateSystemForwardsToInterpolationPort) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto io_port = std::make_shared<FakeIOControlPort>();

    MotionCoordinationUseCase use_case(interpolation_port, io_port, nullptr);

    const auto result = use_case.ConfigureCoordinateSystem(
        2,
        {LogicalAxisId::X, LogicalAxisId::Y},
        120.0f);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_TRUE(interpolation_port->configure_called);
    EXPECT_EQ(interpolation_port->last_coord_sys, 2);
    EXPECT_EQ(interpolation_port->last_config.dimension, 2);
    ASSERT_EQ(interpolation_port->last_config.axis_map.size(), 2u);
    EXPECT_EQ(interpolation_port->last_config.axis_map[0], LogicalAxisId::X);
    EXPECT_EQ(interpolation_port->last_config.axis_map[1], LogicalAxisId::Y);
    EXPECT_FLOAT_EQ(interpolation_port->last_config.max_velocity, 120.0f);
    EXPECT_FLOAT_EQ(interpolation_port->last_config.max_acceleration, 120.0f);
}

TEST(MotionCoordinationUseCaseTest, ControlDigitalOutputAndPulseUseIoPort) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto io_port = std::make_shared<FakeIOControlPort>();

    MotionCoordinationUseCase use_case(interpolation_port, io_port, nullptr);

    MotionIOCommand command;
    command.channel = 3;
    command.type = MotionIOCommand::Type::DIGITAL_OUTPUT;
    command.value = true;

    const auto output_result = use_case.ControlDigitalOutput(command);
    ASSERT_TRUE(output_result.IsSuccess());

    const auto pulse_result = use_case.OutputPulse(5, 1);
    ASSERT_TRUE(pulse_result.IsSuccess());

    ASSERT_EQ(io_port->writes.size(), 3u);
    const auto expected_write_0 = std::make_pair(static_cast<int16>(3), true);
    const auto expected_write_1 = std::make_pair(static_cast<int16>(5), true);
    const auto expected_write_2 = std::make_pair(static_cast<int16>(5), false);
    EXPECT_EQ(io_port->writes[0], expected_write_0);
    EXPECT_EQ(io_port->writes[1], expected_write_1);
    EXPECT_EQ(io_port->writes[2], expected_write_2);
}

TEST(MotionCoordinationUseCaseTest, ConfigureLimitEnableForwardsToAxisControlPort) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto io_port = std::make_shared<FakeIOControlPort>();
    auto axis_control_port = std::make_shared<FakeAxisControlPort>();

    MotionCoordinationUseCase use_case(interpolation_port, io_port, nullptr, axis_control_port);

    const auto result = use_case.ConfigureLimitEnable(LogicalAxisId::Y, false, true);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(axis_control_port->last_axis, LogicalAxisId::Y);
    EXPECT_TRUE(axis_control_port->last_enable);
    EXPECT_EQ(axis_control_port->last_limit_type, 1);
}

TEST(MotionCoordinationUseCaseTest, ConfigureCompareOutputUsesTriggerPort) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto io_port = std::make_shared<FakeIOControlPort>();
    auto trigger_port = std::make_shared<FakeTriggerControllerPort>();

    MotionCoordinationUseCase use_case(interpolation_port, io_port, nullptr, nullptr, trigger_port);

    MotionIOCommand command;
    command.channel = 2;
    command.type = MotionIOCommand::Type::COMPARE_OUTPUT;
    command.position = 12.5f;
    command.pulse_width_us = 250;

    const auto result = use_case.ConfigureCompareOutput(command);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_TRUE(trigger_port->configure_called);
    EXPECT_TRUE(trigger_port->single_trigger_called);
    EXPECT_EQ(trigger_port->last_config.channel, 2);
    EXPECT_EQ(trigger_port->last_config.mode, TriggerMode::SINGLE_POINT);
    EXPECT_EQ(trigger_port->last_axis, LogicalAxisId::X);
    EXPECT_FLOAT_EQ(trigger_port->last_position, 12.5f);
    EXPECT_EQ(trigger_port->last_pulse_width_us, 250);
}

TEST(MotionCoordinationUseCaseTest, UnsupportedOrMissingDependenciesReturnFailure) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto io_port = std::make_shared<FakeIOControlPort>();

    MotionCoordinationUseCase use_case(interpolation_port, io_port, nullptr);

    const auto add_result = use_case.AddInterpolationSegment(Siligen::InterpolationConfig{});
    ASSERT_TRUE(add_result.IsError());
    EXPECT_EQ(add_result.GetError().GetCode(), ErrorCode::NOT_IMPLEMENTED);

    const auto uart_result = use_case.ConfigureUART(1, 115200);
    ASSERT_TRUE(uart_result.IsError());
    EXPECT_EQ(uart_result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}

}  // namespace
