#include "dispense_packaging/application/usecases/dispensing/PlanningPortAdapters.h"

#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace Siligen::Application::Ports::Dispensing {

namespace {

std::string HexEncodeUint64(std::uint64_t value) {
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << value;
    return oss.str();
}

std::uint64_t Fnv1a64(const std::string& text) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : text) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ULL;
    }
    return hash;
}

Siligen::Engineering::Contracts::DxfValidationReport BuildValidationReport(
    const Siligen::Application::Services::DXF::ImportDiagnosticsSummary& diagnostics,
    const PlanningInputPreparationRequest& request) {
    Siligen::Engineering::Contracts::DxfValidationReport report;
    report.stage_id = "S2";
    report.owner_module = "M2";
    report.source_ref = request.source_ref;
    report.source_hash = request.source_hash;
    report.result_classification = diagnostics.result_classification;
    report.preview_ready = diagnostics.preview_ready;
    report.production_ready = diagnostics.production_ready;
    report.summary = diagnostics.summary;
    report.primary_code = diagnostics.primary_code;
    report.warning_codes = diagnostics.warning_codes;
    report.error_codes = diagnostics.error_codes;
    report.resolved_units = diagnostics.resolved_units;
    report.resolved_unit_scale = diagnostics.resolved_unit_scale;
    report.gate_result = Siligen::Engineering::Contracts::ResolveGateResult(
        diagnostics.production_ready,
        diagnostics.warning_codes,
        diagnostics.error_codes);
    return report;
}

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
        prepared.canonical_geometry_ref = "canonical-geometry:" + HexEncodeUint64(
            Fnv1a64(request.source_ref + "|" + request.source_hash + "|" + prepared.prepared_path));
        prepared.validation_report = BuildValidationReport(result.Value().import_diagnostics, request);
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
