#pragma once

#include <string>
#include <vector>

namespace Siligen::TraceDiagnostics::Contracts {

enum class EvidenceResult {
    Pass,
    Fail,
    Blocked,
    Skipped,
};

inline const char* EvidenceResultToString(EvidenceResult result) {
    switch (result) {
        case EvidenceResult::Pass:
            return "pass";
        case EvidenceResult::Fail:
            return "fail";
        case EvidenceResult::Blocked:
            return "blocked";
        case EvidenceResult::Skipped:
            return "skipped";
    }
    return "unknown";
}

struct EvidenceSummary {
    std::string suite;
    std::string case_id;
    EvidenceResult result = EvidenceResult::Blocked;
    std::string started_at;
    std::string finished_at;
    std::string failure_category;
    std::string final_state;
    std::string primary_cause;
};

struct EvidenceTimelineEvent {
    int sequence = 0;
    std::string event_name;
    std::string source;
    std::string state;
    bool fault_trigger = false;
};

struct ValidationEvidenceBundle {
    EvidenceSummary summary;
    std::vector<EvidenceTimelineEvent> timeline;
    std::vector<std::string> artifacts;
    std::string report_path;
};

}  // namespace Siligen::TraceDiagnostics::Contracts
