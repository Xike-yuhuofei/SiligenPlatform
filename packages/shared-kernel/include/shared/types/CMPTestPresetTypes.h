// CMPTestPresetTypes.h
// 版本: 1.0.0
// 描述: CMP测试预设相关类型定义
// 任务: CMP测试默认数据功能

#pragma once

#include "CMPTestTypes.h"

#include <string>
#include <vector>

namespace Siligen {
namespace Shared {
namespace Types {

/**
 * @brief 预设分类
 */
enum class PresetCategory {
    Basic,           // 基础验证
    Advanced,        // 进阶测试
    Performance,     // 性能基准
    Troubleshooting  // 问题诊断
};

/**
 * @brief 预设分类转字符串
 */
inline const char* PresetCategoryToString(PresetCategory category) {
    switch (category) {
        case PresetCategory::Basic:
            return "basic";
        case PresetCategory::Advanced:
            return "advanced";
        case PresetCategory::Performance:
            return "performance";
        case PresetCategory::Troubleshooting:
            return "troubleshooting";
        default:
            return "unknown";
    }
}

/**
 * @brief 字符串转预设分类
 */
inline PresetCategory StringToPresetCategory(const std::string& str) {
    if (str == "basic") return PresetCategory::Basic;
    if (str == "advanced") return PresetCategory::Advanced;
    if (str == "performance") return PresetCategory::Performance;
    if (str == "troubleshooting") return PresetCategory::Troubleshooting;
    return PresetCategory::Basic;
}

/**
 * @brief 预期测试结果
 */
struct ExpectedResults {
    float32 maxDeviationMM;  // 预期最大偏差(mm)
    float32 successRate;     // 预期成功率(%)

    ExpectedResults() : maxDeviationMM(0.1f), successRate(95.0f) {}
};

/**
 * @brief CMP测试预设
 */
struct CMPTestPreset {
    std::string name;                 // 预设名称
    std::string description;          // 场景说明
    PresetCategory category;          // 分类标签
    CMPTestParams params;             // 完整测试参数
    ExpectedResults expectedResults;  // 预期结果(可选)

    CMPTestPreset()
        : name("未命名预设"), description(""), category(PresetCategory::Basic), params(), expectedResults() {}

    /**
     * @brief 验证预设有效性
     */
    bool Validate() const {
        if (name.empty()) return false;
        if (!params.Validate()) return false;
        return true;
    }

    /**
     * @brief 转换为字符串
     */
    std::string ToString() const {
        std::string result = "[CMPTestPreset] ";
        result += name + " (" + std::string(PresetCategoryToString(category)) + ")\n";
        result += "描述: " + description + "\n";
        result += "速度: " + std::to_string(params.trajectory.velocity) + " mm/s\n";
        result += "触发模式: " + std::string(CMPTriggerModeToString(params.cmpConfig.trigger_mode)) + "\n";
        result += "预期最大偏差: " + std::to_string(expectedResults.maxDeviationMM) + " mm\n";
        return result;
    }
};

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
