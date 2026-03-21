#include "DXFContourAugmenter.h"

#include "shared/Geometry/BoostGeometryAdapter.h"
#include "shared/types/Point.h"

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Gps_circle_segment_traits_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/approximated_offset_2.h>
#include <CGAL/number_utils.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Siligen::Application::UseCases::Dispensing::DXF {
namespace {

using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Geometry::DistancePointToPolyline;
using Siligen::Shared::Geometry::IsPointInPolygon;

constexpr float32 kPi = 3.14159265358979323846f;
constexpr double kPiDouble = 3.14159265358979323846;

using CgalKernel = CGAL::Exact_predicates_exact_constructions_kernel;
using CgalPoint2 = CgalKernel::Point_2;
using CgalPolygon2 = CGAL::Polygon_2<CgalKernel>;
using CgalGpsTraits = CGAL::Gps_circle_segment_traits_2<CgalKernel>;
using CgalOffsetPolygon = CgalGpsTraits::Polygon_2;
using CgalXMonotoneCurve = CgalGpsTraits::X_monotone_curve_2;

struct DxfLine {
    Point2D a;
    Point2D b;
    std::string layer;
};

struct DxfPolyline {
    std::vector<Point2D> points;
    bool closed = false;
    std::string layer;
};

struct DxfPoint {
    Point2D p;
    std::string layer;
};

struct DxfTextSections {
    std::vector<std::string> prefix_lines;
    std::vector<std::string> suffix_lines;
};

struct PointKey {
    int64_t x = 0;
    int64_t y = 0;

