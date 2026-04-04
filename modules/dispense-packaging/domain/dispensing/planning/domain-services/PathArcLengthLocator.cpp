#include "PathArcLengthLocator.h"

#include "CurveFlatteningService.h"

#include "process_path/contracts/GeometryUtils.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::ProcessPath::Contracts::ArcPoint;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
using Siligen::ProcessPath::Contracts::ComputeArcSweep;
using Siligen::ProcessPath::Contracts::SegmentType;

namespace {

constexpr float32 kEpsilon = 1e-6f;

struct ResolvedSegmentInfo {
    float32 length_mm = 0.0f;
    std::optional<FlattenedCurvePath> flattened;
};

Point2D InterpolatePolyline(const FlattenedCurvePath& flattened, float32 distance_mm) {
    if (flattened.points.empty()) {
        return {};
    }
    if (flattened.points.size() == 1 || distance_mm <= 0.0f) {
        return flattened.points.front();
    }
    if (distance_mm >= flattened.total_length_mm - kEpsilon) {
        return flattened.points.back();
    }

    for (std::size_t index = 1; index < flattened.points.size(); ++index) {
        const float32 start_length = flattened.cumulative_lengths_mm[index - 1];
        const float32 end_length = flattened.cumulative_lengths_mm[index];
        if (end_length + kEpsilon < distance_mm) {
            continue;
        }
        const float32 segment_length = end_length - start_length;
        if (segment_length <= kEpsilon) {
            return flattened.points[index];
        }
        const float32 ratio = std::clamp((distance_mm - start_length) / segment_length, 0.0f, 1.0f);
        return flattened.points[index - 1] + (flattened.points[index] - flattened.points[index - 1]) * ratio;
    }
    return flattened.points.back();
}

Result<ResolvedSegmentInfo> ResolveSegmentInfo(
    const Segment& segment,
    float32 spline_max_error_mm,
    float32 spline_max_step_mm,
    CurveFlatteningService& flattening_service) {
    ResolvedSegmentInfo info;
    switch (segment.type) {
        case SegmentType::Line: {
            const float32 length_mm = segment.line.start.DistanceTo(segment.line.end);
            if (!std::isfinite(length_mm) || length_mm <= kEpsilon) {
                return Result<ResolvedSegmentInfo>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "line segment length is invalid", "PathArcLengthLocator"));
            }
            info.length_mm = length_mm;
            return Result<ResolvedSegmentInfo>::Success(std::move(info));
        }
        case SegmentType::Arc: {
            const float32 length_mm = ComputeArcLength(segment.arc);
            if (!std::isfinite(length_mm) || length_mm <= kEpsilon) {
                return Result<ResolvedSegmentInfo>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "arc segment length is invalid", "PathArcLengthLocator"));
            }
            info.length_mm = length_mm;
            return Result<ResolvedSegmentInfo>::Success(std::move(info));
        }
        case SegmentType::Spline: {
            auto flattened_result =
                flattening_service.Flatten(segment, spline_max_error_mm, spline_max_step_mm);
            if (flattened_result.IsError()) {
                return Result<ResolvedSegmentInfo>::Failure(flattened_result.GetError());
            }
            info.length_mm = flattened_result.Value().total_length_mm;
            info.flattened = std::move(flattened_result.Value());
            return Result<ResolvedSegmentInfo>::Success(std::move(info));
        }
        default:
            return Result<ResolvedSegmentInfo>::Failure(
                Error(ErrorCode::NOT_IMPLEMENTED, "unsupported segment type", "PathArcLengthLocator"));
    }
}

