/**
 * @file ArcInterpolator.h
 * @brief 圆弧插补优化算法实现
 * @details 实现高精度的圆弧插补，支持三点圆弧、螺旋插补和变半径圆弧
 *
 * @author Claude Code
 * @date 2025-10-30
 */

#pragma once

// CLAUDE_SUPPRESS: DOMAIN_NO_STL_CONTAINERS
// Reason: 圆弧插补器的输出轨迹点数量取决于插补精度、圆弧长度和半径，
//         运行时无法预知轨迹点数量，FixedCapacityVector 会导致容量不足或内存浪费。
//         std::vector 是唯一合理的选择。
// Approved-By: Claude AI Auto-Fix
// Date: 2026-01-07

#include "shared/types/Point.h"
#include "TrajectoryInterpolatorBase.h"

#include <vector>

namespace Siligen::Domain::Motion {

/**
 * @brief 圆弧插补器
 * @details 实现高性能的圆弧插补算法，支持：
 * - 三点圆弧精确插补
 * - 螺旋线插补
 * - 变半径圆弧插补
 * - 圆弧与直线过渡
 * - S型加减速圆弧运动
 */
class ArcInterpolator : public TrajectoryInterpolatorBase {
   public:
    /**
     * @brief 圆弧方向枚举
     */
    enum class ArcDirection {
        COUNTER_CLOCKWISE = 0,  // 逆时针
        CLOCKWISE = 1           // 顺时针
    };

    /**
     * @brief 圆弧参数结构体
     */
    struct ArcParameters {
        Point2D center;          // 圆心坐标
        float32 radius;          // 半径
        float32 start_angle;     // 起始角度（弧度）
        float32 end_angle;       // 结束角度（弧度）
        float32 angle_span;      // 角度跨度（弧度）
        ArcDirection direction;  // 圆弧方向
        Point2D start_point;     // 起点
        Point2D end_point;       // 终点
        bool is_full_circle;     // 是否为整圆
    };

    /**
     * @brief 螺旋线参数结构体
     */
    struct HelixParameters {
        Point2D center;          // 投影圆心
        float32 radius;          // 螺旋半径
        float32 pitch;           // 螺距
        float32 start_angle;     // 起始角度
        float32 revolutions;     // 圈数
        float32 start_z;         // 起始高度
        float32 end_z;           // 终止高度
        ArcDirection direction;  // 旋转方向
    };

    /**
     * @brief 构造函数
     * @param multi_card GAS控制卡实例
     */
    ArcInterpolator();

    /**
     * @brief 析构函数
     */
    ~ArcInterpolator() override;

    /**
     * @brief 执行圆弧插补计算
     * @param points 路径点列表
     * @param config 插补配置参数
     * @return 插补生成的轨迹点序列
     */
    std::vector<TrajectoryPoint> CalculateInterpolation(const std::vector<Point2D>& points,
                                                        const InterpolationConfig& config) override;

    /**
     * @brief 获取插补算法类型
     * @return 算法类型
     */
    InterpolationAlgorithm GetType() const override {
        return InterpolationAlgorithm::ARC;
    }

    /**
     * @brief 三点圆弧精确插补
     * @param p1 第一个点
     * @param p2 第二个点（中间点）
     * @param p3 第三个点
     * @param config 插补配置
     * @return 圆弧轨迹点
     */
    std::vector<TrajectoryPoint> ThreePointArcInterpolation(const Point2D& p1,
                                                            const Point2D& p2,
                                                            const Point2D& p3,
                                                            const InterpolationConfig& config);

    /**
     * @brief 圆心圆弧插补
     * @param center 圆心
     * @param start_point 起点
     * @param end_point 终点
     * @param direction 圆弧方向
     * @param config 插补配置
     * @return 圆弧轨迹点
     */
    std::vector<TrajectoryPoint> CenterArcInterpolation(const Point2D& center,
                                                        const Point2D& start_point,
                                                        const Point2D& end_point,
                                                        ArcDirection direction,
                                                        const InterpolationConfig& config);

    /**
     * @brief 螺旋线插补
     * @param helix_params 螺旋线参数
     * @param config 插补配置
     * @return 螺旋线轨迹点
     */
    std::vector<TrajectoryPoint> HelicalInterpolation(const HelixParameters& helix_params,
                                                      const InterpolationConfig& config);

