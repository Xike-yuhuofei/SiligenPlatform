#pragma once

#include "shared/types/Point.h"

#include <string>
#include <vector>

namespace Siligen::Engineering::Contracts {

struct DxfFormalCompareGateDiagnostic {
    bool has_value = false;
    std::string status;
    std::string reason_code;
    std::string authority_span_ref;
    int trigger_begin_index = 0;
    Siligen::Shared::Types::Point2D current_start_position_mm{0.0f, 0.0f};
    Siligen::Shared::Types::Point2D next_trigger_position_mm{0.0f, 0.0f};
    std::vector<std::string> candidate_failures;
};

struct DxfValidationReport {
    std::string schema_version = "DXFValidationReport.v1";
    std::string stage_id;
    std::string owner_module;
    std::string source_ref;
    std::string source_hash;
    std::string gate_result = "PASS";
    std::string result_classification;
    bool preview_ready = false;
    bool production_ready = false;
    std::string summary;
    std::string primary_code;
    std::vector<std::string> warning_codes;
    std::vector<std::string> error_codes;
    std::string resolved_units;
    double resolved_unit_scale = 1.0;
    DxfFormalCompareGateDiagnostic formal_compare_gate{};
};

inline std::string ResolveGateResult(
    const bool production_ready,
    const std::vector<std::string>& warning_codes,
    const std::vector<std::string>& error_codes) {
    if (!production_ready || !error_codes.empty()) {
        return "FAIL";
    }
    if (!warning_codes.empty()) {
        return "PASS_WITH_WARNINGS";
    }
    return "PASS";
}

}  // namespace Siligen::Engineering::Contracts
