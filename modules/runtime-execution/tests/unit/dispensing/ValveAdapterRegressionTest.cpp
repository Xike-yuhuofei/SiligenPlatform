#include "process_planning/contracts/configuration/ValveConfig.h"
#include "runtime_execution/contracts/dispensing/DispenseCompensationProfile.h"
#include "runtime_execution/contracts/dispensing/IValvePort.h"
#include "shared/types/ConfigTypes.h"
#include "siligen/device/adapters/dispensing/ValveAdapter.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

using Siligen::Domain::Configuration::Ports::ValveSupplyConfig;
using Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using Siligen::Domain::Dispensing::Ports::PathTriggerEvent;
using Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams;
using Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile;
using Siligen::Infrastructure::Adapters::ValveAdapter;
using Siligen::Infrastructure::Hardware::MockMultiCard;
using Siligen::Infrastructure::Hardware::MockMultiCardWrapper;
using Siligen::Shared::Types::DispenserValveConfig;
using Siligen::Shared::Types::ErrorCode;

ValveSupplyConfig BuildSupplyConfig() {
    ValveSupplyConfig config{};
    config.do_card_index = 0;
    config.do_bit_index = 0;
    return config;
}

DispenserValveConfig BuildDispenserConfig() {
    DispenserValveConfig config{};
    config.cmp_channel = 1;
    config.pulse_type = 0;
    return config;
}

PositionTriggeredDispenserParams BuildPositionTriggeredParams() {
    PositionTriggeredDispenserParams params{};
    params.coordinate_system = 1;
    params.pulse_width_ms = 20;
    params.start_level = 0;
    params.position_tolerance_pulse = 0;
    params.trigger_events.push_back(PathTriggerEvent{
        1,
        1.0f,
        1000,
        1000,
    });
    return params;
}

}  // namespace

TEST(ValveAdapterRegressionTest, RejectsManualPauseDuringPositionTriggeredRun) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        BuildDispenserConfig(),
        DispenseCompensationProfile{});

    const auto start_result = adapter.StartPositionTriggeredDispenser(BuildPositionTriggeredParams());
    ASSERT_TRUE(start_result.IsSuccess());

    const auto pause_result = adapter.PauseDispenser();
    ASSERT_TRUE(pause_result.IsError());
    EXPECT_EQ(pause_result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(
        pause_result.GetError().GetMessage().find("位置触发点胶不支持单独暂停"),
        std::string::npos);

    const auto status_result = adapter.GetDispenserStatus();
    ASSERT_TRUE(status_result.IsSuccess());
    EXPECT_EQ(status_result.Value().status, DispenserValveStatus::Running);

    const auto stop_result = adapter.StopDispenser();
    ASSERT_TRUE(stop_result.IsSuccess());
}