Result<Point2D> LocateResolvedSegment(
    const Segment& segment,
    const ResolvedSegmentInfo& info,
    float32 local_distance_mm) {
    if (info.length_mm <= kEpsilon) {
        return Result<Point2D>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "segment length is invalid", "PathArcLengthLocator"));
    }

    const float32 clamped_distance = std::clamp(local_distance_mm, 0.0f, info.length_mm);
    switch (segment.type) {
        case SegmentType::Line: {
            const float32 ratio = std::clamp(clamped_distance / info.length_mm, 0.0f, 1.0f);
            return Result<Point2D>::Success(segment.line.start + (segment.line.end - segment.line.start) * ratio);
        }
        case SegmentType::Arc: {
            const float32 ratio = std::clamp(clamped_distance / info.length_mm, 0.0f, 1.0f);
            const float32 sweep = ComputeArcSweep(
                segment.arc.start_angle_deg,
                segment.arc.end_angle_deg,
                segment.arc.clockwise);
            const float32 direction = segment.arc.clockwise ? -1.0f : 1.0f;
            const float32 angle = segment.arc.start_angle_deg + direction * sweep * ratio;
            return Result<Point2D>::Success(ArcPoint(segment.arc, angle));
        }
        case SegmentType::Spline:
            if (!info.flattened.has_value()) {
                return Result<Point2D>::Failure(
                    Error(ErrorCode::INVALID_STATE, "spline flattened path unavailable", "PathArcLengthLocator"));
            }
            return Result<Point2D>::Success(InterpolatePolyline(info.flattened.value(), clamped_distance));
        default:
            return Result<Point2D>::Failure(
                Error(ErrorCode::NOT_IMPLEMENTED, "unsupported segment type", "PathArcLengthLocator"));
    }
}

}  // namespace

Result<Point2D> PathArcLengthLocator::LocateOnSegment(
    const Segment& segment,
    float32 local_distance_mm,
    float32 spline_max_error_mm,
    float32 spline_max_step_mm) const {
    CurveFlatteningService flattening_service;
    auto info_result = ResolveSegmentInfo(segment, spline_max_error_mm, spline_max_step_mm, flattening_service);
    if (info_result.IsError()) {
        return Result<Point2D>::Failure(info_result.GetError());
    }
    return LocateResolvedSegment(segment, info_result.Value(), local_distance_mm);
}

Result<ArcLengthLocation> PathArcLengthLocator::Locate(
    const ProcessPath& path,
    float32 distance_mm,
    float32 spline_max_error_mm,
    float32 spline_max_step_mm) const {
    return Locate(path.segments, distance_mm, spline_max_error_mm, spline_max_step_mm);
}

Result<ArcLengthLocation> PathArcLengthLocator::Locate(
    const std::vector<ProcessSegment>& segments,
    float32 distance_mm,
    float32 spline_max_error_mm,
    float32 spline_max_step_mm) const {
    if (segments.empty()) {
        return Result<ArcLengthLocation>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "process path is empty", "PathArcLengthLocator"));
    }

    CurveFlatteningService flattening_service;
    std::vector<ResolvedSegmentInfo> segment_infos;
    segment_infos.reserve(segments.size());
    for (const auto& process_segment : segments) {
        auto info_result = ResolveSegmentInfo(
            process_segment.geometry,
            spline_max_error_mm,
            spline_max_step_mm,
            flattening_service);
        if (info_result.IsError()) {
            return Result<ArcLengthLocation>::Failure(info_result.GetError());
        }
        segment_infos.push_back(info_result.Value());
    }

    if (distance_mm <= 0.0f) {
        auto first_result = LocateResolvedSegment(segments.front().geometry, segment_infos.front(), 0.0f);
        if (first_result.IsError()) {
            return Result<ArcLengthLocation>::Failure(first_result.GetError());
        }
        ArcLengthLocation location;
        location.position = first_result.Value();
        location.segment_index = 0;
        return Result<ArcLengthLocation>::Success(std::move(location));
    }

    float32 accumulated = 0.0f;
    for (std::size_t index = 0; index < segments.size(); ++index) {
        const float32 segment_length = segment_infos[index].length_mm;
        if (accumulated + segment_length >= distance_mm - kEpsilon) {
            auto point_result = LocateResolvedSegment(
                segments[index].geometry,
                segment_infos[index],
                distance_mm - accumulated);
            if (point_result.IsError()) {
                return Result<ArcLengthLocation>::Failure(point_result.GetError());
            }
            ArcLengthLocation location;
            location.position = point_result.Value();
            location.segment_index = index;
            return Result<ArcLengthLocation>::Success(std::move(location));
        }
        accumulated += segment_length;
    }

    auto tail_result = LocateResolvedSegment(
        segments.back().geometry,
        segment_infos.back(),
        segment_infos.back().length_mm);
    if (tail_result.IsError()) {
        return Result<ArcLengthLocation>::Failure(tail_result.GetError());
    }
    ArcLengthLocation location;
    location.position = tail_result.Value();
    location.segment_index = segments.size() - 1;
    return Result<ArcLengthLocation>::Success(std::move(location));
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
