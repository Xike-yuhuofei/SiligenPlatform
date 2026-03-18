// DXFVisualizationAdapter.h - DXF可视化适配器
// 职责: 实现IVisualizationPort端口，封装点胶轨迹可视化器
// 架构合规性: 实现Domain层端口，单一职责，使用Result<T>错误处理
#pragma once

#include "domain/planning/ports/IVisualizationPort.h"

#include <memory>

namespace Siligen {

// 前向声明
class DispensingVisualizer;

namespace Infrastructure {
namespace Adapters {
namespace Visualization {

using Siligen::Domain::Planning::Ports::IVisualizationPort;
using Siligen::Shared::Types::DXFSegment;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::VisualizationTriggerPoint;
using Siligen::TrajectoryPoint;

/**
 * @brief DXF可视化适配器
 *
 * 职责:
 * - 实现IVisualizationPort端口
 * - 封装点胶轨迹可视化器
 * - 转换错误为Result<T>
 *
 * 架构合规性:
 * - 实现Domain层端口
 * - 单一职责
 * - 使用Result<T>错误处理
 * - 依赖方向正确：Infrastructure -> Domain
 */
class DXFVisualizationAdapter : public IVisualizationPort {
   public:
    /**
     * @brief 构造函数
     * @param dispensing_visualizer 点胶轨迹可视化器
     */
    explicit DXFVisualizationAdapter(std::shared_ptr<DispensingVisualizer> dispensing_visualizer = nullptr);

    ~DXFVisualizationAdapter() override = default;

    // 禁止拷贝和移动
    DXFVisualizationAdapter(const DXFVisualizationAdapter&) = delete;
    DXFVisualizationAdapter& operator=(const DXFVisualizationAdapter&) = delete;
    DXFVisualizationAdapter(DXFVisualizationAdapter&&) = delete;
    DXFVisualizationAdapter& operator=(DXFVisualizationAdapter&&) = delete;

    // IVisualizationPort接口实现
    Result<std::string> GenerateDXFVisualization(
        const std::vector<DXFSegment>& segments,
        const std::string& title) override;

    Result<std::string> GenerateTrajectoryVisualization(
        const std::vector<TrajectoryPoint>& trajectory_points,
        const std::string& title,
        const std::vector<VisualizationTriggerPoint>* planned_triggers = nullptr) override;

   private:
    std::shared_ptr<DispensingVisualizer> dispensing_visualizer_;
};

}  // namespace Visualization
}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen




