#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Domain::Configuration::Ports {
class IConfigurationPort;
}

namespace Siligen::Shared::Types {
template <typename T>
class Result;
}

namespace Siligen::Application::Services::DXF {

using Siligen::Shared::Types::Result;

struct InputQualitySummary {
    std::string report_id;
    std::string report_path;
    std::string schema_version;
    std::string dxf_hash;
    std::string source_drawing_ref;
    std::string gate_result;
    std::string classification;
    bool preview_ready = false;
    bool production_ready = false;
    std::string summary;
    std::string primary_code;
    std::vector<std::string> warning_codes;
    std::vector<std::string> error_codes;
    std::string resolved_units;
    double resolved_unit_scale = 1.0;
};

struct PreparedInputArtifact {
    std::string prepared_path;
    InputQualitySummary input_quality;
};

class DxfPbPreparationService {
   public:
    explicit DxfPbPreparationService(
        std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port = nullptr);

    Result<std::string> EnsurePbReady(const std::string& filepath) const;
    Result<PreparedInputArtifact> PrepareInputArtifact(const std::string& filepath) const;
    static Result<void> CleanupPreparedInput(const std::string& source_path);

   private:
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port_;
};

}  // namespace Siligen::Application::Services::DXF
