#include "application/usecases/redundancy/RedundancyGovernanceUseCases.h"
#include "infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

namespace {

using Siligen::Application::UseCases::Redundancy::ComputeCandidatesRequest;
using Siligen::Application::UseCases::Redundancy::QueryCandidatesRequest;
using Siligen::Application::UseCases::Redundancy::RedundancyGovernanceUseCases;
using Siligen::Application::UseCases::Redundancy::RegisterEntitiesRequest;
using Siligen::Application::UseCases::Redundancy::ReportRuntimeEvidenceRequest;
using Siligen::Application::UseCases::Redundancy::ReportStaticEvidenceRequest;
using Siligen::Application::UseCases::Redundancy::TransitionCandidateRequest;
using Siligen::Domain::Recovery::Redundancy::CandidateStatus;
using Siligen::Domain::Recovery::Redundancy::CandidateRecord;
using Siligen::Domain::Recovery::Redundancy::CodeEntity;
using Siligen::Domain::Recovery::Redundancy::DecisionLogRecord;
using Siligen::Domain::Recovery::Redundancy::EntityType;
using Siligen::Domain::Recovery::Redundancy::RuntimeEnvironment;
using Siligen::Domain::Recovery::Redundancy::RuntimeEvidenceRecord;
using Siligen::Domain::Recovery::Redundancy::Ports::BatchWriteSummary;
using Siligen::Domain::Recovery::Redundancy::Ports::CandidateQueryFilter;
using Siligen::Domain::Recovery::Redundancy::Ports::CandidateTransitionRequest;
using Siligen::Domain::Recovery::Redundancy::Ports::IRedundancyRepositoryPort;
using Siligen::Domain::Recovery::Redundancy::SourceLanguage;
using Siligen::Domain::Recovery::Redundancy::StaticEvidenceRecord;
using Siligen::Domain::Recovery::Redundancy::StaticSignalType;
using Siligen::Infrastructure::Adapters::Redundancy::JsonRedundancyRepositoryAdapter;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

std::filesystem::path BuildTempDir() {
    return std::filesystem::temp_directory_path() / "siligen_redundancy_usecase_test";
}

CodeEntity BuildEntity() {
    CodeEntity entity;
    entity.entity_id = "ent_usecase";
    entity.entity_type = EntityType::Function;
    entity.path = "src/usecase.cpp";
    entity.module = "safety";
    entity.language = SourceLanguage::Cpp;
    return entity;
}

StaticEvidenceRecord BuildStaticEvidence() {
    StaticEvidenceRecord record;
    record.evidence_id = "se_usecase";
    record.entity_id = "ent_usecase";
    record.rule_id = "rule.unreferenced";
    record.rule_version = "1";
    record.signal_type = StaticSignalType::Unreferenced;
    record.signal_strength = 1.0;
    record.details_json = "{}";
    record.collected_at = "2026-03-21T00:00:00Z";
    return record;
}

RuntimeEvidenceRecord BuildRuntimeEvidence() {
    RuntimeEvidenceRecord record;
    record.evidence_id = "re_usecase";
    record.entity_id = "ent_usecase";
    record.environment = RuntimeEnvironment::Test;
    record.window_start = "2026-03-01T00:00:00Z";
    record.window_end = "2026-03-21T00:00:00Z";
    record.hit_count = 0;
    record.source = "unit";
    record.collected_at = "2026-03-21T00:00:00Z";
    return record;
}

class RollbackFailingRepository final : public IRedundancyRepositoryPort {
   public:
    Result<BatchWriteSummary> UpsertEntities(const std::vector<CodeEntity>&) override {
        return Result<BatchWriteSummary>::Success(BatchWriteSummary{});
    }
    Result<BatchWriteSummary> AppendStaticEvidence(const std::vector<StaticEvidenceRecord>&) override {
        return Result<BatchWriteSummary>::Success(BatchWriteSummary{});
    }
    Result<BatchWriteSummary> AppendRuntimeEvidence(const std::vector<RuntimeEvidenceRecord>&) override {
        return Result<BatchWriteSummary>::Success(BatchWriteSummary{});
    }
    Result<BatchWriteSummary> UpsertCandidates(const std::vector<CandidateRecord>&) override {
        return Result<BatchWriteSummary>::Success(BatchWriteSummary{});
    }
    Result<BatchWriteSummary> AppendDecisionLog(const DecisionLogRecord&) override {
        return Result<BatchWriteSummary>::Failure(
            Siligen::Shared::Types::Error(
                ErrorCode::DUPLICATE_DECISION_ID,
                "duplicate decision id",
                "RollbackFailingRepository"));
    }

