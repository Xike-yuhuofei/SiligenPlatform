#include "workflow/application/planning-trigger/PlanningUseCase.h"

#include <gtest/gtest.h>

namespace {
using Siligen::Application::UseCases::Dispensing::PlanningRequest;
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

TEST(PlanningRequestTest, RejectsEmptyFilepath) {
    auto request = MakeValidRequest();
    request.dxf_filepath.clear();
    EXPECT_FALSE(request.Validate());
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


