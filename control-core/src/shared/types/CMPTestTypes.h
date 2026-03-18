// CMPTestTypes.h
// 版本: 1.0.0
// 描述: CMP测试相关类型定义 - 共享层类型
// 任务: T012-T015, T031-T033 (Phase 3-5)

#pragma once

#include "CMPTypes.h"
#include "Error.h"
#include "Point2D.h"
#include "Result.h"

#include <chrono>
#include <string>
#include <vector>

namespace Siligen {
namespace Shared {
namespace Types {

// ============================================================
// T012-T015: Phase 3 数据模型定义
// ============================================================

/**
 * @brief 轨迹参数
 * @details 定义运动轨迹的几何和动力学参数
 */
struct TrajectoryParams {
    Point2D startPosition;  // 起点坐标 (x, y)
    Point2D endPosition;    // 终点坐标 (x, y)
    float32 velocity;       // 运动速度 (mm/s)
    float32 acceleration;   // 加速度 (mm/s²)
    float32 timeStep;       // 插补时间步长 (s, 默认0.01)

    TrajectoryParams()
        : startPosition{0.0f, 0.0f}, endPosition{0.0f, 0.0f}, velocity(50.0f), acceleration(100.0f), timeStep(0.01f) {}

    // 验证轨迹参数有效性
    bool Validate() const {
        if (velocity <= 0.0f || velocity > 500.0f) return false;
        if (acceleration <= 0.0f || acceleration > 2000.0f) return false;
        if (timeStep <= 0.0f || timeStep < 0.001f || timeStep > 0.1f) return false;
        if (startPosition.x == endPosition.x && startPosition.y == endPosition.y) return false;
        return true;
    }
};

/**
 * @brief 验证容差
 */
struct ValidationTolerance {
    float32 positionToleranceMM;       // 位置容差(mm, 默认0.05)
    float32 timeToleranceMS;           // 时间容差(ms, 默认1.0)
    float32 velocityTolerancePercent;  // 速度容差百分比(默认5.0)

    ValidationTolerance() : positionToleranceMM(0.05f), timeToleranceMS(1.0f), velocityTolerancePercent(5.0f) {}
};

/**
 * @brief CMP测试参数
 * @details 封装测试执行所需的所有输入参数
 */
struct CMPTestParams {
    TrajectoryParams trajectory;              // 轨迹定义参数
    CMPConfiguration cmpConfig;               // CMP触发配置
    float32 samplingRateHz;                   // 实际路径采样率(默认100Hz)
    ValidationTolerance validationTolerance;  // 验证容差

    CMPTestParams() : trajectory(), cmpConfig(), samplingRateHz(100.0f), validationTolerance() {}

    // 验证测试参数有效性
    bool Validate() const {
        if (!trajectory.Validate()) return false;
        if (!cmpConfig.Validate()) return false;
        if (samplingRateHz <= 0.0f || samplingRateHz > 200.0f) return false;
        return true;
    }
};

/**
 * @brief 轨迹点
 * @details 表示轨迹上的单个插补点或采样点
 */
struct TrajectoryPoint {
    Point2D position;              // 位置 (x, y)
    float32 velocity;              // 速度 (mm/s)
    float32 acceleration;          // 加速度 (mm/s²)
    float32 timestamp;             // 时间戳 (s或ms)
    bool enable_position_trigger;  // 是否在此点触发(标记字段)

    TrajectoryPoint()
        : position{0.0f, 0.0f}, velocity(0.0f), acceleration(0.0f), timestamp(0.0f), enable_position_trigger(false) {}
};

// ============================================================
// T031-T033: Phase 5 数据模型扩展
// ============================================================

/**
 * @brief 偏差统计
 * @details 单项偏差统计数据
 */
struct DeviationStatistics {
    float32 positionDeviationMM;       // 位置偏差(mm)
    float32 timeDeviationMS;           // 时间偏差(ms)
    float32 velocityDeviationPercent;  // 速度偏差百分比

