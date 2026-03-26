#pragma once

#include "domain/system/redundancy/RedundancyTypes.h"
#include "shared/types/Result.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace Siligen::Domain::System::Redundancy::Ports {

using Siligen::Shared::Types::Result;
using Siligen::Domain::System::Redundancy::CandidateRecord;
using Siligen::Domain::System::Redundancy::CandidateStatus;
using Siligen::Domain::System::Redundancy::CodeEntity;
using Siligen::Domain::System::Redundancy::DecisionLogRecord;
using Siligen::Domain::System::Redundancy::RuntimeEvidenceRecord;
using Siligen::Domain::System::Redundancy::StaticEvidenceRecord;

struct BatchWriteSummary {
    std::size_t received{0};
    std::size_t upserted{0};
    std::size_t rejected{0};
};

struct CandidateQueryFilter {
    std::string module;
    std::optional<CandidateStatus> status;
    std::optional<std::string> priority;
    std::optional<double> min_redundancy_score;
    std::optional<double> max_deletion_risk_score;
    std::size_t limit{200};
    std::size_t offset{0};
};

struct CandidateTransitionRequest {
    std::string candidate_id;
    CandidateStatus from_status{CandidateStatus::New};
    CandidateStatus to_status{CandidateStatus::Reviewed};
    std::string actor;
    std::string reason;
    std::string ticket;
    std::string transitioned_at;

    bool Validate() const {
        return !candidate_id.empty() && !actor.empty() && !reason.empty() && !transitioned_at.empty();
    }
};

class IRedundancyRepositoryPort {
   public:
    virtual ~IRedundancyRepositoryPort() = default;

    virtual Result<BatchWriteSummary> UpsertEntities(const std::vector<CodeEntity>& entities) = 0;
    virtual Result<BatchWriteSummary> AppendStaticEvidence(const std::vector<StaticEvidenceRecord>& records) = 0;
    virtual Result<BatchWriteSummary> AppendRuntimeEvidence(const std::vector<RuntimeEvidenceRecord>& records) = 0;
    virtual Result<BatchWriteSummary> UpsertCandidates(const std::vector<CandidateRecord>& records) = 0;
    virtual Result<BatchWriteSummary> AppendDecisionLog(const DecisionLogRecord& record) = 0;

    virtual Result<std::vector<CodeEntity>> ListEntities(const std::string& module) const = 0;
    virtual Result<std::vector<StaticEvidenceRecord>> ListStaticEvidence(const std::string& module) const = 0;
    virtual Result<std::vector<RuntimeEvidenceRecord>> ListRuntimeEvidence(const std::string& module,
                                                                           const std::string& window_start,
                                                                           const std::string& window_end) const = 0;

    virtual Result<std::vector<CandidateRecord>> ListCandidates(const CandidateQueryFilter& filter) const = 0;
    virtual Result<CandidateRecord> GetCandidateById(const std::string& candidate_id) const = 0;
    virtual Result<void> TransitionCandidateStatus(const CandidateTransitionRequest& request) = 0;
};

}  // namespace Siligen::Domain::System::Redundancy::Ports
