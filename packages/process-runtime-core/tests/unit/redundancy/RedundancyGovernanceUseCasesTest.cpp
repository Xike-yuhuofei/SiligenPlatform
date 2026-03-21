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
using Siligen::Domain::System::Redundancy::CandidateStatus;
using Siligen::Domain::System::Redundancy::CodeEntity;
using Siligen::Domain::System::Redundancy::EntityType;
using Siligen::Domain::System::Redundancy::RuntimeEnvironment;
using Siligen::Domain::System::Redundancy::RuntimeEvidenceRecord;
using Siligen::Domain::System::Redundancy::SourceLanguage;
using Siligen::Domain::System::Redundancy::StaticEvidenceRecord;
using Siligen::Domain::System::Redundancy::StaticSignalType;
using Siligen::Infrastructure::Adapters::Redundancy::JsonRedundancyRepositoryAdapter;

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
