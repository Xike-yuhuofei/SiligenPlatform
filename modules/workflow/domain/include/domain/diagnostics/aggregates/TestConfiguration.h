#pragma once

#include "shared/types/AxisTypes.h"

#include <cstdint>
#include <map>
#include <string>

namespace Siligen {
namespace Domain {
namespace Diagnostics {
namespace Aggregates {

/**
 * @brief 轴限位配置
 */
struct AxisLimits {
    double minPosition;  // 最小位置(mm)
    double maxPosition;  // 最大位置(mm)

    AxisLimits() : minPosition(-200.0), maxPosition(200.0) {}
    AxisLimits(double min, double max) : minPosition(min), maxPosition(max) {}

    bool isValid() const {
        return minPosition < maxPosition;
    }
};

/**
 * @brief 安全限制参数
 */
struct SafetyLimits {
    double maxJogSpeed;                    // 最大点动速度(mm/s)
    double maxHomingSpeed;                 // 最大回零速度(mm/s)
    std::map<Siligen::Shared::Types::LogicalAxisId, AxisLimits> axisLimits;  // 每个轴的软限位

    SafetyLimits() : maxJogSpeed(100.0), maxHomingSpeed(50.0) {
        // 默认2轴限位
        axisLimits[Siligen::Shared::Types::LogicalAxisId::X] = AxisLimits(-200.0, 200.0);  // X轴
        axisLimits[Siligen::Shared::Types::LogicalAxisId::Y] = AxisLimits(-150.0, 150.0);  // Y轴
    }
};

/**
 * @brief 测试参数配置
 */
struct TestParameters {
    std::int32_t homingTimeoutMs;  // 回零超时(毫秒)
    double triggerToleranceMm;     // 触发位置容差(mm)
    int cmpSampleRateHz;           // CMP采样频率(Hz)
    bool dataRecordEnabled;        // 是否记录测试数据
    int maxStoredRecords;          // 最大存储记录数

    TestParameters()
        : homingTimeoutMs(30000),
          triggerToleranceMm(0.1),
          cmpSampleRateHz(100),
          dataRecordEnabled(true),
          maxStoredRecords(10000) {}
};

/**
 * @brief 显示参数配置
 */
struct DisplayParameters {
    std::int32_t positionRefreshIntervalMs;  // 位置刷新间隔(ms)
    int chartMaxPoints;                      // 图表最大显示点数
    bool showDetailedErrors;                 // 是否显示详细错误信息

    DisplayParameters()
        : positionRefreshIntervalMs(100)  // 10Hz
          ,
          chartMaxPoints(1000),
          showDetailedErrors(true) {}
};

/**
 * @brief 测试配置实体
 *
 * 测试功能的全局配置参数,支持持久化和运行时加载。
 * 从config/hardware_test_config.ini文件加载。
 */
class TestConfiguration {
   public:
    // 配置数据
    SafetyLimits safetyLimits;
    TestParameters testParams;
    DisplayParameters displayParams;

    /**
     * @brief 构造函数 - 使用默认值
     */
    TestConfiguration() = default;

    /**
     * @brief 验证配置有效性
     */
    bool isValid() const {
        // 检查安全限制
        if (safetyLimits.maxJogSpeed <= 0 || safetyLimits.maxJogSpeed >= 500.0) {
            return false;
        }
        if (safetyLimits.maxHomingSpeed <= 0 || safetyLimits.maxHomingSpeed >= safetyLimits.maxJogSpeed) {
            return false;
        }

        // 检查轴限位
        for (const auto& [axis, limits] : safetyLimits.axisLimits) {
            if (!limits.isValid()) {
                return false;
            }
        }

        // 检查测试参数
        if (testParams.homingTimeoutMs <= 0) {
            return false;
        }
        if (testParams.cmpSampleRateHz <= 0 || testParams.cmpSampleRateHz > 1000) {
            return false;
        }

        // 检查显示参数
        if (displayParams.positionRefreshIntervalMs < 50) {  // 最高20Hz
            return false;
        }
        if (displayParams.chartMaxPoints < 100 || displayParams.chartMaxPoints > 10000) {
            return false;
        }

        return true;
    }

    /**
     * @brief 获取指定轴的限位
     */
    const AxisLimits* getAxisLimits(Siligen::Shared::Types::LogicalAxisId axis) const {
        auto it = safetyLimits.axisLimits.find(axis);
        if (it != safetyLimits.axisLimits.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /**
     * @brief 检查位置是否在限位范围内
     */
    bool isPositionInRange(Siligen::Shared::Types::LogicalAxisId axis, double position) const {
        const AxisLimits* limits = getAxisLimits(axis);
        if (limits == nullptr) {
            return false;  // 轴未配置
        }
        return position >= limits->minPosition && position <= limits->maxPosition;
    }

    /**
     * @brief 检查速度是否在安全范围内
     */
    bool isJogSpeedSafe(double speed) const {
        return speed > 0 && speed <= safetyLimits.maxJogSpeed;
    }

    /**
     * @brief 检查回零速度是否在安全范围内
     */
    bool isHomingSpeedSafe(double speed) const {
        return speed > 0 && speed <= safetyLimits.maxHomingSpeed;
    }
};

}  // namespace Aggregates
}  // namespace Diagnostics
}  // namespace Domain
}  // namespace Siligen