    bool operator==(const PointKey& other) const {
        return x == other.x && y == other.y;
    }
};

struct PointKeyHash {
    std::size_t operator()(const PointKey& key) const noexcept {
        return static_cast<std::size_t>(key.x * 1315423911u + key.y);
    }
};

struct SegmentRef {
    int index = -1;
    bool is_start = true;
};

struct Segment2D {
    Point2D a;
    Point2D b;
};

struct IntersectionInfo {
    Point2D point;
    int seg_index_a = -1;
    int seg_index_b = -1;
};

static std::string Trim(const std::string& input) {
    const auto start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(start, end - start + 1);
}

static std::string ToLowerCopy(std::string value) {
    for (auto& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

template <typename NumberT>
static double ToDouble(const NumberT& value) {
    return CGAL::to_double(value);
}

template <typename PointT>
static Point2D ToPoint(const PointT& p) {
    return Point2D(static_cast<float32>(ToDouble(p.x())),
                   static_cast<float32>(ToDouble(p.y())));
}

static void UpdateAcadVersion(std::vector<std::string>& lines, const std::string& version) {
    for (std::size_t i = 0; i + 3 < lines.size(); i += 2) {
        if (lines[i] == "9" && lines[i + 1] == "$ACADVER") {
            lines[i + 3] = version;
            return;
        }
    }
}

static void UpdateHeaderValue(std::vector<std::string>& lines,
                              const std::string& key,
                              const std::string& value) {
    for (std::size_t i = 0; i + 3 < lines.size(); i += 2) {
        if (lines[i] == "9" && lines[i + 1] == key) {
            lines[i + 3] = value;
            return;
        }
    }
}

static std::vector<std::string> ExtractLayerNames(const std::vector<std::string>& lines) {
    std::vector<std::string> names;
    bool in_table = false;
    bool in_layer_table = false;
    bool expect_layer_name = false;
    for (std::size_t i = 0; i + 1 < lines.size(); i += 2) {
        const std::string& code = lines[i];
        const std::string& value = lines[i + 1];
        if (!in_layer_table) {
            if (code == "0" && value == "TABLE") {
                in_table = true;
                continue;
            }
            if (in_table && code == "2") {
                in_layer_table = (value == "LAYER");
                in_table = false;
                continue;
            }
            continue;
        }
        if (code == "0" && value == "ENDTAB") {
            in_layer_table = false;
            expect_layer_name = false;
            continue;
        }
        if (code == "0" && value == "LAYER") {
            expect_layer_name = true;
            continue;
        }
        if (expect_layer_name && code == "2") {
            names.push_back(value);
            expect_layer_name = false;
        }
    }
    return names;
}

static bool LayerExists(const std::vector<std::string>& names, const std::string& name) {
    for (const auto& entry : names) {
        if (entry == name) {
            return true;
        }
    }
    return false;
}

static std::string ResolveLayerName(const std::vector<std::string>& names, const std::string& preferred) {
    if (names.empty()) {
        return preferred;
    }
    if (LayerExists(names, preferred)) {
        return preferred;
    }
    const std::string preferred_lower = ToLowerCopy(preferred);
    for (const auto& entry : names) {
        if (ToLowerCopy(entry) == preferred_lower) {
            return entry;
        }
    }
    if (LayerExists(names, "0")) {
        return "0";
    }
    return names.front();
}

static bool ReadAllLines(const std::string& path, std::vector<std::string>& out_lines) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        out_lines.push_back(Trim(line));
    }
    return true;
}

static bool SplitDxfByEntities(const std::vector<std::string>& lines, DxfTextSections& out_sections) {
    int entities_start_line = -1;
    int entities_end_line = -1;
    bool in_entities = false;
    for (std::size_t i = 0; i + 1 < lines.size(); i += 2) {
        const std::string& code = lines[i];
        const std::string& value = lines[i + 1];
        if (code == "2" && value == "ENTITIES") {
            entities_start_line = static_cast<int>(i + 2);
            in_entities = true;
            continue;
        }
        if (in_entities && code == "0" && value == "ENDSEC") {
            entities_end_line = static_cast<int>(i);
            break;
        }
    }
    if (entities_start_line < 0 || entities_end_line < 0) {
        return false;
    }
    out_sections.prefix_lines.assign(lines.begin(), lines.begin() + entities_start_line);
    out_sections.suffix_lines.assign(lines.begin() + entities_end_line + 2, lines.end());
    return true;
}

static bool ParseDxfLines(const std::vector<std::string>& lines,
                          std::vector<DxfLine>& out_lines,
                          std::vector<DxfPoint>& out_points) {
    bool in_entities = false;
    std::string entity_type;
    std::string entity_layer;
    std::unordered_map<int, std::string> entity_values;

    auto flush_entity = [&]() {
        if (entity_type == "LINE") {
            auto it10 = entity_values.find(10);
            auto it20 = entity_values.find(20);
            auto it11 = entity_values.find(11);
            auto it21 = entity_values.find(21);
            if (it10 != entity_values.end() && it20 != entity_values.end() &&
                it11 != entity_values.end() && it21 != entity_values.end()) {
                DxfLine line;
                line.layer = entity_layer;
                line.a = Point2D(std::stof(it10->second), std::stof(it20->second));
                line.b = Point2D(std::stof(it11->second), std::stof(it21->second));
                out_lines.push_back(line);
            }
        } else if (entity_type == "POINT") {
            auto it10 = entity_values.find(10);
            auto it20 = entity_values.find(20);
            if (it10 != entity_values.end() && it20 != entity_values.end()) {
                DxfPoint p;
                p.layer = entity_layer;
                p.p = Point2D(std::stof(it10->second), std::stof(it20->second));
                out_points.push_back(p);
            }
        }
        entity_type.clear();
        entity_layer.clear();
        entity_values.clear();
    };

    for (std::size_t i = 0; i + 1 < lines.size(); i += 2) {
        const std::string& code = lines[i];
        const std::string& value = lines[i + 1];
        if (code == "2" && value == "ENTITIES") {
            in_entities = true;
            continue;
        }
        if (!in_entities) {
            continue;
        }
        if (code == "0") {
            if (value == "ENDSEC") {
                flush_entity();
                break;
            }
            flush_entity();
            entity_type = value;
            continue;
        }
        if (entity_type.empty()) {
            continue;
        }
        if (code == "8") {
            entity_layer = value;
            continue;
        }
        int code_num = 0;
        try {
            code_num = std::stoi(code);
        } catch (...) {
            continue;
        }
        entity_values[code_num] = value;
    }
    return true;
}

static PointKey MakeKey(const Point2D& p, float32 eps) {
    const float32 inv = (eps <= 0.0f) ? 1.0f : (1.0f / eps);
    return {static_cast<int64_t>(std::llround(p.x * inv)),
            static_cast<int64_t>(std::llround(p.y * inv))};
}

static float32 Distance(const Point2D& a, const Point2D& b) {
    return Siligen::Shared::Geometry::Distance(a, b);
}

static bool IsClosedLoop(const std::vector<Point2D>& points, float32 eps) {
    if (points.size() < 3) {
        return false;
    }
    return Distance(points.front(), points.back()) <= eps;
}

static void RemoveDuplicateTail(std::vector<Point2D>& points, float32 eps) {
    if (points.size() > 1 && Distance(points.front(), points.back()) <= eps) {
        points.pop_back();
    }
}

static std::vector<DxfPolyline> BuildPolylinesFromLines(const std::vector<DxfLine>& lines,
                                                        float32 eps) {
    std::vector<DxfPolyline> polylines;
    if (lines.empty()) {
        return polylines;
    }

    std::vector<Segment2D> segments;
    segments.reserve(lines.size());
    for (const auto& line : lines) {
        segments.push_back({line.a, line.b});
    }

    std::unordered_map<PointKey, std::vector<SegmentRef>, PointKeyHash> adjacency;
    adjacency.reserve(segments.size() * 2);

    for (std::size_t i = 0; i < segments.size(); ++i) {
        adjacency[MakeKey(segments[i].a, eps)].push_back({static_cast<int>(i), true});
        adjacency[MakeKey(segments[i].b, eps)].push_back({static_cast<int>(i), false});
    }

    std::vector<bool> used(segments.size(), false);

    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (used[i]) {
            continue;
        }
        used[i] = true;
        std::vector<Point2D> chain;
        chain.push_back(segments[i].a);
        chain.push_back(segments[i].b);
        Point2D start = segments[i].a;
        Point2D current = segments[i].b;

        while (true) {
            if (Distance(current, start) <= eps) {
                break;
            }
            const auto key = MakeKey(current, eps);
            auto it = adjacency.find(key);
            if (it == adjacency.end()) {
                break;
            }
            int next_index = -1;
            bool next_is_start = true;
            for (const auto& ref : it->second) {
                if (!used[ref.index]) {
                    next_index = ref.index;
                    next_is_start = ref.is_start;
                    break;
                }
            }
            if (next_index < 0) {
                break;
            }
            used[next_index] = true;
            const auto& seg = segments[next_index];
            Point2D next_point = next_is_start ? seg.b : seg.a;
            chain.push_back(next_point);
            current = next_point;
        }

        bool closed = IsClosedLoop(chain, eps);
        RemoveDuplicateTail(chain, eps);
        polylines.push_back({chain, closed, ""});
    }

    return polylines;
}

