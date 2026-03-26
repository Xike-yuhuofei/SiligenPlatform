#include "PathRegularizer.h"

#include "domain/trajectory/value-objects/GeometryUtils.h"
#include "shared/Geometry/BoostGeometryAdapter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::Shared::Types::DXFSegmentType;
using Siligen::Domain::Trajectory::ValueObjects::ArcPoint;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcSweep;
using Siligen::Domain::Trajectory::ValueObjects::NormalizeAngle;
using Siligen::Shared::Geometry::DistancePointToSegment;
using Siligen::Shared::Geometry::DistanceSquared;
using Siligen::Shared::Types::kDegToRad;
using Siligen::Shared::Types::kRadToDeg;

namespace {
constexpr float32 kEpsilon = 1e-6f;

struct ResolvedConfig {
    float32 collinear_angle_tolerance_deg = 0.5f;
    float32 collinear_distance_tolerance = 0.05f;
    float32 overlap_tolerance = 0.05f;
    float32 gap_tolerance = 0.05f;
    float32 connectivity_tolerance = 0.1f;
    float32 min_line_length = 0.0f;
    float32 arc_center_tolerance = 0.05f;
    float32 arc_radius_tolerance = 0.05f;
    float32 arc_gap_tolerance_deg = 0.5f;
    bool enable_bucket_acceleration = true;
    bool enable_line_fit = true;
    bool enable_arc_fit = true;
    float32 line_probe_length = 1.0f;
    float32 cos_threshold = 1.0f;
};

struct LineSample {
    Point2D start;
    Point2D end;
};

struct LineGroup {
    Point2D dir;
    Point2D normal;
    float32 offset = 0.0f;
    Point2D probe_start;
    Point2D probe_end;
    std::vector<LineSample> segments;
};

struct EndpointNode {
    Point2D position;
    std::vector<size_t> segments;
};

struct ArcInterval {
    float32 start_deg = 0.0f;
    float32 end_deg = 0.0f;
};

struct ArcGroup {
    Point2D center;
    float32 radius = 0.0f;
    bool clockwise = false;
    std::vector<DXFSegment> segments;
    std::vector<Point2D> samples;
    std::vector<ArcInterval> intervals;
};

class DisjointSet {
   public:
    explicit DisjointSet(size_t count) : parent_(count), rank_(count, 0) {
        for (size_t i = 0; i < count; ++i) {
            parent_[i] = i;
        }
    }

    size_t Find(size_t index) {
        if (parent_[index] != index) {
            parent_[index] = Find(parent_[index]);
        }
        return parent_[index];
    }

    void Union(size_t a, size_t b) {
        size_t root_a = Find(a);
        size_t root_b = Find(b);
        if (root_a == root_b) {
            return;
        }
        if (rank_[root_a] < rank_[root_b]) {
            parent_[root_a] = root_b;
        } else if (rank_[root_a] > rank_[root_b]) {
            parent_[root_b] = root_a;
        } else {
            parent_[root_b] = root_a;
            rank_[root_a]++;
        }
    }

   private:
    std::vector<size_t> parent_;
    std::vector<size_t> rank_;
};

float32 ClampNonNegative(float32 value) {
    return (value < 0.0f) ? 0.0f : value;
}

Point2D CanonicalDirection(const Point2D& delta) {
    float32 len = delta.Length();
    if (len <= kEpsilon) {
        return Point2D(0.0f, 0.0f);
    }
    Point2D dir = delta / len;
    if (dir.x < -kEpsilon || (std::abs(dir.x) <= kEpsilon && dir.y < 0.0f)) {
        dir = dir * -1.0f;
    }
    return dir;
}

void ReverseSegment(DXFSegment& segment) {
    std::swap(segment.start_point, segment.end_point);
    if (segment.type == DXFSegmentType::ARC) {
        std::swap(segment.start_angle, segment.end_angle);
        segment.clockwise = !segment.clockwise;
    }
}

struct EndpointIndexKey {
    int x = 0;
    int y = 0;
    bool operator==(const EndpointIndexKey& other) const {
        return x == other.x && y == other.y;
    }
};

struct EndpointIndexHasher {
    size_t operator()(const EndpointIndexKey& key) const {
        return (static_cast<size_t>(key.x) << 32) ^ static_cast<size_t>(key.y);
    }
};

struct EndpointIndex {
    explicit EndpointIndex(float32 cell_size)
        : cell_size_(std::max(cell_size, kEpsilon)),
          inv_cell_size_(1.0f / cell_size_) {}

    EndpointIndexKey ToKey(const Point2D& point) const {
        const int gx = static_cast<int>(std::floor(point.x * inv_cell_size_));
        const int gy = static_cast<int>(std::floor(point.y * inv_cell_size_));
        return EndpointIndexKey{gx, gy};
    }

