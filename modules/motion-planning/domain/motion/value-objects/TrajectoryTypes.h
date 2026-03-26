#pragma once

#include "shared/types/Point.h"

#include <boost/describe/enum.hpp>

#include <cstdint>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Motion {
namespace ValueObjects {

// 使用shared/types中的Point2D

/**
 * @brief 轨迹类型枚举
 */
enum class TrajectoryType {
    Linear,       // 直线轨迹
    CircularArc,  // 圆弧轨迹
    BezierCurve,  // 贝塞尔曲线
    BSpline       // B样条曲线
};
BOOST_DESCRIBE_ENUM(TrajectoryType, Linear, CircularArc, BezierCurve, BSpline)

/**
 * @brief 轨迹定义
 */
struct TrajectoryDefinition {
    TrajectoryType type;
    std::vector<Point2D> controlPoints;

    TrajectoryDefinition() : type(TrajectoryType::Linear) {}

    TrajectoryDefinition(TrajectoryType t, const std::vector<Point2D>& points) : type(t), controlPoints(points) {}
};

/**
 * @brief CMP参数配置
 */
struct CMPParameters {
    double compensationGain;  // 补偿增益
    int lookaheadPoints;      // 前瞻点数
    double maxCompensation;   // 最大补偿量(mm)

    CMPParameters() : compensationGain(1.0), lookaheadPoints(5), maxCompensation(1.0) {}

    CMPParameters(double gain, int lookahead, double maxComp)
        : compensationGain(gain), lookaheadPoints(lookahead), maxCompensation(maxComp) {}
};

// NOTE: TrajectoryAnalysis struct removed - use TrajectoryAnalysisTypes.h (formerly TestMetricsTypes.h)
// to avoid duplication. That version contains more detailed analysis capabilities.

/**
 * @brief 轨迹插补类型枚举
 */
enum class TrajectoryInterpolationType {
    Linear,       // 直线插补
    CircularArc,  // 圆弧插补
    BSpline,      // B样条插补
    Bezier        // 贝塞尔曲线插补
};
BOOST_DESCRIBE_ENUM(TrajectoryInterpolationType, Linear, CircularArc, BSpline, Bezier)

/**
 * @brief 插补参数配置
 */
struct InterpolationParameters {
    double targetFeedRate;   // 目标进给速度(mm/s)
    double acceleration;     // 加速度(mm/s²)
    int interpolationCycle;  // 插补周期(ms)

    InterpolationParameters() : targetFeedRate(50.0), acceleration(200.0), interpolationCycle(1) {}

    InterpolationParameters(double feedRate, double accel, int cycle)
        : targetFeedRate(feedRate), acceleration(accel), interpolationCycle(cycle) {}
};

/**
 * @brief 路径质量指标
 */
struct PathQualityMetrics {
    double maxPathError;      // 最大路径误差(mm)
    double avgPathError;      // 平均路径误差(mm)
    double circularityError;  // 圆度误差(仅圆弧插补)
    double radiusError;       // 半径误差(仅圆弧插补)

    PathQualityMetrics() : maxPathError(0.0), avgPathError(0.0), circularityError(0.0), radiusError(0.0) {}
};

/**
 * @brief 运动质量指标
 */
struct MotionQualityMetrics {
    double maxVelocity;               // 最大速度(mm/s)
    double avgVelocity;               // 平均速度(mm/s)
    double maxAcceleration;           // 最大加速度(mm/s²)
    int accelerationDiscontinuities;  // 加速度突变点数量
    double velocitySmoothness;        // 速度平滑度(0-1)

    MotionQualityMetrics()
        : maxVelocity(0.0),
          avgVelocity(0.0),
          maxAcceleration(0.0),
          accelerationDiscontinuities(0),
          velocitySmoothness(0.0) {}
};

}  // namespace ValueObjects
}  // namespace Motion
}  // namespace Domain
}  // namespace Siligen

