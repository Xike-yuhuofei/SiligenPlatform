/**
 * @file SplineInterpolator.h
 * @brief 三次样条插补算法实现
 * @details 实现高精度的样条曲线插补，支持三次样条、Bezier曲线和NURBS
 *
 * @author Claude Code
 * @date 2025-10-30
 */

#pragma once

#include "shared/types/Point.h"
#include "TrajectoryInterpolatorBase.h"

#include <vector>

namespace Siligen::Domain::Motion {

/**
 * @brief 样条插补器
 * @details 实现高性能的样条曲线插补算法，支持：
 * - 三次样条插补（自然样条、固定样条）
 * - Bezier曲线插补
 * - B样条插补
 * - NURBS插补（高级应用）
 * - 自适应步长控制
 */
class SplineInterpolator : public TrajectoryInterpolatorBase {
   public:
    /**
     * @brief 样条类型枚举
     */
    enum class SplineType {
        CUBIC_NATURAL,  // 自然三次样条
        CUBIC_CLAMPED,  // 固定三次样条
        BEZIER,         // Bezier曲线
        B_SPLINE,       // B样条
        NURBS           // NURBS曲线
    };

    /**
     * @brief 样条段结构体
     */
    struct SplineSegment {
        float32 a, b, c, d;  // 三次多项式系数: y = a + b*x + c*x² + d*x³
        float32 x_start;     // 段起始x坐标
        float32 x_end;       // 段结束x坐标
        float32 length;      // 段长度
    };

    /**
     * @brief Bezier控制点结构体
     */
    struct BezierControlPoints {
        std::vector<Point2D> control_points;  // 控制点列表
        int32 degree;                         // 曲线阶数
        bool is_rational;                     // 是否为有理曲线
        std::vector<float32> weights;         // 权重（仅用于有理曲线）
    };

    /**
     * @brief B样条节点向量
     */
    struct BSplineKnotVector {
        std::vector<float32> knots;  // 节点向量
        int32 degree;                // B样条阶数
        int32 num_control_points;    // 控制点数量
    };

    /**
     * @brief 构造函数
     * @param multi_card GAS控制卡实例
     */
    SplineInterpolator();

    /**
     * @brief 析构函数
     */
    ~SplineInterpolator() override;

    /**
     * @brief 执行样条插补计算
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
        return InterpolationAlgorithm::SPLINE;
    }

    /**
     * @brief 三次样条插补
     * @param control_points 控制点
     * @param tension 张力系数 (0.0-1.0)
     * @param config 插补配置
     * @return 样条轨迹点
     */
    std::vector<TrajectoryPoint> CubicSplineInterpolation(const std::vector<Point2D>& control_points,
                                                          float32 tension = 0.5f,
                                                          const InterpolationConfig& config = InterpolationConfig{});

    /**
     * @brief Bezier曲线插补
     * @param control_points Bezier控制点
     * @param config 插补配置
     * @return Bezier轨迹点
     */
    std::vector<TrajectoryPoint> BezierInterpolation(const BezierControlPoints& control_points,
                                                     const InterpolationConfig& config);

    /**
     * @brief B样条插补
     * @param control_points 控制点
     * @param knot_vector 节点向量
     * @param config 插补配置
     * @return B样条轨迹点
     */
    std::vector<TrajectoryPoint> BSplineInterpolation(const std::vector<Point2D>& control_points,
                                                      const BSplineKnotVector& knot_vector,
                                                      const InterpolationConfig& config);

    /**
     * @brief NURBS插补
     * @param control_points 控制点
     * @param weights 权重
     * @param knot_vector 节点向量
     * @param degree 曲线阶数
     * @param config 插补配置
     * @return NURBS轨迹点
     */
    std::vector<TrajectoryPoint> NURBSInterpolation(const std::vector<Point2D>& control_points,
                                                    const std::vector<float32>& weights,
                                                    const std::vector<float32>& knot_vector,
                                                    int32 degree,
                                                    const InterpolationConfig& config);

    /**
     * @brief 自适应样条插补
     * @param points 输入点
     * @param max_error 最大容许误差
     * @param config 插补配置
     * @return 自适应样条轨迹点
     */
    std::vector<TrajectoryPoint> AdaptiveSplineInterpolation(const std::vector<Point2D>& points,
                                                             float32 max_error,
                                                             const InterpolationConfig& config);