    DeviationStatistics() : positionDeviationMM(0.0f), timeDeviationMS(0.0f), velocityDeviationPercent(0.0f) {}
};

/**
 * @brief 触发精度分析
 * @details 分析触发事件的精度
 */
struct TriggerPrecisionAnalysis {
    int32 totalTriggerCount;            // 总触发次数
    int32 successTriggerCount;          // 成功触发次数
    float32 averageTriggerDeviationMM;  // 平均触发位置偏差(mm)
    float32 maxTriggerDeviationMM;      // 最大触发位置偏差(mm)
    float32 triggerResponseTimeMS;      // 平均触发响应时间(ms)

    TriggerPrecisionAnalysis()
        : totalTriggerCount(0),
          successTriggerCount(0),
          averageTriggerDeviationMM(0.0f),
          maxTriggerDeviationMM(0.0f),
          triggerResponseTimeMS(0.0f) {}

    // 计算成功率
    float32 GetSuccessRate() const {
        if (totalTriggerCount == 0) return 0.0f;
        return static_cast<float32>(successTriggerCount) / static_cast<float32>(totalTriggerCount) * 100.0f;
    }
};

/**
 * @brief 偏差点
 * @details 单个轨迹点的偏差信息
 */
struct DeviationPoint {
    Point2D theoreticalPosition;  // 理论位置
    Point2D actualPosition;       // 实际位置
    float32 deviation;            // 偏差距离(mm)
    float32 timestamp;            // 时间戳

    DeviationPoint() : theoreticalPosition{0.0f, 0.0f}, actualPosition{0.0f, 0.0f}, deviation(0.0f), timestamp(0.0f) {}
};

/**
 * @brief 轨迹分析
 * @details 存储理论轨迹与实际路径的偏差分析结果
 */
struct TrajectoryAnalysis {
    DeviationStatistics maxDeviation;             // 最大偏差统计
    DeviationStatistics averageDeviation;         // 平均偏差统计
    DeviationStatistics rmsDeviation;             // RMS偏差统计
    TriggerPrecisionAnalysis triggerPrecision;    // 触发精度分析
    std::vector<DeviationPoint> deviationPoints;  // 逐点偏差数据

    TrajectoryAnalysis() = default;
};

/**
 * @brief 性能指标
 * @details 测试执行的性能统计
 */
struct PerformanceMetrics {
    float32 configValidationTimeMS;      // 配置验证耗时(ms)
    float32 trajectoryGenerationTimeMS;  // 轨迹生成耗时(ms)
    float32 executionTimeSeconds;        // 轨迹执行时间(s)
    float32 analysisTimeMS;              // 偏差分析耗时(ms)
    float32 reportGenerationTimeMS;      // 报告生成耗时(ms)
    float32 totalTestTimeSeconds;        // 总测试时间(s)

    PerformanceMetrics()
        : configValidationTimeMS(0.0f),
          trajectoryGenerationTimeMS(0.0f),
          executionTimeSeconds(0.0f),
          analysisTimeMS(0.0f),
          reportGenerationTimeMS(0.0f),
          totalTestTimeSeconds(0.0f) {}

    // 验证所有时间值非负
    bool Validate() const {
        return configValidationTimeMS >= 0.0f && trajectoryGenerationTimeMS >= 0.0f && executionTimeSeconds >= 0.0f &&
               analysisTimeMS >= 0.0f && reportGenerationTimeMS >= 0.0f && totalTestTimeSeconds >= 0.0f;
    }
};

/**
 * @brief CMP测试数据
 * @details 封装测试执行后的完整结果数据
 */
// T026: 补偿分析数据结构
struct CompensationAnalysis {
    bool compensationEnabled;                    // 补偿是否启用
    float32 averageCompensationDistanceMM;       // 平均补偿距离(mm)
    float32 maxCompensationDistanceMM;           // 最大补偿距离(mm)
    std::vector<Point2D> compensatedPositions;   // 补偿后位置列表
    std::vector<float32> compensationDistances;  // 逐点补偿距离