    size_t FindOrCreate(const Point2D& point,
                        float32 tolerance,
                        std::vector<EndpointNode>& nodes) {
        if (!nodes.empty()) {
            const EndpointIndexKey base = ToKey(point);
            const float32 tol_sq = tolerance * tolerance;
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    EndpointIndexKey key{base.x + dx, base.y + dy};
                    auto it = buckets.find(key);
                    if (it == buckets.end()) {
                        continue;
                    }
                    for (size_t idx : it->second) {
                        if (DistanceSquared(point, nodes[idx].position) <= tol_sq) {
                            return idx;
                        }
                    }
                }
            }
        }

        const size_t index = nodes.size();
        nodes.push_back({point, {}});
        buckets[ToKey(point)].push_back(index);
        return index;
    }

    float32 cell_size_;
    float32 inv_cell_size_;
    std::unordered_map<EndpointIndexKey, std::vector<size_t>, EndpointIndexHasher> buckets;
};

size_t FindOrCreateNode(const Point2D& point,
                        float32 tolerance,
                        std::vector<EndpointNode>& nodes,
                        EndpointIndex& index) {
    return index.FindOrCreate(point, tolerance, nodes);
}

ResolvedConfig ResolveConfig(const std::vector<DXFSegment>& segments,
                              const RegularizerConfig& config) {
    ResolvedConfig resolved;
    resolved.collinear_angle_tolerance_deg = ClampNonNegative(config.collinear_angle_tolerance_deg);
    resolved.min_line_length = ClampNonNegative(config.min_line_length);
    resolved.enable_bucket_acceleration = config.enable_bucket_acceleration;
    resolved.enable_line_fit = config.enable_line_fit;
    resolved.enable_arc_fit = config.enable_arc_fit;

    float32 base_tol = 0.0f;
    float32 diag = 0.0f;
    if (!segments.empty()) {
        Point2D min_pt(std::numeric_limits<float32>::max(), std::numeric_limits<float32>::max());
        Point2D max_pt(-std::numeric_limits<float32>::max(), -std::numeric_limits<float32>::max());
        auto apply_point = [&](const Point2D& pt) {
            min_pt.x = std::min(min_pt.x, pt.x);
            min_pt.y = std::min(min_pt.y, pt.y);
            max_pt.x = std::max(max_pt.x, pt.x);
            max_pt.y = std::max(max_pt.y, pt.y);
        };
        for (const auto& seg : segments) {
            apply_point(seg.start_point);
            apply_point(seg.end_point);
            if (seg.type == DXFSegmentType::ARC) {
                apply_point(seg.center_point);
            }
        }
        diag = min_pt.DistanceTo(max_pt);
        if (config.auto_scale_tolerance) {
            float32 ratio = ClampNonNegative(config.auto_scale_ratio);
            float32 min_mm = ClampNonNegative(config.auto_scale_min_mm);
            base_tol = std::max(min_mm, diag * ratio);
        }
    }

    auto resolve_tol = [&](float32 value, float32 fallback) {
        if (value > kEpsilon) {
            return value;
        }
        if (fallback > kEpsilon) {
            return fallback;
        }
        return kEpsilon;
    };

    resolved.collinear_distance_tolerance =
        resolve_tol(config.collinear_distance_tolerance, base_tol);
    resolved.overlap_tolerance = resolve_tol(config.overlap_tolerance, base_tol);
    resolved.gap_tolerance = resolve_tol(config.gap_tolerance,
                                         std::max(resolved.overlap_tolerance, base_tol * 4.0f));
    resolved.connectivity_tolerance =
        resolve_tol(config.connectivity_tolerance, base_tol);
    resolved.arc_center_tolerance =
        resolve_tol(config.arc_center_tolerance, base_tol);
    resolved.arc_radius_tolerance =
        resolve_tol(config.arc_radius_tolerance, base_tol * 2.0f);
    resolved.arc_gap_tolerance_deg =
        (config.arc_gap_tolerance_deg > kEpsilon)
            ? config.arc_gap_tolerance_deg
            : resolved.collinear_angle_tolerance_deg;

    if (resolved.collinear_angle_tolerance_deg <= kEpsilon) {
        resolved.cos_threshold = 1.0f;
    } else {
        resolved.cos_threshold = std::cos(resolved.collinear_angle_tolerance_deg * kDegToRad);
    }
    if (diag <= kEpsilon) {
        diag = 1.0f;
    }
    resolved.line_probe_length = diag * 2.0f;

    return resolved;
}

struct LineBucketKey {
    int angle_bucket = 0;
    int offset_bucket = 0;
    bool operator==(const LineBucketKey& other) const {
        return angle_bucket == other.angle_bucket && offset_bucket == other.offset_bucket;
    }
};

