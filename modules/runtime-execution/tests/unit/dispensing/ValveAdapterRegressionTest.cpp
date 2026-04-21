#include "process_planning/contracts/configuration/ValveConfig.h"
#include "runtime_execution/contracts/dispensing/DispenseCompensationProfile.h"
#include "runtime_execution/contracts/dispensing/IProfileComparePort.h"
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
using Siligen::Domain::Dispensing::Ports::DispenserValveParams;
using Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using Siligen::Domain::Dispensing::Ports::PathTriggerEvent;
using Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams;
using Siligen::Domain::Dispensing::Ports::ProfileCompareArmRequest;
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
    config.cmp_axis_mask = 0x01;
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

ProfileCompareArmRequest BuildProfileCompareRequest() {
    ProfileCompareArmRequest request{};
    request.compare_positions_pulse = {100L, 200L};
    request.compare_source_axis = 1;
    request.expected_trigger_count = 2U;
    request.start_boundary_trigger_count = 0U;
    request.pulse_width_us = 2000U;
    request.start_level = 0;
    return request;
}

ProfileCompareArmRequest BuildBoundaryOnlyProfileCompareRequest() {
    ProfileCompareArmRequest request{};
    request.compare_source_axis = 1;
    request.expected_trigger_count = 1U;
    request.start_boundary_trigger_count = 1U;
    request.pulse_width_us = 2000U;
    request.start_level = 0;
    return request;
}

ProfileCompareArmRequest BuildBoundaryAndFutureProfileCompareRequest() {
    ProfileCompareArmRequest request = BuildProfileCompareRequest();
    request.expected_trigger_count = 3U;
    request.start_boundary_trigger_count = 1U;
    return request;
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

TEST(ValveAdapterRegressionTest, StartDispenserClearsArmedProfileCompareState) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        BuildDispenserConfig(),
        DispenseCompensationProfile{});

    const auto arm_result = adapter.ArmProfileCompare(BuildProfileCompareRequest());
    ASSERT_TRUE(arm_result.IsSuccess());
    ASSERT_TRUE(adapter.GetProfileCompareStatus().Value().armed);

    DispenserValveParams params{};
    params.count = 1U;
    params.intervalMs = 100U;
    params.durationMs = 20U;
    const auto start_result = adapter.StartDispenser(params);
    ASSERT_TRUE(start_result.IsSuccess());

    const auto status_result = adapter.GetProfileCompareStatus();
    ASSERT_TRUE(status_result.IsSuccess());
    EXPECT_FALSE(status_result.Value().armed);

    const auto stop_result = adapter.StopDispenser();
    ASSERT_TRUE(stop_result.IsSuccess());
}

TEST(ValveAdapterRegressionTest, StopDispenserClearsArmedProfileCompareState) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        BuildDispenserConfig(),
        DispenseCompensationProfile{});

    const auto arm_result = adapter.ArmProfileCompare(BuildProfileCompareRequest());
    ASSERT_TRUE(arm_result.IsSuccess());
    ASSERT_TRUE(adapter.GetProfileCompareStatus().Value().armed);

    const auto stop_result = adapter.StopDispenser();
    ASSERT_TRUE(stop_result.IsSuccess());

    const auto status_result = adapter.GetProfileCompareStatus();
    ASSERT_TRUE(status_result.IsSuccess());
    EXPECT_FALSE(status_result.Value().armed);
}

TEST(ValveAdapterRegressionTest, ArmProfileCompareOffsetsAbsolutePositionsFromCurrentProfilePulse) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto dispenser_config = BuildDispenserConfig();
    dispenser_config.abs_position_flag = 1;
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        dispenser_config,
        DispenseCompensationProfile{});

    ASSERT_EQ(mock_card->MC_SetPos(1, 5000), 0);

    const auto arm_result = adapter.ArmProfileCompare(BuildProfileCompareRequest());
    ASSERT_TRUE(arm_result.IsSuccess()) << arm_result.GetError().GetMessage();

    EXPECT_EQ(mock_card->GetLastCmpEncodeAxis(), 1);
    EXPECT_EQ(mock_card->GetLastCmpAbsPositionFlag(), 1);
    const auto history = mock_card->GetCmpBufDataHistory();
    ASSERT_EQ(history.size(), 1U);
    const std::vector<long> expected_positions = {5100L, 5200L};
    EXPECT_EQ(history.front().buffer1, expected_positions);
}

