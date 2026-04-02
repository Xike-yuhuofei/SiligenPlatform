#include "trace_diagnostics/contracts/TraceDiagnosticsContracts.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

using Siligen::TraceDiagnostics::Contracts::EvidenceResult;
using Siligen::TraceDiagnostics::Contracts::EvidenceResultToString;
using Siligen::TraceDiagnostics::Contracts::ValidationEvidenceBundle;

std::filesystem::path GoldenPath() {
    return std::filesystem::path(__FILE__).parent_path() / "validation_evidence_bundle.baseline.txt";
}

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.is_open()) << "missing golden file: " << path.string();
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

ValidationEvidenceBundle BuildBaselineBundle() {
    ValidationEvidenceBundle bundle;
    bundle.summary.suite = "quick-gate";
    bundle.summary.case_id = "trace-observability-smoke";
    bundle.summary.result = EvidenceResult::Fail;
    bundle.summary.started_at = "2026-04-02T10:00:00Z";
    bundle.summary.finished_at = "2026-04-02T10:00:01Z";
    bundle.summary.failure_category = "missing-config";
    bundle.summary.final_state = "LoggerBootstrapFailed";
    bundle.summary.primary_cause = "CreateLoggingService";
    bundle.timeline = {
        {0, "bootstrap-started", "LoggingServiceFactory", "bootstrapping", false},
        {1, "bootstrap-failed", "SpdlogLoggingAdapter", "error", true},
    };
    bundle.artifacts = {"summary.json", "timeline.json", "report.md"};
    bundle.report_path = "tests/reports/trace-diagnostics/trace-observability-smoke/report.md";
    return bundle;
}

std::string Serialize(const ValidationEvidenceBundle& bundle) {
    std::ostringstream out;
    out << std::boolalpha;
    out << "summary.suite=" << bundle.summary.suite << "\n";
    out << "summary.case_id=" << bundle.summary.case_id << "\n";
    out << "summary.result=" << EvidenceResultToString(bundle.summary.result) << "\n";
    out << "summary.started_at=" << bundle.summary.started_at << "\n";
    out << "summary.finished_at=" << bundle.summary.finished_at << "\n";
    out << "summary.failure_category=" << bundle.summary.failure_category << "\n";
    out << "summary.final_state=" << bundle.summary.final_state << "\n";
    out << "summary.primary_cause=" << bundle.summary.primary_cause << "\n";
    out << "timeline_count=" << bundle.timeline.size() << "\n";
    for (size_t i = 0; i < bundle.timeline.size(); ++i) {
        out << "timeline[" << i << "].sequence=" << bundle.timeline[i].sequence << "\n";
        out << "timeline[" << i << "].event_name=" << bundle.timeline[i].event_name << "\n";
        out << "timeline[" << i << "].source=" << bundle.timeline[i].source << "\n";
        out << "timeline[" << i << "].state=" << bundle.timeline[i].state << "\n";
        out << "timeline[" << i << "].fault_trigger=" << bundle.timeline[i].fault_trigger << "\n";
    }
    out << "artifact_count=" << bundle.artifacts.size() << "\n";
    for (size_t i = 0; i < bundle.artifacts.size(); ++i) {
        out << "artifact[" << i << "]=" << bundle.artifacts[i] << "\n";
    }
    out << "report_path=" << bundle.report_path << "\n";
    return out.str();
}

TEST(ValidationEvidenceBundleGoldenTest, BaselineBundleMatchesGoldenSnapshot) {
    const auto actual = Serialize(BuildBaselineBundle());
    const auto expected = ReadTextFile(GoldenPath());

    EXPECT_EQ(actual, expected);
}

}  // namespace
