#include "application/services/redundancy/CandidateScoringService.h"

#include "shared/types/Error.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace Siligen::Application::Services::Redundancy {

using Siligen::Domain::System::Redundancy::CandidateAction;
using Siligen::Domain::System::Redundancy::CandidatePriority;
using Siligen::Domain::System::Redundancy::CandidateRecord;
using Siligen::Domain::System::Redundancy::CandidateStatus;
using Siligen::Domain::System::Redundancy::CodeEntity;
using Siligen::Domain::System::Redundancy::RuntimeEvidenceRecord;
using Siligen::Domain::System::Redundancy::StaticEvidenceRecord;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

double ClampScore(double value) {
    return std::max(0.0, std::min(100.0, value));
}

double ModuleRiskBase(const std::string& module) {
    if (module == "safety" || module == "motion" || module == "machine") {
        return 80.0;
    }
    if (module == "dispensing" || module == "process" || module == "planning") {
        return 70.0;
    }
    if (module == "recipes" || module == "diagnostics" || module == "configuration") {
        return 60.0;
    }
    if (module == "application") {
        return 50.0;
    }
    if (module == "tests") {
        return 10.0;
    }
    return 60.0;
}

std::uint64_t Fnv1a64(const std::string& input) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto ch : input) {
        hash ^= static_cast<std::uint8_t>(ch);
        hash *= 1099511628211ULL;
    }
    return hash;
}

double ComputeRuntimeZeroHitFactor(std::int64_t total_hits, bool has_runtime_record) {
    if (!has_runtime_record) {
        return 0.0;
    }
    if (total_hits == 0) {
        return 1.0;
    }
    return std::max(0.0, 1.0 - (std::log10(static_cast<double>(total_hits) + 1.0) / 3.0));
}

double ComputeTransitionPenalty(std::int64_t total_hits, bool has_runtime_record) {
    if (!has_runtime_record) {
        return 15.0;
    }
    if (total_hits > 0) {
        return 10.0;
    }
    return 0.0;
}

CandidatePriority ComputePriority(double redundancy_score, double deletion_risk_score) {
    if (redundancy_score >= 80.0 && deletion_risk_score <= 40.0) {
        return CandidatePriority::P0;
    }
    if (redundancy_score >= 70.0 && deletion_risk_score <= 60.0) {
        return CandidatePriority::P1;
    }
    if (redundancy_score >= 50.0) {
        return CandidatePriority::P2;
    }
    return CandidatePriority::P3;
}

CandidateAction ComputeRecommendedAction(double redundancy_score, double deletion_risk_score) {
    if (redundancy_score >= 80.0 && deletion_risk_score <= 40.0) {
        return CandidateAction::Remove;
    }
    if (redundancy_score >= 60.0) {
        return CandidateAction::Deprecate;
    }
    if (redundancy_score >= 40.0) {
        return CandidateAction::Observe;
    }
    return CandidateAction::Hold;
}

}  // namespace

std::string CandidateScoringService::MakeCandidateId(const std::string& entity_id,
                                                     const std::string& module,
                                                     const std::string& snapshot_id,
                                                     const std::string& policy_version) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16)
        << Fnv1a64(entity_id + "|" + module + "|" + snapshot_id + "|" + policy_version);
    return "cand_" + oss.str();
}

Result<std::vector<CandidateRecord>> CandidateScoringService::ComputeCandidates(
    const std::vector<CodeEntity>& entities,
    const std::vector<StaticEvidenceRecord>& static_evidence,
    const std::vector<RuntimeEvidenceRecord>& runtime_evidence,
    const std::string& snapshot_id,
    const std::string& policy_version,
    const std::string& computed_at) const {
    if (snapshot_id.empty() || policy_version.empty() || computed_at.empty()) {
        return Result<std::vector<CandidateRecord>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "snapshot_id/policy_version/computed_at cannot be empty", "CandidateScoringService"));
    }

    std::unordered_map<std::string, double> static_strength_map;
    for (const auto& record : static_evidence) {
        const auto it = static_strength_map.find(record.entity_id);
        if (it == static_strength_map.end()) {
            static_strength_map[record.entity_id] = record.signal_strength;
        } else {
            static_strength_map[record.entity_id] = std::max(it->second, record.signal_strength);
        }
    }

    struct RuntimeAgg {
        std::int64_t total_hits{0};
        bool has_record{false};
    };

    std::unordered_map<std::string, RuntimeAgg> runtime_map;
    for (const auto& record : runtime_evidence) {
        auto& agg = runtime_map[record.entity_id];
        agg.total_hits += record.hit_count;
        agg.has_record = true;
    }

    std::vector<CandidateRecord> candidates;
    candidates.reserve(entities.size());
    for (const auto& entity : entities) {
        if (!entity.Validate()) {
            continue;
        }

        const auto static_strength_it = static_strength_map.find(entity.entity_id);
        const double static_strength = static_strength_it == static_strength_map.end() ? 0.0 : static_strength_it->second;

        const auto runtime_it = runtime_map.find(entity.entity_id);
        const std::int64_t total_hits = runtime_it == runtime_map.end() ? 0 : runtime_it->second.total_hits;
        const bool has_runtime_record = runtime_it != runtime_map.end() && runtime_it->second.has_record;

        const double runtime_zero_hit_factor = ComputeRuntimeZeroHitFactor(total_hits, has_runtime_record);
        const double redundancy_score = ClampScore(100.0 * (0.6 * static_strength + 0.4 * runtime_zero_hit_factor));

        const double transition_penalty = ComputeTransitionPenalty(total_hits, has_runtime_record);
        const double deletion_risk_score = ClampScore(ModuleRiskBase(entity.module) + transition_penalty);

        CandidateRecord record;
        record.candidate_id = MakeCandidateId(entity.entity_id, entity.module, snapshot_id, policy_version);
        record.entity_id = entity.entity_id;
        record.redundancy_score = redundancy_score;
        record.deletion_risk_score = deletion_risk_score;
        record.priority = ComputePriority(redundancy_score, deletion_risk_score);
        record.recommended_action = ComputeRecommendedAction(redundancy_score, deletion_risk_score);
        record.status = CandidateStatus::New;
        record.policy_version = policy_version;
        record.snapshot_id = snapshot_id;
        record.computed_at = computed_at;
        candidates.push_back(record);
    }

    return Result<std::vector<CandidateRecord>>::Success(std::move(candidates));
}

}  // namespace Siligen::Application::Services::Redundancy
