#include <gtest/gtest.h>

#include "application/services/dispensing/DispensingExecutionCompatibilityService.h"

namespace {

using Siligen::Application::Services::Dispensing::DispensingExecutionCompatibilityService;
using Siligen::Application::UseCases::Dispensing::DispensingMVPRequest;
using Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode;
using Siligen::Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Siligen::Domain::Machine::ValueObjects::MachineMode;

}  // namespace

TEST(DispensingWorkflowModeResolutionTest, BuildPlanningRequestUsesValidationSpeedForExplicitDryCycle) {
    DispensingExecutionCompatibilityService compatibility_service;

    DispensingMVPRequest request;
    request.dxf_filepath = "dummy.dxf";
    request.machine_mode = MachineMode::Test;
    request.execution_mode = JobExecutionMode::ValidationDryCycle;
    request.output_policy = ProcessOutputPolicy::Inhibited;
    request.dispensing_speed_mm_s = 25.0f;
    request.dry_run_speed_mm_s = 80.0f;

    auto planning_request_result = compatibility_service.BuildPlanningRequest(request, "prepared.pb");
    ASSERT_TRUE(planning_request_result.IsSuccess()) << planning_request_result.GetError().ToString();

    const auto& planning_request = planning_request_result.Value();
    EXPECT_FLOAT_EQ(planning_request.trajectory_config.max_velocity, 80.0f);
}
