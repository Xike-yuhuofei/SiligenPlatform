#include "application/usecases/redundancy/RedundancyGovernanceUseCases.h"

#include "shared/types/Error.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Siligen::Application::UseCases::Redundancy {

using Siligen::Domain::System::Redundancy::CandidateStatus;
using Siligen::Domain::System::Redundancy::DecisionAction;
using Siligen::Domain::System::Redundancy::DecisionLogRecord;
using Siligen::Domain::System::Redundancy::Ports::CandidateTransitionRequest;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

namespace {

DecisionAction DecisionActionFromStatus(CandidateStatus status) {
    switch (status) {
        case CandidateStatus::Deprecated:
            return DecisionAction::Deprecate;
        case CandidateStatus::Removed:
            return DecisionAction::Remove;
        case CandidateStatus::RolledBack:
            return DecisionAction::Rollback;
        default:
            return DecisionAction::Approve;
    }
}

std::tm ToUtcTm(std::time_t now) {
    std::tm tm {};
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    return tm;
}

}  // namespace

RedundancyGovernanceUseCases::RedundancyGovernanceUseCases(
    std::shared_ptr<Siligen::Domain::System::Redundancy::Ports::IRedundancyRepositoryPort> repository,
    std::shared_ptr<Siligen::Application::Services::Redundancy::CandidateScoringService> scoring_service)
    : repository_(std::move(repository)),
      scoring_service_(std::move(scoring_service)) {
    if (!scoring_service_) {
        scoring_service_ = std::make_shared<Siligen::Application::Services::Redundancy::CandidateScoringService>();
    }
}

Result<BatchWriteSummary> RedundancyGovernanceUseCases::RegisterEntities(const RegisterEntitiesRequest& request) const {
    if (!repository_) {
        return Result<BatchWriteSummary>::Failure(
            Error(ErrorCode::REPOSITORY_NOT_AVAILABLE, "Repository not initialized", "RedundancyGovernanceUseCases"));
    }
    return repository_->UpsertEntities(request.entities);
}

Result<BatchWriteSummary> RedundancyGovernanceUseCases::ReportStaticEvidence(const ReportStaticEvidenceRequest& request) const {
    if (!repository_) {
        return Result<BatchWriteSummary>::Failure(
            Error(ErrorCode::REPOSITORY_NOT_AVAILABLE, "Repository not initialized", "RedundancyGovernanceUseCases"));
    }
    return repository_->AppendStaticEvidence(request.records);
}

Result<BatchWriteSummary> RedundancyGovernanceUseCases::ReportRuntimeEvidence(const ReportRuntimeEvidenceRequest& request) const {
    if (!repository_) {
        return Result<BatchWriteSummary>::Failure(
            Error(ErrorCode::REPOSITORY_NOT_AVAILABLE, "Repository not initialized", "RedundancyGovernanceUseCases"));
    }
    return repository_->AppendRuntimeEvidence(request.records);
}

Result<ComputeCandidatesResponse> RedundancyGovernanceUseCases::ComputeCandidates(const ComputeCandidatesRequest& request) const {
    if (!request.Validate()) {
        return Result<ComputeCandidatesResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid compute request", "RedundancyGovernanceUseCases"));
    }
    if (!repository_ || !scoring_service_) {
        return Result<ComputeCandidatesResponse>::Failure(
            Error(ErrorCode::REPOSITORY_NOT_AVAILABLE, "Repository or scoring service not initialized", "RedundancyGovernanceUseCases"));
    }

    auto entities_result = repository_->ListEntities(request.module);
    if (entities_result.IsError()) {
        return Result<ComputeCandidatesResponse>::Failure(entities_result.GetError());
    }

    auto static_result = repository_->ListStaticEvidence(request.module);
    if (static_result.IsError()) {
        return Result<ComputeCandidatesResponse>::Failure(static_result.GetError());
    }

    auto runtime_result = repository_->ListRuntimeEvidence(request.module, request.window_start, request.window_end);
    if (runtime_result.IsError()) {
        return Result<ComputeCandidatesResponse>::Failure(runtime_result.GetError());
    }

    const auto computed_at = NowIso8601Utc();
    const auto snapshot_id = BuildSnapshotId(request.module, request.policy_version, computed_at);
    auto candidates_result = scoring_service_->ComputeCandidates(entities_result.Value(),
                                                                 static_result.Value(),
                                                                 runtime_result.Value(),
                                                                 snapshot_id,
                                                                 request.policy_version,
                                                                 computed_at);
    if (candidates_result.IsError()) {
        return Result<ComputeCandidatesResponse>::Failure(candidates_result.GetError());
    }

    auto upsert_result = repository_->UpsertCandidates(candidates_result.Value());
    if (upsert_result.IsError()) {
        return Result<ComputeCandidatesResponse>::Failure(upsert_result.GetError());
    }

    ComputeCandidatesResponse response;
    response.snapshot_id = snapshot_id;
    response.generated_candidates = upsert_result.Value().upserted;
    return Result<ComputeCandidatesResponse>::Success(response);
}

Result<QueryCandidatesResponse> RedundancyGovernanceUseCases::QueryCandidates(const QueryCandidatesRequest& request) const {
    if (!repository_) {
        return Result<QueryCandidatesResponse>::Failure(
            Error(ErrorCode::REPOSITORY_NOT_AVAILABLE, "Repository not initialized", "RedundancyGovernanceUseCases"));
    }
    auto candidates_result = repository_->ListCandidates(request.filter);
    if (candidates_result.IsError()) {
        return Result<QueryCandidatesResponse>::Failure(candidates_result.GetError());
    }
    QueryCandidatesResponse response;
    response.total = candidates_result.Value().size();
    response.items = candidates_result.Value();
    return Result<QueryCandidatesResponse>::Success(std::move(response));
}

