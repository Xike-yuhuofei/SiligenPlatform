#include "trace_diagnostics/contracts/TraceDiagnosticsContracts.h"

#include <gtest/gtest.h>

#include <type_traits>

namespace {

using Siligen::TraceDiagnostics::Contracts::EvidenceResult;
using Siligen::TraceDiagnostics::Contracts::EvidenceResultToString;
using Siligen::TraceDiagnostics::Contracts::ValidationEvidenceBundle;

static_assert(std::is_default_constructible_v<ValidationEvidenceBundle>);

TEST(ValidationEvidenceContractsTest, DefaultBundleStateRemainsFrozen) {
    const ValidationEvidenceBundle bundle;

    EXPECT_EQ(bundle.summary.result, EvidenceResult::Blocked);
    EXPECT_TRUE(bundle.summary.suite.empty());
    EXPECT_TRUE(bundle.summary.case_id.empty());
    EXPECT_TRUE(bundle.timeline.empty());
    EXPECT_TRUE(bundle.artifacts.empty());
    EXPECT_TRUE(bundle.report_path.empty());
}

TEST(ValidationEvidenceContractsTest, BundleCarriesMinimalEvidenceShape) {
    ValidationEvidenceBundle bundle;
    bundle.summary.suite = "quick-gate";
    bundle.summary.case_id = "trace-observability-smoke";
    bundle.summary.result = EvidenceResult::Fail;
    bundle.summary.failure_category = "missing-config";
    bundle.summary.final_state = "LoggerBootstrapFailed";
    bundle.summary.primary_cause = "CreateLoggingService";
    bundle.timeline = {
        {0, "bootstrap-started", "LoggingServiceFactory", "bootstrapping", false},
        {1, "bootstrap-failed", "SpdlogLoggingAdapter", "error", true},
    };
    bundle.artifacts = {"summary.json", "timeline.json", "report.md"};
    bundle.report_path = "tests/reports/trace-diagnostics/trace-observability-smoke/report.md";

    EXPECT_EQ(EvidenceResultToString(bundle.summary.result), std::string("fail"));
    ASSERT_EQ(bundle.timeline.size(), 2u);
    EXPECT_EQ(bundle.timeline[0].event_name, "bootstrap-started");
    EXPECT_EQ(bundle.timeline[1].event_name, "bootstrap-failed");
    EXPECT_TRUE(bundle.timeline[1].fault_trigger);
    ASSERT_EQ(bundle.artifacts.size(), 3u);
    EXPECT_EQ(bundle.artifacts[2], "report.md");
}

}  // namespace