static std::pair<Point2D, Point2D> ComputeBounds(const std::vector<Point2D>& points) {
    Point2D min_pt(std::numeric_limits<float32>::max(), std::numeric_limits<float32>::max());
    Point2D max_pt(std::numeric_limits<float32>::lowest(), std::numeric_limits<float32>::lowest());
    for (const auto& p : points) {
        min_pt.x = std::min(min_pt.x, p.x);
        min_pt.y = std::min(min_pt.y, p.y);
        max_pt.x = std::max(max_pt.x, p.x);
        max_pt.y = std::max(max_pt.y, p.y);
    }
    return {min_pt, max_pt};
}

static float32 PolylineLength(const std::vector<Point2D>& points, bool closed) {
    if (points.size() < 2) {
        return 0.0f;
    }
    float32 total = 0.0f;
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        total += Distance(points[i], points[i + 1]);
    }
    if (closed) {
        total += Distance(points.back(), points.front());
    }
    return total;
}

static bool IntersectSegments(const Point2D& a,
                              const Point2D& b,
                              const Point2D& c,
                              const Point2D& d,
                              float32 eps,
                              Point2D& out_point) {
    bool collinear = false;
    return Siligen::Shared::Geometry::ComputeSegmentIntersection(a, b, c, d, eps, out_point, collinear);
}

static std::vector<IntersectionInfo> FindPolylineIntersections(const std::vector<Point2D>& a,
                                                                const std::vector<Point2D>& b,
                                                                bool closed_a,
                                                                bool closed_b,
                                                                float32 eps) {
    std::vector<IntersectionInfo> intersections;
    if (a.size() < 2 || b.size() < 2) {
        return intersections;
    }
    const std::size_t a_count = closed_a ? a.size() : (a.size() - 1);
    const std::size_t b_count = closed_b ? b.size() : (b.size() - 1);
    for (std::size_t i = 0; i < a_count; ++i) {
        const Point2D a0 = a[i];
        const Point2D a1 = a[(i + 1) % a.size()];
        for (std::size_t j = 0; j < b_count; ++j) {
            const Point2D b0 = b[j];
            const Point2D b1 = b[(j + 1) % b.size()];
            Point2D p;
            if (IntersectSegments(a0, a1, b0, b1, eps, p)) {
                intersections.push_back({p, static_cast<int>(i), static_cast<int>(j)});
            }
        }
    }
    return intersections;
}


static bool BuildCgalPolygon(const std::vector<Point2D>& poly, CgalPolygon2& out) {
    out.clear();
    if (poly.size() < 3) {
        return false;
    }
    out.reserve(poly.size());
    for (const auto& p : poly) {
        out.push_back(CgalPoint2(p.x, p.y));
    }
    if (!out.is_simple()) {
        return false;
    }
    if (!out.is_counterclockwise_oriented()) {
        out.reverse_orientation();
    }
    return out.size() >= 3;
}

static void AppendArcSamples(const CgalXMonotoneCurve& curve,
                             double max_error,
                             std::vector<Point2D>& out) {
    if (curve.is_linear()) {
        out.push_back(ToPoint(curve.target()));
        return;
    }
    const auto& source = curve.source();
    const auto& target = curve.target();
    const auto circle = curve.supporting_circle();
    const double cx = ToDouble(circle.center().x());
    const double cy = ToDouble(circle.center().y());
    const double sx = ToDouble(source.x()) - cx;
    const double sy = ToDouble(source.y()) - cy;
    const double ex = ToDouble(target.x()) - cx;
    const double ey = ToDouble(target.y()) - cy;
    const double radius = std::sqrt(ToDouble(circle.squared_radius()));
    if (radius <= 1e-9) {
        out.push_back(ToPoint(target));
        return;
    }
    double start_angle = std::atan2(sy, sx);
    double end_angle = std::atan2(ey, ex);
    double delta = end_angle - start_angle;
    if (curve.orientation() == CGAL::COUNTERCLOCKWISE) {
        if (delta <= 0.0) {
            delta += 2.0 * kPiDouble;
        }
    } else if (curve.orientation() == CGAL::CLOCKWISE) {
        if (delta >= 0.0) {
            delta -= 2.0 * kPiDouble;
        }
    } else {
        out.push_back(ToPoint(target));
        return;
    }
    const double abs_delta = std::abs(delta);
    double step = abs_delta;
    if (max_error > 0.0) {
        double cos_val = 1.0 - max_error / radius;
        cos_val = std::clamp(cos_val, -1.0, 1.0);
        const double candidate = 2.0 * std::acos(cos_val);
        if (candidate > 1e-9) {
            step = candidate;
        }
    }
    const int segments = std::max(1, static_cast<int>(std::ceil(abs_delta / step)));
    for (int i = 1; i <= segments; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(segments);
        const double angle = start_angle + delta * t;
        const double x = cx + radius * std::cos(angle);
        const double y = cy + radius * std::sin(angle);
        out.push_back(Point2D(static_cast<float32>(x), static_cast<float32>(y)));
    }
}

static std::vector<Point2D> FlattenOffsetBoundary(const CgalOffsetPolygon& boundary,
                                                  float32 merge_eps,
                                                  double max_error) {
    std::vector<Point2D> out;
    auto it = boundary.curves_begin();
    if (it == boundary.curves_end()) {
        return out;
    }
    out.push_back(ToPoint(it->source()));
    for (; it != boundary.curves_end(); ++it) {
        AppendArcSamples(*it, max_error, out);
    }
    RemoveDuplicateTail(out, merge_eps);
    return out;
}

