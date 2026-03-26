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

namespace Siligen {
namespace Domain {
namespace Planning {
namespace Ports {

/**
 * @brief 空间索引项（带ID的点）
 */
struct SpatialItem {
    size_t id;                      ///< 项目ID（通常是段索引）
    Shared::Types::Point2D point;   ///< 空间坐标
};

/**
 * @brief 最近邻查询结果
 */
struct NearestNeighborResult {
    size_t id;                      ///< 最近项的ID
    Shared::Types::Point2D point;   ///< 最近项的坐标
    float32 distance;               ///< 距离
};

/**
 * @brief 空间索引端口接口
 *
 * 职责：提供高效的空间查询能力
 * - 最近邻查询：O(log n)
 * - 范围查询：O(log n + k)，k为结果数量
 *
 * 典型实现：R-tree、KD-tree
 */
class ISpatialIndexPort {
   public:
    virtual ~ISpatialIndexPort() = default;

    /**
     * @brief 清空索引
     */
    virtual void Clear() noexcept = 0;

    /**
     * @brief 批量构建索引
     * @param items 空间项列表
     * @return 成功返回void，失败返回错误
     */
    virtual Shared::Types::Result<void> Build(const std::vector<SpatialItem>& items) noexcept = 0;

    /**
     * @brief 插入单个项
     * @param item 空间项
     * @return 成功返回void，失败返回错误
     */
    virtual Shared::Types::Result<void> Insert(const SpatialItem& item) noexcept = 0;

    /**
     * @brief 删除指定ID的项
     * @param id 项目ID
     * @return 成功返回void，失败返回错误
     */
    virtual Shared::Types::Result<void> Remove(size_t id) noexcept = 0;

    /**
     * @brief 查询最近邻
     * @param query_point 查询点
     * @return 最近邻结果，如果索引为空返回nullopt
     */
    virtual std::optional<NearestNeighborResult> FindNearest(
        const Shared::Types::Point2D& query_point) const noexcept = 0;

    /**
     * @brief 查询K个最近邻
     * @param query_point 查询点
     * @param k 返回数量
     * @return K个最近邻结果（按距离升序）
     */
    virtual std::vector<NearestNeighborResult> FindKNearest(
        const Shared::Types::Point2D& query_point,
        size_t k) const noexcept = 0;

    /**
     * @brief 范围查询（矩形区域）
     * @param min_point 最小点（左下角）
     * @param max_point 最大点（右上角）
     * @return 区域内的所有项
     */
    virtual std::vector<SpatialItem> QueryRange(
        const Shared::Types::Point2D& min_point,
        const Shared::Types::Point2D& max_point) const noexcept = 0;

    /**
     * @brief 获取索引中的项数量
     */
    virtual size_t Size() const noexcept = 0;

    /**
     * @brief 检查索引是否为空
     */
    virtual bool Empty() const noexcept = 0;
};

}  // namespace Ports
}  // namespace Planning
}  // namespace Domain
}  // namespace Siligen
