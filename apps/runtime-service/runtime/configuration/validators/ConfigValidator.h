#pragma once

#include "domain/configuration/ports/IConfigurationPort.h"

#include <string>
#include <vector>

namespace Siligen {
using Domain::Configuration::Ports::HomingConfig;
using Domain::Configuration::Ports::DispensingConfig;
using Domain::Configuration::Ports::MachineConfig;

/**
 * @brief 配置验证结果
 */
struct ValidationResult {
    bool is_valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    void AddError(const std::string& error) {
        errors.push_back(error);
        is_valid = false;
    }

    void AddWarning(const std::string& warning) {
        warnings.push_back(warning);
    }

    void Clear() {
        is_valid = true;
        errors.clear();
        warnings.clear();
    }
};

class ConfigValidator {
   public:
    // 验证回零配置
    static bool ValidateHomingConfig(const HomingConfig& config);
    static ValidationResult ValidateHomingConfigDetailed(const HomingConfig& config);
    static ValidationResult ValidateHomingConfigDetailed(const HomingConfig& config,
                                                         const MachineConfig& machine,
                                                         int axis_count);
    // 验证速度参数
    static bool ValidateVelocity(float32 velocity, float32 min_val = 0.1f, float32 max_val = 1000.0f);
    static bool ValidateVelocityRelationship(float32 rapid, float32 locate, float32 index);

    // 验证加速度参数
    static bool ValidateAcceleration(float32 acceleration, float32 min_val = 1.0f, float32 max_val = 10000.0f);

    // 验证距离参数
    static bool ValidateDistance(float32 distance, float32 min_val = -1000.0f, float32 max_val = 1000.0f);

    // 验证时间参数
    static bool ValidateTimeout(int32 timeout_ms, int32 min_val = 100, int32 max_val = 300000);

    // 验证范围值
    static bool ValidateRange(int32 value, int32 min_val, int32 max_val);
    static bool ValidateRange(float32 value, float32 min_val, float32 max_val);

    // 获取参数建议
    static std::string GetVelocitySuggestion(float32 current, float32 recommended);
    static std::string GetAccelerationSuggestion(float32 current, float32 recommended);
    static std::string GetTimeoutSuggestion(int32 current, int32 recommended);

    // 验证模式兼容性
    static bool ValidateModeCompatibility(int32 mode, bool enable_limit_switch, bool enable_index);

    // 最佳实践验证（返回警告，不阻断流程）
    static ValidationResult ValidateSeamlessUnderwearBestPractices(const DispensingConfig& config);
    static ValidationResult ValidateMotionBestPractices(const MachineConfig& config);

   private:
    // 内部辅助方法
    static bool IsInRange(float32 value, float32 min_val, float32 max_val);
    static bool IsInRange(int32 value, int32 min_val, int32 max_val);
    static std::string FormatParameterName(const std::string& param_name);
    static std::string FormatRangeError(const std::string& param_name, float32 value, float32 min_val, float32 max_val);
    static std::string FormatRangeError(const std::string& param_name, int32 value, int32 min_val, int32 max_val);
};

}  // namespace Siligen