static std::vector<Point2D> OffsetClosedPolyline(const std::vector<Point2D>& poly,
                                                 float32 offset,
                                                 float32 approx_epsilon,
                                                 float32 merge_eps) {
    std::vector<Point2D> out;
    if (poly.size() < 3 || std::abs(offset) <= 1e-6f) {
        return out;
    }
    CgalPolygon2 cgal_poly;
    if (!BuildCgalPolygon(poly, cgal_poly)) {
        return out;
    }
    const double eps = std::max(1e-6, static_cast<double>(approx_epsilon));
    try {
        if (offset > 0.0f) {
            const auto offset_poly =
                CGAL::approximated_offset_2(cgal_poly, CgalKernel::FT(offset), eps);
            out = FlattenOffsetBoundary(offset_poly.outer_boundary(), merge_eps, eps);
        }
    } catch (...) {
        out.clear();
    }
    return out;
}

static int FindMinYIndex(const std::vector<Point2D>& points, bool prefer_left) {
    int best = -1;
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (best < 0 || points[i].y < points[best].y - 1e-6f) {
            best = static_cast<int>(i);
        } else if (std::abs(points[i].y - points[best].y) <= 1e-6f) {
            if (prefer_left) {
                if (points[i].x < points[best].x) {
                    best = static_cast<int>(i);
                }
            } else {
                if (points[i].x > points[best].x) {
                    best = static_cast<int>(i);
                }
            }
        }
    }
    return best;
}

static int FindOuterExtremeIndex(const std::vector<Point2D>& points, bool is_left) {
    int best = -1;
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (best < 0) {
            best = static_cast<int>(i);
            continue;
        }
        if (is_left) {
            if (points[i].x < points[best].x) {
                best = static_cast<int>(i);
            }
        } else {
            if (points[i].x > points[best].x) {
                best = static_cast<int>(i);
            }
        }
    }
    return best;
}

static bool PathContainsIndex(int count, int start, int end, bool forward, int target) {
    if (count <= 0) {
        return false;
    }
    int idx = start;
    for (int step = 0; step <= count; ++step) {
        if (idx == target) {
            return true;
        }
        if (idx == end) {
            return false;
        }
        idx = forward ? (idx + 1) % count : (idx - 1 + count) % count;
    }
    return false;
}

static std::vector<Point2D> ExtractPath(const std::vector<Point2D>& points,
                                        int start,
                                        int end,
                                        bool forward) {
    std::vector<Point2D> path;
    if (points.empty()) {
        return path;
    }
    int idx = start;
    const int count = static_cast<int>(points.size());
    for (int step = 0; step <= count; ++step) {
        path.push_back(points[idx]);
        if (idx == end) {
            break;
        }
        idx = forward ? (idx + 1) % count : (idx - 1 + count) % count;
    }
    return path;
}

static std::vector<Point2D> InsertPoint(const std::vector<Point2D>& poly,
                                        int seg_index,
                                        const Point2D& point) {
    std::vector<Point2D> out;
    out.reserve(poly.size() + 1);
    for (int i = 0; i < static_cast<int>(poly.size()); ++i) {
        out.push_back(poly[i]);
        if (i == seg_index) {
            out.push_back(point);
        }
    }
    return out;
}

static std::string FormatFloat(float32 value) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(6);
    oss << value;
    return oss.str();
}

static void AppendLine(std::vector<std::string>& out_lines, const std::string& line) {
    out_lines.push_back(line);
}

static std::string GenerateHandle(uint32_t& handle_counter) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << handle_counter++;
    return oss.str();
}

static void WriteLwPolyline(std::vector<std::string>& out_lines,
                            const std::vector<Point2D>& points,
                            bool closed,
                            const std::string& layer,
                            int color_index,
                            uint32_t& handle_counter) {
    AppendLine(out_lines, "0");
    AppendLine(out_lines, "LWPOLYLINE");
    AppendLine(out_lines, "5");
    AppendLine(out_lines, GenerateHandle(handle_counter));
    AppendLine(out_lines, "100");
    AppendLine(out_lines, "AcDbEntity");
    AppendLine(out_lines, "8");
    AppendLine(out_lines, layer);
    AppendLine(out_lines, "62");
    AppendLine(out_lines, std::to_string(color_index));
    AppendLine(out_lines, "100");
    AppendLine(out_lines, "AcDbPolyline");
    AppendLine(out_lines, "90");
    AppendLine(out_lines, std::to_string(points.size()));
    AppendLine(out_lines, "70");
    AppendLine(out_lines, closed ? "1" : "0");
    for (const auto& p : points) {
        AppendLine(out_lines, "10");
        AppendLine(out_lines, FormatFloat(p.x));
        AppendLine(out_lines, "20");
        AppendLine(out_lines, FormatFloat(p.y));
    }
}

static void WritePoint(std::vector<std::string>& out_lines,
                       const Point2D& p,
                       const std::string& layer,
                       int color_index,
                       uint32_t& handle_counter) {
    AppendLine(out_lines, "0");
    AppendLine(out_lines, "POINT");
    AppendLine(out_lines, "5");
    AppendLine(out_lines, GenerateHandle(handle_counter));
    AppendLine(out_lines, "100");
    AppendLine(out_lines, "AcDbEntity");
    AppendLine(out_lines, "8");
    AppendLine(out_lines, layer);
    AppendLine(out_lines, "62");
    AppendLine(out_lines, std::to_string(color_index));
    AppendLine(out_lines, "100");
    AppendLine(out_lines, "AcDbPoint");
    AppendLine(out_lines, "10");
    AppendLine(out_lines, FormatFloat(p.x));
    AppendLine(out_lines, "20");
    AppendLine(out_lines, FormatFloat(p.y));
    AppendLine(out_lines, "30");
    AppendLine(out_lines, "0.0");
}

