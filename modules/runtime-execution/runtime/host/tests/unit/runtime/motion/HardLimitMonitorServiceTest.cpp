#include "services/motion/HardLimitMonitorService.h"

#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#include "siligen/device/adapters/motion/MultiCardMotionAdapter.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace {

using HardLimitMonitorConfig = Siligen::Application::Services::Motion::HardLimitMonitorConfig;
using HardLimitMonitorService = Siligen::Application::Services::Motion::HardLimitMonitorService;
using IIOControlPort = Siligen::Domain::Motion::Ports::IIOControlPort;
using IOStatus = Siligen::Domain::Motion::Ports::IOStatus;
using IPositionControlPort = Siligen::Domain::Motion::Ports::IPositionControlPort;
using MotionCommand = Siligen::Domain::Motion::Ports::MotionCommand;
using MultiCardMotionAdapter = Siligen::Infrastructure::Adapters::MultiCardMotionAdapter;
using MockMultiCard = Siligen::Infrastructure::Hardware::MockMultiCard;
using MockMultiCardWrapper = Siligen::Infrastructure::Hardware::MockMultiCardWrapper;
using ErrorCode = Siligen::Shared::Types::ErrorCode;
using HardwareConfiguration = Siligen::Shared::Types::HardwareConfiguration;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using Point2D = Siligen::Shared::Types::Point2D;
using Result = Siligen::Shared::Types::Result<void>;
using float32 = Siligen::Shared::Types::float32;
using int32 = Siligen::Shared::Types::int32;

template <typename T>
using ResultT = Siligen::Shared::Types::Result<T>;

class CountingPositionControlPort final : public IPositionControlPort {
   public:
    Siligen::Shared::Types::Result<void> MoveToPosition(const Point2D&, float32) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> MoveAxisToPosition(LogicalAxisId, float32, float32) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> RelativeMove(LogicalAxisId, float32, float32) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> SynchronizedMove(const std::vector<MotionCommand>&) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> StopAxis(LogicalAxisId axis, bool immediate) override {
        last_axis = axis;
        last_immediate = immediate;
        ++stop_axis_calls;
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> StopAllAxes(bool) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> EmergencyStop() override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> RecoverFromEmergencyStop() override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    Siligen::Shared::Types::Result<void> WaitForMotionComplete(LogicalAxisId, int32) override {
        return Siligen::Shared::Types::Result<void>::Success();
    }

    std::atomic<int> stop_axis_calls{0};
    LogicalAxisId last_axis = LogicalAxisId::INVALID;
    bool last_immediate = false;
};

class StubIIOControlPort final : public IIOControlPort {
   public:
    ResultT<IOStatus> ReadDigitalInput(int16 channel) override {
        return ResultT<IOStatus>::Success(IOStatus{false, channel, 0U, 0});
    }

    ResultT<IOStatus> ReadDigitalOutput(int16 channel) override {
        return ResultT<IOStatus>::Success(IOStatus{false, channel, 0U, 0});
    }

    Result WriteDigitalOutput(int16, bool) override {
        return Result::Success();
    }

    ResultT<bool> ReadLimitStatus(LogicalAxisId axis, bool positive) override {
        if (axis == LogicalAxisId::Y) {
            return ResultT<bool>::Success(positive ? y_positive_triggered.load() : y_negative_triggered.load());
        }
        return ResultT<bool>::Success(positive ? x_positive_triggered.load() : x_negative_triggered.load());
    }

    ResultT<bool> ReadServoAlarm(LogicalAxisId) override {
        return ResultT<bool>::Success(false);
    }

    std::atomic<bool> x_positive_triggered{false};
    std::atomic<bool> x_negative_triggered{false};
    std::atomic<bool> y_positive_triggered{false};
    std::atomic<bool> y_negative_triggered{false};
};

bool WaitForStopCount(
    const std::shared_ptr<CountingPositionControlPort>& position_port,
    int expected_count,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(250)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (position_port->stop_axis_calls.load() >= expected_count) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return position_port->stop_axis_calls.load() >= expected_count;
}

std::shared_ptr<MultiCardMotionAdapter> CreateAdapter(const std::shared_ptr<MockMultiCard>& mock_card) {
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    EXPECT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();
    if (adapter_result.IsError()) {
        return nullptr;
    }
    return adapter_result.Value();
}

TEST(HardLimitMonitorServiceTest, DoesNotStopAxisWhenOnlyHomeSignalIsActiveOutsideHoming) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto adapter = CreateAdapter(mock_card);
    ASSERT_NE(adapter, nullptr);

