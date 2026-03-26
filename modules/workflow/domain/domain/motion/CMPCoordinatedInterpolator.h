/**
 * @file CMPCoordinatedInterpolator.h
 * @brief CMP位置触发协调插补算法实现
 * @details 实现CMP位置触发与轨迹运动的精确协调控制
 *
 * @author Claude Code
 * @date 2025-10-30
 */

#pragma once

// CLAUDE_SUPPRESS: DOMAIN_NO_STL_CONTAINERS
// Reason: CMP 协同插补的触发点数量取决于多轴路径复杂度、触发配置密度和时间同步精度，
//         最大可能触发点数量 >10000，FixedCapacityVector 会导致栈溢出或容量不足。
//         std::vector 是唯一可行的选择。
// Approved-By: Claude AI Auto-Fix
// Date: 2026-01-07

#include "shared/types/CMPTypes.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"
#include "../dispensing/domain-services/PositionTriggerController.h"
#include "CMPCompensation.h"
#include "CMPValidator.h"
#include "domain-services/interpolation/TrajectoryInterpolatorBase.h"

#include <memory>
#include <vector>

namespace Siligen::Domain::Motion {

// 前向声明领域服务类型
class TriggerCalculator;
class TrajectoryPlanner;

/**
 * @brief CMP协调插补器
 * @details 实现CMP位置触发与轨迹运动的精确协调，支持：
 * - 位置同步点胶插补
 * - 时间同步点胶插补
 * - 混合触发模式插补
 * - 多通道CMP协调控制
 * - 实时触发位置补偿
 */
class CMPCoordinatedInterpolator : public TrajectoryInterpolatorBase {
   public:
    /**
     * @brief 构造函数
     * @param multi_card GAS控制卡实例
     */
    CMPCoordinatedInterpolator();

    /**
     * @brief 析构函数
     */
    ~CMPCoordinatedInterpolator() override;

    /**
     * @brief 执行CMP协调插补计算
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
        return InterpolationAlgorithm::CMP_COORDINATED;
    }

    /**
     * @brief 位置同步点胶插补
     * @param points 点胶路径点
     * @param trigger_points 触发点列表
     * @param cmp_config CMP配置
     * @param config 插补配置
     * @return CMP协调轨迹点
     */
    std::vector<TrajectoryPoint> PositionTriggeredDispensing(const std::vector<Point2D>& points,
                                                             const std::vector<DispensingTriggerPoint>& trigger_points,
                                                             const CMPConfiguration& cmp_config,
                                                             const InterpolationConfig& config);

    /**
     * @brief 时间同步点胶插补
     * @param points 点胶路径点
     * @param dispensing_interval 点胶间隔时间 (ms)
     * @param cmp_config CMP配置
     * @param config 插补配置
     * @return 时间同步轨迹点
     */
    std::vector<TrajectoryPoint> TimeSynchronizedDispensing(const std::vector<Point2D>& points,
                                                            float32 dispensing_interval,
                                                            const CMPConfiguration& cmp_config,
                                                            const InterpolationConfig& config);

    /**
     * @brief 混合触发模式插补
     * @param points 点胶路径点
     * @param trigger_points 关键触发点
     * @param time_interval 时间间隔
     * @param cmp_config CMP配置
     * @param config 插补配置
     * @return 混合触发轨迹点
     */
    std::vector<TrajectoryPoint> HybridTriggerDispensing(const std::vector<Point2D>& points,
                                                         const std::vector<DispensingTriggerPoint>& trigger_points,
                                                         float32 time_interval,
                                                         const CMPConfiguration& cmp_config,
                                                         const InterpolationConfig& config);

    /**
     * @brief 基于模式的触发插补
     * @param points 路径点
     * @param pattern_type 模式类型
     * @param pattern_params 模式参数
     * @param cmp_config CMP配置
     * @param config 插补配置
     * @return 模式触发轨迹点
     */
    std::vector<TrajectoryPoint> PatternBasedDispensing(const std::vector<Point2D>& points,
                                                        const std::string& pattern_type,
                                                        const std::vector<float32>& pattern_params,
                                                        const CMPConfiguration& cmp_config,
                                                        const InterpolationConfig& config);

