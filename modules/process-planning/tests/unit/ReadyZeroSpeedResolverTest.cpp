#include "process_planning/contracts/configuration/ReadyZeroSpeedResolver.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Configuration::Services::ApplyUnifiedReadyZeroSpeed;
using Siligen::Domain::Configuration::Services::ResolveReadyZeroSpeed;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;

TEST(ReadyZeroSpeedResolverTest, RejectsMissingReadyZeroSpeed) {
    HomingConfig homing_config;
    homing_config.locate_velocity = 12.5f;

    const auto result = ResolveReadyZeroSpeed(homing_config, "ReadyZeroSpeedResolverTest");

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_CONFIG_VALUE);
}

TEST(ReadyZeroSpeedResolverTest, RejectsMissingReadyZeroAndLocateVelocity) {
    const HomingConfig homing_config;

    const auto result = ResolveReadyZeroSpeed(homing_config, "ReadyZeroSpeedResolverTest");

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_CONFIG_VALUE);
}

TEST(ReadyZeroSpeedResolverTest, RejectsMissingConfigurationPort) {
    const auto result = ResolveReadyZeroSpeed(
        LogicalAxisId::X,
        std::shared_ptr<IConfigurationPort>{},
        "ReadyZeroSpeedResolverTest");

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(ReadyZeroSpeedResolverTest, AppliesUnifiedSpeedToAllHomingPhases) {
    HomingConfig homing_config;
    homing_config.ready_zero_speed_mm_s = 8.0f;

    const auto result = ApplyUnifiedReadyZeroSpeed(homing_config, "ReadyZeroSpeedResolverTest");

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_FLOAT_EQ(result.Value().ready_zero_speed_mm_s, 8.0f);
    EXPECT_FLOAT_EQ(result.Value().rapid_velocity, 8.0f);
    EXPECT_FLOAT_EQ(result.Value().locate_velocity, 8.0f);
    EXPECT_FLOAT_EQ(result.Value().index_velocity, 8.0f);
    EXPECT_FLOAT_EQ(result.Value().escape_velocity, 8.0f);
}

}  // namespace
