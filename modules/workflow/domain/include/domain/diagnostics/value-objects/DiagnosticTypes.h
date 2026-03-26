#pragma once

#include "shared/types/AxisTypes.h"

#include <boost/describe/enum.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Diagnostics {
namespace ValueObjects {

/**
 * @brief 问题严重性枚举
 */
enum class IssueSeverity {
    Info,     // 信息
    Warning,  // 警告
    Error,    // 错误
    Critical  // 严重错误
};
BOOST_DESCRIBE_ENUM(IssueSeverity, Info, Warning, Error, Critical)

/**
 * @brief 诊断问题
 */
struct DiagnosticIssue {
    IssueSeverity severity;
    std::string description;      // 问题描述
    std::string suggestedAction;  // 建议解决方案

    DiagnosticIssue() : severity(IssueSeverity::Info) {}

    DiagnosticIssue(IssueSeverity sev, const std::string& desc, const std::string& action)
        : severity(sev), description(desc), suggestedAction(action) {}
};

/**
 * @brief 硬件检查结果
 */
struct HardwareCheckResult {
    bool controllerConnected;
    std::vector<Siligen::Shared::Types::LogicalAxisId> enabledAxes;       // 已启用的轴
    std::map<Siligen::Shared::Types::LogicalAxisId, bool> limitSwitchOk;  // 限位开关状态正常
    std::map<Siligen::Shared::Types::LogicalAxisId, bool> encoderOk;      // 编码器状态正常

    HardwareCheckResult() : controllerConnected(false) {}
};

/**
 * @brief 通信检查结果
 */
struct CommunicationCheckResult {
    double avgResponseTimeMs;  // 平均响应时间
    double maxResponseTimeMs;  // 最大响应时间
    double packetLossRate;     // 丢包率(百分比)
    int totalCommands;         // 测试命令总数
    int failedCommands;        // 失败命令数

    CommunicationCheckResult()
        : avgResponseTimeMs(0.0), maxResponseTimeMs(0.0), packetLossRate(0.0), totalCommands(0), failedCommands(0) {}

    /**
     * @brief 计算通信成功率
     */
    double successRate() const {
        if (totalCommands == 0) return 0.0;
        return 100.0 * (totalCommands - failedCommands) / totalCommands;
    }
};

/**
 * @brief 响应时间检查结果
 */
struct ResponseTimeCheckResult {
    std::map<Siligen::Shared::Types::LogicalAxisId, double> axisResponseTimeMs;  // 每个轴的响应时间
    bool allWithinSpec;                        // 所有轴响应时间是否在规范内
    double specLimitMs;                        // 规范限制值

    ResponseTimeCheckResult() : allWithinSpec(true), specLimitMs(10.0) {}
};

/**
 * @brief 精度检查结果
 */
struct AccuracyCheckResult {
    std::map<Siligen::Shared::Types::LogicalAxisId, double> repeatabilityError;  // 重复定位精度(mm)
    std::map<Siligen::Shared::Types::LogicalAxisId, double> positioningError;    // 单向定位精度(mm)
    bool meetsAccuracyRequirement;             // 是否满足精度要求
    double requiredAccuracy;                   // 要求精度(mm)

    AccuracyCheckResult() : meetsAccuracyRequirement(true), requiredAccuracy(0.05) {}
};

/**
 * @brief 触发统计数据
 */
struct TriggerStatistics {
    double maxPositionError;         // 最大位置误差(mm)
    double avgPositionError;         // 平均位置误差(mm)
    std::int32_t maxResponseTimeUs;  // 最大响应时间(微秒)
    std::int32_t avgResponseTimeUs;  // 平均响应时间(微秒)
    int successfulTriggers;          // 成功触发次数
    int missedTriggers;              // 漏触发次数

    TriggerStatistics()
        : maxPositionError(0.0),
          avgPositionError(0.0),
          maxResponseTimeUs(0),
          avgResponseTimeUs(0),
          successfulTriggers(0),
          missedTriggers(0) {}

    /**
     * @brief 计算触发成功率
     */
    double successRate() const {
        int total = successfulTriggers + missedTriggers;
        if (total == 0) return 0.0;
        return 100.0 * successfulTriggers / total;
    }
};

}  // namespace ValueObjects
}  // namespace Diagnostics
}  // namespace Domain
}  // namespace Siligen
