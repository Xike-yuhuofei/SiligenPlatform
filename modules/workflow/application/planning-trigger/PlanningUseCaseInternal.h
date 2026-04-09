#pragma once

#include "PlanningUseCase.h"

namespace Siligen::Application::UseCases::Dispensing {

struct PlanningUseCaseInternalAccess {
    static Result<PreparedAuthorityPreview> PrepareAuthorityPreview(
        PlanningUseCase& use_case,
        const PlanningRequest& request) {
        return use_case.PrepareAuthorityPreview(request);
    }

    static Result<ExecutionAssemblyResponse> AssembleExecutionFromAuthority(
        PlanningUseCase& use_case,
        const PlanningRequest& request,
        const PreparedAuthorityPreview& authority_preview) {
        return use_case.AssembleExecutionFromAuthority(request, authority_preview);
    }
};

}  // namespace Siligen::Application::UseCases::Dispensing
