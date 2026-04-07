#pragma once

#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/IDXFPathSourcePort.h"
#include "PbPathSourceAdapter.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Infrastructure::Adapters::Parsing {

using Siligen::Domain::Trajectory::Ports::DXFPathSourceResult;
using Siligen::Domain::Trajectory::Ports::DXFValidationResult;
using Siligen::Domain::Trajectory::Ports::PathSourceResult;
using Siligen::Shared::Types::Result;

class AutoPathSourceAdapter :
    public Siligen::Domain::Trajectory::Ports::IPathSourcePort,
    public Siligen::Domain::Trajectory::Ports::IDXFPathSourcePort {
public:
    AutoPathSourceAdapter();
    ~AutoPathSourceAdapter() override = default;

    Result<PathSourceResult> LoadFromFile(const std::string& filepath) override;
    Result<DXFPathSourceResult> LoadDXFFile(const std::string& filepath) override;
    Result<DXFValidationResult> ValidateDXFFile(const std::string& filepath) override;
    std::vector<std::string> GetSupportedFormats() override;
    Result<bool> RequiresPreprocessing(const std::string& filepath) override;

private:
    std::shared_ptr<PbPathSourceAdapter> pb_adapter_;
};

} // namespace Siligen::Infrastructure::Adapters::Parsing
