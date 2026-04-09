#pragma once

#include "CandidateScoringService.h"
#include "domain/recovery/redundancy/ports/IRedundancyRepositoryPort.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Redundancy {

using Siligen::Domain::Recovery::Redundancy::CandidateRecord;
using Siligen::Domain::Recovery::Redundancy::CandidateStatus;
using Siligen::Domain::Recovery::Redundancy::CodeEntity;
using Siligen::Domain::Recovery::Redundancy::DecisionLogRecord;
using Siligen::Domain::Recovery::Redundancy::RuntimeEvidenceRecord;
using Siligen::Domain::Recovery::Redundancy::StaticEvidenceRecord;
using Siligen::Domain::Recovery::Redundancy::Ports::BatchWriteSummary;
using Siligen::Domain::Recovery::Redundancy::Ports::CandidateQueryFilter;
using Siligen::Shared::Types::Result;

struct RegisterEntitiesRequest {
    std::vector<CodeEntity> entities;
};

struct ReportStaticEvidenceRequest {
    std::vector<StaticEvidenceRecord> records;
};

struct ReportRuntimeEvidenceRequest {
    std::vector<RuntimeEvidenceRecord> records;
};

struct ComputeCandidatesRequest {
    std::string module;
    std::string window_start;
    std::string window_end;
    std::string policy_version;
    bool force_recompute{false};

    bool Validate() const {
        return !policy_version.empty();
    }
};

struct ComputeCandidatesResponse {
    std::string snapshot_id;
    std::size_t generated_candidates{0};
};

struct QueryCandidatesRequest {
    CandidateQueryFilter filter;
};

struct QueryCandidatesResponse {
    std::size_t total{0};
    std::vector<CandidateRecord> items;
};

struct TransitionCandidateRequest {
    std::string candidate_id;
    CandidateStatus from_status{CandidateStatus::New};
    CandidateStatus to_status{CandidateStatus::Reviewed};
    std::string actor;
    std::string reason;
    std::string ticket;
    std::string decision_id;
    std::string evidence_snapshot_json;
    std::string transitioned_at;

    bool Validate() const {
        return !candidate_id.empty() && !actor.empty() && !reason.empty() && !decision_id.empty();
    }
};

struct TransitionCandidateResponse {
    std::string candidate_id;
    CandidateStatus status{CandidateStatus::New};
    std::string transitioned_at;
};

struct AppendDecisionLogRequest {
    DecisionLogRecord record;
};

class RedundancyGovernanceUseCases {
   public:
    explicit RedundancyGovernanceUseCases(
        std::shared_ptr<Siligen::Domain::Recovery::Redundancy::Ports::IRedundancyRepositoryPort> repository,
        std::shared_ptr<Siligen::Application::Services::Redundancy::CandidateScoringService> scoring_service = nullptr);

    Result<BatchWriteSummary> RegisterEntities(const RegisterEntitiesRequest& request) const;
    Result<BatchWriteSummary> ReportStaticEvidence(const ReportStaticEvidenceRequest& request) const;
    Result<BatchWriteSummary> ReportRuntimeEvidence(const ReportRuntimeEvidenceRequest& request) const;
    Result<ComputeCandidatesResponse> ComputeCandidates(const ComputeCandidatesRequest& request) const;
    Result<QueryCandidatesResponse> QueryCandidates(const QueryCandidatesRequest& request) const;
    Result<TransitionCandidateResponse> TransitionCandidate(const TransitionCandidateRequest& request) const;
    Result<BatchWriteSummary> AppendDecisionLog(const AppendDecisionLogRequest& request) const;

   private:
    static bool IsTransitionAllowed(CandidateStatus from, CandidateStatus to);
    static std::string BuildSnapshotId(const std::string& module, const std::string& policy_version, const std::string& timestamp);
    static std::string NowIso8601Utc();

    std::shared_ptr<Siligen::Domain::Recovery::Redundancy::Ports::IRedundancyRepositoryPort> repository_;
    std::shared_ptr<Siligen::Application::Services::Redundancy::CandidateScoringService> scoring_service_;
};

}  // namespace Siligen::Application::UseCases::Redundancy
