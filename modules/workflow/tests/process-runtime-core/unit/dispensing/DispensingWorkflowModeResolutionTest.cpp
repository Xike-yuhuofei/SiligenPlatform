#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "application/usecases/dispensing/DispensingWorkflowUseCase.h"

namespace {

using Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using Siligen::Application::UseCases::Dispensing::DispensingMVPRequest;
using Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase;
using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Application::UseCases::Dispensing::PlanningUseCase;
using Siligen::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode;
using Siligen::Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Siligen::Domain::Machine::ValueObjects::MachineMode;

template <typename Tag, typename Tag::type Member>
struct PrivateMemberAccessor {
    friend typename Tag::type GetPrivateMember(Tag) {
        return Member;
    }
};

struct BuildPlanningRequestTag {
    using type = PlanningRequest (DispensingWorkflowUseCase::*)(const std::string&,
                                                                const DispensingMVPRequest&) const;
    friend type GetPrivateMember(BuildPlanningRequestTag);
};

template struct PrivateMemberAccessor<
    BuildPlanningRequestTag,
    &DispensingWorkflowUseCase::BuildPlanningRequest>;

template <typename T>
std::shared_ptr<T> MakeDummyShared() {
    return std::shared_ptr<T>(reinterpret_cast<T*>(0x1), [](T*) {});
}

DispensingWorkflowUseCase CreateWorkflowUseCase() {
    return DispensingWorkflowUseCase(
        MakeDummyShared<UploadFileUseCase>(),
        MakeDummyShared<PlanningUseCase>(),
        MakeDummyShared<DispensingExecutionUseCase>(),
        nullptr,
        nullptr,
        nullptr,
        nullptr);
}

}  // namespace

TEST(DispensingWorkflowModeResolutionTest, BuildPlanningRequestUsesValidationSpeedForExplicitDryCycle) {
    auto use_case = CreateWorkflowUseCase();

    DispensingMVPRequest request;
    request.dxf_filepath = "dummy.dxf";
    request.machine_mode = MachineMode::Test;
    request.execution_mode = JobExecutionMode::ValidationDryCycle;
    request.output_policy = ProcessOutputPolicy::Inhibited;
    request.dispensing_speed_mm_s = 25.0f;
    request.dry_run_speed_mm_s = 80.0f;

    const auto build_planning_request = GetPrivateMember(BuildPlanningRequestTag{});
    const auto planning_request = (use_case.*build_planning_request)("prepared.pb", request);
    EXPECT_FLOAT_EQ(planning_request.trajectory_config.max_velocity, 80.0f);
}
