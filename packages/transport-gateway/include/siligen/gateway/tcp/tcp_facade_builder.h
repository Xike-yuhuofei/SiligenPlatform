#pragma once

#include "siligen/gateway/tcp/tcp_facade_bundle.h"

#include "application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "application/usecases/dispensing/PlanningUseCase.h"
#include "application/usecases/dispensing/UploadFileUseCase.h"
#include "application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "application/usecases/recipes/CompareRecipeVersionsUseCase.h"
#include "application/usecases/recipes/CreateDraftVersionUseCase.h"
#include "application/usecases/recipes/CreateRecipeUseCase.h"
#include "application/usecases/recipes/CreateVersionFromPublishedUseCase.h"
#include "application/usecases/recipes/ExportRecipeBundlePayloadUseCase.h"
#include "application/usecases/recipes/ImportRecipeBundlePayloadUseCase.h"
#include "application/usecases/recipes/RecipeCommandUseCase.h"
#include "application/usecases/recipes/RecipeQueryUseCase.h"
#include "application/usecases/recipes/UpdateDraftVersionUseCase.h"
#include "application/usecases/recipes/UpdateRecipeUseCase.h"
#include "application/usecases/system/EmergencyStopUseCase.h"
#include "application/usecases/system/InitializeSystemUseCase.h"
#include "facades/tcp/TcpDispensingFacade.h"
#include "facades/tcp/TcpMotionFacade.h"
#include "facades/tcp/TcpRecipeFacade.h"
#include "facades/tcp/TcpSystemFacade.h"

#include <memory>

namespace Siligen::Gateway::Tcp {

template <typename Resolver>
TcpFacadeBundle BuildTcpFacadeBundle(Resolver& resolver) {
    TcpFacadeBundle bundle;

    bundle.system = std::make_shared<Application::Facades::Tcp::TcpSystemFacade>(
        resolver.template Resolve<Application::UseCases::System::InitializeSystemUseCase>(),
        resolver.template Resolve<Application::UseCases::System::EmergencyStopUseCase>());

    bundle.motion = std::make_shared<Application::Facades::Tcp::TcpMotionFacade>(
        resolver.template Resolve<Application::UseCases::Motion::Homing::HomeAxesUseCase>(),
        resolver.template Resolve<Application::UseCases::Motion::Manual::ManualMotionControlUseCase>(),
        resolver.template Resolve<Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase>());

    bundle.dispensing = std::make_shared<Application::Facades::Tcp::TcpDispensingFacade>(
        resolver.template Resolve<Application::UseCases::Dispensing::Valve::ValveCommandUseCase>(),
        resolver.template Resolve<Application::UseCases::Dispensing::Valve::ValveQueryUseCase>(),
        resolver.template Resolve<Application::UseCases::Dispensing::DispensingExecutionUseCase>(),
        resolver.template Resolve<Application::UseCases::Dispensing::UploadFileUseCase>(),
        resolver.template Resolve<Application::UseCases::Dispensing::PlanningUseCase>());

    bundle.recipe = std::make_shared<Application::Facades::Tcp::TcpRecipeFacade>(
        resolver.template Resolve<Application::UseCases::Recipes::CreateRecipeUseCase>(),
        resolver.template Resolve<Application::UseCases::Recipes::UpdateRecipeUseCase>(),
        resolver.template Resolve<Application::UseCases::Recipes::CreateDraftVersionUseCase>(),
        resolver.template Resolve<Application::UseCases::Recipes::UpdateDraftVersionUseCase>(),
        resolver.template Resolve<Application::UseCases::Recipes::RecipeCommandUseCase>(),
        resolver.template Resolve<Application::UseCases::Recipes::RecipeQueryUseCase>(),
        resolver.template Resolve<Application::UseCases::Recipes::CreateVersionFromPublishedUseCase>(),
        resolver.template Resolve<Application::UseCases::Recipes::CompareRecipeVersionsUseCase>(),
        resolver.template Resolve<Application::UseCases::Recipes::ExportRecipeBundlePayloadUseCase>(),
        resolver.template Resolve<Application::UseCases::Recipes::ImportRecipeBundlePayloadUseCase>());

    return bundle;
}

}  // namespace Siligen::Gateway::Tcp


