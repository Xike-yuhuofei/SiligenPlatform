/**
 * @file ISpatialIndexPort.h
 * @brief 空间索引端口接口 - 加速最近邻搜索
 *
 * 六边形架构 - Domain层端口
 * 用于将O(n^2)的最近邻搜索优化为O(n log n)
 */

#pragma once

#include "shared/types/Point2D.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace Siligen::Domain::PlanningBoundary::Ports {

struct SpatialItem {
    size_t id;
    Shared::Types::Point2D point;
};

struct NearestNeighborResult {
    size_t id;
    Shared::Types::Point2D point;
    float32 distance;
};

class ISpatialIndexPort {
   public:
    virtual ~ISpatialIndexPort() = default;

    virtual void Clear() noexcept = 0;
    virtual Shared::Types::Result<void> Build(const std::vector<SpatialItem>& items) noexcept = 0;
    virtual Shared::Types::Result<void> Insert(const SpatialItem& item) noexcept = 0;
    virtual Shared::Types::Result<void> Remove(size_t id) noexcept = 0;
    virtual std::optional<NearestNeighborResult> FindNearest(
        const Shared::Types::Point2D& query_point) const noexcept = 0;
    virtual std::vector<NearestNeighborResult> FindKNearest(
        const Shared::Types::Point2D& query_point,
        size_t k) const noexcept = 0;
    virtual std::vector<SpatialItem> QueryRange(
        const Shared::Types::Point2D& min_point,
        const Shared::Types::Point2D& max_point) const noexcept = 0;
    virtual size_t Size() const noexcept = 0;
    virtual bool Empty() const noexcept = 0;
};

}  // namespace Siligen::Domain::PlanningBoundary::Ports