    CompensationAnalysis()
        : compensationEnabled(false), averageCompensationDistanceMM(0.0f), maxCompensationDistanceMM(0.0f) {}
};

struct CMPTestData {
    std::vector<TrajectoryPoint> theoreticalTrajectory;   // 理论轨迹
    std::vector<TrajectoryPoint> actualPath;              // 实际采集路径
    TrajectoryAnalysis trajectoryAnalysis;                // 轨迹偏差分析
    PerformanceMetrics performanceMetrics;                // 性能指标
    CMPTestParams testParams;                             // 原始测试参数(用于报告)
    std::chrono::system_clock::time_point testTimestamp;  // 测试时间
    float32 testDurationSeconds;                          // 测试总耗时(秒)

    // T026: 补偿分析数据
    CompensationAnalysis compensationAnalysis;  // 补偿分析结果

    CMPTestData() : testTimestamp(std::chrono::system_clock::now()), testDurationSeconds(0.0f) {}

    /**
     * @brief 生成简要摘要
     * @return 摘要字符串
     */
    std::string summary() const {
        std::string result = "=== CMP测试摘要 ===\n";
        result += "理论轨迹点数: " + std::to_string(theoreticalTrajectory.size()) + "\n";
        result += "实际路径点数: " + std::to_string(actualPath.size()) + "\n";
        result += "最大位置偏差: " + std::to_string(trajectoryAnalysis.maxDeviation.positionDeviationMM) + "mm\n";
        result += "平均位置偏差: " + std::to_string(trajectoryAnalysis.averageDeviation.positionDeviationMM) + "mm\n";
        result += "RMS位置偏差: " + std::to_string(trajectoryAnalysis.rmsDeviation.positionDeviationMM) + "mm\n";
        result += "触发成功率: " + std::to_string(trajectoryAnalysis.triggerPrecision.GetSuccessRate()) + "%\n";
        result += "总测试时间: " + std::to_string(performanceMetrics.totalTestTimeSeconds) + "s\n";
        return result;
    }

