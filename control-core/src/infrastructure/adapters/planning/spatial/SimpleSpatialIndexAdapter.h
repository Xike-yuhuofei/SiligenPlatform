/**
 * @file SimpleSpatialIndexAdapter.h
 * @brief 简化空间索引适配器 - 基于排序数组的实现
 *
 * 六边形架构 - Infrastructure层适配器
 * 实现ISpatialIndexPort接口，提供O(n)最近邻查询
 *
 * 注意：这是一个简化实现，适用于中等规模数据（<10000段）
 * 对于更大规模数据，建议使用完整的R-tree库（如boost::geometry）
 */

#pragma once

#include "domain/planning/ports/ISpatialIndexPort.h"
#include "shared/Geometry/BoostGeometryAdapter.h"

#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/geometries/box.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {
namespace Planning {
namespace Spatial {

/**
 * @brief 简化空间索引适配器
 *
 * 实现策略：
 * - 使用排序数组存储点
 * - 最近邻查询：O(n)，但常数因子小
 * - 适用于段数量 < 10000 的场景
 */
class SimpleSpatialIndexAdapter : public Domain::Planning::Ports::ISpatialIndexPort {
   public:
    SimpleSpatialIndexAdapter() = default;
    ~SimpleSpatialIndexAdapter() override = default;

    void Clear() noexcept override {
        rtree_.clear();
        id_to_point_.clear();
    }

    Shared::Types::Result<void> Build(
        const std::vector<Domain::Planning::Ports::SpatialItem>& items) noexcept override {
        Clear();
        for (const auto& item : items) {
            if (id_to_point_.find(item.id) != id_to_point_.end()) {
                return Shared::Types::Result<void>::Failure(
                    Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                         "Item with this ID already exists"));
            }
            rtree_.insert(Value(item.point, item.id));
            id_to_point_[item.id] = item.point;
        }
        return Shared::Types::Result<void>::Success();
    }

    Shared::Types::Result<void> Insert(
        const Domain::Planning::Ports::SpatialItem& item) noexcept override {
        if (id_to_point_.find(item.id) != id_to_point_.end()) {
            return Shared::Types::Result<void>::Failure(
                Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                     "Item with this ID already exists"));
        }
        rtree_.insert(Value(item.point, item.id));
        id_to_point_[item.id] = item.point;
        return Shared::Types::Result<void>::Success();
    }

    Shared::Types::Result<void> Remove(size_t id) noexcept override {
        auto it = id_to_point_.find(id);
        if (it == id_to_point_.end()) {
            return Shared::Types::Result<void>::Failure(
                Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                     "Item not found"));
        }
        rtree_.remove(Value(it->second, id));
        id_to_point_.erase(it);
        return Shared::Types::Result<void>::Success();
    }

    std::optional<Domain::Planning::Ports::NearestNeighborResult> FindNearest(
        const Shared::Types::Point2D& query_point) const noexcept override {
        if (rtree_.empty()) {
            return std::nullopt;
        }
        std::vector<Value> results;
        results.reserve(1);
        rtree_.query(boost::geometry::index::nearest(query_point, 1), std::back_inserter(results));
        if (results.empty()) {
            return std::nullopt;
        }

        Domain::Planning::Ports::NearestNeighborResult result;
        result.id = results.front().second;
        result.point = results.front().first;
        result.distance = boost::geometry::distance(query_point, result.point);
        return result;
    }

    std::vector<Domain::Planning::Ports::NearestNeighborResult> FindKNearest(
        const Shared::Types::Point2D& query_point,
        size_t k) const noexcept override {
        std::vector<Domain::Planning::Ports::NearestNeighborResult> results;
        if (rtree_.empty() || k == 0) {
            return results;
        }
        std::vector<Value> matches;
        matches.reserve(k);
        rtree_.query(boost::geometry::index::nearest(query_point, k), std::back_inserter(matches));

        results.reserve(matches.size());
        for (const auto& match : matches) {
            Domain::Planning::Ports::NearestNeighborResult r;
            r.id = match.second;
            r.point = match.first;
            r.distance = boost::geometry::distance(query_point, r.point);
            results.push_back(r);
        }
        return results;
    }

    std::vector<Domain::Planning::Ports::SpatialItem> QueryRange(
        const Shared::Types::Point2D& min_point,
        const Shared::Types::Point2D& max_point) const noexcept override {
        std::vector<Domain::Planning::Ports::SpatialItem> results;
        using Box = boost::geometry::model::box<Shared::Types::Point2D>;
        Box box(min_point, max_point);
        std::vector<Value> matches;
        rtree_.query(boost::geometry::index::intersects(box), std::back_inserter(matches));
        results.reserve(matches.size());
        for (const auto& match : matches) {
            results.push_back({match.second, match.first});
        }
        return results;
    }

    size_t Size() const noexcept override {
        return rtree_.size();
    }

    bool Empty() const noexcept override {
        return rtree_.empty();
    }

   private:
    using Value = std::pair<Shared::Types::Point2D, size_t>;
    using RTree = boost::geometry::index::rtree<Value, boost::geometry::index::quadratic<16>>;

    RTree rtree_;
    std::unordered_map<size_t, Shared::Types::Point2D> id_to_point_;  // id -> point
};

}  // namespace Spatial
}  // namespace Planning
}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen

