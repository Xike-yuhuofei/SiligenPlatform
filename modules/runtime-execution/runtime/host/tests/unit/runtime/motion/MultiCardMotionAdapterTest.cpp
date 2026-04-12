#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#include "siligen/device/adapters/motion/MultiCardMotionAdapter.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using MultiCardMotionAdapter = Siligen::Infrastructure::Adapters::MultiCardMotionAdapter;
using MockMultiCard = Siligen::Infrastructure::Hardware::MockMultiCard;
using MockMultiCardWrapper = Siligen::Infrastructure::Hardware::MockMultiCardWrapper;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using MotionState = Siligen::Domain::Motion::Ports::MotionState;
using HardwareConfiguration = Siligen::Shared::Types::HardwareConfiguration;

TEST(MultiCardMotionAdapterTest, BlocksNegativeJogWhenHomeBoundaryIsTriggered) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();

    mock_card->SetDigitalInputRaw(MC_HOME, 0x1);

    auto jog_result = adapter_result.Value()->StartJog(LogicalAxisId::X, -1, 10.0f);

    ASSERT_TRUE(jog_result.IsError());
    EXPECT_EQ(jog_result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::POSITION_OUT_OF_RANGE);
}

TEST(MultiCardMotionAdapterTest, AllowsNegativeJogWhenAxisIsAlreadyHomedAtHomeBoundary) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();

    mock_card->SetDigitalInputRaw(MC_HOME, 0x1);
    mock_card->SetAxisStatus(1, AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SUCESS | AXIS_STATUS_HOME_SWITCH);

    auto jog_result = adapter_result.Value()->StartJog(LogicalAxisId::X, -1, 10.0f);

    ASSERT_TRUE(jog_result.IsSuccess()) << jog_result.GetError().GetMessage();
}

TEST(MultiCardMotionAdapterTest, AllowsNegativePointMoveWhenAxisIsAlreadyHomedAtHomeBoundary) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();

    mock_card->SetDigitalInputRaw(MC_HOME, 0x1);
    mock_card->SetAxisStatus(1, AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SUCESS | AXIS_STATUS_HOME_SWITCH);
    ASSERT_EQ(mock_card->MC_SetPos(1, 0), 0);

    auto move_result = adapter_result.Value()->MoveAxisToPosition(LogicalAxisId::X, -1.0f, 10.0f);

    ASSERT_TRUE(move_result.IsSuccess()) << move_result.GetError().GetMessage();
}

TEST(MultiCardMotionAdapterTest, DoesNotReportNegativeHardLimitWhenOnlyHomeInputIsTriggered) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();

    mock_card->SetDigitalInputRaw(MC_HOME, 0x1);
    mock_card->SetAxisStatus(1, AXIS_STATUS_ENABLE);

    long home_raw = 0;
    ASSERT_EQ(mock_card->MC_GetDiRaw(MC_HOME, &home_raw), 0);

    auto status_result = adapter_result.Value()->GetAxisStatus(LogicalAxisId::X);

    long hw_status = 0;
    ASSERT_EQ(mock_card->MC_GetSts(1, &hw_status), 0);

    ASSERT_TRUE(status_result.IsSuccess()) << status_result.GetError().GetMessage();
    EXPECT_EQ(status_result.Value().state, MotionState::IDLE);
    EXPECT_TRUE(status_result.Value().home_signal)
        << "home_raw=0x" << std::hex << home_raw
        << " hw_status=0x" << hw_status << std::dec
        << " enabled=" << status_result.Value().enabled
        << " has_error=" << status_result.Value().has_error
        << " error_code=" << status_result.Value().error_code;
    EXPECT_FALSE(status_result.Value().hard_limit_negative);
}

TEST(MultiCardMotionAdapterTest, ReadLimitStatusDoesNotExposeHomeInputAsNegativeHardLimitWhileHoming) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();

    mock_card->SetDigitalInputRaw(MC_HOME, 0x1);
    mock_card->SetAxisStatus(1, AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_RUNNING);

    auto limit_result = adapter_result.Value()->ReadLimitStatus(LogicalAxisId::X, false);

    ASSERT_TRUE(limit_result.IsSuccess()) << limit_result.GetError().GetMessage();
    EXPECT_FALSE(limit_result.Value());
}