static void WritePolylineR12(std::vector<std::string>& out_lines,
                             const std::vector<Point2D>& points,
                             bool closed,
                             const std::string& layer,
                             int color_index,
                             uint32_t& handle_counter) {
    AppendLine(out_lines, "0");
    AppendLine(out_lines, "POLYLINE");
    AppendLine(out_lines, "5");
    AppendLine(out_lines, GenerateHandle(handle_counter));
    AppendLine(out_lines, "8");
    AppendLine(out_lines, layer);
    AppendLine(out_lines, "62");
    AppendLine(out_lines, std::to_string(color_index));
    AppendLine(out_lines, "66");
    AppendLine(out_lines, "1");
    AppendLine(out_lines, "70");
    AppendLine(out_lines, closed ? "1" : "0");
    AppendLine(out_lines, "10");
    AppendLine(out_lines, "0.0");
    AppendLine(out_lines, "20");
    AppendLine(out_lines, "0.0");
    AppendLine(out_lines, "30");
    AppendLine(out_lines, "0.0");
    for (const auto& p : points) {
        AppendLine(out_lines, "0");
        AppendLine(out_lines, "VERTEX");
        AppendLine(out_lines, "5");
        AppendLine(out_lines, GenerateHandle(handle_counter));
        AppendLine(out_lines, "8");
        AppendLine(out_lines, layer);
        AppendLine(out_lines, "62");
        AppendLine(out_lines, std::to_string(color_index));
        AppendLine(out_lines, "10");
        AppendLine(out_lines, FormatFloat(p.x));
        AppendLine(out_lines, "20");
        AppendLine(out_lines, FormatFloat(p.y));
        AppendLine(out_lines, "30");
        AppendLine(out_lines, "0.0");
        AppendLine(out_lines, "70");
        AppendLine(out_lines, "0");
    }
    AppendLine(out_lines, "0");
    AppendLine(out_lines, "SEQEND");
    AppendLine(out_lines, "5");
    AppendLine(out_lines, GenerateHandle(handle_counter));
}

static void WritePointR12(std::vector<std::string>& out_lines,
                          const Point2D& p,
                          const std::string& layer,
                          int color_index,
                          uint32_t& handle_counter) {
    AppendLine(out_lines, "0");
    AppendLine(out_lines, "POINT");
    AppendLine(out_lines, "5");
    AppendLine(out_lines, GenerateHandle(handle_counter));
    AppendLine(out_lines, "8");
    AppendLine(out_lines, layer);
    AppendLine(out_lines, "62");
    AppendLine(out_lines, std::to_string(color_index));
    AppendLine(out_lines, "10");
    AppendLine(out_lines, FormatFloat(p.x));
    AppendLine(out_lines, "20");
    AppendLine(out_lines, FormatFloat(p.y));
    AppendLine(out_lines, "30");
    AppendLine(out_lines, "0.0");
}

static std::vector<Point2D> NormalizePath(std::vector<Point2D> path, float32 eps) {
    if (path.empty()) {
        return path;
    }
    std::vector<Point2D> out;
    out.reserve(path.size());
    out.push_back(path.front());
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (Distance(path[i], out.back()) > eps) {
            out.push_back(path[i]);
        }
    }
    return out;
}

static bool ChooseOuterPath(const std::vector<Point2D>& poly,
                            int start_idx,
                            int end_idx,
                            int outer_idx,
                            std::vector<Point2D>& out_path) {
    const int count = static_cast<int>(poly.size());
    const bool forward_has = PathContainsIndex(count, start_idx, end_idx, true, outer_idx);
    const bool backward_has = PathContainsIndex(count, start_idx, end_idx, false, outer_idx);
    if (forward_has && !backward_has) {
        out_path = ExtractPath(poly, start_idx, end_idx, true);
        return true;
    }
    if (!forward_has && backward_has) {
        out_path = ExtractPath(poly, start_idx, end_idx, false);
        return true;
    }
    const auto path_f = ExtractPath(poly, start_idx, end_idx, true);
    const auto path_b = ExtractPath(poly, start_idx, end_idx, false);
    const float32 len_f = PolylineLength(path_f, false);
    const float32 len_b = PolylineLength(path_b, false);
    out_path = (len_f <= len_b) ? path_f : path_b;
    return true;
}

static bool BuildCupPathWithInner(const std::vector<Point2D>& cup,
                                  const std::vector<Point2D>& inner,
                                  float32 offset,
                                  bool is_left,
                                  const DXFContourAugmentConfig& config,
                                  std::vector<Point2D>& out_path,
                                  Point2D& out_bottom) {
    auto offset_poly = OffsetClosedPolyline(cup, offset, config.offset_approx_epsilon, config.merge_epsilon);
    if (offset_poly.empty()) {
        return false;
    }
    auto intersections = FindPolylineIntersections(offset_poly, inner, true, true, config.intersect_epsilon);
    if (intersections.empty()) {
        return false;
    }
    auto best = intersections.front();
    for (const auto& inter : intersections) {
        if (inter.point.y > best.point.y) {
            best = inter;
        }
    }
    auto inserted = InsertPoint(offset_poly, best.seg_index_a, best.point);
    const int start_idx = best.seg_index_a + 1;
    const int bottom_idx = FindMinYIndex(inserted, is_left);
    const int outer_idx = FindOuterExtremeIndex(inserted, is_left);
    std::vector<Point2D> path;
    ChooseOuterPath(inserted, start_idx, bottom_idx, outer_idx, path);
    if (path.empty()) {
        return false;
    }
    out_path = NormalizePath(path, config.merge_epsilon);
    out_bottom = inserted[bottom_idx];
    return true;
}

