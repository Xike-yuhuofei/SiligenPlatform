#include "DXFContourAugmenter.h"

namespace Siligen::Application::UseCases::Dispensing::DXF {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

Result<void> DXFContourAugmenter::ConvertFile(const std::string& /*input_path*/,
                                             const std::string& /*output_path*/,
                                             const DXFContourAugmentConfig& /*config*/) {
    return Result<void>::Failure(Error(
        ErrorCode::NOT_IMPLEMENTED,
        "DXF contour augmentation requires CGAL (disabled at build time)",
        "DXFContourAugmenter"));
}

}  // namespace Siligen::Application::UseCases::Dispensing::DXF
