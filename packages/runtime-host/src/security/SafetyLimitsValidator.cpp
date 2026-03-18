#include "SafetyLimitsValidator.h"

#include "SecurityLogHelper.h"
#include "shared/errors/ErrorHandler.h"

#include <cmath>
#include <sstream>

namespace Siligen {

using Shared::Types::LogLevel;

SafetyLimitsValidator::SafetyLimitsValidator(AuditLogger& audit_logger, const SecurityConfig& config)
    : audit_logger_(audit_logger), config_(config) {
    SecurityLogHelper::Log(LogLevel::INFO,
                           "SafetyLimitsValidator",
                           "初始化安全限制: 速度=" + std::to_string(config_.max_speed_mm_s) + "mm/s, " +
                               "加速度=" + std::to_string(config_.max_acceleration_mm_s2) + "mm/s², " +
                               "加加速度=" + std::to_string(config_.max_jerk_mm_s3) + "mm/s³");
}

ValidationResult SafetyLimitsValidator::ValidateSpeed(float32 speed_mm_s, const std::string& username) {
    if (std::abs(speed_mm_s) > config_.max_speed_mm_s) {
        LogValidationFailure("速度", speed_mm_s, config_.max_speed_mm_s, username);
        return CreateError(
            "速度超限: " + std::to_string(speed_mm_s) + " > " + std::to_string(config_.max_speed_mm_s) + " mm/s",
            static_cast<SystemErrorCode>(-750));
    }
    return ValidationResult{};
}

ValidationResult SafetyLimitsValidator::ValidateAcceleration(float32 accel_mm_s2, const std::string& username) {
    if (std::abs(accel_mm_s2) > config_.max_acceleration_mm_s2) {
        LogValidationFailure("加速度", accel_mm_s2, config_.max_acceleration_mm_s2, username);
        return CreateError("加速度超限: " + std::to_string(accel_mm_s2) + " > " +
                               std::to_string(config_.max_acceleration_mm_s2) + " mm/s²",
                           static_cast<SystemErrorCode>(-751));
    }
    return ValidationResult{};
}

ValidationResult SafetyLimitsValidator::ValidateJerk(float32 jerk_mm_s3, const std::string& username) {
    if (std::abs(jerk_mm_s3) > config_.max_jerk_mm_s3) {
        LogValidationFailure("加加速度", jerk_mm_s3, config_.max_jerk_mm_s3, username);
        return CreateError(
            "加加速度超限: " + std::to_string(jerk_mm_s3) + " > " + std::to_string(config_.max_jerk_mm_s3) + " mm/s³",
            static_cast<SystemErrorCode>(-752));
    }
    return ValidationResult{};
}

ValidationResult SafetyLimitsValidator::ValidatePosition(float32 x_mm, float32 y_mm, const std::string& username) {
    // 验证X轴
    if (x_mm < config_.x_min_mm || x_mm > config_.x_max_mm) {
        LogValidationFailure("X位置", x_mm, (x_mm < config_.x_min_mm) ? config_.x_min_mm : config_.x_max_mm, username);
        return CreateError("X轴位置超限: " + std::to_string(x_mm) + " 不在 [" + std::to_string(config_.x_min_mm) +
                               ", " + std::to_string(config_.x_max_mm) + "] 范围内",
                           static_cast<SystemErrorCode>(-753));
    }

    // 验证Y轴
    if (y_mm < config_.y_min_mm || y_mm > config_.y_max_mm) {
        LogValidationFailure("Y位置", y_mm, (y_mm < config_.y_min_mm) ? config_.y_min_mm : config_.y_max_mm, username);
        return CreateError("Y轴位置超限: " + std::to_string(y_mm) + " 不在 [" + std::to_string(config_.y_min_mm) +
                               ", " + std::to_string(config_.y_max_mm) + "] 范围内",
                           static_cast<SystemErrorCode>(-753));
    }

    return ValidationResult{};
}

ValidationResult SafetyLimitsValidator::ValidateAxisPosition(int32 axis, float32 pos_mm, const std::string& username) {
    // 轴号约定: 0-based (X=0, Y=1)
    float32 min_val = 0.0f, max_val = 0.0f;

    if (axis == 0) {  // X轴 (0-based)
        min_val = config_.x_min_mm;
        max_val = config_.x_max_mm;
    } else if (axis == 1) {  // Y轴 (0-based)
        min_val = config_.y_min_mm;
        max_val = config_.y_max_mm;
    } else {
        return CreateError("不支持的轴号: " + std::to_string(axis) + " (期望0=X或1=Y)", static_cast<SystemErrorCode>(-500));
    }

    if (pos_mm < min_val || pos_mm > max_val) {
        std::string axis_name = (axis == 0) ? "X" : "Y";
        LogValidationFailure(axis_name + "位置", pos_mm, (pos_mm < min_val) ? min_val : max_val, username);
        return CreateError(axis_name + "轴位置超限: " + std::to_string(pos_mm) + " 不在 [" + std::to_string(min_val) +
                               ", " + std::to_string(max_val) + "] 范围内",
                           static_cast<SystemErrorCode>(-753));
    }

    return ValidationResult{};
}

ValidationResult SafetyLimitsValidator::ValidateMotionParams(float32 speed_mm_s,
                                                             float32 accel_mm_s2,
                                                             float32 jerk_mm_s3,
                                                             const std::string& username) {
    // 依次验证所有参数
    auto speed_result = ValidateSpeed(speed_mm_s, username);
    if (!speed_result.valid) return speed_result;

    auto accel_result = ValidateAcceleration(accel_mm_s2, username);
    if (!accel_result.valid) return accel_result;

    auto jerk_result = ValidateJerk(jerk_mm_s3, username);
    if (!jerk_result.valid) return jerk_result;

    return ValidationResult{};
}

void SafetyLimitsValidator::UpdateConfig(const SecurityConfig& config) {
    config_ = config;
    SecurityLogHelper::Log(LogLevel::INFO,
                           "SafetyLimitsValidator",
                           "更新安全限制: 速度=" + std::to_string(config_.max_speed_mm_s) + "mm/s, " +
                               "加速度=" + std::to_string(config_.max_acceleration_mm_s2) + "mm/s², " +
                               "加加速度=" + std::to_string(config_.max_jerk_mm_s3) + "mm/s³");
}

void SafetyLimitsValidator::LogValidationFailure(const std::string& parameter,
                                                 float32 value,
                                                 float32 limit,
                                                 const std::string& username) {
    // 系统日志
    SecurityLogHelper::Log(LogLevel::ERR,
                           "SafetyLimitsValidator",
                           parameter + "验证失败: " + std::to_string(value) + " 超过限制 " + std::to_string(limit));

    // 审计日志
    audit_logger_.LogSafetyEvent(username.empty() ? "系统" : username,
                                 parameter + "超限尝试",
                                 "值=" + std::to_string(value) + ", 限制=" + std::to_string(limit));
}

ValidationResult SafetyLimitsValidator::CreateError(const std::string& message, SystemErrorCode code) {
    ValidationResult result;
    result.valid = false;
    result.error_message = message;
    result.error_code = code;
    return result;
}

}  // namespace Siligen