static bool BuildCupPathWithCut(const std::vector<Point2D>& cup,
                                float32 offset,
                                float32 y_cut,
                                bool is_left,
                                const DXFContourAugmentConfig& config,
                                std::vector<Point2D>& out_path,
                                Point2D& out_bottom,
                                Point2D& out_cut_point) {
    auto offset_poly = OffsetClosedPolyline(cup, offset, config.offset_approx_epsilon, config.merge_epsilon);
    if (offset_poly.empty()) {
        return false;
    }
    std::vector<IntersectionInfo> intersections;
    for (std::size_t i = 0; i < offset_poly.size(); ++i) {
        const Point2D& p0 = offset_poly[i];
        const Point2D& p1 = offset_poly[(i + 1) % offset_poly.size()];
        const float32 dy0 = p0.y - y_cut;
        const float32 dy1 = p1.y - y_cut;
        if (std::abs(dy0) <= config.intersect_epsilon && std::abs(dy1) <= config.intersect_epsilon) {
            intersections.push_back({p0, static_cast<int>(i), -1});
            intersections.push_back({p1, static_cast<int>(i), -1});
            continue;
        }
        if ((dy0 > 0 && dy1 > 0) || (dy0 < 0 && dy1 < 0)) {
            continue;
        }
        if (std::abs(dy0 - dy1) <= config.intersect_epsilon) {
            continue;
        }
        const float32 t = (y_cut - p0.y) / (p1.y - p0.y);
        if (t < -config.intersect_epsilon || t > 1.0f + config.intersect_epsilon) {
            continue;
        }
        const Point2D ip = p0 + (p1 - p0) * t;
        intersections.push_back({ip, static_cast<int>(i), -1});
    }
    if (intersections.empty()) {
        return false;
    }
    IntersectionInfo selected = intersections.front();
    for (const auto& inter : intersections) {
        if (is_left) {
            if (inter.point.x < selected.point.x) {
                selected = inter;
            }
        } else {
            if (inter.point.x > selected.point.x) {
                selected = inter;
            }
        }
    }
    auto inserted = InsertPoint(offset_poly, selected.seg_index_a, selected.point);
    const int start_idx = selected.seg_index_a + 1;
    const int bottom_idx = FindMinYIndex(inserted, is_left);
    const int outer_idx = FindOuterExtremeIndex(inserted, is_left);
    std::vector<Point2D> path;
    ChooseOuterPath(inserted, start_idx, bottom_idx, outer_idx, path);
    if (path.empty()) {
        return false;
    }
    out_path = NormalizePath(path, config.merge_epsilon);
    out_bottom = inserted[bottom_idx];
    out_cut_point = selected.point;
    return true;
}

static void AppendPath(std::vector<Point2D>& target, const std::vector<Point2D>& path, float32 eps) {
    if (path.empty()) {
        return;
    }
    if (target.empty()) {
        target = path;
        return;
    }
    if (Distance(target.back(), path.front()) > eps) {
        target.push_back(path.front());
    }
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (Distance(path[i], target.back()) > eps) {
            target.push_back(path[i]);
        }
    }
}

static std::vector<Point2D> BuildBridgePathWithInner(const std::vector<Point2D>& left_cup,
                                                     const std::vector<Point2D>& right_cup,
                                                     const std::vector<Point2D>& inner,
                                                     float32 offset,
                                                     const DXFContourAugmentConfig& config) {
    std::vector<Point2D> left_path;
    std::vector<Point2D> right_path;
    Point2D left_bottom;
    Point2D right_bottom;
    if (!BuildCupPathWithInner(left_cup, inner, offset, true, config, left_path, left_bottom)) {
        return {};
    }
    if (!BuildCupPathWithInner(right_cup, inner, offset, false, config, right_path, right_bottom)) {
        return {};
    }
    std::reverse(right_path.begin(), right_path.end());
    std::vector<Point2D> bridge = left_path;
    if (Distance(bridge.back(), right_bottom) > config.merge_epsilon) {
        bridge.push_back(right_bottom);
    }
    AppendPath(bridge, right_path, config.merge_epsilon);
    return NormalizePath(bridge, config.merge_epsilon);
}

static std::vector<Point2D> BuildBridgePathWithCut(const std::vector<Point2D>& left_cup,
                                                   const std::vector<Point2D>& right_cup,
                                                   float32 offset,
                                                   float32 y_cut,
                                                   const DXFContourAugmentConfig& config) {
    std::vector<Point2D> left_path;
    std::vector<Point2D> right_path;
    Point2D left_bottom;
    Point2D right_bottom;
    Point2D left_cut;
    Point2D right_cut;
    if (!BuildCupPathWithCut(left_cup, offset, y_cut, true, config, left_path, left_bottom, left_cut)) {
        return {};
    }
    if (!BuildCupPathWithCut(right_cup, offset, y_cut, false, config, right_path, right_bottom, right_cut)) {
        return {};
    }
    std::reverse(right_path.begin(), right_path.end());
    std::vector<Point2D> bridge = left_path;
    if (Distance(bridge.back(), right_bottom) > config.merge_epsilon) {
        bridge.push_back(right_bottom);
    }
    AppendPath(bridge, right_path, config.merge_epsilon);
    return NormalizePath(bridge, config.merge_epsilon);
}