    /**
     * @brief 计算三次样条系数
     * @param points 控制点
     * @param tension 张力系数
     * @param boundary_type 边界条件类型
     * @return 样条段列表
     */
    std::vector<SplineSegment> CalculateCubicSplineCoefficients(const std::vector<Point2D>& points,
                                                                float32 tension,
                                                                const std::string& boundary_type = "natural") const;

    /**
     * @brief De Casteljau算法计算Bezier点
     * @param control_points 控制点
     * @param t 参数值 (0.0-1.0)
     * @return Bezier曲线上的点
     */
    Point2D DeCasteljau(const std::vector<Point2D>& control_points, float32 t) const;

    /**
     * @brief 计算B样条基函数
     * @param i 基函数索引
     * @param p 阶数
     * @param t 参数值
     * @param knots 节点向量
     * @return 基函数值
     */
    float32 BSplineBasis(int32 i, int32 p, float32 t, const std::vector<float32>& knots) const;

    /**
     * @brief 计算曲线弧长
     * @param segments 样条段列表
     * @param num_samples 采样点数
     * @return 曲线弧长
     */
    float32 CalculateSplineLength(const std::vector<SplineSegment>& segments, int32 num_samples = 100) const;

    /**
     * @brief 验证样条参数
     * @param control_points 控制点
     * @param spline_type 样条类型
     * @return 验证结果
     */
    bool ValidateSplineParameters(const std::vector<Point2D>& control_points, SplineType spline_type) const;

   protected:
    /**
     * @brief 生成样条轨迹点
     * @param segments 样条段列表
     * @param config 插补配置
     * @return 样条轨迹点
     */
    std::vector<TrajectoryPoint> GenerateSplineTrajectory(const std::vector<SplineSegment>& segments,
                                                          const InterpolationConfig& config) const;

    /**
     * @brief 生成Bezier轨迹点
     * @param control_points Bezier控制点
     * @param config 插补配置
     * @return Bezier轨迹点
     */
    std::vector<TrajectoryPoint> GenerateBezierTrajectory(const BezierControlPoints& control_points,
                                                          const InterpolationConfig& config) const;

    /**
     * @brief 计算样条曲线导数
     * @param segments 样条段列表
     * @param x x坐标
     * @return 一阶导数
     */
    float32 CalculateSplineDerivative(const std::vector<SplineSegment>& segments, float32 x) const;

    /**
     * @brief 计算样条曲线曲率
     * @param segments 样条段列表
     * @param x x坐标
     * @return 曲率
     */
    float32 CalculateSplineCurvature(const std::vector<SplineSegment>& segments, float32 x) const;

    /**
     * @brief 自适应步长控制
     * @param segments 样条段列表
     * @param max_error 最大容许误差
     * @param initial_step 初始步长
     * @return 优化后的步长序列
     */
    std::vector<float32> AdaptiveStepControl(const std::vector<SplineSegment>& segments,
                                             float32 max_error,
                                             float32 initial_step) const;

    /**
     * @brief 样条点胶触发位置计算
     * @param segments 样条段列表
     * @param config 插补配置
     * @return 触发位置列表
     */
    std::vector<float32> CalculateSplineTriggerPositions(const std::vector<SplineSegment>& segments,
                                                         const InterpolationConfig& config) const;

    /**
     * @brief 参数化曲线转换为笛卡尔坐标
     * @param control_points 控制点
     * @param parameter_values 参数值列表
     * @param spline_type 样条类型
     * @return 笛卡尔坐标点
     */
    std::vector<Point2D> ParametericToCartesian(const std::vector<Point2D>& control_points,
                                                const std::vector<float32>& parameter_values,
                                                SplineType spline_type) const;

   private:
    // 内部缓存，提高计算效率
    mutable std::vector<SplineSegment> m_spline_cache;
    mutable std::vector<Point2D> m_bezier_cache;
    mutable std::vector<float32> m_basis_cache;

    // 组件
    std::unique_ptr<class SplineGeometryMath> m_math_utils;
    std::unique_ptr<class BezierCalculator> m_bezier_calculator;
    std::unique_ptr<class BSplineCalculator> m_bspline_calculator;
};

}  // namespace Siligen::Domain::Motion


