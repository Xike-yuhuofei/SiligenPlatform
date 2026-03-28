#include "ContourAugmenterAdapter.h"

namespace Siligen::Infrastructure::Adapters::Planning::Geometry {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

Result<void> ContourAugmenterAdapter::ConvertFile(const std::string& /*input_path*/,
                                             const std::string& /*output_path*/,
                                             const ContourAugmentConfig& /*config*/) {
    return Result<void>::Failure(Error(
        ErrorCode::NOT_IMPLEMENTED,
        "DXF contour augmentation requires CGAL (disabled at build time)",
        "ContourAugmenterAdapter"));
}

}  // namespace Siligen::Infrastructure::Adapters::Planning::Geometry


