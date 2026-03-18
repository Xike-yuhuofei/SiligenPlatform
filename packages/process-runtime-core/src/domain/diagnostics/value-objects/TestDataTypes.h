#pragma once

#include "DiagnosticTypes.h"
#include "../../motion/value-objects/HardwareTestTypes.h"
#include "../../motion/value-objects/TrajectoryAnalysisTypes.h"
#include "../../motion/value-objects/TrajectoryTypes.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Diagnostics {
namespace ValueObjects {

// 工艺/测试结果的结构化定义与校验应集中在 Domain 层；
// 持久化与格式适配由仓储端口与序列化模块负责。

using ::Siligen::Point2D;
using Siligen::Domain::Motion::ValueObjects::CMPParameters;
using Siligen::Domain::Motion::ValueObjects::InterpolationParameters;
using Siligen::Domain::Motion::ValueObjects::MotionQualityMetrics;
using Siligen::Domain::Motion::ValueObjects::PathQualityMetrics;
using Siligen::Domain::Motion::ValueObjects::TrajectoryAnalysis;
using Siligen::Domain::Motion::ValueObjects::TrajectoryInterpolationType;
using Siligen::Domain::Motion::ValueObjects::TrajectoryType;

using Siligen::Domain::Motion::ValueObjects::JogDirection;
using Siligen::Domain::Motion::ValueObjects::HomingTestMode;
using Siligen::Domain::Motion::ValueObjects::HomingTestDirection;
using Siligen::Domain::Motion::ValueObjects::HomingTestStatus;
using Siligen::Domain::Motion::ValueObjects::TriggerType;
using Siligen::Domain::Motion::ValueObjects::TriggerPoint;
using Siligen::Domain::Motion::ValueObjects::TriggerEvent;
using Siligen::Domain::Motion::ValueObjects::LimitSwitchState;
using Siligen::Domain::Motion::ValueObjects::HomingResult;
using Siligen::Shared::Types::LogicalAxisId;

/**
 * @brief 点动测试数据
 */
class JogTestData {
   public:
    std::int64_t recordId;  // 关联TestRecord.id

    // 测试参数
    LogicalAxisId axis;      // 移动轴(0=X, 1=Y, 2=Z, 3=U)
    JogDirection direction;  // 移动方向
    double speed;            // 移动速度(mm/s)
    double distance;         // 移动距离(mm)

    // 测试结果
    double startPosition;         // 起始位置(mm)
    double endPosition;           // 结束位置(mm)
    double actualDistance;        // 实际移动距离(mm)
    std::int32_t responseTimeMs;  // 响应时间(毫秒)

    /**
     * @brief 默认构造函数
     */
    JogTestData()
        : recordId(0),
          axis(LogicalAxisId::X),
          direction(JogDirection::Positive),
          speed(0.0),
          distance(0.0),
          startPosition(0.0),
          endPosition(0.0),
          actualDistance(0.0),
          responseTimeMs(0) {}

    /**
     * @brief 计算位置误差
     */
    double positionError() const {
        return std::abs(actualDistance - distance);
    }

    /**
     * @brief 验证数据有效性
     */
    bool isValid() const {
        return Siligen::Shared::Types::IsValid(axis) && speed > 0 && distance > 0 && responseTimeMs >= 0;
    }
};

/**
 * @brief 回零测试数据
 */
class HomingTestData {
   public:
    std::int64_t recordId;  // 关联TestRecord.id

    // 测试参数
    std::vector<LogicalAxisId> axes;  // 回零轴列表
    HomingTestMode mode;            // 回零模式
    double searchSpeed;             // 搜索速度(mm/s)
    double returnSpeed;             // 返回速度(mm/s)
    HomingTestDirection direction;  // 回零方向

    // 测试结果
    std::map<LogicalAxisId, HomingResult> axisResults;  // 每个轴的回零结果

    // 诊断信息
    std::map<LogicalAxisId, LimitSwitchState> limitStates;  // 每个轴的限位开关状态

    /**
     * @brief 默认构造函数
     */
    HomingTestData()
        : recordId(0),
          mode(HomingTestMode::SingleAxis),
          searchSpeed(0.0),
          returnSpeed(0.0),
          direction(HomingTestDirection::Negative) {}

    /**
     * @brief 检查所有轴是否回零成功
     */
    bool allSucceeded() const {
        for (const auto& [axis, result] : axisResults) {
            if (!result.success) {
                return false;
            }
        }
        return !axisResults.empty();
    }

    /**
     * @brief 验证数据有效性
     */
    bool isValid() const {
        return !axes.empty() && searchSpeed > 0 && returnSpeed > 0 && returnSpeed < searchSpeed;
    }
};

/**
 * @brief 触发测试数据
 */
class TriggerTestData {
   public:
    std::int64_t recordId;  // 关联TestRecord.id

