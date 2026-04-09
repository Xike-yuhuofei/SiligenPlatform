#include "dxf_geometry/adapters/planning/dxf/PbPathSourceAdapter.h"

#include "shared/types/Error.h"
#include "shared/types/Result.h"

#if SILIGEN_ENABLE_PROTOBUF
#include "dxf_primitives.pb.h"
#endif

#include <fstream>

namespace Siligen::Infrastructure::Adapters::Parsing {

using Siligen::ProcessPath::Contracts::ContourElement;
using Siligen::ProcessPath::Contracts::ContourElementType;
using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
using Siligen::ProcessPath::Contracts::PathSourceResult;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::Shared::Types::DXFEntityType;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;

#if SILIGEN_ENABLE_PROTOBUF
namespace {

Point2D ToPoint(const siligen::dxf::Point2D& point) {
    return Point2D(static_cast<float32>(point.x()), static_cast<float32>(point.y()));
}

bool ConvertContourElement(const siligen::dxf::ContourElement& element, ContourElement& out) {
    switch (element.type()) {
        case siligen::dxf::CONTOUR_LINE: {
            out.type = ContourElementType::Line;
            out.line.start = ToPoint(element.line().start());
            out.line.end = ToPoint(element.line().end());
            return true;
        }
        case siligen::dxf::CONTOUR_ARC: {
            out.type = ContourElementType::Arc;
            out.arc.center = ToPoint(element.arc().center());
            out.arc.radius = static_cast<float32>(element.arc().radius());
            out.arc.start_angle_deg = static_cast<float32>(element.arc().start_angle_deg());
            out.arc.end_angle_deg = static_cast<float32>(element.arc().end_angle_deg());
            out.arc.clockwise = element.arc().clockwise();
            return true;
        }
        case siligen::dxf::CONTOUR_SPLINE: {
            out.type = ContourElementType::Spline;
            out.spline.control_points.clear();
            out.spline.control_points.reserve(element.spline().control_points_size());
            for (const auto& cp : element.spline().control_points()) {
                out.spline.control_points.push_back(ToPoint(cp));
            }
            out.spline.closed = element.spline().closed();
            return true;
        }
        default:
            return false;
    }
}

bool ConvertPrimitive(const siligen::dxf::Primitive& in, Primitive& out) {
    switch (in.type()) {
        case siligen::dxf::PRIMITIVE_LINE: {
            out = Primitive::MakeLine(ToPoint(in.line().start()), ToPoint(in.line().end()));
            return true;
        }
        case siligen::dxf::PRIMITIVE_ARC: {
            out = Primitive::MakeArc(ToPoint(in.arc().center()),
                                     static_cast<float32>(in.arc().radius()),
                                     static_cast<float32>(in.arc().start_angle_deg()),
                                     static_cast<float32>(in.arc().end_angle_deg()),
                                     in.arc().clockwise());
            return true;
        }
        case siligen::dxf::PRIMITIVE_SPLINE: {
            std::vector<Point2D> points;
            points.reserve(in.spline().control_points_size());
            for (const auto& cp : in.spline().control_points()) {
                points.push_back(ToPoint(cp));
            }
            out = Primitive::MakeSpline(points, in.spline().closed());
            return true;
        }
        case siligen::dxf::PRIMITIVE_CIRCLE: {
            out = Primitive::MakeCircle(ToPoint(in.circle().center()),
                                        static_cast<float32>(in.circle().radius()),
                                        static_cast<float32>(in.circle().start_angle_deg()),
                                        in.circle().clockwise());
            return true;
        }
        case siligen::dxf::PRIMITIVE_ELLIPSE: {
            out = Primitive::MakeEllipse(ToPoint(in.ellipse().center()),
                                         ToPoint(in.ellipse().major_axis()),
                                         static_cast<float32>(in.ellipse().ratio()),
                                         static_cast<float32>(in.ellipse().start_param()),
                                         static_cast<float32>(in.ellipse().end_param()),
                                         in.ellipse().clockwise());
            return true;
        }
        case siligen::dxf::PRIMITIVE_POINT: {
            out = Primitive::MakePoint(ToPoint(in.point().position()));
            return true;
        }
        case siligen::dxf::PRIMITIVE_CONTOUR: {
            std::vector<ContourElement> elements;
            elements.reserve(in.contour().elements_size());
            for (const auto& elem : in.contour().elements()) {
                ContourElement out_elem;
                if (!ConvertContourElement(elem, out_elem)) {
                    continue;
                }
                elements.push_back(std::move(out_elem));
            }
            out = Primitive::MakeContour(elements, in.contour().closed());
            return true;
        }
        default:
            return false;
    }
}

PathPrimitiveMeta ConvertMeta(const siligen::dxf::PrimitiveMeta& meta) {
    PathPrimitiveMeta out;
    out.entity_id = meta.entity_id();
    out.entity_type = static_cast<DXFEntityType>(meta.entity_type());
    out.entity_segment_index = meta.entity_segment_index();
    out.entity_closed = meta.entity_closed();
    return out;
}

}  // namespace
#endif

Result<PathSourceResult> PbPathSourceAdapter::LoadFromFile(const std::string& filepath) {
#if !SILIGEN_ENABLE_PROTOBUF
    return Result<PathSourceResult>::Failure(
        Error(ErrorCode::NOT_IMPLEMENTED,
              "PB path loading is disabled because SILIGEN_ENABLE_PROTOBUF=OFF in this build",
              "PbPathSourceAdapter"));
#else
    std::ifstream input(filepath, std::ios::binary);
    if (!input.good()) {
        return Result<PathSourceResult>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, "PB file not found: " + filepath, "PbPathSourceAdapter"));
    }

    siligen::dxf::PathBundle bundle;
    if (!bundle.ParseFromIstream(&input)) {
        return Result<PathSourceResult>::Failure(
            Error(ErrorCode::FILE_FORMAT_INVALID, "PB parse failed: " + filepath, "PbPathSourceAdapter"));
    }

    PathSourceResult result;
    result.success = true;
    result.primitives.reserve(bundle.primitives_size());
    result.metadata.reserve(bundle.primitives_size());

    for (const auto& primitive : bundle.primitives()) {
        Primitive out;
        if (!ConvertPrimitive(primitive, out)) {
            return Result<PathSourceResult>::Failure(
                Error(ErrorCode::FILE_FORMAT_INVALID, "PB primitive type unsupported", "PbPathSourceAdapter"));
        }
        result.primitives.push_back(std::move(out));
    }

    const int meta_count = bundle.metadata_size();
    if (meta_count == static_cast<int>(result.primitives.size())) {
        for (const auto& meta : bundle.metadata()) {
            result.metadata.push_back(ConvertMeta(meta));
        }
    } else {
        for (int i = 0; i < static_cast<int>(result.primitives.size()); ++i) {
            if (i < meta_count) {
                result.metadata.push_back(ConvertMeta(bundle.metadata(i)));
            } else {
                result.metadata.push_back(PathPrimitiveMeta{});
            }
        }
    }

    if (result.primitives.empty()) {
        return Result<PathSourceResult>::Failure(
            Error(ErrorCode::FILE_FORMAT_INVALID, "PB file has no primitives", "PbPathSourceAdapter"));
    }

    return Result<PathSourceResult>::Success(result);
#endif
}

}  // namespace Siligen::Infrastructure::Adapters::Parsing