TEST(ValveAdapterRegressionTest, ArmProfileCompareUsesDispenserConfigRelativeModeAsSingleAuthority) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto dispenser_config = BuildDispenserConfig();
    dispenser_config.abs_position_flag = 0;
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        dispenser_config,
        DispenseCompensationProfile{});

    ASSERT_EQ(mock_card->MC_SetPos(1, 5000), 0);

    const auto arm_result = adapter.ArmProfileCompare(BuildProfileCompareRequest());
    ASSERT_TRUE(arm_result.IsSuccess()) << arm_result.GetError().GetMessage();

    EXPECT_EQ(mock_card->GetLastCmpAbsPositionFlag(), 0);
    const auto history = mock_card->GetCmpBufDataHistory();
    ASSERT_EQ(history.size(), 1U);
    const std::vector<long> expected_positions = {100L, 200L};
    EXPECT_EQ(history.front().buffer1, expected_positions);
}

TEST(ValveAdapterRegressionTest, ArmProfileCompareUsesRequestedYAxisWhenMaskAllowsXY) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto dispenser_config = BuildDispenserConfig();
    dispenser_config.cmp_axis_mask = 0x03;
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        dispenser_config,
        DispenseCompensationProfile{});

    auto request = BuildProfileCompareRequest();
    request.compare_source_axis = 2;

    const auto arm_result = adapter.ArmProfileCompare(request);
    ASSERT_TRUE(arm_result.IsSuccess()) << arm_result.GetError().GetMessage();

    EXPECT_EQ(mock_card->GetLastCmpEncodeAxis(), 2);
    const auto history = mock_card->GetCmpBufDataHistory();
    ASSERT_EQ(history.size(), 1U);
    EXPECT_EQ(history.front().buffer1, request.compare_positions_pulse);
}

TEST(ValveAdapterRegressionTest, ArmProfileCompareRejectsFutureCompareCountBeyondConfiguredMaximum) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto dispenser_config = BuildDispenserConfig();
    dispenser_config.max_count = 1;
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        dispenser_config,
        DispenseCompensationProfile{});

    const auto arm_result = adapter.ArmProfileCompare(BuildProfileCompareRequest());
    ASSERT_TRUE(arm_result.IsError());
    EXPECT_EQ(arm_result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
    EXPECT_NE(
        arm_result.GetError().GetMessage().find("future_compare_count 超出 ValveDispenser.max_count"),
        std::string::npos);
}

TEST(ValveAdapterRegressionTest, ArmProfileCompareBoundaryOnlyCompletesImmediatelyWithoutBuffer) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        BuildDispenserConfig(),
        DispenseCompensationProfile{});

    const auto arm_result = adapter.ArmProfileCompare(BuildBoundaryOnlyProfileCompareRequest());
    ASSERT_TRUE(arm_result.IsSuccess()) << arm_result.GetError().GetMessage();

    const auto status_result = adapter.GetProfileCompareStatus();
    ASSERT_TRUE(status_result.IsSuccess());
    EXPECT_FALSE(status_result.Value().armed);
    EXPECT_EQ(status_result.Value().expected_trigger_count, 1U);
    EXPECT_EQ(status_result.Value().completed_trigger_count, 1U);
    EXPECT_EQ(status_result.Value().remaining_trigger_count, 0U);
    EXPECT_TRUE(mock_card->GetLastCmpBuffer1().empty());
    EXPECT_EQ(mock_card->GetCmpPulseCallCount(), 3);
    EXPECT_EQ(mock_card->GetLastCmpPulseChannelMask(), 0x01);
    EXPECT_EQ(mock_card->GetLastCmpPulseType1(), 2);
    EXPECT_EQ(mock_card->GetLastCmpPulseType2(), 0);
    EXPECT_EQ(mock_card->GetLastCmpPulseTime1(), 2000);
    EXPECT_EQ(mock_card->GetLastCmpPulseTime2(), 0);
    EXPECT_EQ(mock_card->GetLastCmpPulseTimeFlag1(), 0);
    EXPECT_EQ(mock_card->GetLastCmpPulseTimeFlag2(), 0);
}