TEST(MultiCardMotionAdapterTest, DoesNotReportNegativeHardLimitFromHomeSignalWhileHoming) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();

    mock_card->SetDigitalInputRaw(MC_HOME, 0x1);
    mock_card->SetAxisStatus(1, AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_RUNNING | AXIS_STATUS_HOME_SWITCH);

    auto status_result = adapter_result.Value()->GetAxisStatus(LogicalAxisId::X);

    ASSERT_TRUE(status_result.IsSuccess()) << status_result.GetError().GetMessage();
    EXPECT_EQ(status_result.Value().state, MotionState::HOMING);
    EXPECT_TRUE(status_result.Value().home_signal);
    EXPECT_FALSE(status_result.Value().hard_limit_negative);
}

TEST(MultiCardMotionAdapterTest, ReadLimitStatusDoesNotExposeHomeInputAsNegativeHardLimitOutsideHoming) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();

    mock_card->SetDigitalInputRaw(MC_HOME, 0x1);
    mock_card->SetAxisStatus(1, AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SWITCH);

    auto limit_result = adapter_result.Value()->ReadLimitStatus(LogicalAxisId::X, false);

    ASSERT_TRUE(limit_result.IsSuccess()) << limit_result.GetError().GetMessage();
    EXPECT_FALSE(limit_result.Value());
}

TEST(MultiCardMotionAdapterTest, StartJogAndStopExposeVelocityFromMockRuntime) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();

    auto jog_result = adapter_result.Value()->StartJog(LogicalAxisId::X, 1, 10.0f);
    ASSERT_TRUE(jog_result.IsSuccess()) << jog_result.GetError().GetMessage();

    auto running_status = adapter_result.Value()->GetAxisStatus(LogicalAxisId::X);
    ASSERT_TRUE(running_status.IsSuccess()) << running_status.GetError().GetMessage();
    EXPECT_EQ(running_status.Value().state, MotionState::MOVING);
    EXPECT_GT(running_status.Value().velocity, 0.0f);

    auto stop_result = adapter_result.Value()->StopAxis(LogicalAxisId::X, true);
    ASSERT_TRUE(stop_result.IsSuccess()) << stop_result.GetError().GetMessage();

    auto stopped_status = adapter_result.Value()->GetAxisStatus(LogicalAxisId::X);
    ASSERT_TRUE(stopped_status.IsSuccess()) << stopped_status.GetError().GetMessage();
    EXPECT_EQ(stopped_status.Value().state, MotionState::IDLE);
    EXPECT_FLOAT_EQ(stopped_status.Value().velocity, 0.0f);
}

TEST(MultiCardMotionAdapterTest, RecoverFromEmergencyStopClearsEstopAndReenablesAxis) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();

    auto connect_result = adapter_result.Value()->Connect("192.168.10.20", "192.168.10.10", 5000);
    ASSERT_TRUE(connect_result.IsSuccess()) << connect_result.GetError().GetMessage();

    const int reset_calls_before_recover = mock_card->GetResetCallCount();
    mock_card->SetAxisStatus(1, AXIS_STATUS_ESTOP);

    auto estop_status = adapter_result.Value()->GetAxisStatus(LogicalAxisId::X);
    ASSERT_TRUE(estop_status.IsSuccess()) << estop_status.GetError().GetMessage();
    EXPECT_EQ(estop_status.Value().state, MotionState::ESTOP);
    EXPECT_FALSE(estop_status.Value().enabled);

    auto recover_result = adapter_result.Value()->RecoverFromEmergencyStop();
    ASSERT_TRUE(recover_result.IsSuccess()) << recover_result.GetError().GetMessage();
    EXPECT_EQ(mock_card->GetResetCallCount(), reset_calls_before_recover + 1);

    auto recovered_status = adapter_result.Value()->GetAxisStatus(LogicalAxisId::X);
    ASSERT_TRUE(recovered_status.IsSuccess()) << recovered_status.GetError().GetMessage();
    EXPECT_NE(recovered_status.Value().state, MotionState::ESTOP);
    EXPECT_TRUE(recovered_status.Value().enabled);
}

TEST(MultiCardMotionAdapterTest, RunningStateTakesPriorityOverHomedState) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();

    mock_card->SetAxisStatus(1, AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SUCESS | AXIS_STATUS_RUNNING);

    auto status_result = adapter_result.Value()->GetAxisStatus(LogicalAxisId::X);

    ASSERT_TRUE(status_result.IsSuccess()) << status_result.GetError().GetMessage();
    EXPECT_EQ(status_result.Value().state, MotionState::MOVING);
}

TEST(MockMultiCardCharacterizationTest, AxisOnSetsEnableBitInRawStatus) {
    auto mock_card = std::make_shared<MockMultiCard>();

    long status = 0;
    ASSERT_EQ(mock_card->MC_AxisOn(1), 0);
    ASSERT_EQ(mock_card->MC_GetSts(1, &status), 0);
    EXPECT_NE((status & AXIS_STATUS_ENABLE), 0);
}