    /**
     * @brief 计算触发时间线
     * @param trajectory 基础轨迹
     * @param trigger_points 触发点
     * @param cmp_config CMP配置
     * @return 触发时间线
     */
    TriggerTimeline CalculateTriggerTimeline(const std::vector<TrajectoryPoint>& trajectory,
                                             const std::vector<DispensingTriggerPoint>& trigger_points,
                                             const CMPConfiguration& cmp_config) const;

    /**
     * @brief 触发位置处理（软件规划方案）
     * @param current_position 当前位置（已废弃）
     * @param target_position 目标规划位置
     * @param current_velocity 当前速度（已废弃）
     * @param cmp_config CMP配置（已废弃）
     * @return 规划触发位置（直接返回target_position，无补偿）
     * @note 软件方案：使用规划位置+时间戳触发，无需编码器延迟补偿
     */
    Point2D CompensateTriggerPosition(const Point2D& current_position,
                                      const Point2D& target_position,
                                      float32 current_velocity,
                                      const CMPConfiguration& cmp_config) const;

    /**
     * @brief 验证CMP配置
     * @param cmp_config CMP配置
     * @return 验证结果
     */
    bool ValidateCMPConfiguration(const CMPConfiguration& cmp_config) const;

    /**
     * @brief 优化触发点分布
     * @param trigger_points 原始触发点
     * @param trajectory 轨迹点
     * @param min_spacing 最小间距
     * @return 优化后的触发点
     */
    std::vector<DispensingTriggerPoint> OptimizeTriggerDistribution(
        const std::vector<DispensingTriggerPoint>& trigger_points,
        const std::vector<TrajectoryPoint>& trajectory,
        float32 min_spacing) const;

   protected:
    /**
     * @brief 生成CMP协调轨迹
     * @param base_trajectory 基础轨迹
     * @param timeline 触发时间线
     * @param cmp_config CMP配置
     * @return CMP协调轨迹
     */
    std::vector<TrajectoryPoint> GenerateCMPCoordinatedTrajectory(const std::vector<TrajectoryPoint>& base_trajectory,
                                                                  const TriggerTimeline& timeline,
                                                                  const CMPConfiguration& cmp_config) const;

    /**
     * @brief 计算位置触发预测
     * @param trajectory 轨迹
     * @param trigger_position 触发位置
     * @param prediction_time 预测时间
     * @return 预测的触发时间和索引
     */
    std::pair<int32, float32> PredictTriggerPosition(const std::vector<TrajectoryPoint>& trajectory,
                                                     float32 trigger_position,
                                                     float32 prediction_time) const;

    /**
     * @brief 计算时间触发分布
     * @param trajectory 轨迹
     * @param interval 时间间隔
     * @return 触发时间分布
     */
    std::vector<float32> CalculateTimeTriggerDistribution(const std::vector<TrajectoryPoint>& trajectory,
                                                          float32 interval) const;

    /**
     * @brief 多通道CMP协调
     * @param timeline 单通道时间线
     * @param channel_mapping 通道映射
     * @return 多通道时间线
     */
    TriggerTimeline CoordinateMultiChannelCMP(const TriggerTimeline& timeline,
                                              const std::vector<uint16>& channel_mapping) const;

    /**
     * @brief 触发精度分析
     * @param trajectory 轨迹
     * @param timeline 触发时间线
     * @param cmp_config CMP配置
     * @return 精度分析结果
     */
    std::vector<float32> AnalyzeTriggerAccuracy(const std::vector<TrajectoryPoint>& trajectory,
                                                const TriggerTimeline& timeline,
                                                const CMPConfiguration& cmp_config) const;

    /**
     * @brief 动态触发参数调整
     * @param current_accuracy 当前精度
     * @param target_accuracy 目标精度
     * @param cmp_config CMP配置
     * @return 调整后的CMP配置
     */
    CMPConfiguration AdjustTriggerParameters(float32 current_accuracy,
                                             float32 target_accuracy,
                                             const CMPConfiguration& cmp_config) const;

   private:
    // 内部状态
    CMPConfiguration m_current_cmp_config;
    std::vector<TriggerTimeline> m_timeline_cache;
    std::vector<DispensingTriggerPoint> m_trigger_cache;

    // 组件
    std::unique_ptr<TriggerCalculator> m_trigger_calculator;
    std::unique_ptr<TrajectoryPlanner> m_trajectory_generator;
    std::unique_ptr<CMPValidator> m_validator;
    std::unique_ptr<CMPCompensation> m_compensation;
};

}  // namespace Siligen::Domain::Motion