    // 测试参数
    TriggerType triggerType;                  // 触发类型
    std::vector<TriggerPoint> triggerPoints;  // 触发点配置

    // 测试结果
    std::vector<TriggerEvent> triggerEvents;  // 实际触发事件

    // 统计数据
    TriggerStatistics statistics;

    /**
     * @brief 默认构造函数
     */
    TriggerTestData() : recordId(0), triggerType(TriggerType::SinglePoint) {}

    /**
     * @brief 验证数据有效性
     */
    bool isValid() const {
        return !triggerPoints.empty();
    }
};

/**
 * @brief CMP测试数据
 */
class CMPTestData {
   public:
    std::int64_t recordId;  // 关联TestRecord.id

    // 测试参数
    TrajectoryType trajectoryType;       // 轨迹类型
    std::vector<Point2D> controlPoints;  // 控制点
    CMPParameters cmpParams;             // CMP参数配置

    // 轨迹数据
    std::vector<Point2D> theoreticalPath;   // 理论轨迹点
    std::vector<Point2D> actualPath;        // 实际轨迹点
    std::vector<Point2D> compensationData;  // 补偿数据

    // 精度分析
    TrajectoryAnalysis analysis;

    /**
     * @brief 默认构造函数
     */
    CMPTestData() : recordId(0), trajectoryType(TrajectoryType::Linear) {}

    /**
     * @brief 验证数据有效性
     */
    bool isValid() const {
        // 控制点数量检查
        if (controlPoints.size() < 2) {
            return false;
        }
        // 轨迹点数量一致性检查
        if (!theoreticalPath.empty() && !actualPath.empty()) {
            if (theoreticalPath.size() != actualPath.size()) {
                return false;
            }
        }
        return true;
    }
};

/**
 * @brief 插补测试数据
 */
class InterpolationTestData {
   public:
    std::int64_t recordId;  // 关联TestRecord.id

    // 测试参数
    TrajectoryInterpolationType interpolationType;  // 插补算法类型
    std::vector<Point2D> controlPoints;             // 控制点
    InterpolationParameters interpParams;           // 插补参数

    // 生成的轨迹
    std::vector<Point2D> interpolatedPath;  // 插补生成的轨迹点

    // 性能分析
    PathQualityMetrics pathQuality;
    MotionQualityMetrics motionQuality;

    /**
     * @brief 默认构造函数
     */
    InterpolationTestData() : recordId(0), interpolationType(TrajectoryInterpolationType::Linear) {}

    /**
     * @brief 验证数据有效性
     */
    bool isValid() const {
        // 根据插补类型检查控制点数量
        int minPoints = 2;  // 默认直线插补至少2个点
        switch (interpolationType) {
            case TrajectoryInterpolationType::Linear:
                minPoints = 2;
                break;
            case TrajectoryInterpolationType::CircularArc:
                minPoints = 3;
                break;
            case TrajectoryInterpolationType::BSpline:
            case TrajectoryInterpolationType::Bezier:
                minPoints = 4;
                break;
        }
        return controlPoints.size() >= static_cast<size_t>(minPoints) && !interpolatedPath.empty() &&
               interpParams.targetFeedRate > 0;
    }
};

/**
 * @brief 系统诊断结果
 */
class DiagnosticResult {
   public:
    std::int64_t recordId;  // 关联TestRecord.id

    // 诊断项结果
    HardwareCheckResult hardwareCheck;
    CommunicationCheckResult commCheck;
    ResponseTimeCheckResult responseCheck;
    AccuracyCheckResult accuracyCheck;

    // 综合评估
    int healthScore;                      // 健康评分(0-100)
    std::vector<DiagnosticIssue> issues;  // 发现的问题列表

    /**
     * @brief 默认构造函数
     */
    DiagnosticResult() : recordId(0), healthScore(100) {}

    /**
     * @brief 计算健康评分
     * @return 评分(0-100), 100表示完全健康
     */
    int calculateHealthScore() const {
        int score = 100;
        for (const auto& issue : issues) {
            switch (issue.severity) {
                case IssueSeverity::Critical:
                    score -= 25;
                    break;
                case IssueSeverity::Error:
                    score -= 10;
                    break;
                case IssueSeverity::Warning:
                    score -= 5;
                    break;
                case IssueSeverity::Info:
                    // Info不扣分
                    break;
            }
        }
        return (score < 0) ? 0 : score;
    }

    /**
     * @brief 验证数据有效性
     */
    bool isValid() const {
        return healthScore >= 0 && healthScore <= 100;
    }
};

}  // namespace ValueObjects
}  // namespace Diagnostics
}  // namespace Domain
}  // namespace Siligen
