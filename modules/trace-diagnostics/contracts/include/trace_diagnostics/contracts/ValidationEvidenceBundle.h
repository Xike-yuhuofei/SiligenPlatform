#pragma once

#include <string>
#include <vector>

namespace Siligen::TraceDiagnostics::Contracts {

enum class EvidenceResult {
    Pass,
    Fail,
    Blocked,
};

inline std::string EvidenceResultToString(EvidenceResult result) {
    switch (result) {
        case EvidenceResult::Pass:
            return "pass";
        case EvidenceResult::Fail:
            return "fail";
        case EvidenceResult::Blocked:
            return "blocked";
    }
    return "unknown";
}

struct EvidenceTimelineEntry {
    int sequence = 0;
    std::string event_name;
    std::string source;
    std::string state;
    bool fault_trigger = false;
};

struct ValidationEvidenceSummary {
    std::string suite;
    std::string case_id;
    EvidenceResult result = EvidenceResult::Blocked;
    std::string started_at;
    std::string finished_at;
    std::string failure_category;
    std::string final_state;
    std::string primary_cause;
};

struct ValidationEvidenceBundle {
    ValidationEvidenceSummary summary;
    std::vector<EvidenceTimelineEntry> timeline;
    std::vector<std::string> artifacts;
    std::string report_path;
};

}  // namespace Siligen::TraceDiagnostics::Contracts
