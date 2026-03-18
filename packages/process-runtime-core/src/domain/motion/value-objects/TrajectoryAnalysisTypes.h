#pragma once

#include "shared/types/Point2D.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen {
namespace Domain {
namespace Motion {
namespace ValueObjects {

/**
 * @brief 偏差统计
 * @details 单项偏差统计数据
 */
struct DeviationStatistics {
    float32 position_deviation_mm;       // 位置偏差(mm)
    float32 time_deviation_ms;           // 时间偏差(ms)
    float32 velocity_deviation_percent;  // 速度偏差百分比

    DeviationStatistics()
        : position_deviation_mm(0.0f), time_deviation_ms(0.0f), velocity_deviation_percent(0.0f) {}
};

/**
 * @brief 触发精度分析
 * @details 分析触发事件的精度
 */
struct TriggerPrecisionAnalysis {
    int32 total_trigger_count;            // 总触发次数
    int32 success_trigger_count;          // 成功触发次数
    float32 average_trigger_deviation_mm;  // 平均触发位置偏差(mm)
    float32 max_trigger_deviation_mm;      // 最大触发位置偏差(mm)
    float32 trigger_response_time_ms;      // 平均触发响应时间(ms)

    TriggerPrecisionAnalysis()
        : total_trigger_count(0),
          success_trigger_count(0),
          average_trigger_deviation_mm(0.0f),
          max_trigger_deviation_mm(0.0f),
          trigger_response_time_ms(0.0f) {}

    /**
     * @brief 计算成功率
     * @return 成功率百分比 (0-100)
     */
    float32 GetSuccessRate() const noexcept {
        if (total_trigger_count == 0) return 0.0f;
        return static_cast<float32>(success_trigger_count) / static_cast<float32>(total_trigger_count) * 100.0f;
    }
};

/**
 * @brief 偏差点
 * @details 单个轨迹点的偏差信息
 */
struct DeviationPoint {
    Shared::Types::Point2D theoretical_position;  // 理论位置
    Shared::Types::Point2D actual_position;       // 实际位置
    float32 deviation;                            // 偏差距离(mm)
    float32 timestamp;                            // 时间戳

    DeviationPoint()
        : theoretical_position{0.0f, 0.0f}, actual_position{0.0f, 0.0f}, deviation(0.0f), timestamp(0.0f) {}
};

/**
 * @brief 轨迹分析
 * @details 存储理论轨迹与实际路径的偏差分析结果
 */
struct TrajectoryAnalysis {
    DeviationStatistics max_deviation;             // 最大偏差统计
    DeviationStatistics average_deviation;         // 平均偏差统计
    DeviationStatistics rms_deviation;             // RMS偏差统计
    TriggerPrecisionAnalysis trigger_precision;    // 触发精度分析
    std::vector<DeviationPoint> deviation_points;  // 逐点偏差数据

    TrajectoryAnalysis() = default;
};

}  // namespace ValueObjects
}  // namespace Motion
}  // namespace Domain
}  // namespace Siligen