static std::vector<Point2D> GenerateGridPoints(const std::vector<Point2D>& inner,
                                               const std::vector<Point2D>& left_cup,
                                               const std::vector<Point2D>& right_cup,
                                               const DXFContourAugmentConfig& config) {
    std::vector<Point2D> points;
    if (inner.size() < 3) {
        return points;
    }
    auto bounds = ComputeBounds(inner);
    const Point2D min_pt = bounds.first;
    const Point2D max_pt = bounds.second;

    const float32 angle_rad = config.grid_angle_deg * kPi / 180.0f;
    const Point2D u(std::cos(angle_rad), std::sin(angle_rad));
    const Point2D v(-u.y, u.x);
    const float32 spacing = config.grid_spacing;
    if (spacing <= 1e-6f) {
        return points;
    }
    const float32 u_anchor = min_pt.Dot(u);
    const float32 v_anchor = min_pt.Dot(v);
    const float32 u0 = u_anchor + config.grid_u_offset;
    const float32 v0 = v_anchor + config.grid_v_offset;

    const std::vector<Point2D> corners = {
        Point2D(min_pt.x, min_pt.y),
        Point2D(min_pt.x, max_pt.y),
        Point2D(max_pt.x, min_pt.y),
        Point2D(max_pt.x, max_pt.y),
    };
    float32 u_min = std::numeric_limits<float32>::max();
    float32 u_max = std::numeric_limits<float32>::lowest();
    float32 v_min = std::numeric_limits<float32>::max();
    float32 v_max = std::numeric_limits<float32>::lowest();
    for (const auto& c : corners) {
        const float32 cu = c.Dot(u);
        const float32 cv = c.Dot(v);
        u_min = std::min(u_min, cu);
        u_max = std::max(u_max, cu);
        v_min = std::min(v_min, cv);
        v_max = std::max(v_max, cv);
    }

    const int i_min = static_cast<int>(std::floor((u_min - u0) / spacing)) - 2;
    const int i_max = static_cast<int>(std::ceil((u_max - u0) / spacing)) + 2;
    const int j_min = static_cast<int>(std::floor((v_min - v0) / spacing)) - 2;
    const int j_max = static_cast<int>(std::ceil((v_max - v0) / spacing)) + 2;

    for (int i = i_min; i <= i_max; ++i) {
        for (int j = j_min; j <= j_max; ++j) {
            const float32 uu = u0 + spacing * static_cast<float32>(i);
            const float32 vv = v0 + spacing * static_cast<float32>(j);
            const Point2D p = u * uu + v * vv;
            if (p.x < min_pt.x - spacing || p.x > max_pt.x + spacing ||
                p.y < min_pt.y - spacing || p.y > max_pt.y + spacing) {
                continue;
            }
            const bool inside = IsPointInPolygon(p, inner);
            const float32 dist_to_inner = DistancePointToPolyline(p, inner, true);
            if (!inside && dist_to_inner > config.grid_border_tolerance) {
                continue;
            }
            const float32 dist_left = DistancePointToPolyline(p, left_cup, true);
            const float32 dist_right = DistancePointToPolyline(p, right_cup, true);
            const float32 dist_cup = std::min(dist_left, dist_right);
            if (dist_cup < config.grid_min_distance_to_cups - config.grid_border_tolerance) {
                continue;
            }
            points.push_back(p);
        }
    }
    return points;
}

}  // namespace