struct LineBucketHasher {
    size_t operator()(const LineBucketKey& key) const {
        return (static_cast<size_t>(key.angle_bucket) << 32) ^ static_cast<size_t>(key.offset_bucket);
    }
};

std::vector<DXFSegment> MergeCollinearLines(const std::vector<DXFSegment>& lines,
                                            const ResolvedConfig& config,
                                            RegularizationReport& report) {
    std::vector<LineGroup> groups;
    const float32 angle_tol = ClampNonNegative(config.collinear_angle_tolerance_deg);
    const float32 cos_threshold = config.cos_threshold;
    const float32 distance_tol = ClampNonNegative(config.collinear_distance_tolerance);
    std::unordered_map<LineBucketKey, std::vector<size_t>, LineBucketHasher> buckets;
    buckets.reserve(lines.size());

    for (const auto& line : lines) {
        Point2D delta = line.end_point - line.start_point;
        float32 length = delta.Length();
        if (length <= std::max(kEpsilon, config.min_line_length)) {
            continue;
        }

        Point2D dir = CanonicalDirection(delta);
        if (dir.Length() <= kEpsilon) {
            continue;
        }
        Point2D normal(-dir.y, dir.x);
        float32 offset = normal.Dot(line.start_point);

        LineGroup* selected = nullptr;
        size_t selected_index = 0;
        if (config.enable_bucket_acceleration && angle_tol > kEpsilon && distance_tol > kEpsilon) {
            float32 angle_deg = std::atan2(dir.y, dir.x) * kRadToDeg;
            int angle_bucket = static_cast<int>(std::llround(angle_deg / angle_tol));
            int offset_bucket = static_cast<int>(std::llround(offset / distance_tol));
            for (int da = -1; da <= 1 && !selected; ++da) {
                for (int db = -1; db <= 1 && !selected; ++db) {
                    LineBucketKey key{angle_bucket + da, offset_bucket + db};
                    auto it = buckets.find(key);
                    if (it == buckets.end()) {
                        continue;
                    }
                    for (size_t idx : it->second) {
                        auto& group = groups[idx];
                        if (group.dir.Dot(dir) < cos_threshold) {
                            continue;
                        }
                        if (std::abs(group.offset - offset) > distance_tol) {
                            continue;
                        }
                        if (DistancePointToSegment(line.start_point, group.probe_start, group.probe_end) >
                            distance_tol) {
                            continue;
                        }
                        selected = &group;
                        selected_index = idx;
                        break;
                    }
                }
            }
        } else {
            for (size_t idx = 0; idx < groups.size(); ++idx) {
                auto& group = groups[idx];
                if (group.dir.Dot(dir) < cos_threshold) {
                    continue;
                }
                if (std::abs(group.offset - offset) > distance_tol) {
                    continue;
                }
                if (DistancePointToSegment(line.start_point, group.probe_start, group.probe_end) > distance_tol) {
                    continue;
                }
                selected = &group;
                selected_index = idx;
                break;
            }
        }

        if (!selected) {
            LineGroup group;
            group.dir = dir;
            group.normal = normal;
            group.offset = offset;
            const float32 probe_len = std::max(config.line_probe_length, kEpsilon);
            group.probe_start = line.start_point - dir * probe_len;
            group.probe_end = line.start_point + dir * probe_len;
            groups.push_back(std::move(group));
            selected_index = groups.size() - 1;
            selected = &groups.back();
            if (config.enable_bucket_acceleration && angle_tol > kEpsilon && distance_tol > kEpsilon) {
                float32 angle_deg = std::atan2(dir.y, dir.x) * kRadToDeg;
                int angle_bucket = static_cast<int>(std::llround(angle_deg / angle_tol));
                int offset_bucket = static_cast<int>(std::llround(offset / distance_tol));
                buckets[{angle_bucket, offset_bucket}].push_back(selected_index);
            }
        }

        selected->segments.push_back({line.start_point, line.end_point});
    }

    report.line_groups = groups.size();

    std::vector<DXFSegment> merged_lines;
    for (auto& group : groups) {
        if (group.segments.empty()) {
            continue;
        }
        Point2D fit_dir = group.dir;
        Point2D fit_normal = group.normal;
        float32 fit_offset = group.offset;

        if (config.enable_line_fit && group.segments.size() >= 2) {
            double sum_x = 0.0;
            double sum_y = 0.0;
            size_t count = 0;
            for (const auto& seg : group.segments) {
                sum_x += seg.start.x + seg.end.x;
                sum_y += seg.start.y + seg.end.y;
                count += 2;
            }
            if (count > 0) {
                const double mean_x = sum_x / static_cast<double>(count);
                const double mean_y = sum_y / static_cast<double>(count);
                double sxx = 0.0;
                double sxy = 0.0;
                double syy = 0.0;
                for (const auto& seg : group.segments) {
                    const double dx0 = seg.start.x - mean_x;
                    const double dy0 = seg.start.y - mean_y;
                    const double dx1 = seg.end.x - mean_x;
                    const double dy1 = seg.end.y - mean_y;
                    sxx += dx0 * dx0 + dx1 * dx1;
                    sxy += dx0 * dy0 + dx1 * dy1;
                    syy += dy0 * dy0 + dy1 * dy1;
                }
                if (std::abs(sxx) > kEpsilon || std::abs(syy) > kEpsilon) {
                    const double angle = 0.5 * std::atan2(2.0 * sxy, sxx - syy);
                    Point2D dir(static_cast<float32>(std::cos(angle)),
                                static_cast<float32>(std::sin(angle)));
                    if (dir.Length() > kEpsilon) {
                        if (dir.Dot(group.dir) < 0.0f) {
                            dir = dir * -1.0f;
                        }
                        fit_dir = dir;
                        fit_normal = Point2D(-dir.y, dir.x);
                        fit_offset = static_cast<float32>(fit_normal.x * mean_x + fit_normal.y * mean_y);
                    }
                }
            }
        }

        std::vector<std::pair<float32, float32>> intervals;
        intervals.reserve(group.segments.size());
        for (const auto& seg : group.segments) {
            float32 s0 = fit_dir.Dot(seg.start);
            float32 s1 = fit_dir.Dot(seg.end);
            if (s0 > s1) {
                std::swap(s0, s1);
            }
            intervals.push_back({s0, s1});
        }

        std::sort(intervals.begin(),
                  intervals.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        std::vector<std::pair<float32, float32>> merged;
        const float32 overlap_tol = ClampNonNegative(config.overlap_tolerance);
        const float32 gap_tol = ClampNonNegative(config.gap_tolerance);
        for (const auto& interval : intervals) {
            if (merged.empty() || interval.first > merged.back().second + std::max(overlap_tol, gap_tol)) {
                merged.push_back(interval);
            } else {
                merged.back().second = std::max(merged.back().second, interval.second);
            }
        }

        if (merged.size() < intervals.size()) {
            report.line_removed += intervals.size() - merged.size();
            report.line_merged += intervals.size() - merged.size();
        }

        for (const auto& interval : merged) {
            float32 length = interval.second - interval.first;
            if (length <= std::max(kEpsilon, config.min_line_length)) {
                continue;
            }
            DXFSegment segment;
            segment.type = DXFSegmentType::LINE;
            segment.start_point = fit_dir * interval.first + fit_normal * fit_offset;
            segment.end_point = fit_dir * interval.second + fit_normal * fit_offset;
            segment.length = segment.start_point.DistanceTo(segment.end_point);
            merged_lines.push_back(segment);
        }
    }

    return merged_lines;
}

struct ArcBucketKey {
    int center_x = 0;
    int center_y = 0;
    int radius = 0;
    int direction = 0;
    bool operator==(const ArcBucketKey& other) const {
        return center_x == other.center_x &&
               center_y == other.center_y &&
               radius == other.radius &&
               direction == other.direction;
    }
};

struct ArcBucketHasher {
    size_t operator()(const ArcBucketKey& key) const {
        size_t h = 1469598103934665603ULL;
        auto mix = [&](int v) {
            h ^= static_cast<size_t>(v);
            h *= 1099511628211ULL;
        };
        mix(key.center_x);
        mix(key.center_y);
        mix(key.radius);
        mix(key.direction);
        return h;
    }
};

std::vector<ArcInterval> ToCCWIntervals(const DXFSegment& arc) {
    float32 start = NormalizeAngle(arc.start_angle);
    float32 end = NormalizeAngle(arc.end_angle);
    float32 sweep = ComputeArcSweep(start, end, arc.clockwise);
    if (std::abs(sweep - 360.0f) < 1e-3f) {
        return {ArcInterval{0.0f, 360.0f}};
    }
    if (!arc.clockwise) {
        if (end >= start) {
            return {ArcInterval{start, end}};
        }
        return {ArcInterval{start, 360.0f}, ArcInterval{0.0f, end}};
    }
    // clockwise: equivalent CCW interval from end to start
    if (start >= end) {
        return {ArcInterval{end, start}};
    }
    return {ArcInterval{end, 360.0f}, ArcInterval{0.0f, start}};
}

std::vector<ArcInterval> ToCCWIntervals(float32 start_angle,
                                        float32 end_angle,
                                        bool clockwise,
                                        bool full_circle) {
    float32 start = NormalizeAngle(start_angle);
    float32 end = NormalizeAngle(end_angle);
    if (full_circle) {
        return {ArcInterval{0.0f, 360.0f}};
    }
    float32 sweep = ComputeArcSweep(start, end, clockwise);
    if (std::abs(sweep - 360.0f) < 1e-3f) {
        return {ArcInterval{0.0f, 360.0f}};
    }
    if (!clockwise) {
        if (end >= start) {
            return {ArcInterval{start, end}};
        }
        return {ArcInterval{start, 360.0f}, ArcInterval{0.0f, end}};
    }
    if (start >= end) {
        return {ArcInterval{end, start}};
    }
    return {ArcInterval{end, 360.0f}, ArcInterval{0.0f, start}};
}

bool FitCircleTaubin(const std::vector<Point2D>& points, Point2D& center, float32& radius) {
    if (points.size() < 3) {
        return false;
    }

    double mean_x = 0.0;
    double mean_y = 0.0;
    for (const auto& p : points) {
        mean_x += p.x;
        mean_y += p.y;
    }
    const double n = static_cast<double>(points.size());
    mean_x /= n;
    mean_y /= n;

    double Mxx = 0.0;
    double Myy = 0.0;
    double Mxy = 0.0;
    double Mxz = 0.0;
    double Myz = 0.0;
    double Mzz = 0.0;

    for (const auto& p : points) {
        const double Xi = p.x - mean_x;
        const double Yi = p.y - mean_y;
        const double Zi = Xi * Xi + Yi * Yi;
        Mxy += Xi * Yi;
        Mxx += Xi * Xi;
        Myy += Yi * Yi;
        Mxz += Xi * Zi;
        Myz += Yi * Zi;
        Mzz += Zi * Zi;
    }

    Mxx /= n;
    Myy /= n;
    Mxy /= n;
    Mxz /= n;
    Myz /= n;
    Mzz /= n;

    const double Mz = Mxx + Myy;
    const double Cov_xy = Mxx * Myy - Mxy * Mxy;
    const double A3 = 4.0 * Mz;
    const double A2 = -3.0 * Mz * Mz - Mzz;
    const double A1 = Mzz * Mz + 4.0 * Cov_xy * Mz - Mxz * Mxz - Myz * Myz - Mz * Mz * Mz;
    const double A0 = Mxz * Mxz * Myy + Myz * Myz * Mxx - Mzz * Cov_xy - 2.0 * Mxz * Myz * Mxy +
                      Mz * Mz * Cov_xy;
    const double A22 = A2 + A2;
    const double A33 = A3 + A3 + A3;

    double xnew = 0.0;
    double ynew = 1e20;
    const double eps = 1e-12;
    const int iter_max = 20;

    for (int iter = 0; iter < iter_max; ++iter) {
        const double yold = ynew;
        ynew = A0 + xnew * (A1 + xnew * (A2 + xnew * A3));
        if (std::abs(ynew) > std::abs(yold)) {
            xnew = 0.0;
            break;
        }
        const double Dy = A1 + xnew * (A22 + xnew * A33);
        if (std::abs(Dy) < eps) {
            xnew = 0.0;
            break;
        }
        const double xold = xnew;
        xnew = xold - ynew / Dy;
        if (xnew < 0.0) {
            xnew = 0.0;
            break;
        }
        if (std::abs(xnew - xold) <= eps * std::max(1.0, std::abs(xnew))) {
            break;
        }
    }

    const double DET = xnew * xnew - xnew * Mz + Cov_xy;
    if (std::abs(DET) < eps) {
        return false;
    }

    const double center_x = (Mxz * (Myy - xnew) - Myz * Mxy) / (2.0 * DET);
    const double center_y = (Myz * (Mxx - xnew) - Mxz * Mxy) / (2.0 * DET);
    const double radius_sq = center_x * center_x + center_y * center_y + Mz;
    if (radius_sq <= 0.0 || !std::isfinite(radius_sq)) {
        return false;
    }

    center = Point2D(static_cast<float32>(center_x + mean_x),
                     static_cast<float32>(center_y + mean_y));
    radius = static_cast<float32>(std::sqrt(radius_sq));
    return radius > kEpsilon;
}

Point2D ComputeArcMidPoint(const DXFSegment& arc) {
    float32 start = NormalizeAngle(arc.start_angle);
    float32 end = NormalizeAngle(arc.end_angle);
    float32 sweep = ComputeArcSweep(start, end, arc.clockwise);
    float32 mid = arc.clockwise ? (start - sweep * 0.5f) : (start + sweep * 0.5f);
    return ArcPoint(arc.center_point, arc.radius, mid);
}

std::vector<DXFSegment> MergeCoCircularArcs(const std::vector<DXFSegment>& arcs,
                                            const ResolvedConfig& config,
                                            RegularizationReport& report) {
    std::vector<ArcGroup> groups;
    std::unordered_map<ArcBucketKey, std::vector<size_t>, ArcBucketHasher> buckets;
    buckets.reserve(arcs.size());

    const float32 center_tol = ClampNonNegative(config.arc_center_tolerance);
    const float32 radius_tol = ClampNonNegative(config.arc_radius_tolerance);
    const float32 center_tol_sq = center_tol * center_tol;

    for (const auto& arc : arcs) {
        if (arc.type != DXFSegmentType::ARC || arc.radius <= kEpsilon) {
            continue;
        }
        ArcGroup* selected = nullptr;
        size_t selected_index = 0;

        if (config.enable_bucket_acceleration && center_tol > kEpsilon && radius_tol > kEpsilon) {
            int cx_bucket = static_cast<int>(std::llround(arc.center_point.x / center_tol));
            int cy_bucket = static_cast<int>(std::llround(arc.center_point.y / center_tol));
            int r_bucket = static_cast<int>(std::llround(arc.radius / radius_tol));
            int dir_bucket = arc.clockwise ? 1 : 0;
            for (int dx = -1; dx <= 1 && !selected; ++dx) {
                for (int dy = -1; dy <= 1 && !selected; ++dy) {
                    for (int dr = -1; dr <= 1 && !selected; ++dr) {
                        ArcBucketKey key{cx_bucket + dx, cy_bucket + dy, r_bucket + dr, dir_bucket};
                        auto it = buckets.find(key);
                        if (it == buckets.end()) {
                            continue;
                        }
                        for (size_t idx : it->second) {
                            auto& group = groups[idx];
                            if (group.clockwise != arc.clockwise) {
                                continue;
                            }
                            if (DistanceSquared(group.center, arc.center_point) > center_tol_sq) {
                                continue;
                            }
                            if (std::abs(group.radius - arc.radius) > radius_tol) {
                                continue;
                            }
                            selected = &group;
                            selected_index = idx;
                            break;
                        }
                    }
                }
            }
        } else {
            for (size_t idx = 0; idx < groups.size(); ++idx) {
                auto& group = groups[idx];
                if (group.clockwise != arc.clockwise) {
                    continue;
                }
                if (DistanceSquared(group.center, arc.center_point) > center_tol_sq) {
                    continue;
                }
                if (std::abs(group.radius - arc.radius) > radius_tol) {
                    continue;
                }
                selected = &group;
                selected_index = idx;
                break;
            }
        }

        if (!selected) {
            ArcGroup group;
            group.center = arc.center_point;
            group.radius = arc.radius;
            group.clockwise = arc.clockwise;
            groups.push_back(std::move(group));
            selected_index = groups.size() - 1;
            selected = &groups.back();
            if (config.enable_bucket_acceleration && center_tol > kEpsilon && radius_tol > kEpsilon) {
                int cx_bucket = static_cast<int>(std::llround(arc.center_point.x / center_tol));
                int cy_bucket = static_cast<int>(std::llround(arc.center_point.y / center_tol));
                int r_bucket = static_cast<int>(std::llround(arc.radius / radius_tol));
                int dir_bucket = arc.clockwise ? 1 : 0;
                buckets[{cx_bucket, cy_bucket, r_bucket, dir_bucket}].push_back(selected_index);
            }
        }

        selected->segments.push_back(arc);
        selected->samples.push_back(arc.start_point);
        selected->samples.push_back(arc.end_point);
        if (arc.radius > kEpsilon) {
            selected->samples.push_back(ComputeArcMidPoint(arc));
        }
    }

    report.arc_groups = groups.size();

    std::vector<DXFSegment> merged_arcs;
    merged_arcs.reserve(groups.size());

    const float32 gap_tol = ClampNonNegative(config.arc_gap_tolerance_deg);
    for (auto& group : groups) {
        if (group.segments.empty()) {
            continue;
        }
        if (config.enable_arc_fit && group.samples.size() >= 3) {
            Point2D fitted_center = group.center;
            float32 fitted_radius = group.radius;
            if (FitCircleTaubin(group.samples, fitted_center, fitted_radius)) {
                group.center = fitted_center;
                group.radius = fitted_radius;
            }
        }

        group.intervals.clear();
        group.intervals.reserve(group.segments.size());
        for (const auto& arc : group.segments) {
            float32 start_angle = std::atan2(arc.start_point.y - group.center.y,
                                             arc.start_point.x - group.center.x) * kRadToDeg;
            float32 end_angle = std::atan2(arc.end_point.y - group.center.y,
                                           arc.end_point.x - group.center.x) * kRadToDeg;
            float32 original_start = NormalizeAngle(arc.start_angle);
            float32 original_end = NormalizeAngle(arc.end_angle);
            float32 original_sweep = ComputeArcSweep(original_start, original_end, arc.clockwise);
            bool full_circle = original_sweep > 359.0f;
            auto intervals = ToCCWIntervals(start_angle, end_angle, arc.clockwise, full_circle);
            group.intervals.insert(group.intervals.end(), intervals.begin(), intervals.end());
        }

        std::sort(group.intervals.begin(),
                  group.intervals.end(),
                  [](const ArcInterval& a, const ArcInterval& b) { return a.start_deg < b.start_deg; });
        std::vector<ArcInterval> merged;
        for (const auto& interval : group.intervals) {
            if (merged.empty() || interval.start_deg > merged.back().end_deg + gap_tol) {
                merged.push_back(interval);
            } else {
                merged.back().end_deg = std::max(merged.back().end_deg, interval.end_deg);
            }
        }

        if (merged.size() < group.intervals.size()) {
            report.arc_removed += group.intervals.size() - merged.size();
            report.arc_merged += group.intervals.size() - merged.size();
        }

        for (const auto& interval : merged) {
            float32 sweep = interval.end_deg - interval.start_deg;
            if (sweep <= kEpsilon) {
                continue;
            }
            DXFSegment arc;
            arc.type = DXFSegmentType::ARC;
            arc.center_point = group.center;
            arc.radius = group.radius;
            if (!group.clockwise) {
                arc.start_angle = interval.start_deg;
                arc.end_angle = interval.end_deg;
                arc.clockwise = false;
            } else {
                arc.start_angle = interval.end_deg;
                arc.end_angle = interval.start_deg;
                arc.clockwise = true;
            }
            arc.start_point = ArcPoint(arc.center_point, arc.radius, arc.start_angle);
            arc.end_point = ArcPoint(arc.center_point, arc.radius, arc.end_angle);
            float32 arc_sweep = ComputeArcSweep(arc.start_angle, arc.end_angle, arc.clockwise);
            arc.length = arc.radius * arc_sweep * kDegToRad;
            merged_arcs.push_back(arc);
        }
    }

    return merged_arcs;
}

Contour OrderContour(const std::vector<DXFSegment>& segments,
                     const std::vector<size_t>& indices,
                     float32 tolerance,
                     RegularizationReport& report) {
    Contour contour;
    if (indices.empty()) {
        return contour;
    }

    std::vector<EndpointNode> nodes;
    EndpointIndex index(tolerance);
    std::unordered_map<size_t, std::pair<size_t, size_t>> segment_nodes;
    nodes.reserve(indices.size() * 2);

    for (size_t seg_id : indices) {
        const auto& seg = segments[seg_id];
        size_t start_node = FindOrCreateNode(seg.start_point, tolerance, nodes, index);
        size_t end_node = FindOrCreateNode(seg.end_point, tolerance, nodes, index);
        segment_nodes[seg_id] = {start_node, end_node};
        nodes[start_node].segments.push_back(seg_id);
        nodes[end_node].segments.push_back(seg_id);
    }

    std::vector<size_t> open_nodes;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].segments.size() == 1) {
            open_nodes.push_back(i);
        }
    }

    size_t start_node = 0;
    if (!open_nodes.empty()) {
        start_node = *std::min_element(open_nodes.begin(), open_nodes.end());
    }

    size_t start_segment = nodes[start_node].segments.front();
    for (size_t seg_id : nodes[start_node].segments) {
        if (seg_id < start_segment) {
            start_segment = seg_id;
        }
    }

    std::vector<bool> used(segments.size(), false);
    contour.segments.reserve(indices.size());

    auto pick_next = [&](size_t node_id) -> size_t {
        if (node_id >= nodes.size()) {
            return std::numeric_limits<size_t>::max();
        }
        size_t chosen = std::numeric_limits<size_t>::max();
        for (size_t seg_id : nodes[node_id].segments) {
            if (used[seg_id]) {
                continue;
            }
            if (seg_id < chosen) {
                chosen = seg_id;
            }
        }
        return chosen;
    };

    size_t current_node = start_node;
    size_t current_segment = start_segment;

    auto append_segment = [&](size_t seg_id, size_t at_node) -> size_t {
        DXFSegment seg = segments[seg_id];
        auto mapping = segment_nodes[seg_id];
        if (mapping.first != at_node) {
            ReverseSegment(seg);
            report.reordered_segments++;
        }
        contour.segments.push_back(seg);
        used[seg_id] = true;
        return (mapping.first == at_node) ? mapping.second : mapping.first;
    };

    current_node = append_segment(current_segment, current_node);

    size_t used_count = 1;
    while (used_count < indices.size()) {
        size_t next_segment = pick_next(current_node);
        if (next_segment == std::numeric_limits<size_t>::max()) {
            size_t fallback = std::numeric_limits<size_t>::max();
            for (size_t seg_id : indices) {
                if (!used[seg_id]) {
                    fallback = seg_id;
                    break;
                }
            }
            if (fallback == std::numeric_limits<size_t>::max()) {
                break;
            }
            current_node = append_segment(fallback, segment_nodes[fallback].first);
            report.disconnected_chains++;
            used_count++;
            continue;
        }
        current_node = append_segment(next_segment, current_node);
        used_count++;
    }

    if (!contour.segments.empty()) {
        Point2D start_point = contour.segments.front().start_point;
        Point2D end_point = contour.segments.back().end_point;
        const float32 tol_sq = tolerance * tolerance;
        contour.closed = open_nodes.empty() &&
                         DistanceSquared(start_point, end_point) <= tol_sq;
    }

    return contour;
}

