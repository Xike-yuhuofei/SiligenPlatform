#include "PathOptimizationStrategy.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Siligen {
namespace Domain {
namespace Dispensing {
namespace DomainServices {

PathOptimizationStrategy::PathOptimizationStrategy(
    std::shared_ptr<PlanningBoundary::Ports::ISpatialIndexPort> spatial_index) noexcept
    : spatial_index_(std::move(spatial_index)) {}

float32 PathOptimizationStrategy::Distance(const Shared::Types::Point2D& a, const Shared::Types::Point2D& b) noexcept {
    return a.DistanceTo(b);
}

std::vector<SegmentWithDirection> PathOptimizationStrategy::OptimizeByNearestNeighbor(
    const std::vector<Shared::Types::DXFSegment>& segments,
    const Shared::Types::Point2D& start_pos) noexcept {
    // 如果有空间索引且段数量较大，使用加速算法
    if (spatial_index_ && segments.size() > 50) {
        return OptimizeWithSpatialIndex(segments, start_pos);
    }

    // 原始O(n²)算法（小规模数据或无空间索引时使用）
    std::vector<SegmentWithDirection> order;
    std::vector<bool> visited(segments.size(), false);
    Shared::Types::Point2D current = start_pos;
    Shared::Types::Point2D last_dir;
    bool has_last_dir = false;

    constexpr float32 kPi = 3.14159265359f;
    constexpr float32 kDegToRad = kPi / 180.0f;
    constexpr float32 kDirectionPenaltyAngleDeg = 150.0f;
    constexpr float32 kDirectionPenaltyWeight = 5.0f;
    const float32 cos_threshold = std::cos(kDirectionPenaltyAngleDeg * kDegToRad);

    auto clamp = [](float32 v, float32 lo, float32 hi) {
        return std::max(lo, std::min(hi, v));
    };

    auto direction_penalty = [&](const Shared::Types::Point2D& candidate_dir) -> float32 {
        if (!has_last_dir) {
            return 0.0f;
        }
        float32 last_len = last_dir.Length();
        float32 cand_len = candidate_dir.Length();
        if (last_len <= 1e-6f || cand_len <= 1e-6f) {
            return 0.0f;
        }
        Shared::Types::Point2D n1 = last_dir / last_len;
        Shared::Types::Point2D n2 = candidate_dir / cand_len;
        float32 dot = clamp(n1.Dot(n2), -1.0f, 1.0f);
        if (dot >= cos_threshold) {
            return 0.0f;
        }
        return (cos_threshold - dot) / (cos_threshold + 1.0f);
    };

    for (size_t i = 0; i < segments.size(); ++i) {
        size_t nearest = 0;
        bool reversed = false;
        float32 min_cost = std::numeric_limits<float32>::max();

        for (size_t j = 0; j < segments.size(); ++j) {
            if (visited[j]) continue;

            // 计算正向连接距离（current → start_point）
            float32 forward_dist = Distance(current, segments[j].start_point);
            // 计算反向连接距离（current → end_point）
            float32 reverse_dist = Distance(current, segments[j].end_point);

            Shared::Types::Point2D forward_dir = segments[j].end_point - segments[j].start_point;
            Shared::Types::Point2D reverse_dir = segments[j].start_point - segments[j].end_point;
            float32 forward_cost = forward_dist + kDirectionPenaltyWeight * direction_penalty(forward_dir);
            float32 reverse_cost = reverse_dist + kDirectionPenaltyWeight * direction_penalty(reverse_dir);

            if (forward_cost < min_cost) {
                min_cost = forward_cost;
                nearest = j;
                reversed = false;
            }
            if (reverse_cost < min_cost) {
                min_cost = reverse_cost;
                nearest = j;
                reversed = true;
            }
        }

        order.push_back({nearest, reversed});
        visited[nearest] = true;
        // 更新当前位置为所选段的出口点
        current = reversed ? segments[nearest].start_point : segments[nearest].end_point;
        last_dir = reversed ? (segments[nearest].start_point - segments[nearest].end_point)
                            : (segments[nearest].end_point - segments[nearest].start_point);
        has_last_dir = last_dir.Length() > 1e-6f;
    }

    return order;
}

std::vector<SegmentWithDirection> PathOptimizationStrategy::TwoOptImprove(
    const std::vector<SegmentWithDirection>& initial_order,
    const std::vector<Shared::Types::DXFSegment>& segments,
    int max_iterations) noexcept {
    if (initial_order.size() < 4 || max_iterations <= 0) {
        return initial_order;
    }

    auto order = initial_order;
    bool improved = true;

    // 辅助函数：获取段的出口点
    auto GetExitPoint = [&](const SegmentWithDirection& sd) -> Shared::Types::Point2D {
        return sd.reversed ? segments[sd.index].start_point : segments[sd.index].end_point;
    };

    // 辅助函数：获取段的入口点
    auto GetEntryPoint = [&](const SegmentWithDirection& sd) -> Shared::Types::Point2D {
        return sd.reversed ? segments[sd.index].end_point : segments[sd.index].start_point;
    };

    while (improved && max_iterations-- > 0) {
        improved = false;

        for (size_t i = 0; i + 2 < order.size(); ++i) {
            for (size_t j = i + 2; j < order.size(); ++j) {
                // 计算反转前的连接距离
                float32 old_dist = Distance(GetExitPoint(order[i]), GetEntryPoint(order[i + 1]));
                if (j + 1 < order.size()) {
                    old_dist += Distance(GetExitPoint(order[j]), GetEntryPoint(order[j + 1]));
                }

                // 计算反转后的连接距离（反转后order[j]方向反转，order[i+1]方向反转）
                SegmentWithDirection reversed_j = {order[j].index, !order[j].reversed};
                SegmentWithDirection reversed_i1 = {order[i + 1].index, !order[i + 1].reversed};

                float32 new_dist = Distance(GetExitPoint(order[i]), GetEntryPoint(reversed_j));
                if (j + 1 < order.size()) {
                    new_dist += Distance(GetExitPoint(reversed_i1), GetEntryPoint(order[j + 1]));
                }

                // 如果新距离更短，执行反转
                if (new_dist < old_dist) {
                    // 反转区间 [i+1, j]
                    std::reverse(order.begin() + i + 1, order.begin() + j + 1);
                    // 同时反转每个段的方向标志
                    for (size_t k = i + 1; k <= j; ++k) {
                        order[k].reversed = !order[k].reversed;
                    }
                    improved = true;
                }
            }
        }
    }

    return order;
}

std::vector<SegmentWithDirection> PathOptimizationStrategy::OptimizeWithSpatialIndex(
    const std::vector<Shared::Types::DXFSegment>& segments,
    const Shared::Types::Point2D& start_pos) noexcept {
    // 构建空间索引：每个段有两个点（起点和终点）
    // ID编码：偶数ID = 段i的起点，奇数ID = 段i的终点
    std::vector<PlanningBoundary::Ports::SpatialItem> items;
    items.reserve(segments.size() * 2);
    for (size_t i = 0; i < segments.size(); ++i) {
        items.push_back({i * 2, segments[i].start_point});      // 起点
        items.push_back({i * 2 + 1, segments[i].end_point});    // 终点
    }
    spatial_index_->Build(items);

    std::vector<SegmentWithDirection> order;
    std::vector<bool> visited(segments.size(), false);
    Shared::Types::Point2D current = start_pos;
    Shared::Types::Point2D last_dir;
    bool has_last_dir = false;

    constexpr float32 kPi = 3.14159265359f;
    constexpr float32 kDegToRad = kPi / 180.0f;
    constexpr float32 kDirectionPenaltyAngleDeg = 150.0f;
    constexpr float32 kDirectionPenaltyWeight = 5.0f;
    const float32 cos_threshold = std::cos(kDirectionPenaltyAngleDeg * kDegToRad);

    auto clamp = [](float32 v, float32 lo, float32 hi) {
        return std::max(lo, std::min(hi, v));
    };

    auto direction_penalty = [&](const Shared::Types::Point2D& candidate_dir) -> float32 {
        if (!has_last_dir) {
            return 0.0f;
        }
        float32 last_len = last_dir.Length();
        float32 cand_len = candidate_dir.Length();
        if (last_len <= 1e-6f || cand_len <= 1e-6f) {
            return 0.0f;
        }
        Shared::Types::Point2D n1 = last_dir / last_len;
        Shared::Types::Point2D n2 = candidate_dir / cand_len;
        float32 dot = clamp(n1.Dot(n2), -1.0f, 1.0f);
        if (dot >= cos_threshold) {
            return 0.0f;
        }
        return (cos_threshold - dot) / (cos_threshold + 1.0f);
    };

    for (size_t count = 0; count < segments.size(); ++count) {
        // 查询K个最近邻（K足够大以覆盖已访问的段）
        size_t k = std::min(segments.size() * 2, static_cast<size_t>(20));
        auto neighbors = spatial_index_->FindKNearest(current, k);

        size_t nearest = 0;
        bool reversed = false;
        float32 min_cost = std::numeric_limits<float32>::max();
        bool found = false;

        for (const auto& neighbor : neighbors) {
            size_t seg_idx = neighbor.id / 2;
            bool is_end_point = (neighbor.id % 2 == 1);

            if (visited[seg_idx]) continue;

            Shared::Types::Point2D forward_dir = segments[seg_idx].end_point - segments[seg_idx].start_point;
            Shared::Types::Point2D reverse_dir = segments[seg_idx].start_point - segments[seg_idx].end_point;
            float32 forward_dist = Distance(current, segments[seg_idx].start_point);
            float32 reverse_dist = Distance(current, segments[seg_idx].end_point);
            float32 forward_cost = forward_dist + kDirectionPenaltyWeight * direction_penalty(forward_dir);
            float32 reverse_cost = reverse_dist + kDirectionPenaltyWeight * direction_penalty(reverse_dir);
            float32 candidate_cost = is_end_point ? reverse_cost : forward_cost;

            if (candidate_cost < min_cost) {
                min_cost = candidate_cost;
                nearest = seg_idx;
                reversed = is_end_point;  // 如果最近点是终点，则反向
                found = true;
            }
        }

        // 如果空间索引没找到（极端情况），回退到线性搜索
        if (!found) {
            for (size_t j = 0; j < segments.size(); ++j) {
                if (visited[j]) continue;
                float32 forward_dist = Distance(current, segments[j].start_point);
                float32 reverse_dist = Distance(current, segments[j].end_point);
                Shared::Types::Point2D forward_dir = segments[j].end_point - segments[j].start_point;
                Shared::Types::Point2D reverse_dir = segments[j].start_point - segments[j].end_point;
                float32 forward_cost = forward_dist + kDirectionPenaltyWeight * direction_penalty(forward_dir);
                float32 reverse_cost = reverse_dist + kDirectionPenaltyWeight * direction_penalty(reverse_dir);
                if (forward_cost < min_cost) {
                    min_cost = forward_cost;
                    nearest = j;
                    reversed = false;
                }
                if (reverse_cost < min_cost) {
                    min_cost = reverse_cost;
                    nearest = j;
                    reversed = true;
                }
            }
        }

        order.push_back({nearest, reversed});
        visited[nearest] = true;
        current = reversed ? segments[nearest].start_point : segments[nearest].end_point;
        last_dir = reversed ? (segments[nearest].start_point - segments[nearest].end_point)
                            : (segments[nearest].end_point - segments[nearest].start_point);
        has_last_dir = last_dir.Length() > 1e-6f;

        // 从空间索引中移除已访问段的两个点
        spatial_index_->Remove(nearest * 2);
        spatial_index_->Remove(nearest * 2 + 1);
    }

    return order;
}

}  // namespace DomainServices
}  // namespace Dispensing
}  // namespace Domain
}  // namespace Siligen
