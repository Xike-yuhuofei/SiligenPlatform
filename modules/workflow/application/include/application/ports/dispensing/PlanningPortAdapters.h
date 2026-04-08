#pragma once

#include "application/ports/dispensing/PlanningPorts.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace Siligen::Application::Ports::Dispensing {

namespace detail {

class ProcessPathFacadePortAdapter final : public IProcessPathBuildPort {
public:
    explicit ProcessPathFacadePortAdapter(
        std::shared_ptr<Siligen::Application::Services::ProcessPath::ProcessPathFacade> facade)
        : facade_(std::move(facade)) {
        if (!facade_) {
            throw std::invalid_argument("ProcessPathFacadePortAdapter: facade cannot be null");
        }
    }

    ProcessPathBuildResult Build(const ProcessPathBuildRequest& request) const override {
        return facade_->Build(request);
    }

private:
    std::shared_ptr<Siligen::Application::Services::ProcessPath::ProcessPathFacade> facade_;
};

class MotionPlanningFacadePortAdapter final : public IMotionPlanningPort {
public:
    explicit MotionPlanningFacadePortAdapter(
        std::shared_ptr<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade> facade)
        : facade_(std::move(facade)) {
        if (!facade_) {
            throw std::invalid_argument("MotionPlanningFacadePortAdapter: facade cannot be null");
        }
    }

    MotionPlan Plan(
        const Siligen::ProcessPath::Contracts::ProcessPath& path,
        const MotionPlanningConfig& config) const override {
        return facade_->Plan(path, config);
    }

private:
    std::shared_ptr<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade> facade_;
};

class DxfPreparationPortAdapter final : public IPlanningInputPreparationPort {
public:
    explicit DxfPreparationPortAdapter(
        std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> service)
        : service_(std::move(service)) {
        if (!service_) {
            throw std::invalid_argument("DxfPreparationPortAdapter: service cannot be null");
        }
    }

    Result<PreparedPlanningInputPath> EnsurePreparedInput(const std::string& source_path) const override {
        return service_->EnsurePbReady(source_path);
    }

private:
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> service_;
};

}  // namespace detail

inline std::shared_ptr<IProcessPathBuildPort> AdaptProcessPathFacade(
    std::shared_ptr<Siligen::Application::Services::ProcessPath::ProcessPathFacade> facade) {
    return std::make_shared<detail::ProcessPathFacadePortAdapter>(std::move(facade));
}

inline std::shared_ptr<IMotionPlanningPort> AdaptMotionPlanningFacade(
    std::shared_ptr<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade> facade) {
    return std::make_shared<detail::MotionPlanningFacadePortAdapter>(std::move(facade));
}

inline std::shared_ptr<IPlanningInputPreparationPort> AdaptDxfPreparationService(
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> service) {
    return std::make_shared<detail::DxfPreparationPortAdapter>(std::move(service));
}

}  // namespace Siligen::Application::Ports::Dispensing