std::vector<Contour> BuildContours(const std::vector<DXFSegment>& segments,
                                   float32 tolerance,
                                   RegularizationReport& report) {
    std::vector<Contour> contours;
    if (segments.empty()) {
        return contours;
    }

    std::vector<EndpointNode> nodes;
    EndpointIndex index(tolerance);
    nodes.reserve(segments.size() * 2);
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& seg = segments[i];
        size_t start_node = FindOrCreateNode(seg.start_point, tolerance, nodes, index);
        size_t end_node = FindOrCreateNode(seg.end_point, tolerance, nodes, index);
        nodes[start_node].segments.push_back(i);
        nodes[end_node].segments.push_back(i);
    }

    DisjointSet dsu(segments.size());
    for (const auto& node : nodes) {
        if (node.segments.size() < 2) {
            continue;
        }
        size_t first = node.segments.front();
        for (size_t idx = 1; idx < node.segments.size(); ++idx) {
            dsu.Union(first, node.segments[idx]);
        }
    }

    std::unordered_map<size_t, std::vector<size_t>> groups;
    groups.reserve(segments.size());
    for (size_t i = 0; i < segments.size(); ++i) {
        groups[dsu.Find(i)].push_back(i);
    }

    struct Component {
        size_t min_index = 0;
        std::vector<size_t> indices;
    };

    std::vector<Component> components;
    components.reserve(groups.size());
    for (auto& entry : groups) {
        Component comp;
        comp.indices = std::move(entry.second);
        comp.min_index = *std::min_element(comp.indices.begin(), comp.indices.end());
        components.push_back(std::move(comp));
    }

    std::sort(components.begin(),
              components.end(),
              [](const Component& a, const Component& b) {
                  return a.min_index < b.min_index;
              });

    contours.reserve(components.size());
    for (const auto& comp : components) {
        Contour contour = OrderContour(segments, comp.indices, tolerance, report);
        contours.push_back(std::move(contour));
    }

    report.contour_count = contours.size();
    return contours;
}

}  // namespace