TEST(ValveAdapterRegressionTest, ArmProfileCompareBoundaryAndFutureTriggersBoundaryPulseThenArmsFutureCompare) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto dispenser_config = BuildDispenserConfig();
    dispenser_config.abs_position_flag = 1;
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        dispenser_config,
        DispenseCompensationProfile{});

    ASSERT_EQ(mock_card->MC_SetPos(1, 5000), 0);

    const auto arm_result = adapter.ArmProfileCompare(BuildBoundaryAndFutureProfileCompareRequest());
    ASSERT_TRUE(arm_result.IsSuccess()) << arm_result.GetError().GetMessage();

    const auto status_result = adapter.GetProfileCompareStatus();
    ASSERT_TRUE(status_result.IsSuccess());
    EXPECT_TRUE(status_result.Value().armed);
    EXPECT_EQ(status_result.Value().expected_trigger_count, 3U);
    EXPECT_EQ(status_result.Value().completed_trigger_count, 1U);
    EXPECT_EQ(status_result.Value().remaining_trigger_count, 2U);
    EXPECT_EQ(mock_card->GetCmpPulseCallCount(), 3);
    EXPECT_EQ(mock_card->GetLastCmpPulseChannelMask(), 0x01);
    EXPECT_EQ(mock_card->GetLastCmpPulseType1(), 2);
    EXPECT_EQ(mock_card->GetLastCmpPulseType2(), 0);
    EXPECT_EQ(mock_card->GetLastCmpPulseTime1(), 2000);
    EXPECT_EQ(mock_card->GetLastCmpPulseTime2(), 0);
    EXPECT_EQ(mock_card->GetLastCmpPulseTimeFlag1(), 0);
    EXPECT_EQ(mock_card->GetLastCmpPulseTimeFlag2(), 0);
    EXPECT_EQ(mock_card->GetLastCmpEncodeAxis(), 1);
    EXPECT_EQ(mock_card->GetLastCmpAbsPositionFlag(), 1);
    const auto history = mock_card->GetCmpBufDataHistory();
    ASSERT_EQ(history.size(), 1U);
    const std::vector<long> expected_positions = {5100L, 5200L};
    EXPECT_EQ(history.front().buffer1, expected_positions);
}

TEST(ValveAdapterRegressionTest, ArmProfileCompareUsesDualBufferPreloadWhenBoardSpaceAllows) {
    auto mock_card = std::make_shared<MockMultiCard>();
    mock_card->SetCmpBufferCapacity(1, 1);
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        BuildDispenserConfig(),
        DispenseCompensationProfile{});

    ProfileCompareArmRequest request{};
    request.compare_positions_pulse = {100L, 200L};
    request.compare_source_axis = 1;
    request.expected_trigger_count = 2U;
    request.pulse_width_us = 2000U;
    request.start_level = 0;

    const auto arm_result = adapter.ArmProfileCompare(request);
    ASSERT_TRUE(arm_result.IsSuccess()) << arm_result.GetError().GetMessage();

    EXPECT_EQ(mock_card->GetLastCmpBuffer1Channel(), 1);
    EXPECT_EQ(mock_card->GetLastCmpBuffer2Channel(), 1);
    const auto history = mock_card->GetCmpBufDataHistory();
    ASSERT_EQ(history.size(), 1U);
    EXPECT_EQ(history.front().buffer1, std::vector<long>({100L}));
    EXPECT_EQ(history.front().buffer2, std::vector<long>({200L}));
}