    /**
     * @brief 变半径圆弧插补
     * @param start_point 起点
     * @param end_point 终点
     * @param start_radius 起始半径
     * @param end_radius 终止半径
     * @param center 圆心参考点
     * @param config 插补配置
     * @return 变半径圆弧轨迹点
     */
    std::vector<TrajectoryPoint> VariableRadiusArcInterpolation(const Point2D& start_point,
                                                                const Point2D& end_point,
                                                                float32 start_radius,
                                                                float32 end_radius,
                                                                const Point2D& center,
                                                                const InterpolationConfig& config);

    /**
     * @brief 圆弧与直线过渡插补
     * @param line_end 直线终点
     * @param arc_start 圆弧起点
     * @param arc_center 圆弧圆心
     * @param blend_radius 过渡半径
     * @param config 插补配置
     * @return 过渡轨迹点
     */
    std::vector<TrajectoryPoint> LineArcBlend(const Point2D& line_end,
                                              const Point2D& arc_start,
                                              const Point2D& arc_center,
                                              float32 blend_radius,
                                              const InterpolationConfig& config);

    /**
     * @brief 计算圆弧参数（三点）
     * @param p1 第一个点
     * @param p2 第二个点
     * @param p3 第三个点
     * @return 圆弧参数
     */
    ArcParameters CalculateArcParameters(const Point2D& p1, const Point2D& p2, const Point2D& p3) const;

    /**
     * @brief 计算圆弧参数（圆心+两点）
     * @param center 圆心
     * @param start_point 起点
     * @param end_point 终点
     * @param direction 圆弧方向
     * @return 圆弧参数
     */
    ArcParameters CalculateArcParameters(const Point2D& center,
                                         const Point2D& start_point,
                                         const Point2D& end_point,
                                         ArcDirection direction) const;

    /**
     * @brief 验证圆弧参数
     * @param params 圆弧参数
     * @param config 插补配置
     * @return 验证结果
     */
    bool ValidateArcParameters(const ArcParameters& params, const InterpolationConfig& config) const;

   protected:
    /**
     * @brief 生成圆弧轨迹点
     * @param params 圆弧参数
     * @param config 插补配置
     * @return 圆弧轨迹点
     */
    std::vector<TrajectoryPoint> GenerateArcTrajectory(const ArcParameters& params,
                                                       const InterpolationConfig& config) const;

    /**
     * @brief 生成螺旋线轨迹点
     * @param params 螺旋线参数
     * @param config 插补配置
     * @return 螺旋线轨迹点
     */
    std::vector<TrajectoryPoint> GenerateHelixTrajectory(const HelixParameters& params,
                                                         const InterpolationConfig& config) const;

    /**
     * @brief 计算圆弧上的点
     * @param center 圆心
     * @param radius 半径
     * @param angle 角度（弧度）
     * @return 圆弧上的点
     */
    Point2D CalculatePointOnArc(const Point2D& center, float32 radius, float32 angle) const;

    /**
     * @brief 计算圆弧长度
     * @param radius 半径
     * @param angle_span 角度跨度（弧度）
     * @return 圆弧长度
     */
    float32 CalculateArcLength(float32 radius, float32 angle_span) const;

    /**
     * @brief 角度归一化到[-π, π]
     * @param angle 原始角度
     * @return 归一化后的角度
     */
    float32 NormalizeAngle(float32 angle) const;

    /**
     * @brief 计算两点间角度差
     * @param start_angle 起始角度
     * @param end_angle 结束角度
     * @param direction 方向
     * @return 角度差
     */
    float32 CalculateAngleDifference(float32 start_angle, float32 end_angle, ArcDirection direction) const;

    /**
     * @brief 优化圆弧速度分布
     * @param params 圆弧参数
     * @param config 插补配置
     * @return 优化后的速度配置
     */
    std::vector<float32> OptimizeArcVelocity(const ArcParameters& params, const InterpolationConfig& config) const;

    /**
     * @brief 计算圆弧点胶触发位置
     * @param params 圆弧参数
     * @param config 插补配置
     * @return 触发位置列表
     */
    std::vector<float32> CalculateArcTriggerPositions(const ArcParameters& params,
                                                      const InterpolationConfig& config) const;

   private:
    // 内部缓存，提高计算效率
    mutable std::vector<ArcParameters> m_arc_cache;
    mutable std::vector<float32> m_angle_cache;

    // 组件
    std::unique_ptr<class ArcGeometryMath> m_math_utils;
    std::unique_ptr<class CircleCalculator> m_circle_calculator;
};

}  // namespace Siligen::Domain::Motion