    Result<std::vector<CodeEntity>> ListEntities(const std::string&) const override {
        return Result<std::vector<CodeEntity>>::Success({});
    }
    Result<std::vector<StaticEvidenceRecord>> ListStaticEvidence(const std::string&) const override {
        return Result<std::vector<StaticEvidenceRecord>>::Success({});
    }
    Result<std::vector<RuntimeEvidenceRecord>> ListRuntimeEvidence(const std::string&,
                                                                   const std::string&,
                                                                   const std::string&) const override {
        return Result<std::vector<RuntimeEvidenceRecord>>::Success({});
    }
    Result<std::vector<CandidateRecord>> ListCandidates(const CandidateQueryFilter&) const override {
        return Result<std::vector<CandidateRecord>>::Success({});
    }
    Result<CandidateRecord> GetCandidateById(const std::string&) const override {
        return Result<CandidateRecord>::Failure(
            Siligen::Shared::Types::Error(
                ErrorCode::NOT_FOUND,
                "candidate not found",
                "RollbackFailingRepository"));
    }

    Result<void> TransitionCandidateStatus(const CandidateTransitionRequest&) override {
        ++transition_calls;
        if (transition_calls == 1) {
            return Result<void>::Success();
        }
        return Result<void>::Failure(
            Siligen::Shared::Types::Error(
                ErrorCode::DATABASE_WRITE_FAILED,
                "rollback failed",
                "RollbackFailingRepository"));
    }

    int transition_calls = 0;
};

}  // namespace

TEST(RedundancyGovernanceUseCasesTest, EndToEndFlowFromIngestionToTransition) {
    const auto temp_dir = BuildTempDir();
    std::filesystem::remove_all(temp_dir);

    auto repository = std::make_shared<JsonRedundancyRepositoryAdapter>(temp_dir);
    RedundancyGovernanceUseCases usecases(repository);

    RegisterEntitiesRequest register_request;
    register_request.entities = {BuildEntity()};
    auto register_result = usecases.RegisterEntities(register_request);
    ASSERT_TRUE(register_result.IsSuccess()) << register_result.GetError().ToString();

    ReportStaticEvidenceRequest static_request;
    static_request.records = {BuildStaticEvidence()};
    auto static_result = usecases.ReportStaticEvidence(static_request);
    ASSERT_TRUE(static_result.IsSuccess()) << static_result.GetError().ToString();

    ReportRuntimeEvidenceRequest runtime_request;
    runtime_request.records = {BuildRuntimeEvidence()};
    auto runtime_result = usecases.ReportRuntimeEvidence(runtime_request);
    ASSERT_TRUE(runtime_result.IsSuccess()) << runtime_result.GetError().ToString();

    ComputeCandidatesRequest compute_request;
    compute_request.module = "safety";
    compute_request.window_start = "2026-03-01T00:00:00Z";
    compute_request.window_end = "2026-03-21T00:00:00Z";
    compute_request.policy_version = "v1";
    auto compute_result = usecases.ComputeCandidates(compute_request);
    ASSERT_TRUE(compute_result.IsSuccess()) << compute_result.GetError().ToString();
    EXPECT_EQ(compute_result.Value().generated_candidates, 1U);

    QueryCandidatesRequest query_request;
    query_request.filter.module = "safety";
    auto query_result = usecases.QueryCandidates(query_request);
    ASSERT_TRUE(query_result.IsSuccess()) << query_result.GetError().ToString();
    ASSERT_EQ(query_result.Value().items.size(), 1U);

    TransitionCandidateRequest transition_request;
    transition_request.candidate_id = query_result.Value().items[0].candidate_id;
    transition_request.from_status = CandidateStatus::New;
    transition_request.to_status = CandidateStatus::Reviewed;
    transition_request.actor = "tester";
    transition_request.reason = "manual_review";
    transition_request.ticket = "NOISSUE";
    transition_request.decision_id = "dec_usecase";
    auto transition_result = usecases.TransitionCandidate(transition_request);
    ASSERT_TRUE(transition_result.IsSuccess()) << transition_result.GetError().ToString();
    EXPECT_EQ(transition_result.Value().status, CandidateStatus::Reviewed);

    QueryCandidatesRequest reviewed_query;
    reviewed_query.filter.module = "safety";
    reviewed_query.filter.status = CandidateStatus::Reviewed;
    auto reviewed_result = usecases.QueryCandidates(reviewed_query);
    ASSERT_TRUE(reviewed_result.IsSuccess()) << reviewed_result.GetError().ToString();
    ASSERT_EQ(reviewed_result.Value().items.size(), 1U);

    std::filesystem::remove_all(temp_dir);
}

