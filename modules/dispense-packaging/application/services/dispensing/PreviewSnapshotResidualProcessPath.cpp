#include "application/services/dispensing/PreviewSnapshotResidualProcessPath.h"

#include "domain/dispensing/planning/domain-services/CurveFlatteningService.h"
#include "process_path/contracts/ProcessPath.h"

namespace Siligen::Application::Services::Dispensing::Internal {

using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;

namespace {

constexpr double kPreviewDuplicateEpsilonMm = 1e-4;
constexpr float32 kProcessPathSplineErrorMm = 0.05f;
constexpr float32 kProcessPathSampleStepMm = 1.0f;

double DistanceSquared(const Point2D& lhs, const Point2D& rhs) {
    const double dx = static_cast<double>(lhs.x) - static_cast<double>(rhs.x);
    const double dy = static_cast<double>(lhs.y) - static_cast<double>(rhs.y);
    return dx * dx + dy * dy;
}

bool NearlyEqualPoint(const Point2D& lhs, const Point2D& rhs) {
    const double epsilon_sq = kPreviewDuplicateEpsilonMm * kPreviewDuplicateEpsilonMm;
    return DistanceSquared(lhs, rhs) <= epsilon_sq;
}

void AppendDistinctPoint(
    std::vector<Point2D>& points,
    const Point2D& candidate) {
    if (points.empty() || !NearlyEqualPoint(points.back(), candidate)) {
        points.push_back(candidate);
    }
}

}  // namespace

std::vector<Point2D> BuildPreviewProcessPathPoints(const Siligen::ProcessPath::Contracts::ProcessPath& process_path) {
    std::vector<Point2D> points;
    Siligen::Domain::Dispensing::DomainServices::CurveFlatteningService flattening_service;

    for (const auto& process_segment : process_path.segments) {
        const auto& geometry = process_segment.geometry;
        if (geometry.is_point) {
            AppendDistinctPoint(points, geometry.line.start);
            continue;
        }

        const auto flatten_result =
            flattening_service.Flatten(geometry, kProcessPathSplineErrorMm, kProcessPathSampleStepMm);
        if (flatten_result.IsError()) {
            continue;
        }
        for (const auto& point : flatten_result.Value().points) {
            AppendDistinctPoint(points, point);
        }
    }

    return points;
}

}  // namespace Siligen::Application::Services::Dispensing::Internal
