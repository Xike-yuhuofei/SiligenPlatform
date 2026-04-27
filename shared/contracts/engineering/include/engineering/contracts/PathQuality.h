#pragma once

#include <string>
#include <vector>

namespace Siligen::Engineering::Contracts {

struct PathQualityInputs {
    bool spacing_valid = false;
    std::string validation_classification;
    std::string formal_compare_gate;
    std::string preview_diagnostic_code;
    int discontinuity_count = 0;
};

struct PathQuality {
    std::string verdict = "fail";
    bool blocking = true;
    std::vector<std::string> reason_codes;
    std::string summary;
    std::string source = "runtime_authority_path_quality";
    PathQualityInputs inputs{};
    std::string thresholds_version = "runtime_path_quality.v1";
};

}  // namespace Siligen::Engineering::Contracts