    auto position_port = std::make_shared<CountingPositionControlPort>();

    mock_card->SetDigitalInputRaw(MC_HOME, 0x1);
    mock_card->SetAxisStatus(1, AXIS_STATUS_ENABLE);

    HardLimitMonitorConfig config;
    config.monitoring_interval_ms = 5;
    config.monitored_axes = {LogicalAxisId::X};

    HardLimitMonitorService service(adapter, position_port, config);
    auto start_result = service.Start();
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().GetMessage();

    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    auto stop_result = service.Stop();
    ASSERT_TRUE(stop_result.IsSuccess()) << stop_result.GetError().GetMessage();
    EXPECT_EQ(position_port->stop_axis_calls.load(), 0);
}

TEST(HardLimitMonitorServiceTest, DoesNotStopAxisWhenOnlyHomeSignalIsActiveDuringHoming) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto adapter = CreateAdapter(mock_card);
    ASSERT_NE(adapter, nullptr);

    auto position_port = std::make_shared<CountingPositionControlPort>();

    mock_card->SetDigitalInputRaw(MC_HOME, 0x1);
    mock_card->SetAxisStatus(1, AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_RUNNING | AXIS_STATUS_HOME_SWITCH);

    HardLimitMonitorConfig config;
    config.monitoring_interval_ms = 5;
    config.monitored_axes = {LogicalAxisId::X};

    HardLimitMonitorService service(adapter, position_port, config);
    auto start_result = service.Start();
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().GetMessage();

    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    auto stop_result = service.Stop();
    ASSERT_TRUE(stop_result.IsSuccess()) << stop_result.GetError().GetMessage();
    EXPECT_EQ(position_port->stop_axis_calls.load(), 0);
}

TEST(HardLimitMonitorServiceTest, StopsAxisOnHardLimitRisingEdge) {
    auto io_port = std::make_shared<StubIIOControlPort>();
    auto position_port = std::make_shared<CountingPositionControlPort>();

    HardLimitMonitorConfig config;
    config.monitoring_interval_ms = 5;
    config.monitored_axes = {LogicalAxisId::X};

    HardLimitMonitorService service(io_port, position_port, config);
    auto start_result = service.Start();
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().GetMessage();

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    io_port->x_positive_triggered.store(true);

    EXPECT_TRUE(WaitForStopCount(position_port, 1));
    EXPECT_EQ(position_port->last_axis, LogicalAxisId::X);
    EXPECT_TRUE(position_port->last_immediate);

    auto stop_result = service.Stop();
    ASSERT_TRUE(stop_result.IsSuccess()) << stop_result.GetError().GetMessage();
}

TEST(HardLimitMonitorServiceTest, SustainedHardLimitSignalDoesNotTriggerDuplicateStops) {
    auto io_port = std::make_shared<StubIIOControlPort>();
    io_port->x_positive_triggered.store(true);
    auto position_port = std::make_shared<CountingPositionControlPort>();

    HardLimitMonitorConfig config;
    config.monitoring_interval_ms = 5;
    config.monitored_axes = {LogicalAxisId::X};

    HardLimitMonitorService service(io_port, position_port, config);
    auto start_result = service.Start();
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().GetMessage();

    EXPECT_TRUE(WaitForStopCount(position_port, 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    EXPECT_EQ(position_port->stop_axis_calls.load(), 1);

    auto stop_result = service.Stop();
    ASSERT_TRUE(stop_result.IsSuccess()) << stop_result.GetError().GetMessage();
}

TEST(HardLimitMonitorServiceTest, ClearedHardLimitCanTriggerStopAgainAfterRetrigger) {
    auto io_port = std::make_shared<StubIIOControlPort>();
    auto position_port = std::make_shared<CountingPositionControlPort>();

    HardLimitMonitorConfig config;
    config.monitoring_interval_ms = 5;
    config.monitored_axes = {LogicalAxisId::X};

    HardLimitMonitorService service(io_port, position_port, config);
    auto start_result = service.Start();
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().GetMessage();

    io_port->x_positive_triggered.store(true);
    EXPECT_TRUE(WaitForStopCount(position_port, 1));

    io_port->x_positive_triggered.store(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    io_port->x_positive_triggered.store(true);
    EXPECT_TRUE(WaitForStopCount(position_port, 2));

    auto stop_result = service.Stop();
    ASSERT_TRUE(stop_result.IsSuccess()) << stop_result.GetError().GetMessage();
}

}  // namespace