TEST(ValveAdapterRegressionTest, GetProfileCompareStatusExposesRawHardwareSnapshotForPendingTail) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        BuildDispenserConfig(),
        DispenseCompensationProfile{});

    ProfileCompareArmRequest request{};
    request.compare_source_axis = 1;
    request.expected_trigger_count = 47U;
    request.pulse_width_us = 2000U;
    request.start_level = 0;
    request.compare_positions_pulse.reserve(47U);
    for (long index = 1; index <= 47; ++index) {
        request.compare_positions_pulse.push_back(index * 100L);
    }

    const auto arm_result = adapter.ArmProfileCompare(request);
    ASSERT_TRUE(arm_result.IsSuccess()) << arm_result.GetError().GetMessage();

    mock_card->ConsumeCmpBufferData(38, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const auto status_result = adapter.GetProfileCompareStatus();
    ASSERT_TRUE(status_result.IsSuccess()) << status_result.GetError().GetMessage();
    EXPECT_TRUE(status_result.Value().armed);
    EXPECT_EQ(status_result.Value().completed_trigger_count, 38U);
    EXPECT_EQ(status_result.Value().remaining_trigger_count, 9U);
    EXPECT_EQ(status_result.Value().submitted_future_compare_count, 47U);
    EXPECT_EQ(status_result.Value().hardware_status_word, 0x0001);
    EXPECT_EQ(status_result.Value().hardware_remain_data_1, 9U);
    EXPECT_EQ(status_result.Value().hardware_remain_data_2, 0U);
    EXPECT_EQ(status_result.Value().hardware_remain_space_1, 247U);
    EXPECT_EQ(status_result.Value().hardware_remain_space_2, 256U);
}

TEST(ValveAdapterRegressionTest, GetProfileCompareStatusReflectsWorkerRefillProgress) {
    auto mock_card = std::make_shared<MockMultiCard>();
    mock_card->SetCmpBufferCapacity(1, 1);
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    ValveAdapter adapter(
        wrapper,
        BuildSupplyConfig(),
        BuildDispenserConfig(),
        DispenseCompensationProfile{});

    ProfileCompareArmRequest request{};
    request.compare_positions_pulse = {100L, 200L, 300L};
    request.compare_source_axis = 1;
    request.expected_trigger_count = 3U;
    request.pulse_width_us = 2000U;
    request.start_level = 0;

    const auto arm_result = adapter.ArmProfileCompare(request);
    ASSERT_TRUE(arm_result.IsSuccess()) << arm_result.GetError().GetMessage();
    EXPECT_EQ(mock_card->GetCmpBufDataCallCount(), 1);

    mock_card->ConsumeCmpBufferData(1, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const auto status_result = adapter.GetProfileCompareStatus();
    ASSERT_TRUE(status_result.IsSuccess()) << status_result.GetError().GetMessage();
    EXPECT_EQ(mock_card->GetCmpBufDataCallCount(), 2);
    EXPECT_EQ(status_result.Value().completed_trigger_count, 2U);
    EXPECT_EQ(status_result.Value().remaining_trigger_count, 1U);

    mock_card->ConsumeCmpBufferData(1, 0);
    auto final_status_result = adapter.GetProfileCompareStatus();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < deadline) {
        final_status_result = adapter.GetProfileCompareStatus();
        ASSERT_TRUE(final_status_result.IsSuccess()) << final_status_result.GetError().GetMessage();
        if (final_status_result.Value().completed_trigger_count == 3U &&
            final_status_result.Value().remaining_trigger_count == 0U &&
            !final_status_result.Value().armed) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    ASSERT_TRUE(final_status_result.IsSuccess()) << final_status_result.GetError().GetMessage();
    EXPECT_EQ(final_status_result.Value().completed_trigger_count, 3U);
    EXPECT_EQ(final_status_result.Value().remaining_trigger_count, 0U);
    EXPECT_FALSE(final_status_result.Value().armed);
}
