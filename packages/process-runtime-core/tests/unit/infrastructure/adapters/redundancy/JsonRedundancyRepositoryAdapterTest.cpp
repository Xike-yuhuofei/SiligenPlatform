#include "infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace {

using Siligen::Domain::System::Redundancy::CandidateAction;
using Siligen::Domain::System::Redundancy::CandidatePriority;
using Siligen::Domain::System::Redundancy::CandidateRecord;
using Siligen::Domain::System::Redundancy::CandidateStatus;
using Siligen::Domain::System::Redundancy::CodeEntity;
using Siligen::Domain::System::Redundancy::EntityType;
using Siligen::Domain::System::Redundancy::Ports::CandidateQueryFilter;
using Siligen::Domain::System::Redundancy::Ports::CandidateTransitionRequest;
using Siligen::Domain::System::Redundancy::SourceLanguage;
using Siligen::Infrastructure::Adapters::Redundancy::JsonRedundancyRepositoryAdapter;

std::filesystem::path BuildTempDir() {
    return std::filesystem::temp_directory_path() / "siligen_redundancy_repo_test";
}

CodeEntity BuildEntity() {
    CodeEntity entity;
    entity.entity_id = "ent_repo";
    entity.entity_type = EntityType::Function;
    entity.path = "src/repo.cpp";
    entity.module = "application";
    entity.language = SourceLanguage::Cpp;
    return entity;
}

CandidateRecord BuildCandidate() {
    CandidateRecord record;
    record.candidate_id = "cand_repo";
    record.entity_id = "ent_repo";
    record.redundancy_score = 66.0;
    record.deletion_risk_score = 40.0;
    record.priority = CandidatePriority::P1;
    record.recommended_action = CandidateAction::Deprecate;
    record.status = CandidateStatus::New;
    record.policy_version = "v1";
    record.snapshot_id = "snap_repo";
    record.computed_at = "2026-03-21T00:00:00Z";
    return record;
}

}  // namespace

TEST(JsonRedundancyRepositoryAdapterTest, UpsertAndQueryCandidateSuccessfully) {
    const auto temp_dir = BuildTempDir();
    std::filesystem::remove_all(temp_dir);
    JsonRedundancyRepositoryAdapter adapter(temp_dir);

    auto entity_result = adapter.UpsertEntities({BuildEntity()});
    ASSERT_TRUE(entity_result.IsSuccess()) << entity_result.GetError().ToString();

    auto candidate_result = adapter.UpsertCandidates({BuildCandidate()});
    ASSERT_TRUE(candidate_result.IsSuccess()) << candidate_result.GetError().ToString();

    CandidateQueryFilter filter;
    filter.module = "application";
    auto list_result = adapter.ListCandidates(filter);
    ASSERT_TRUE(list_result.IsSuccess()) << list_result.GetError().ToString();
    ASSERT_EQ(list_result.Value().size(), 1U);
    EXPECT_EQ(list_result.Value()[0].candidate_id, "cand_repo");
}

TEST(JsonRedundancyRepositoryAdapterTest, TransitionCandidateStatusRequiresMatchingFromState) {
    const auto temp_dir = BuildTempDir();
    std::filesystem::remove_all(temp_dir);
    JsonRedundancyRepositoryAdapter adapter(temp_dir);

    auto entity_result = adapter.UpsertEntities({BuildEntity()});
    ASSERT_TRUE(entity_result.IsSuccess()) << entity_result.GetError().ToString();
    auto candidate_result = adapter.UpsertCandidates({BuildCandidate()});
    ASSERT_TRUE(candidate_result.IsSuccess()) << candidate_result.GetError().ToString();

    CandidateTransitionRequest request;
    request.candidate_id = "cand_repo";
    request.from_status = CandidateStatus::New;
    request.to_status = CandidateStatus::Reviewed;
    request.actor = "tester";
    request.reason = "review";
    request.transitioned_at = "2026-03-21T01:00:00Z";
    auto transition_result = adapter.TransitionCandidateStatus(request);
    ASSERT_TRUE(transition_result.IsSuccess()) << transition_result.GetError().ToString();

    CandidateTransitionRequest invalid_request;
    invalid_request.candidate_id = "cand_repo";
    invalid_request.from_status = CandidateStatus::New;
    invalid_request.to_status = CandidateStatus::Deprecated;
    invalid_request.actor = "tester";
    invalid_request.reason = "invalid";
    invalid_request.transitioned_at = "2026-03-21T01:01:00Z";
    auto invalid_result = adapter.TransitionCandidateStatus(invalid_request);
    ASSERT_TRUE(invalid_result.IsError());

    std::filesystem::remove_all(temp_dir);
}