Result<void> DXFContourAugmenter::ConvertFile(const std::string& input_path,
                                              const std::string& output_path,
                                              const DXFContourAugmentConfig& config) {
    std::vector<std::string> lines;
    if (!ReadAllLines(input_path, lines)) {
        return Result<void>::Failure(Error(ErrorCode::FILE_IO_ERROR, "Failed to read DXF file: " + input_path));
    }

    DxfTextSections sections;
    if (!SplitDxfByEntities(lines, sections)) {
        return Result<void>::Failure(Error(ErrorCode::FILE_FORMAT_INVALID, "Failed to parse DXF ENTITIES section"));
    }

    std::vector<DxfLine> dxf_lines;
    std::vector<DxfPoint> dxf_points;
    if (!ParseDxfLines(lines, dxf_lines, dxf_points)) {
        return Result<void>::Failure(Error(ErrorCode::FILE_FORMAT_INVALID, "Failed to parse DXF entities"));
    }
    static_cast<void>(dxf_points);
    if (dxf_lines.empty()) {
        return Result<void>::Failure(Error(ErrorCode::FILE_FORMAT_INVALID, "DXF has no LINE contours"));
    }

    auto polylines = BuildPolylinesFromLines(dxf_lines, config.merge_epsilon);
    std::vector<DxfPolyline> closed_loops;
    closed_loops.reserve(polylines.size());
    for (auto& poly : polylines) {
        if (poly.closed && poly.points.size() >= 3) {
            closed_loops.push_back(poly);
        }
    }
    if (closed_loops.size() < 4) {
        return Result<void>::Failure(Error(ErrorCode::FILE_FORMAT_INVALID, "Not enough closed loops to detect contours"));
    }

    struct LoopInfo {
        std::size_t index;
        float32 area;
        Point2D min_pt;
        Point2D max_pt;
        Point2D center;
    };

    std::vector<LoopInfo> loop_infos;
    loop_infos.reserve(closed_loops.size());
    for (std::size_t i = 0; i < closed_loops.size(); ++i) {
        const auto bounds = ComputeBounds(closed_loops[i].points);
        const float32 area = (bounds.second.x - bounds.first.x) * (bounds.second.y - bounds.first.y);
        Point2D center(0.0f, 0.0f);
        for (const auto& p : closed_loops[i].points) {
            center += p;
        }
        center = center / static_cast<float32>(closed_loops[i].points.size());
        loop_infos.push_back({i, area, bounds.first, bounds.second, center});
    }
    std::sort(loop_infos.begin(), loop_infos.end(), [](const LoopInfo& a, const LoopInfo& b) {
        return a.area > b.area;
    });

    const auto& outer_loop = closed_loops[loop_infos[0].index].points;
    const auto& inner_loop = closed_loops[loop_infos[1].index].points;
    const auto& cup_a = closed_loops[loop_infos[2].index].points;
    const auto& cup_b = closed_loops[loop_infos[3].index].points;
    const auto& cup_info_a = loop_infos[2];
    const auto& cup_info_b = loop_infos[3];

    const bool cup_a_is_left = cup_info_a.center.x < cup_info_b.center.x;
    const std::vector<Point2D>& left_cup = cup_a_is_left ? cup_a : cup_b;
    const std::vector<Point2D>& right_cup = cup_a_is_left ? cup_b : cup_a;
    const LoopInfo& left_info = cup_a_is_left ? cup_info_a : cup_info_b;
    const LoopInfo& right_info = cup_a_is_left ? cup_info_b : cup_info_a;

    const float32 cup_min_y = std::min(left_info.min_pt.y, right_info.min_pt.y);
    const float32 cup_max_y = std::max(left_info.max_pt.y, right_info.max_pt.y);
    const float32 y_cut = cup_max_y - config.mid_path_cut_ratio * (cup_max_y - cup_min_y);

    const auto path_far = BuildBridgePathWithInner(left_cup, right_cup, inner_loop, config.cup_offset_far, config);
    const auto path_mid = BuildBridgePathWithInner(left_cup, right_cup, inner_loop, config.cup_offset_mid, config);
    const auto path_near = BuildBridgePathWithCut(left_cup, right_cup, config.cup_offset_near, y_cut, config);

    if (path_far.empty() || path_mid.empty() || path_near.empty()) {
        return Result<void>::Failure(Error(ErrorCode::FILE_FORMAT_INVALID, "Failed to generate glue paths"));
    }

    const auto grid_points = GenerateGridPoints(inner_loop, left_cup, right_cup, config);

    std::vector<std::string> output_lines = sections.prefix_lines;
    const bool use_r12 = config.output_r12;
    UpdateAcadVersion(output_lines, use_r12 ? "AC1009" : "AC1018");
    UpdateHeaderValue(output_lines, "$PDMODE", "3");
    UpdateHeaderValue(output_lines, "$PDSIZE", "2.0");

    const auto layer_names = ExtractLayerNames(sections.prefix_lines);
    const std::string contour_layer = ResolveLayerName(layer_names, config.contour_layer);
    const std::string glue_layer = ResolveLayerName(layer_names, config.glue_layer);
    const std::string point_layer = ResolveLayerName(layer_names, config.point_layer);

    uint32_t handle_counter = 0x1000;

    if (use_r12) {
        WritePolylineR12(output_lines, outer_loop, true, contour_layer, 7, handle_counter);
        WritePolylineR12(output_lines, inner_loop, true, contour_layer, 7, handle_counter);
        WritePolylineR12(output_lines, left_cup, true, glue_layer, 7, handle_counter);
        WritePolylineR12(output_lines, right_cup, true, glue_layer, 7, handle_counter);

        WritePolylineR12(output_lines, path_far, false, glue_layer, 7, handle_counter);
        WritePolylineR12(output_lines, path_mid, false, glue_layer, 7, handle_counter);
        WritePolylineR12(output_lines, path_near, false, glue_layer, 7, handle_counter);

        for (const auto& p : grid_points) {
            WritePointR12(output_lines, p, point_layer, 7, handle_counter);
        }
    } else {
        WriteLwPolyline(output_lines, outer_loop, true, contour_layer, 7, handle_counter);
        WriteLwPolyline(output_lines, inner_loop, true, contour_layer, 7, handle_counter);
        WriteLwPolyline(output_lines, left_cup, true, glue_layer, 7, handle_counter);
        WriteLwPolyline(output_lines, right_cup, true, glue_layer, 7, handle_counter);

        WriteLwPolyline(output_lines, path_far, false, glue_layer, 7, handle_counter);
        WriteLwPolyline(output_lines, path_mid, false, glue_layer, 7, handle_counter);
        WriteLwPolyline(output_lines, path_near, false, glue_layer, 7, handle_counter);

        for (const auto& p : grid_points) {
            WritePoint(output_lines, p, point_layer, 7, handle_counter);
        }
    }

    AppendLine(output_lines, "0");
    AppendLine(output_lines, "ENDSEC");
    output_lines.insert(output_lines.end(), sections.suffix_lines.begin(), sections.suffix_lines.end());

    std::ofstream out(output_path);
    if (!out.is_open()) {
        return Result<void>::Failure(Error(ErrorCode::FILE_IO_ERROR, "Failed to write DXF file: " + output_path));
    }
    for (const auto& line : output_lines) {
        out << line << "\n";
    }
    return Result<void>::Success();
}

}  // namespace Siligen::Application::UseCases::Dispensing::DXF
