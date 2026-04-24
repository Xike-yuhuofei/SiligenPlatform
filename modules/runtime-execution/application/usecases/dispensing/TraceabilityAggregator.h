#pragma once

#include "DispensingExecutionUseCase.Internal.h"

namespace Siligen::Application::UseCases::Dispensing {

class TraceabilityAggregator {
   public:
    static void AppendCycleTraceability(
        const std::shared_ptr<JobExecutionContext>& job_context,
        const std::shared_ptr<TaskExecutionContext>& task_context,
        uint32 cycle_index) {
        std::lock_guard<std::mutex> lock(job_context->mutex_);
        for (const auto& actual_item : task_context->result.actual_trace) {
            auto cycle_item = actual_item;
            cycle_item.cycle_index = cycle_index;
            job_context->actual_trace.push_back(std::move(cycle_item));
        }
        for (const auto& mismatch : task_context->result.traceability_mismatches) {
            auto cycle_mismatch = mismatch;
            cycle_mismatch.cycle_index = cycle_index;
            job_context->mismatches.push_back(std::move(cycle_mismatch));
        }

        if (task_context->result.traceability_verdict == "failed") {
            job_context->traceability_verdict = "failed";
            job_context->strict_one_to_one_proven = false;
            if (job_context->traceability_verdict_reason.empty()) {
                job_context->traceability_verdict_reason =
                    task_context->result.traceability_verdict_reason.empty()
                        ? ("traceability failed at cycle " + std::to_string(cycle_index))
                        : ("cycle " + std::to_string(cycle_index) + ": " +
                           task_context->result.traceability_verdict_reason);
            }
            return;
        }

        if (task_context->result.traceability_verdict == "insufficient_evidence") {
            if (job_context->traceability_verdict != "failed") {
                job_context->traceability_verdict = "insufficient_evidence";
            }
            job_context->strict_one_to_one_proven = false;
            if (job_context->traceability_verdict_reason.empty() ||
                job_context->traceability_verdict_reason ==
                    "strict traceability evidence is pending runtime terminal proof") {
                job_context->traceability_verdict_reason =
                    task_context->result.traceability_verdict_reason.empty()
                        ? ("strict traceability evidence remained incomplete at cycle " +
                           std::to_string(cycle_index))
                        : ("cycle " + std::to_string(cycle_index) + ": " +
                           task_context->result.traceability_verdict_reason);
            }
            return;
        }

        job_context->strict_one_to_one_proven =
            job_context->strict_one_to_one_proven && task_context->result.strict_one_to_one_proven;
    }

    static void FinalizeTerminalTraceability(const std::shared_ptr<JobExecutionContext>& context) {
        std::lock_guard<std::mutex> lock(context->mutex_);
        if (context->expected_trace.empty()) {
            return;
        }

        if (context->actual_trace.size() != context->expected_trace.size()) {
            Domain::Dispensing::ValueObjects::ProfileCompareTraceabilityMismatch mismatch;
            mismatch.cycle_index = 1U;
            mismatch.code = "actual_trace_count_mismatch";
            mismatch.message = "actual trace count does not match expected trace count at terminal state";
            context->mismatches.push_back(std::move(mismatch));
            if (context->traceability_verdict != "failed") {
                context->traceability_verdict = "insufficient_evidence";
            }
            if (context->traceability_verdict_reason.empty() ||
                context->traceability_verdict_reason ==
                    "strict traceability evidence is pending runtime terminal proof") {
                context->traceability_verdict_reason =
                    "actual trace count does not match expected trace count at terminal state";
            }
            context->strict_one_to_one_proven = false;
            return;
        }

        if (context->mismatches.empty() && context->traceability_verdict != "failed") {
            context->traceability_verdict = "passed";
            context->traceability_verdict_reason.clear();
            context->strict_one_to_one_proven = true;
            return;
        }

        if (context->traceability_verdict == "failed") {
            if (context->traceability_verdict_reason.empty()) {
                context->traceability_verdict_reason = "traceability mismatches detected";
            }
            context->strict_one_to_one_proven = false;
            return;
        }

        if (context->traceability_verdict_reason.empty()) {
            context->traceability_verdict_reason = "strict traceability evidence remains incomplete";
        }
        context->traceability_verdict = "insufficient_evidence";
        context->strict_one_to_one_proven = false;
    }
};

}  // namespace Siligen::Application::UseCases::Dispensing
