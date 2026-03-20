// HardwareConfiguration.h - 硬件配置和单位转换 (Hardware configuration and unit conversion)
// Task: P0-6 - 统一硬件单位转换配置
#pragma once

#include "Error.h"
#include "Result.h"

#include <string>

namespace Siligen {
namespace Shared {
namespace Types {

// 硬件配置结构体 (Hardware configuration structure)
// 用于统一管理所有硬件相关的配置参数,特别是单位转换系数
// (Used to uniformly manage all hardware-related configuration parameters, especially unit conversion coefficients)
struct HardwareConfiguration {
    // 单位转换系数 (Unit conversion coefficient)
    // 每毫米的脉冲数 - 所有适配器必须使用此统一配置
    // (Pulses per millimeter - all adapters must use this unified configuration)
    float32 pulse_per_mm = 10000.0f;

    // 运动参数限制 (Motion parameter limits)
    float32 max_velocity_mm_s = 0.0f;        // 最大速度 mm/s
    float32 max_acceleration_mm_s2 = 0.0f;  // 最大加速度 mm/s²
    float32 max_deceleration_mm_s2 = 0.0f;  // 最大减速度 mm/s²

    // 位置限制 (Position limits)
    float32 position_tolerance_mm = 0.1f;      // 位置容差 mm
    float32 soft_limit_positive_mm = 480.0f;  // 正向软限位 mm
    float32 soft_limit_negative_mm = 0.0f;     // 负向软限位 mm

    // 超时设置 (Timeout settings)
    int32 response_timeout_ms = 5000;  // 响应超时 ms
    int32 motion_timeout_ms = 30000;   // 运动超时 ms

    // 轴配置 (Axis configuration)
    int32 num_axes = 2;  // 轴数量

    // 统一轴数量裁剪 (Clamp axis count to supported range)
    static int32 ClampAxisCount(int32 requested, int32 max_axes = 2) {
        if (max_axes < 1) {
            max_axes = 1;
        }
        if (requested < 1) {
            return 1;
        }
        return requested > max_axes ? max_axes : requested;
    }

    int32 EffectiveAxisCount(int32 max_axes = 2) const {
        return ClampAxisCount(num_axes, max_axes);
    }

    // 验证配置有效性 (Validate configuration validity)
    Result<void> Validate() const {
        // 验证单位转换系数
        if (pulse_per_mm <= 0.0f) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_CONFIG_VALUE, "pulse_per_mm must be positive"));
        }

        // 验证速度限制
        if (max_velocity_mm_s <= 0.0f) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_CONFIG_VALUE, "max_velocity_mm_s must be positive"));
        }

        // 验证加速度限制
        if (max_acceleration_mm_s2 <= 0.0f) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_CONFIG_VALUE, "max_acceleration_mm_s2 must be positive"));
        }

        // 验证减速度限制
        if (max_deceleration_mm_s2 <= 0.0f) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_CONFIG_VALUE, "max_deceleration_mm_s2 must be positive"));
        }

        // 验证位置容差
        if (position_tolerance_mm <= 0.0f) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_CONFIG_VALUE, "position_tolerance_mm must be positive"));
        }

        // 验证软限位
        if (soft_limit_positive_mm <= soft_limit_negative_mm) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_CONFIG_VALUE,
                                               "soft_limit_positive_mm must be greater than soft_limit_negative_mm"));
        }

        // 验证超时设置
        if (response_timeout_ms <= 0 || motion_timeout_ms <= 0) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_CONFIG_VALUE, "timeout values must be positive"));
        }

        // 验证轴数量
        if (num_axes <= 0 || num_axes > 8) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_CONFIG_VALUE, "num_axes must be in range [1, 8]"));
        }

        return Result<void>::Success();
    }

    // 转换为字符串用于日志 (Convert to string for logging)
    std::string ToString() const {
        return std::string("HardwareConfiguration{") + "pulse_per_mm=" + std::to_string(pulse_per_mm) +
               ", max_velocity_mm_s=" + std::to_string(max_velocity_mm_s) +
               ", max_acceleration_mm_s2=" + std::to_string(max_acceleration_mm_s2) +
               ", num_axes=" + std::to_string(num_axes) + "}";
    }
};

// 单位转换器类 (Unit converter class)
// 封装所有单位转换逻辑,确保所有适配器使用一致的转换系数
// (Encapsulates all unit conversion logic to ensure all adapters use consistent conversion coefficients)
class UnitConverter {
   public:
    // 从硬件配置构造 (Construct from hardware configuration)
    explicit UnitConverter(const HardwareConfiguration& config) : pulse_per_mm_(config.pulse_per_mm) {
        // 预计算倒数以优化除法性能
        mm_per_pulse_ = 1.0f / pulse_per_mm_;
    }

    // 默认构造函数 - 使用默认配置
    UnitConverter() : pulse_per_mm_(10000.0f), mm_per_pulse_(1.0f / 10000.0f) {}

    // 毫米转脉冲 (Millimeters to pulses)
    int32 MmToPulse(float32 mm) const {
        return static_cast<int32>(mm * pulse_per_mm_);
    }

    // 脉冲转毫米 (Pulses to millimeters)
    float32 PulseToMm(int32 pulse) const {
        return static_cast<float32>(pulse) * mm_per_pulse_;
    }

    // 速度转换: mm/s -> pulse/s (Velocity conversion: mm/s -> pulse/s)
    double VelocityMmSToPS(float32 velocity_mm_s) const {
        return static_cast<double>(velocity_mm_s * pulse_per_mm_);
    }

    // 速度转换: pulse/s -> mm/s (Velocity conversion: pulse/s -> mm/s)
    float32 VelocityPSToMmS(double velocity_pulse_s) const {
        return static_cast<float32>(velocity_pulse_s * mm_per_pulse_);
    }

    // 加速度转换: mm/s² -> pulse/s² (Acceleration conversion: mm/s² -> pulse/s²)
    double AccelerationMmS2ToPS2(float32 acceleration_mm_s2) const {
        return static_cast<double>(acceleration_mm_s2 * pulse_per_mm_);
    }

    // 加速度转换: pulse/s² -> mm/s² (Acceleration conversion: pulse/s² -> mm/s²)
    float32 AccelerationPS2ToMmS2(double acceleration_pulse_s2) const {
        return static_cast<float32>(acceleration_pulse_s2 * mm_per_pulse_);
    }

    // 获取转换系数 (Get conversion coefficient)
    float32 GetPulsePerMm() const {
        return pulse_per_mm_;
    }

    // 获取倒数转换系数 (Get inverse conversion coefficient)
    float32 GetMmPerPulse() const {
        return mm_per_pulse_;
    }

   private:
    float32 pulse_per_mm_;  // 每毫米的脉冲数
    float32 mm_per_pulse_;  // 每脉冲的毫米数 (预计算优化)
};

// 硬件模式枚举 (Hardware mode enumeration)
// 用于运行时选择使用真实硬件还是Mock硬件
// (Used to select between real hardware and mock hardware at runtime)
enum class HardwareMode {
    Real,  ///< Use actual MultiCard hardware
    Mock   ///< Use MockMultiCard for testing
};

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
