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

}  // namespace
