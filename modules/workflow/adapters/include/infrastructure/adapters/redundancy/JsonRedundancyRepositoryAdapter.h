#pragma once

#include "domain/recovery/redundancy/ports/IRedundancyRepositoryPort.h"

#include <filesystem>
#include <mutex>

namespace Siligen::Infrastructure::Adapters::Redundancy {

class JsonRedundancyRepositoryAdapter final : public Siligen::Domain::Recovery::Redundancy::Ports::IRedundancyRepositoryPort {
   public:
    explicit JsonRedundancyRepositoryAdapter(std::filesystem::path base_dir = {});
    ~JsonRedundancyRepositoryAdapter() override = default;

    Siligen::Shared::Types::Result<Siligen::Domain::Recovery::Redundancy::Ports::BatchWriteSummary> UpsertEntities(
        const std::vector<Siligen::Domain::Recovery::Redundancy::CodeEntity>& entities) override;

    Siligen::Shared::Types::Result<Siligen::Domain::Recovery::Redundancy::Ports::BatchWriteSummary> AppendStaticEvidence(
        const std::vector<Siligen::Domain::Recovery::Redundancy::StaticEvidenceRecord>& records) override;

    Siligen::Shared::Types::Result<Siligen::Domain::Recovery::Redundancy::Ports::BatchWriteSummary> AppendRuntimeEvidence(
        const std::vector<Siligen::Domain::Recovery::Redundancy::RuntimeEvidenceRecord>& records) override;

    Siligen::Shared::Types::Result<Siligen::Domain::Recovery::Redundancy::Ports::BatchWriteSummary> UpsertCandidates(
        const std::vector<Siligen::Domain::Recovery::Redundancy::CandidateRecord>& records) override;

    Siligen::Shared::Types::Result<Siligen::Domain::Recovery::Redundancy::Ports::BatchWriteSummary> AppendDecisionLog(
        const Siligen::Domain::Recovery::Redundancy::DecisionLogRecord& record) override;

    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Recovery::Redundancy::CodeEntity>> ListEntities(
        const std::string& module) const override;

    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Recovery::Redundancy::StaticEvidenceRecord>> ListStaticEvidence(
        const std::string& module) const override;

    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Recovery::Redundancy::RuntimeEvidenceRecord>> ListRuntimeEvidence(
        const std::string& module,
        const std::string& window_start,
        const std::string& window_end) const override;

    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Recovery::Redundancy::CandidateRecord>> ListCandidates(
        const Siligen::Domain::Recovery::Redundancy::Ports::CandidateQueryFilter& filter) const override;

    Siligen::Shared::Types::Result<Siligen::Domain::Recovery::Redundancy::CandidateRecord> GetCandidateById(
        const std::string& candidate_id) const override;

    Siligen::Shared::Types::Result<void> TransitionCandidateStatus(
        const Siligen::Domain::Recovery::Redundancy::Ports::CandidateTransitionRequest& request) override;

   private:
    std::filesystem::path base_dir_;
    std::filesystem::path entities_file_;
    std::filesystem::path static_evidence_file_;
    std::filesystem::path runtime_evidence_file_;
    std::filesystem::path candidates_file_;
    std::filesystem::path decision_logs_file_;
    mutable std::mutex mutex_;

    Siligen::Shared::Types::Result<void> EnsureStoreInitialized() const;
};

}  // namespace Siligen::Infrastructure::Adapters::Redundancy