TEST(MockMultiCardCharacterizationTest, GetStsStaysAvailableUntilExplicitDisconnect) {
    auto mock_card = std::make_shared<MockMultiCard>();

    long status = 0;
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(mock_card->MC_GetSts(1, &status), 0) << "call=" << (i + 1);
    }

    mock_card->SimulateDisconnect();
    EXPECT_NE(mock_card->MC_GetSts(1, &status), 0);
}

TEST(MockMultiCardCharacterizationTest, CrdClearDropsBufferedTrajectoryBeforeNextRun) {
    auto mock_card = std::make_shared<MockMultiCard>();

    long axis_map[4] = {1, 2, 0, 0};
    ASSERT_EQ(mock_card->MC_CrdSpace(0, axis_map, 0), 0);
    ASSERT_EQ(mock_card->MC_SetPos(1, 0), 0);
    ASSERT_EQ(mock_card->MC_SetPos(2, 0), 0);

    ASSERT_EQ(mock_card->MC_LnXY(0, 100, 0, 0.0, 0.0, 0.0, 0), 0);
    ASSERT_EQ(mock_card->MC_LnXY(0, 200, 0, 0.0, 0.0, 0.0, 0), 0);
    ASSERT_EQ(mock_card->MC_CrdStart(0, 0), 0);
    mock_card->TickMs(20.0);

    long pos_x = 0;
    long pos_y = 0;
    ASSERT_EQ(mock_card->MC_GetPos(1, &pos_x), 0);
    ASSERT_EQ(mock_card->MC_GetPos(2, &pos_y), 0);
    EXPECT_EQ(pos_x, 200);
    EXPECT_EQ(pos_y, 0);

    ASSERT_EQ(mock_card->MC_SetPos(1, 0), 0);
    ASSERT_EQ(mock_card->MC_SetPos(2, 0), 0);
    ASSERT_EQ(mock_card->MC_CrdClear(0, 0), 0);

    ASSERT_EQ(mock_card->MC_LnXY(0, 50, 25, 0.0, 0.0, 0.0, 0), 0);
    ASSERT_EQ(mock_card->MC_CrdStart(0, 0), 0);
    mock_card->TickMs(10.0);

    ASSERT_EQ(mock_card->MC_GetPos(1, &pos_x), 0);
    ASSERT_EQ(mock_card->MC_GetPos(2, &pos_y), 0);
    EXPECT_EQ(pos_x, 50);
    EXPECT_EQ(pos_y, 25);

    mock_card->TickMs(10.0);
    ASSERT_EQ(mock_card->MC_GetPos(1, &pos_x), 0);
    ASSERT_EQ(mock_card->MC_GetPos(2, &pos_y), 0);
    EXPECT_EQ(pos_x, 50);
    EXPECT_EQ(pos_y, 25);
}

TEST(MockMultiCardCharacterizationTest, WrapperStopExWithZeroAxisMaskDoesNotStopJogAxis) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    ASSERT_EQ(mock_card->MC_AxisOn(1), 0);
    ASSERT_EQ(mock_card->MC_PrfJog(1), 0);
    ASSERT_EQ(mock_card->MC_SetVel(1, 5.0), 0);
    ASSERT_EQ(mock_card->MC_Update(0x1), 0);

    double velocity = 0.0;
    ASSERT_EQ(mock_card->MC_GetAxisPrfVel(1, &velocity), 0);
    EXPECT_GT(velocity, 0.0);

    ASSERT_EQ(wrapper->MC_StopEx(0x1, 0), 0);

    ASSERT_EQ(mock_card->MC_GetAxisPrfVel(1, &velocity), 0);
    EXPECT_GT(velocity, 0.0);
}

TEST(MultiCardMotionAdapterTest, RunningBitOverridesHomedBitInMappedAxisState) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;

    auto adapter_result = MultiCardMotionAdapter::Create(wrapper, hardware_config);
    ASSERT_TRUE(adapter_result.IsSuccess()) << adapter_result.GetError().GetMessage();

    mock_card->SetAxisStatus(1, AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SUCESS | AXIS_STATUS_RUNNING);

    auto status_result = adapter_result.Value()->GetAxisStatus(LogicalAxisId::X);

    ASSERT_TRUE(status_result.IsSuccess()) << status_result.GetError().GetMessage();
    EXPECT_EQ(status_result.Value().state, MotionState::MOVING);
}

}  // namespace
