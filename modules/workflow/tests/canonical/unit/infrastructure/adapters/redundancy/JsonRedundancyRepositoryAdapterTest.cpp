#include "infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

using Siligen::Domain::System::Redundancy::CandidateAction;
using Siligen::Domain::System::Redundancy::CandidatePriority;
using Siligen::Domain::System::Redundancy::CandidateRecord;
using Siligen::Domain::System::Redundancy::CandidateStatus;
using Siligen::Domain::System::Redundancy::CodeEntity;
using Siligen::Domain::System::Redundancy::DecisionAction;
using Siligen::Domain::System::Redundancy::DecisionLogRecord;
using Siligen::Domain::System::Redundancy::EntityType;
using Siligen::Domain::System::Redundancy::Ports::CandidateQueryFilter;
using Siligen::Domain::System::Redundancy::Ports::CandidateTransitionRequest;
using Siligen::Domain::System::Redundancy::SourceLanguage;
using Siligen::Infrastructure::Adapters::Redundancy::JsonRedundancyRepositoryAdapter;
using Siligen::Shared::Types::ErrorCode;

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

DecisionLogRecord BuildDecisionLogRecord() {
    DecisionLogRecord record;
    record.decision_id = "decision_repo";
    record.candidate_id = "cand_repo";
    record.action = DecisionAction::Approve;
    record.actor = "tester";
    record.reason = "approve";
    record.evidence_snapshot_json = "{ \"confidence\": 1 }";
    record.ticket = "NOISSUE";
    record.created_at = "2026-03-21T02:00:00Z";
    return record;
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
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

TEST(JsonRedundancyRepositoryAdapterTest, AppendDecisionLogTreatsSamePayloadAsIdempotentSuccess) {
    const auto temp_dir = BuildTempDir();
    std::filesystem::remove_all(temp_dir);
    JsonRedundancyRepositoryAdapter adapter(temp_dir);

    auto first = adapter.AppendDecisionLog(BuildDecisionLogRecord());
    ASSERT_TRUE(first.IsSuccess()) << first.GetError().ToString();
    EXPECT_EQ(first.Value().received, 1U);
    EXPECT_EQ(first.Value().upserted, 1U);

    auto second = adapter.AppendDecisionLog(BuildDecisionLogRecord());
    ASSERT_TRUE(second.IsSuccess()) << second.GetError().ToString();
    EXPECT_EQ(second.Value().received, 1U);
    EXPECT_EQ(second.Value().upserted, 0U);
}

TEST(JsonRedundancyRepositoryAdapterTest, AppendDecisionLogRejectsDifferentPayloadWithSameDecisionId) {
    const auto temp_dir = BuildTempDir();
    std::filesystem::remove_all(temp_dir);
    JsonRedundancyRepositoryAdapter adapter(temp_dir);

    auto record = BuildDecisionLogRecord();
    auto first = adapter.AppendDecisionLog(record);
    ASSERT_TRUE(first.IsSuccess()) << first.GetError().ToString();

    record.reason = "changed_reason";
    auto conflict = adapter.AppendDecisionLog(record);
    ASSERT_TRUE(conflict.IsError());
    EXPECT_EQ(conflict.GetError().GetCode(), ErrorCode::DUPLICATE_DECISION_ID);
}

TEST(JsonRedundancyRepositoryAdapterTest, ListCandidatesRejectsInvalidPriorityFilter) {
    const auto temp_dir = BuildTempDir();
    std::filesystem::remove_all(temp_dir);
    JsonRedundancyRepositoryAdapter adapter(temp_dir);

    ASSERT_TRUE(adapter.UpsertEntities({BuildEntity()}).IsSuccess());
    ASSERT_TRUE(adapter.UpsertCandidates({BuildCandidate()}).IsSuccess());

    CandidateQueryFilter filter;
    filter.priority = std::string("P9");
    auto list_result = adapter.ListCandidates(filter);

    ASSERT_TRUE(list_result.IsError());
    EXPECT_EQ(list_result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
    EXPECT_NE(list_result.GetError().GetMessage().find("failure_stage=list_candidates_filter"), std::string::npos);
}

TEST(JsonRedundancyRepositoryAdapterTest, ListCandidatesRecoversFromCorruptPrimaryStoreUsingBackup) {
    const auto temp_dir = BuildTempDir();
    std::filesystem::remove_all(temp_dir);
    JsonRedundancyRepositoryAdapter adapter(temp_dir);

    ASSERT_TRUE(adapter.UpsertEntities({BuildEntity()}).IsSuccess());
    ASSERT_TRUE(adapter.UpsertCandidates({BuildCandidate()}).IsSuccess());

    const auto candidates_path = temp_dir / "candidates.json";
    auto backup_path = candidates_path;
    backup_path += ".bak";
    ASSERT_TRUE(std::filesystem::copy_file(
        candidates_path,
        backup_path,
        std::filesystem::copy_options::overwrite_existing));
    WriteTextFile(candidates_path, "{ invalid-json");

    auto recovered_result = adapter.ListCandidates({});
    ASSERT_TRUE(recovered_result.IsSuccess()) << recovered_result.GetError().ToString();
    ASSERT_EQ(recovered_result.Value().size(), 1U);
    EXPECT_EQ(recovered_result.Value()[0].candidate_id, "cand_repo");

    auto second_read_result = adapter.ListCandidates({});
    ASSERT_TRUE(second_read_result.IsSuccess()) << second_read_result.GetError().ToString();
    ASSERT_EQ(second_read_result.Value().size(), 1U);

    std::filesystem::remove_all(temp_dir);
}

TEST(JsonRedundancyRepositoryAdapterTest, ListCandidatesFailsWhenPrimaryAndBackupStoresAreBothCorrupted) {
    const auto temp_dir = BuildTempDir();
    std::filesystem::remove_all(temp_dir);
    JsonRedundancyRepositoryAdapter adapter(temp_dir);

    ASSERT_TRUE(adapter.UpsertEntities({BuildEntity()}).IsSuccess());
    ASSERT_TRUE(adapter.UpsertCandidates({BuildCandidate()}).IsSuccess());

    const auto candidates_path = temp_dir / "candidates.json";
    auto backup_path = candidates_path;
    backup_path += ".bak";
    WriteTextFile(candidates_path, "{ invalid-primary");
    WriteTextFile(backup_path, "{ invalid-backup");

    auto list_result = adapter.ListCandidates({});
    ASSERT_TRUE(list_result.IsError());
    EXPECT_EQ(list_result.GetError().GetCode(), ErrorCode::JSON_PARSE_ERROR);

    std::filesystem::remove_all(temp_dir);
}
