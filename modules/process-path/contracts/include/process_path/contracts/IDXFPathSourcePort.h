#pragma once

#include "process_path/contracts/IPathSourcePort.h"

#include <string>
#include <vector>

namespace Siligen::Domain::Trajectory::Ports {

struct DXFValidationResult {
    bool is_valid = false;
    std::string format_version;
    std::string error_details;
    int entity_count = 0;
    bool requires_preprocessing = false;
};

struct DXFPathSourceResult {
    bool success = false;
    std::string error_message;
    std::vector<Primitive> primitives;
    std::string source_format;
    bool was_preprocessed = false;
    std::string preprocessing_log;
};

class IDXFPathSourcePort {
   public:
    virtual ~IDXFPathSourcePort() = default;

    virtual Result<DXFPathSourceResult> LoadDXFFile(const std::string& filepath) = 0;
    virtual Result<DXFValidationResult> ValidateDXFFile(const std::string& filepath) = 0;
    virtual std::vector<std::string> GetSupportedFormats() = 0;
    virtual Result<bool> RequiresPreprocessing(const std::string& filepath) = 0;
};

}  // namespace Siligen::Domain::Trajectory::Ports
