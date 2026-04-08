#pragma once

#include "application/ports/dispensing/WorkflowExecutionPort.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace Siligen::Application::Ports::Dispensing {

namespace detail {

class RuntimeDispensingExecutionPortAdapter final : public IWorkflowExecutionPort {
public:
    explicit RuntimeDispensingExecutionPortAdapter(
        std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase> use_case)
        : use_case_(std::move(use_case)) {
        if (!use_case_) {
            throw std::invalid_argument("RuntimeDispensingExecutionPortAdapter: use_case cannot be null");
        }
    }

    Result<WorkflowJobId> StartJob(const WorkflowRuntimeStartJobRequest& request) override {
        Siligen::Application::UseCases::Dispensing::RuntimeStartJobRequest runtime_request;
        runtime_request.plan_id = request.plan_id;
        runtime_request.plan_fingerprint = request.plan_fingerprint;
        runtime_request.target_count = request.target_count;
        runtime_request.execution_request.execution_package = request.execution_request.execution_package;
        runtime_request.execution_request.source_path = request.execution_request.source_path;
        runtime_request.execution_request.use_hardware_trigger = request.execution_request.use_hardware_trigger;
        runtime_request.execution_request.dry_run = request.execution_request.dry_run;
        runtime_request.execution_request.machine_mode = request.execution_request.machine_mode;
        runtime_request.execution_request.execution_mode = request.execution_request.execution_mode;
        runtime_request.execution_request.output_policy = request.execution_request.output_policy;
        runtime_request.execution_request.max_jerk = request.execution_request.max_jerk;
        runtime_request.execution_request.arc_tolerance_mm = request.execution_request.arc_tolerance_mm;
        runtime_request.execution_request.dispensing_speed_mm_s = request.execution_request.dispensing_speed_mm_s;
        runtime_request.execution_request.dry_run_speed_mm_s = request.execution_request.dry_run_speed_mm_s;
        runtime_request.execution_request.rapid_speed_mm_s = request.execution_request.rapid_speed_mm_s;
        runtime_request.execution_request.acceleration_mm_s2 = request.execution_request.acceleration_mm_s2;
        runtime_request.execution_request.velocity_trace_enabled = request.execution_request.velocity_trace_enabled;
        runtime_request.execution_request.velocity_trace_interval_ms =
            request.execution_request.velocity_trace_interval_ms;
        runtime_request.execution_request.velocity_trace_path = request.execution_request.velocity_trace_path;
        runtime_request.execution_request.velocity_guard_enabled = request.execution_request.velocity_guard_enabled;
        runtime_request.execution_request.velocity_guard_ratio = request.execution_request.velocity_guard_ratio;
        runtime_request.execution_request.velocity_guard_abs_mm_s = request.execution_request.velocity_guard_abs_mm_s;
        runtime_request.execution_request.velocity_guard_min_expected_mm_s =
            request.execution_request.velocity_guard_min_expected_mm_s;
        runtime_request.execution_request.velocity_guard_grace_ms = request.execution_request.velocity_guard_grace_ms;
        runtime_request.execution_request.velocity_guard_interval_ms =
            request.execution_request.velocity_guard_interval_ms;
        runtime_request.execution_request.velocity_guard_max_consecutive =
            request.execution_request.velocity_guard_max_consecutive;
        runtime_request.execution_request.velocity_guard_stop_on_violation =
            request.execution_request.velocity_guard_stop_on_violation;
        return use_case_->StartJob(runtime_request);
    }

private:
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase> use_case_;
};

}  // namespace detail

inline std::shared_ptr<IWorkflowExecutionPort> AdaptRuntimeDispensingExecutionUseCase(
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase> use_case) {
    return std::make_shared<detail::RuntimeDispensingExecutionPortAdapter>(std::move(use_case));
}

}  // namespace Siligen::Application::Ports::Dispensing