Result<TransitionCandidateResponse> RedundancyGovernanceUseCases::TransitionCandidate(const TransitionCandidateRequest& request) const {
    if (!request.Validate()) {
        return Result<TransitionCandidateResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid transition request", "RedundancyGovernanceUseCases"));
    }
    if (!repository_) {
        return Result<TransitionCandidateResponse>::Failure(
            Error(ErrorCode::REPOSITORY_NOT_AVAILABLE, "Repository not initialized", "RedundancyGovernanceUseCases"));
    }
    if (!IsTransitionAllowed(request.from_status, request.to_status)) {
        return Result<TransitionCandidateResponse>::Failure(
            Error(ErrorCode::INVALID_STATE, "Invalid candidate state transition", "RedundancyGovernanceUseCases"));
    }

    const auto transitioned_at = request.transitioned_at.empty() ? NowIso8601Utc() : request.transitioned_at;
    CandidateTransitionRequest repository_request;
    repository_request.candidate_id = request.candidate_id;
    repository_request.from_status = request.from_status;
    repository_request.to_status = request.to_status;
    repository_request.actor = request.actor;
    repository_request.reason = request.reason;
    repository_request.ticket = request.ticket;
    repository_request.transitioned_at = transitioned_at;

    auto transition_result = repository_->TransitionCandidateStatus(repository_request);
    if (transition_result.IsError()) {
        return Result<TransitionCandidateResponse>::Failure(transition_result.GetError());
    }

    DecisionLogRecord decision_record;
    decision_record.decision_id = request.decision_id;
    decision_record.candidate_id = request.candidate_id;
    decision_record.action = DecisionActionFromStatus(request.to_status);
    decision_record.actor = request.actor;
    decision_record.reason = request.reason;
    decision_record.evidence_snapshot_json = request.evidence_snapshot_json.empty() ? "{}" : request.evidence_snapshot_json;
    decision_record.ticket = request.ticket;
    decision_record.created_at = transitioned_at;

    auto decision_result = repository_->AppendDecisionLog(decision_record);
    if (decision_result.IsError()) {
        CandidateTransitionRequest rollback_request;
        rollback_request.candidate_id = request.candidate_id;
        rollback_request.from_status = request.to_status;
        rollback_request.to_status = request.from_status;
        rollback_request.actor = request.actor;
        rollback_request.reason = "rollback_due_to_decision_log_failure";
        rollback_request.ticket = request.ticket;
        rollback_request.transitioned_at = transitioned_at;
        auto rollback_result = repository_->TransitionCandidateStatus(rollback_request);
        if (rollback_result.IsError()) {
            return Result<TransitionCandidateResponse>::Failure(
                Error(
                    rollback_result.GetError().GetCode(),
                    std::string("failure_stage=rollback_transition;decision_failure_code=") +
                        std::to_string(static_cast<int>(decision_result.GetError().GetCode())) +
                        ";decision_failure_message=" + decision_result.GetError().GetMessage() +
                        ";rollback_failure_code=" + std::to_string(static_cast<int>(rollback_result.GetError().GetCode())) +
                        ";rollback_failure_message=" + rollback_result.GetError().GetMessage(),
                    "RedundancyGovernanceUseCases::TransitionCandidate"));
        }
        return Result<TransitionCandidateResponse>::Failure(
            Error(
                decision_result.GetError().GetCode(),
                std::string("failure_stage=append_decision_log;failure_code=") +
                    std::to_string(static_cast<int>(decision_result.GetError().GetCode())) +
                    ";failure_message=" + decision_result.GetError().GetMessage() +
                    ";rollback=success",
                "RedundancyGovernanceUseCases::TransitionCandidate"));
    }

    TransitionCandidateResponse response;
    response.candidate_id = request.candidate_id;
    response.status = request.to_status;
    response.transitioned_at = transitioned_at;
    return Result<TransitionCandidateResponse>::Success(response);
}

Result<BatchWriteSummary> RedundancyGovernanceUseCases::AppendDecisionLog(const AppendDecisionLogRequest& request) const {
    if (!repository_) {
        return Result<BatchWriteSummary>::Failure(
            Error(ErrorCode::REPOSITORY_NOT_AVAILABLE, "Repository not initialized", "RedundancyGovernanceUseCases"));
    }
    return repository_->AppendDecisionLog(request.record);
}

bool RedundancyGovernanceUseCases::IsTransitionAllowed(CandidateStatus from, CandidateStatus to) {
    if (from == CandidateStatus::New && to == CandidateStatus::Reviewed) {
        return true;
    }
    if (from == CandidateStatus::Reviewed && to == CandidateStatus::Deprecated) {
        return true;
    }
    if (from == CandidateStatus::Deprecated && to == CandidateStatus::Observed) {
        return true;
    }
    if (from == CandidateStatus::Observed && to == CandidateStatus::Removed) {
        return true;
    }
    if ((from == CandidateStatus::Removed || from == CandidateStatus::Deprecated || from == CandidateStatus::Observed) &&
        to == CandidateStatus::RolledBack) {
        return true;
    }
    return false;
}

std::string RedundancyGovernanceUseCases::BuildSnapshotId(const std::string& module,
                                                          const std::string& policy_version,
                                                          const std::string& timestamp) {
    std::string module_token = module.empty() ? "all" : module;
    return "snap_" + module_token + "_" + policy_version + "_" + timestamp;
}

std::string RedundancyGovernanceUseCases::NowIso8601Utc() {
    const auto now_tp = std::chrono::system_clock::now();
    const auto now = std::chrono::system_clock::to_time_t(now_tp);
    const auto utc_tm = ToUtcTm(now);
    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

}  // namespace Siligen::Application::UseCases::Redundancy
