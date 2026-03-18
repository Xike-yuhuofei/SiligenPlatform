/**
 * @file ArcTriggerPointCalculator.h
 * @brief 圆弧触发点计算器 - 计算沿圆弧均匀分布的CMP触发位置
 */

#ifndef SILIGEN_DOMAIN_DISPENSING_DOMAIN_SERVICES_ARC_TRIGGER_POINT_CALCULATOR_H
#define SILIGEN_DOMAIN_DISPENSING_DOMAIN_SERVICES_ARC_TRIGGER_POINT_CALCULATOR_H

#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/VisualizationTypes.h"
#include <vector>

namespace Siligen {
namespace Domain {
namespace Dispensing {
namespace DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::DXFSegment;
using Siligen::Shared::Types::float32;

/**
 * @brief 圆弧触发点计算结果
 */
struct ArcTriggerResult {
    std::vector<long> trigger_positions;  ///< 触发位置数组（脉冲单位，按轨迹顺序）
    LogicalAxisId trigger_axis = LogicalAxisId::X;  ///< 触发轴号（0-based: X=0, Y=1）
};

/**
 * @brief 圆弧触发点计算器
 *
 * 职责：计算沿圆弧路径均匀分布的CMP触发位置数组
 * 算法：参数化角度均匀采样，投影到触发轴并转换为脉冲位置
 */
class ArcTriggerPointCalculator {
public:
    /**
     * @brief 计算圆弧路径的触发位置数组
     *
     * @param segment DXF圆弧段（包含圆心、半径、起止角度）
     * @param spatial_interval_mm 触发点空间间隔（mm）
     * @param pulse_per_mm 脉冲当量（pulse/mm）
     * @return 成功返回触发结果（位置数组+触发轴），失败返回错误
     *
     * @note 触发轴选择策略：优先选择变化趋势更稳定的轴（单调性作为启发式），否则回退到变化范围更大的轴
     * @note 触发位置按轨迹顺序生成，允许出现非单调变化
     */
    static Result<ArcTriggerResult> Calculate(
        const DXFSegment& segment,
        float32 spatial_interval_mm,
        float32 pulse_per_mm) noexcept;

private:
    /**
     * @brief 选择最优触发轴（启发式）
     * @param segment DXF圆弧段
     * @return 触发轴号（0-based: X=0, Y=1）
     */
    static LogicalAxisId SelectOptimalAxis(const DXFSegment& segment) noexcept;
};

}  // namespace DomainServices
}  // namespace Dispensing
}  // namespace Domain
}  // namespace Siligen

#endif  // SILIGEN_DOMAIN_DISPENSING_DOMAIN_SERVICES_ARC_TRIGGER_POINT_CALCULATOR_H

