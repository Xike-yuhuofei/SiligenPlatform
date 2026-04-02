#include "domain/safety/domain-services/SoftLimitValidator.h"
#include "process_planning/contracts/configuration/ConfigTypes.h"
#include "shared/types/Error.h"
#include "shared/types/Point.h"

#include <gtest/gtest.h>

namespace {
Siligen::Domain::Configuration::ValueObjects::MotionConfiguration BuildMotionConfig(
    double x_min, double x_max, bool x_enabled,
    double y_min, double y_max, bool y_enabled) {
    Siligen::Domain::Configuration::ValueObjects::MotionConfiguration config;
    Siligen::Domain::Configuration::ValueObjects::AxisConfiguration axis_x;
    axis_x.axisId = Siligen::Shared::Types::LogicalAxisId::X;
    axis_x.softLimits.enabled = x_enabled;
    axis_x.softLimits.min = x_min;
    axis_x.softLimits.max = x_max;

    Siligen::Domain::Configuration::ValueObjects::AxisConfiguration axis_y;
    axis_y.axisId = Siligen::Shared::Types::LogicalAxisId::Y;
    axis_y.softLimits.enabled = y_enabled;
    axis_y.softLimits.min = y_min;
    axis_y.softLimits.max = y_max;

    config.axes = {axis_x, axis_y};
    return config;
}
}  // namespace

TEST(SoftLimitValidatorTest, AcceptsPointWithinLimits) {
    auto config = BuildMotionConfig(0.0, 10.0, true, 0.0, 10.0, true);
    Siligen::Shared::Types::Point2D point{5.0f, 5.0f};

    auto result = Siligen::Domain::Safety::DomainServices::SoftLimitValidator::ValidatePoint(point, config);
    EXPECT_TRUE(result.IsSuccess());
}

TEST(SoftLimitValidatorTest, RejectsInvalidConfig) {
    auto config = BuildMotionConfig(10.0, 0.0, true, 0.0, 10.0, true);
    Siligen::Shared::Types::Point2D point{5.0f, 5.0f};

    auto result = Siligen::Domain::Safety::DomainServices::SoftLimitValidator::ValidatePoint(point, config);
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::INVALID_CONFIG_VALUE);
}

TEST(SoftLimitValidatorTest, RejectsPointOutsideLimits) {
    auto config = BuildMotionConfig(0.0, 10.0, true, 0.0, 10.0, false);
    Siligen::Shared::Types::Point2D point{11.0f, 0.0f};

    auto result = Siligen::Domain::Safety::DomainServices::SoftLimitValidator::ValidatePoint(point, config);
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::POSITION_OUT_OF_RANGE);
}
