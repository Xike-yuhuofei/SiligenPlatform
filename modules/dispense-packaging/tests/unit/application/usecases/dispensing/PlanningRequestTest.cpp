#include "dispense_packaging/application/usecases/dispensing/PlanningUseCase.h"

#include <gtest/gtest.h>

namespace {
using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Shared::Types::DispensingExecutionStrategy;
using Siligen::Shared::Types::PointFlyingCarrierDirectionMode;
using Siligen::Shared::Types::PointFlyingCarrierPolicy;
using Siligen::Shared::Types::TrajectoryConfig;

PlanningRequest MakeValidRequest() {
    PlanningRequest request;
    request.dxf_filepath = "dummy.dxf";
    request.trajectory_config = TrajectoryConfig();
    request.trajectory_config.max_velocity = 100.0f;
    request.trajectory_config.max_acceleration = 500.0f;
    request.trajectory_config.time_step = 0.01f;
    return request;
}
}  // namespace

TEST(PlanningRequestTest, ValidRequestPasses) {
    auto request = MakeValidRequest();
    EXPECT_TRUE(request.Validate());
}

TEST(PlanningRequestTest, RequestedExecutionStrategyDefaultsToFlyingShotWhenUnset) {
    auto request = MakeValidRequest();
    EXPECT_FALSE(request.HasExplicitExecutionStrategy());
    EXPECT_EQ(request.ResolveRequestedExecutionStrategy(), DispensingExecutionStrategy::FLYING_SHOT);
}

TEST(PlanningRequestTest, RequestedExecutionStrategyPreservesExplicitChoice) {
    auto request = MakeValidRequest();
    request.requested_execution_strategy = DispensingExecutionStrategy::STATIONARY_SHOT;
    EXPECT_TRUE(request.HasExplicitExecutionStrategy());
    EXPECT_EQ(request.ResolveRequestedExecutionStrategy(), DispensingExecutionStrategy::STATIONARY_SHOT);
}

TEST(PlanningRequestTest, RejectsEmptyFilepath) {
    auto request = MakeValidRequest();
    request.dxf_filepath.clear();
    EXPECT_FALSE(request.Validate());
}

TEST(PlanningRequestTest, AllowsCurrentChainWithoutRecipeOrVersion) {
    auto request = MakeValidRequest();
    EXPECT_TRUE(request.Validate());
}

TEST(PlanningRequestTest, RejectsVelocityOutOfRange) {
    auto request = MakeValidRequest();
    request.trajectory_config.max_velocity = 0.0f;
    EXPECT_FALSE(request.Validate());

    request = MakeValidRequest();
    request.trajectory_config.max_velocity = 1000.1f;
    EXPECT_FALSE(request.Validate());
}

TEST(PlanningRequestTest, RejectsAccelerationOutOfRange) {
    auto request = MakeValidRequest();
    request.trajectory_config.max_acceleration = 0.0f;
    EXPECT_FALSE(request.Validate());

    request = MakeValidRequest();
    request.trajectory_config.max_acceleration = 10000.1f;
    EXPECT_FALSE(request.Validate());
}

TEST(PlanningRequestTest, RejectsTimeStepOutOfRange) {
    auto request = MakeValidRequest();
    request.trajectory_config.time_step = 0.0f;
    EXPECT_FALSE(request.Validate());

    request = MakeValidRequest();
    request.trajectory_config.time_step = 0.1001f;
    EXPECT_FALSE(request.Validate());
}

TEST(PlanningRequestTest, RejectsInvalidPointFlyingCarrierPolicy) {
    auto request = MakeValidRequest();
    PointFlyingCarrierPolicy policy;
    policy.direction_mode = PointFlyingCarrierDirectionMode::APPROACH_DIRECTION;
    policy.trigger_spatial_interval_mm = 0.0f;
    request.point_flying_carrier_policy = policy;

    EXPECT_FALSE(request.Validate());
}


