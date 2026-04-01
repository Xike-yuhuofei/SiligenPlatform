#include "runtime/configuration/validators/ConfigValidator.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace {

using Siligen::ConfigValidator;
using Siligen::Domain::Configuration::Ports::HomingConfig;

HomingConfig BuildValidHomingConfig() {
    HomingConfig config;
    config.axis = 0;
    config.mode = 1;
    config.direction = 0;
    config.home_input_source = 3;
    config.home_input_bit = 0;
    config.home_active_low = false;
    config.home_debounce_ms = 3;
    config.ready_zero_speed_mm_s = 8.0f;
    config.rapid_velocity = 8.0f;
    config.locate_velocity = 8.0f;
    config.index_velocity = 8.0f;
    config.acceleration = 500.0f;
    config.deceleration = 500.0f;
    config.jerk = 2000.0f;
    config.offset = 0.0f;
    config.search_distance = 485.0f;
    config.escape_distance = 5.0f;
    config.escape_velocity = 8.0f;
    config.home_backoff_enabled = false;
    config.timeout_ms = 80000;
    config.settle_time_ms = 500;
    config.escape_timeout_ms = 3000;
    config.retry_count = 0;
    config.enable_escape = true;
    config.enable_limit_switch = true;
    config.enable_index = false;
    config.escape_max_attempts = 2;
    return config;
}

TEST(ConfigValidatorTest, RejectsTimeoutShorterThanTravelDistanceOverReadyZeroSpeed) {
    auto config = BuildValidHomingConfig();
    config.timeout_ms = 60000;

    const auto result = ConfigValidator::ValidateHomingConfigDetailed(config);

    EXPECT_FALSE(result.is_valid);
    EXPECT_TRUE(std::any_of(result.errors.begin(), result.errors.end(), [](const std::string& error) {
        return error.find("超时时间不能小于搜索距离/回零速度 + 稳定时间") != std::string::npos;
    }));
}

TEST(ConfigValidatorTest, AcceptsTimeoutCoveringTravelDistanceAndSettleTime) {
    const auto config = BuildValidHomingConfig();

    const auto result = ConfigValidator::ValidateHomingConfigDetailed(config);

    EXPECT_TRUE(result.is_valid);
}

}  // namespace
