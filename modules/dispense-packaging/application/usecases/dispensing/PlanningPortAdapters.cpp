#include "dispense_packaging/application/usecases/dispensing/PlanningPortAdapters.h"

#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"

#include <stdexcept>
#include <utility>

namespace Siligen::Application::Ports::Dispensing {

namespace {

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

    Result<PreparedPlanningInput> EnsurePreparedInput(const PlanningInputPreparationRequest& request) const override {
        auto result = service_->PrepareInputArtifact(request.source_path);
        if (result.IsError()) {
            return Result<PreparedPlanningInput>::Failure(result.GetError());
        }

        PreparedPlanningInput prepared;
        prepared.prepared_path = result.Value().prepared_path;
        prepared.input_quality_projection.report_id = result.Value().input_quality.report_id;
        prepared.input_quality_projection.report_path = result.Value().input_quality.report_path;
        prepared.input_quality_projection.schema_version = result.Value().input_quality.schema_version;
        prepared.input_quality_projection.dxf_hash = request.source_hash;
        prepared.input_quality_projection.source_drawing_ref = request.source_ref;
        prepared.input_quality_projection.gate_result = result.Value().input_quality.gate_result;
        prepared.input_quality_projection.classification = result.Value().input_quality.classification;
        prepared.input_quality_projection.preview_ready = result.Value().input_quality.preview_ready;
        prepared.input_quality_projection.production_ready = result.Value().input_quality.production_ready;
        prepared.input_quality_projection.summary = result.Value().input_quality.summary;
        prepared.input_quality_projection.primary_code = result.Value().input_quality.primary_code;
        prepared.input_quality_projection.warning_codes = result.Value().input_quality.warning_codes;
        prepared.input_quality_projection.error_codes = result.Value().input_quality.error_codes;
        prepared.input_quality_projection.resolved_units = result.Value().input_quality.resolved_units;
        prepared.input_quality_projection.resolved_unit_scale = result.Value().input_quality.resolved_unit_scale;
        return Result<PreparedPlanningInput>::Success(std::move(prepared));
    }

private:
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> service_;
};

}  // namespace

std::shared_ptr<IProcessPathBuildPort> AdaptProcessPathFacade(
    std::shared_ptr<Siligen::Application::Services::ProcessPath::ProcessPathFacade> facade) {
    return std::make_shared<ProcessPathFacadePortAdapter>(std::move(facade));
}

std::shared_ptr<IMotionPlanningPort> AdaptMotionPlanningFacade(
    std::shared_ptr<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade> facade) {
    return std::make_shared<MotionPlanningFacadePortAdapter>(std::move(facade));
}

std::shared_ptr<IPlanningInputPreparationPort> AdaptDxfPreparationService(
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> service) {
    return std::make_shared<DxfPreparationPortAdapter>(std::move(service));
}

}  // namespace Siligen::Application::Ports::Dispensing
