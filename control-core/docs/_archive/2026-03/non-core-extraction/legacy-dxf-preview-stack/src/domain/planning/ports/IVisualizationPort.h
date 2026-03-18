#pragma once

#include "shared/types/Result.h"
#include "shared/types/VisualizationTypes.h"
#include "shared/types/Point.h"
#include <string>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Planning {
namespace Ports {

using Siligen::Shared::Types::DXFSegment;
using Siligen::Shared::Types::Result;
using Siligen::TrajectoryPoint;
using Siligen::Shared::Types::VisualizationTriggerPoint;

/**
 * @brief 可视化端口接口
 *
 * 职责:
 * - 定义轨迹可视化的抽象
 * - 支持DXF路径预览生成
 * - 支持实时轨迹可视化
 *
 * 架构合规性:
 * - Domain层零IO依赖
 * - 纯虚接口
 * - 使用Result<T>错误处理
 */
class IVisualizationPort {
public:
    virtual ~IVisualizationPort() = default;

    /**
     * @brief 生成DXF路径的可视化HTML
     * @param segments DXF段序列
     * @param title 可视化标题
     * @return Result<std::string> HTML内容或错误信息
     */
    virtual Result<std::string> GenerateDXFVisualization(
        const std::vector<DXFSegment>& segments,
        const std::string& title = "DXF Path Preview") = 0;

    /**
     * @brief 生成轨迹点的可视化HTML
     * @param trajectory_points 轨迹点序列
     * @param title 可视化标题
     * @return Result<std::string> HTML内容或错误信息
     */
    virtual Result<std::string> GenerateTrajectoryVisualization(
        const std::vector<TrajectoryPoint>& trajectory_points,
        const std::string& title = "Trajectory Preview",
        const std::vector<VisualizationTriggerPoint>* planned_triggers = nullptr) = 0;
};

} // namespace Ports
} // namespace Planning
} // namespace Domain
} // namespace Siligen