TEST(RedundancyGovernanceUseCasesTest, TransitionRollsBackWhenDecisionIdConflictsWithDifferentPayload) {
    const auto temp_dir = BuildTempDir();
    std::filesystem::remove_all(temp_dir);

    auto repository = std::make_shared<JsonRedundancyRepositoryAdapter>(temp_dir);
    RedundancyGovernanceUseCases usecases(repository);

    RegisterEntitiesRequest register_request;
    register_request.entities = {BuildEntity()};
    ASSERT_TRUE(usecases.RegisterEntities(register_request).IsSuccess());

    ReportStaticEvidenceRequest static_request;
    static_request.records = {BuildStaticEvidence()};
    ASSERT_TRUE(usecases.ReportStaticEvidence(static_request).IsSuccess());

    ReportRuntimeEvidenceRequest runtime_request;
    runtime_request.records = {BuildRuntimeEvidence()};
    ASSERT_TRUE(usecases.ReportRuntimeEvidence(runtime_request).IsSuccess());

    ComputeCandidatesRequest compute_request;
    compute_request.module = "safety";
    compute_request.window_start = "2026-03-01T00:00:00Z";
    compute_request.window_end = "2026-03-21T00:00:00Z";
    compute_request.policy_version = "v1";
    auto compute_result = usecases.ComputeCandidates(compute_request);
    ASSERT_TRUE(compute_result.IsSuccess()) << compute_result.GetError().ToString();

    QueryCandidatesRequest query_request;
    query_request.filter.module = "safety";
    auto query_result = usecases.QueryCandidates(query_request);
    ASSERT_TRUE(query_result.IsSuccess()) << query_result.GetError().ToString();
    ASSERT_EQ(query_result.Value().items.size(), 1U);
    const auto candidate_id = query_result.Value().items[0].candidate_id;

    TransitionCandidateRequest review_request;
    review_request.candidate_id = candidate_id;
    review_request.from_status = CandidateStatus::New;
    review_request.to_status = CandidateStatus::Reviewed;
    review_request.actor = "tester";
    review_request.reason = "review";
    review_request.ticket = "NOISSUE";
    review_request.decision_id = "dup_decision";
    auto review_result = usecases.TransitionCandidate(review_request);
    ASSERT_TRUE(review_result.IsSuccess()) << review_result.GetError().ToString();

    TransitionCandidateRequest conflict_request;
    conflict_request.candidate_id = candidate_id;
    conflict_request.from_status = CandidateStatus::Reviewed;
    conflict_request.to_status = CandidateStatus::Deprecated;
    conflict_request.actor = "tester";
    conflict_request.reason = "different payload";
    conflict_request.ticket = "NOISSUE";
    conflict_request.decision_id = "dup_decision";
    auto conflict_result = usecases.TransitionCandidate(conflict_request);
    ASSERT_TRUE(conflict_result.IsError());
    EXPECT_EQ(conflict_result.GetError().GetCode(), ErrorCode::DUPLICATE_DECISION_ID);
    EXPECT_NE(conflict_result.GetError().GetMessage().find("failure_stage=append_decision_log"), std::string::npos);

    QueryCandidatesRequest reviewed_query;
    reviewed_query.filter.module = "safety";
    reviewed_query.filter.status = CandidateStatus::Reviewed;
    auto reviewed_result = usecases.QueryCandidates(reviewed_query);
    ASSERT_TRUE(reviewed_result.IsSuccess()) << reviewed_result.GetError().ToString();
    ASSERT_EQ(reviewed_result.Value().items.size(), 1U);
    EXPECT_EQ(reviewed_result.Value().items[0].candidate_id, candidate_id);

    std::filesystem::remove_all(temp_dir);
}

TEST(RedundancyGovernanceUseCasesTest, TransitionReportsRollbackStageWhenRollbackFails) {
    auto repository = std::make_shared<RollbackFailingRepository>();
    RedundancyGovernanceUseCases usecases(repository);

    TransitionCandidateRequest transition_request;
    transition_request.candidate_id = "cand-rollback";
    transition_request.from_status = CandidateStatus::New;
    transition_request.to_status = CandidateStatus::Reviewed;
    transition_request.actor = "tester";
    transition_request.reason = "review";
    transition_request.ticket = "NOISSUE";
    transition_request.decision_id = "dup_decision";

    auto result = usecases.TransitionCandidate(transition_request);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::DATABASE_WRITE_FAILED);
    EXPECT_NE(result.GetError().GetMessage().find("failure_stage=rollback_transition"), std::string::npos);
    EXPECT_NE(result.GetError().GetMessage().find("decision_failure_code=7104"), std::string::npos);
    EXPECT_EQ(repository->transition_calls, 2);
}