    /**
     * @brief 生成Markdown格式报告
     * @return Markdown格式的测试报告
     */
    std::string toMarkdownReport() const {
        std::string report = "# CMP测试报告\n\n";

        // 测试参数
        report += "## 测试参数\n\n";
        report += "| 参数 | 值 |\n";
        report += "|------|----|\n";
        report += "| 起点 | (" + std::to_string(testParams.trajectory.startPosition.x) + ", " +
                  std::to_string(testParams.trajectory.startPosition.y) + ") mm |\n";
        report += "| 终点 | (" + std::to_string(testParams.trajectory.endPosition.x) + ", " +
                  std::to_string(testParams.trajectory.endPosition.y) + ") mm |\n";
        report += "| 速度 | " + std::to_string(testParams.trajectory.velocity) + " mm/s |\n";
        report += "| 加速度 | " + std::to_string(testParams.trajectory.acceleration) + " mm/s² |\n";
        report += "| 采样率 | " + std::to_string(testParams.samplingRateHz) + " Hz |\n";
        report += "| 触发模式 | " + std::string(CMPTriggerModeToString(testParams.cmpConfig.trigger_mode)) + " |\n";
        report += "\n";

        // 轨迹分析
        report += "## 轨迹分析\n\n";
        report += "| 指标 | 最大值 | 平均值 | RMS值 |\n";
        report += "|------|--------|--------|-------|\n";
        report += "| 位置偏差(mm) | " + std::to_string(trajectoryAnalysis.maxDeviation.positionDeviationMM) + " | " +
                  std::to_string(trajectoryAnalysis.averageDeviation.positionDeviationMM) + " | " +
                  std::to_string(trajectoryAnalysis.rmsDeviation.positionDeviationMM) + " |\n";
        report += "| 时间偏差(ms) | " + std::to_string(trajectoryAnalysis.maxDeviation.timeDeviationMS) + " | " +
                  std::to_string(trajectoryAnalysis.averageDeviation.timeDeviationMS) + " | " +
                  std::to_string(trajectoryAnalysis.rmsDeviation.timeDeviationMS) + " |\n";
        report += "| 速度偏差(%) | " + std::to_string(trajectoryAnalysis.maxDeviation.velocityDeviationPercent) +
                  " | " + std::to_string(trajectoryAnalysis.averageDeviation.velocityDeviationPercent) + " | " +
                  std::to_string(trajectoryAnalysis.rmsDeviation.velocityDeviationPercent) + " |\n";
        report += "\n";

        // 触发精度分析
        report += "## 触发精度分析\n\n";
        report += "| 指标 | 值 |\n";
        report += "|------|----|\n";
        report += "| 总触发次数 | " + std::to_string(trajectoryAnalysis.triggerPrecision.totalTriggerCount) + " |\n";
        report +=
            "| 成功触发次数 | " + std::to_string(trajectoryAnalysis.triggerPrecision.successTriggerCount) + " |\n";
        report += "| 成功率 | " + std::to_string(trajectoryAnalysis.triggerPrecision.GetSuccessRate()) + "% |\n";
        report += "| 平均触发偏差 | " + std::to_string(trajectoryAnalysis.triggerPrecision.averageTriggerDeviationMM) +
                  " mm |\n";
        report +=
            "| 最大触发偏差 | " + std::to_string(trajectoryAnalysis.triggerPrecision.maxTriggerDeviationMM) + " mm |\n";
        report +=
            "| 平均响应时间 | " + std::to_string(trajectoryAnalysis.triggerPrecision.triggerResponseTimeMS) + " ms |\n";
        report += "\n";

        // 性能指标
        report += "## 性能指标\n\n";
        report += "| 阶段 | 耗时 |\n";
        report += "|------|------|\n";
        report += "| 配置验证 | " + std::to_string(performanceMetrics.configValidationTimeMS) + " ms |\n";
        report += "| 轨迹生成 | " + std::to_string(performanceMetrics.trajectoryGenerationTimeMS) + " ms |\n";
        report += "| 轨迹执行 | " + std::to_string(performanceMetrics.executionTimeSeconds) + " s |\n";
        report += "| 偏差分析 | " + std::to_string(performanceMetrics.analysisTimeMS) + " ms |\n";
        report += "| 报告生成 | " + std::to_string(performanceMetrics.reportGenerationTimeMS) + " ms |\n";
        report += "| **总计** | **" + std::to_string(performanceMetrics.totalTestTimeSeconds) + " s** |\n";
        report += "\n";

        // 结论
        report += "## 结论\n\n";
        bool positionPassed =
            trajectoryAnalysis.maxDeviation.positionDeviationMM <= testParams.validationTolerance.positionToleranceMM;
        bool timePassed =
            trajectoryAnalysis.maxDeviation.timeDeviationMS <= testParams.validationTolerance.timeToleranceMS;
        bool testPassed = positionPassed && timePassed;

        report += "- **位置精度**: " + std::string(positionPassed ? "通过" : "失败") +
                  " (最大偏差: " + std::to_string(trajectoryAnalysis.maxDeviation.positionDeviationMM) +
                  "mm, 容差: " + std::to_string(testParams.validationTolerance.positionToleranceMM) + "mm)\n";
        report += "- **时间精度**: " + std::string(timePassed ? "通过" : "失败") +
                  " (最大偏差: " + std::to_string(trajectoryAnalysis.maxDeviation.timeDeviationMS) +
                  "ms, 容差: " + std::to_string(testParams.validationTolerance.timeToleranceMS) + "ms)\n";
        report += "- **综合结论**: " + std::string(testPassed ? "**测试通过**" : "**测试失败**") + "\n";

        return report;
    }

    /**
     * @brief 导出CSV数据 (可选功能)
     * @param filepath CSV文件路径
     */
    void exportToCSV(const std::string& filepath) const {
        // TODO: Phase 5 可选实现 - CSV导出功能
        (void)filepath;  // 避免未使用参数警告
    }
};

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen

