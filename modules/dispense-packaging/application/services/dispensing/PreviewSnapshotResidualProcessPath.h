#pragma once

#include "shared/types/Point.h"

#include <vector>

namespace Siligen::ProcessPath::Contracts {
struct ProcessPath;
}

namespace Siligen::Application::Services::Dispensing::Internal {

std::vector<Siligen::Shared::Types::Point2D> BuildPreviewProcessPathPoints(
    const Siligen::ProcessPath::Contracts::ProcessPath& process_path);

}  // namespace Siligen::Application::Services::Dispensing::Internal
