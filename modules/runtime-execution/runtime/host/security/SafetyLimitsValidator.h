#pragma once

#include "security/config/SecurityConfigLoader.h"
#include "shared/errors/ErrorHandler.h"
#include "shared/types/Types.h"
#include "AuditLogger.h"

#include <string>

namespace Siligen {

// 验证结果
struct ValidationResult {
    bool valid = true;
    std::string error_message;
    SystemErrorCode error_code = SystemErrorCode::SUCCESS;
};

// 安全限制验证器
class SafetyLimitsValidator {
   public:
    SafetyLimitsValidator(AuditLogger& audit_logger, const SecurityConfig& config);

    // 验证速度参数
    ValidationResult ValidateSpeed(float32 speed_mm_s, const std::string& username = "");

    // 验证加速度参数
    ValidationResult ValidateAcceleration(float32 accel_mm_s2, const std::string& username = "");

    // 验证加加速度参数
    ValidationResult ValidateJerk(float32 jerk_mm_s3, const std::string& username = "");

    // 验证位置边界
    ValidationResult ValidatePosition(float32 x_mm, float32 y_mm, const std::string& username = "");

    // 验证单轴位置
    ValidationResult ValidateAxisPosition(int32 axis, float32 pos_mm, const std::string& username = "");

    // 验证运动参数组合
    ValidationResult ValidateMotionParams(float32 speed_mm_s,
                                          float32 accel_mm_s2,
                                          float32 jerk_mm_s3,
                                          const std::string& username = "");

    // 更新配置
    void UpdateConfig(const SecurityConfig& config);

    // 获取当前限制值
    const SecurityConfig& GetCurrentLimits() const {
        return config_;
    }

   private:
    AuditLogger& audit_logger_;
    SecurityConfig config_;

    // 记录验证失败
    void LogValidationFailure(const std::string& parameter, float32 value, float32 limit, const std::string& username);

    // 创建错误结果
    ValidationResult CreateError(const std::string& message, SystemErrorCode code);
};

}  // namespace Siligen