RegularizedPath PathRegularizer::Regularize(const std::vector<DXFSegment>& segments,
                                            const RegularizerConfig& config) const {
    RegularizedPath result;
    if (segments.empty()) {
        return result;
    }

    ResolvedConfig resolved = ResolveConfig(segments, config);

    std::vector<DXFSegment> lines;
    std::vector<DXFSegment> arcs;
    std::vector<DXFSegment> others;
    lines.reserve(segments.size());
    arcs.reserve(segments.size());
    others.reserve(segments.size());

    for (const auto& seg : segments) {
        if (seg.type == DXFSegmentType::LINE) {
            lines.push_back(seg);
        } else if (seg.type == DXFSegmentType::ARC) {
            arcs.push_back(seg);
        } else {
            others.push_back(seg);
        }
    }

    std::vector<DXFSegment> merged_lines = MergeCollinearLines(lines, resolved, result.report);
    std::vector<DXFSegment> merged_arcs = MergeCoCircularArcs(arcs, resolved, result.report);

    std::vector<DXFSegment> merged_segments;
    merged_segments.reserve(merged_lines.size() + merged_arcs.size() + others.size());
    for (auto& line : merged_lines) {
        merged_segments.push_back(line);
    }
    for (auto& arc : merged_arcs) {
        merged_segments.push_back(arc);
    }
    for (auto& seg : others) {
        merged_segments.push_back(seg);
    }

    const float32 tolerance = std::max(resolved.connectivity_tolerance, kEpsilon);
    result.contours = BuildContours(merged_segments, tolerance, result.report);
    return result;
}

}  // namespace Siligen::Domain::Trajectory::DomainServices
