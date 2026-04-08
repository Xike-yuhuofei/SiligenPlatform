// PathOptimizationStrategy.h
// 版本: 1.1.0
// 描述: 路径优化策略领域服务
// 职责: 实现点胶路径顺序优化的业务逻辑

#pragma once

#include "domain/planning-boundary/ports/ISpatialIndexPort.h"
#include "shared/types/Point2D.h"
#include "shared/types/Types.h"
#include "shared/types/VisualizationTypes.h"

#include <memory>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Dispensing {
namespace DomainServices {

/**
 * @brief 带方向的段索引
 */
struct SegmentWithDirection {
    size_t index;   // 段索引
    bool reversed;  // 是否反向点胶（true=从end到start）
};

/**
 * @brief 路径优化配置
 */
struct PathOptimizationConfig {
    Shared::Types::Point2D start_position{0.0f, 0.0f};
    int two_opt_iterations = 100;  // 2-Opt迭代次数（0=禁用）
};

/**
 * @brief 路径优化策略领域服务
 * @details 实现点胶路径顺序优化的业务规则
 *
 * 业务规则:
 * - 最近邻算法：选择距离当前位置最近的未访问段
 * - 段方向选择：自动选择正向或反向以最小化空移距离
 * - 2-Opt局部搜索：进一步优化总空移距离
 *
 * 性能约束:
 * - 无空间索引: O(n²) 时间复杂度
 * - 有空间索引: O(n log n) 时间复杂度
 * - 2-Opt优化: O(n² × iterations) 时间复杂度
 */
class PathOptimizationStrategy {
   public:
    PathOptimizationStrategy() = default;

    /**
     * @brief 带空间索引的构造函数（推荐用于大规模数据）
     * @param spatial_index 空间索引端口（可选）
     */
    explicit PathOptimizationStrategy(
        std::shared_ptr<PlanningBoundary::Ports::ISpatialIndexPort> spatial_index) noexcept;

    ~PathOptimizationStrategy() = default;

    // 禁止拷贝和移动
    PathOptimizationStrategy(const PathOptimizationStrategy&) = delete;
    PathOptimizationStrategy& operator=(const PathOptimizationStrategy&) = delete;
    PathOptimizationStrategy(PathOptimizationStrategy&&) = delete;
    PathOptimizationStrategy& operator=(PathOptimizationStrategy&&) = delete;

    /**
     * @brief 使用最近邻算法优化路径顺序
     * @param segments DXF段列表
     * @param start_pos 起始位置
     * @return 优化后的段顺序（带方向）
     */
    std::vector<SegmentWithDirection> OptimizeByNearestNeighbor(
        const std::vector<Shared::Types::DXFSegment>& segments,
        const Shared::Types::Point2D& start_pos) noexcept;

    /**
     * @brief 使用2-Opt局部搜索进一步优化
     * @param initial_order 初始顺序
     * @param segments DXF段列表
     * @param max_iterations 最大迭代次数
     * @return 优化后的段顺序
     */
    std::vector<SegmentWithDirection> TwoOptImprove(
        const std::vector<SegmentWithDirection>& initial_order,
        const std::vector<Shared::Types::DXFSegment>& segments,
        int max_iterations) noexcept;

   private:
    /**
     * @brief 计算两点间距离
     */
    static float32 Distance(const Shared::Types::Point2D& a, const Shared::Types::Point2D& b) noexcept;

    /**
     * @brief 使用空间索引加速的最近邻查询
     */
    std::vector<SegmentWithDirection> OptimizeWithSpatialIndex(
        const std::vector<Shared::Types::DXFSegment>& segments,
        const Shared::Types::Point2D& start_pos) noexcept;

    std::shared_ptr<PlanningBoundary::Ports::ISpatialIndexPort> spatial_index_;
};

}  // namespace DomainServices
}  // namespace Dispensing
}  // namespace Domain
}  // namespace Siligen

