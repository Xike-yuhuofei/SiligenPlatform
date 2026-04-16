#include "process_planning/contracts/ConfigurationContracts.h"

#include <gtest/gtest.h>

#include <type_traits>

namespace {

using Siligen::Domain::Configuration::Ports::DispensingConfig;
using Siligen::Domain::Configuration::Ports::DxfImportConfig;
using Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig;
using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Configuration::Ports::MachineConfig;
using Siligen::Domain::Configuration::Ports::ValveSupplyConfig;
using Siligen::Domain::Configuration::Services::ResolveReadyZeroSpeed;
using Siligen::Shared::Types::DispensingStrategy;
using Siligen::Shared::Types::ErrorCode;

static_assert(std::is_default_constructible_v<DispensingConfig>);
static_assert(std::is_default_constructible_v<DxfImportConfig>);
static_assert(std::is_default_constructible_v<DxfTrajectoryConfig>);
static_assert(std::is_default_constructible_v<MachineConfig>);
static_assert(std::is_default_constructible_v<HomingConfig>);

TEST(ConfigurationContractsTest, UmbrellaHeaderExposesCanonicalConfigurationSurface) {
    const DispensingConfig dispensing;
    const DxfImportConfig preprocess;
    const DxfTrajectoryConfig trajectory;
    const MachineConfig machine;
    const HomingConfig homing;

    EXPECT_EQ(dispensing.mode, Siligen::DispensingMode::CONTACT);
    EXPECT_EQ(dispensing.strategy, DispensingStrategy::BASELINE);
    EXPECT_TRUE(preprocess.normalize_units);
    EXPECT_EQ(trajectory.python, "python");
    EXPECT_FLOAT_EQ(machine.work_area_width, 480.0f);

    const auto result = ResolveReadyZeroSpeed(homing, "ConfigurationContractsTest");
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_CONFIG_VALUE);
}

TEST(ConfigurationContractsTest, ValveSupplyConfigHonorsDigitalOutputBounds) {
    const ValveSupplyConfig valid{0, 15};
    const ValveSupplyConfig invalid_card{-1, 0};
    const ValveSupplyConfig invalid_bit{0, 16};

    EXPECT_TRUE(valid.IsValid());
    EXPECT_FALSE(invalid_card.IsValid());
    EXPECT_FALSE(invalid_bit.IsValid());
}

}  // namespace
