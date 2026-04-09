#include "application/recovery-control/CandidateScoringService.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Application::Services::Redundancy::CandidateScoringService;
using Siligen::Domain::Recovery::Redundancy::CodeEntity;
using Siligen::Domain::Recovery::Redundancy::EntityType;
using Siligen::Domain::Recovery::Redundancy::RuntimeEnvironment;
using Siligen::Domain::Recovery::Redundancy::RuntimeEvidenceRecord;
using Siligen::Domain::Recovery::Redundancy::SourceLanguage;
using Siligen::Domain::Recovery::Redundancy::StaticEvidenceRecord;
using Siligen::Domain::Recovery::Redundancy::StaticSignalType;

CodeEntity BuildEntity() {
    CodeEntity entity;
    entity.entity_id = "ent_alpha";
    entity.entity_type = EntityType::Function;
    entity.path = "src/a.cpp";
    entity.module = "safety";
    entity.language = SourceLanguage::Cpp;
    return entity;
}

}  // namespace

TEST(CandidateScoringServiceTest, ZeroHitAndStrongStaticSignalProduceHighRedundancyScore) {
    CandidateScoringService service;
    const auto entity = BuildEntity();

    StaticEvidenceRecord static_record;
    static_record.evidence_id = "se_1";
    static_record.entity_id = entity.entity_id;
    static_record.rule_id = "rule.unreferenced";
    static_record.rule_version = "1";
    static_record.signal_type = StaticSignalType::Unreferenced;
    static_record.signal_strength = 1.0;
    static_record.details_json = "{}";
    static_record.collected_at = "2026-03-21T00:00:00Z";

    RuntimeEvidenceRecord runtime_record;
    runtime_record.evidence_id = "re_1";
    runtime_record.entity_id = entity.entity_id;
    runtime_record.environment = RuntimeEnvironment::Test;
    runtime_record.window_start = "2026-03-01T00:00:00Z";
    runtime_record.window_end = "2026-03-21T00:00:00Z";
    runtime_record.hit_count = 0;
    runtime_record.source = "unit";
    runtime_record.collected_at = "2026-03-21T00:00:00Z";

    auto result = service.ComputeCandidates({entity},
                                            {static_record},
                                            {runtime_record},
                                            "snap_a",
                                            "v1",
                                            "2026-03-21T00:00:00Z");
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    ASSERT_EQ(result.Value().size(), 1U);
    EXPECT_NEAR(result.Value()[0].redundancy_score, 100.0, 1e-6);
}

TEST(CandidateScoringServiceTest, RuntimeHitsLowerRedundancyScore) {
    CandidateScoringService service;
    const auto entity = BuildEntity();

    StaticEvidenceRecord static_record;
    static_record.evidence_id = "se_2";
    static_record.entity_id = entity.entity_id;
    static_record.rule_id = "rule.unreferenced";
    static_record.rule_version = "1";
    static_record.signal_type = StaticSignalType::Unreferenced;
    static_record.signal_strength = 0.8;
    static_record.details_json = "{}";
    static_record.collected_at = "2026-03-21T00:00:00Z";

    RuntimeEvidenceRecord runtime_record;
    runtime_record.evidence_id = "re_2";
    runtime_record.entity_id = entity.entity_id;
    runtime_record.environment = RuntimeEnvironment::Test;
    runtime_record.window_start = "2026-03-01T00:00:00Z";
    runtime_record.window_end = "2026-03-21T00:00:00Z";
    runtime_record.hit_count = 120;
    runtime_record.source = "unit";
    runtime_record.collected_at = "2026-03-21T00:00:00Z";

    auto result = service.ComputeCandidates({entity},
                                            {static_record},
                                            {runtime_record},
                                            "snap_b",
                                            "v1",
                                            "2026-03-21T00:00:00Z");
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    ASSERT_EQ(result.Value().size(), 1U);
    EXPECT_LT(result.Value()[0].redundancy_score, 70.0);
}
