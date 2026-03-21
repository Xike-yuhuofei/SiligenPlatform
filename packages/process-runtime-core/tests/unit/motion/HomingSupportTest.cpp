#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "legacy/adapters/motion/controller/homing/HomingSupport.h"

#include <gtest/gtest.h>

#include <array>

namespace {

using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Motion::ValueObjects::HomeInputState;
using Siligen::Domain::Motion::ValueObjects::HomingStage;
using Siligen::Domain::Motion::ValueObjects::HomingTestStatus;
using Siligen::Infrastructure::Adapters::Internal::Homing::FinalizeSuccessfulHoming;
using Siligen::Infrastructure::Adapters::Internal::Homing::PrepareHomeParameters;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::HardwareConfiguration;
using Siligen::Shared::Types::UnitConverter;

TEST(HomingSupportTest, FinalizeSuccessfulHomingWaitsForTransientHomeSwitchRelease) {
    HomingConfig config;
    config.enable_limit_switch = true;
    config.enable_escape = true;
    config.home_input_source = MC_HOME;
    config.home_debounce_ms = 3;
    config.settle_time_ms = 60;

    std::array<bool, 4> success_latch{};
    std::array<bool, 4> failed_latch{};
    std::array<bool, 4> active_latch{};
    std::array<HomingStage, 4> stage_latch{};
    std::array<bool, 4> timer_active_latch{};

    int read_count = 0;
    auto status = FinalizeSuccessfulHoming(
        0,
        config,
        success_latch,
        failed_latch,
        active_latch,
        stage_latch,
        timer_active_latch,
        [&read_count](int) -> Result<HomeInputState> {
            HomeInputState state;
            state.triggered = (read_count++ == 0);
            return Result<HomeInputState>::Success(state);
        });

    EXPECT_EQ(status, HomingTestStatus::Completed);
    EXPECT_TRUE(success_latch[0]);
    EXPECT_FALSE(failed_latch[0]);
    EXPECT_EQ(stage_latch[0], HomingStage::Completed);
}

TEST(HomingSupportTest, FinalizeSuccessfulHomingFailsWhenHomeSwitchStaysTriggered) {
    HomingConfig config;
    config.enable_limit_switch = true;
    config.enable_escape = true;
    config.home_input_source = MC_HOME;
    config.home_debounce_ms = 0;
    config.settle_time_ms = 50;

    std::array<bool, 4> success_latch{};
    std::array<bool, 4> failed_latch{};
    std::array<bool, 4> active_latch{};
    std::array<HomingStage, 4> stage_latch{};
    std::array<bool, 4> timer_active_latch{};

    auto status = FinalizeSuccessfulHoming(
        0,
        config,
        success_latch,
        failed_latch,
        active_latch,
        stage_latch,
        timer_active_latch,
        [](int) -> Result<HomeInputState> {
            HomeInputState state;
            state.triggered = true;
            return Result<HomeInputState>::Success(state);
        });

    EXPECT_EQ(status, HomingTestStatus::Failed);
    EXPECT_FALSE(success_latch[0]);
    EXPECT_TRUE(failed_latch[0]);
    EXPECT_EQ(stage_latch[0], HomingStage::Failed);
}

TEST(HomingSupportTest, FinalizeSuccessfulHomingAllowsTriggeredHomeSwitchWhenBoardBackoffDisabled) {
    HomingConfig config;
    config.enable_limit_switch = true;
    config.enable_escape = true;
    config.home_input_source = MC_HOME;
    config.home_backoff_enabled = false;

    std::array<bool, 4> success_latch{};
    std::array<bool, 4> failed_latch{};
    std::array<bool, 4> active_latch{};
    std::array<HomingStage, 4> stage_latch{};
    std::array<bool, 4> timer_active_latch{};

    auto status = FinalizeSuccessfulHoming(
        0,
        config,
        success_latch,
        failed_latch,
        active_latch,
        stage_latch,
        timer_active_latch,
        [](int) -> Result<HomeInputState> {
            HomeInputState state;
            state.triggered = true;
            return Result<HomeInputState>::Success(state);
        });

    EXPECT_EQ(status, HomingTestStatus::Completed);
    EXPECT_TRUE(success_latch[0]);
    EXPECT_FALSE(failed_latch[0]);
    EXPECT_EQ(stage_latch[0], HomingStage::Completed);
}

TEST(HomingSupportTest, PrepareHomeParametersForcesZeroBackoffWhenDisabled) {
    HomingConfig config;
    config.mode = 1;
    config.direction = 0;
    config.rapid_velocity = 10.0f;
    config.locate_velocity = 5.0f;
    config.index_velocity = 2.0f;
    config.acceleration = 500.0f;
    config.search_distance = 100.0f;
    config.escape_distance = 5.0f;
    config.home_backoff_enabled = false;

    HardwareConfiguration hardware_config;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.soft_limit_negative_mm = 0.0f;
    hardware_config.soft_limit_positive_mm = 480.0f;
    UnitConverter unit_converter(hardware_config);

    auto prepared_result = PrepareHomeParameters(
        0, config, hardware_config, unit_converter, "HomingSupportTest", 1000.0);

    ASSERT_TRUE(prepared_result.IsSuccess());
    EXPECT_EQ(prepared_result.Value().home_parameters.ulHomeBackDis, 0);
}

}  // namespace
