#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Motion::Ports {

using Shared::Types::float32;
using Shared::Types::Result;

/**
 * @brief 速度曲线类型
 */
enum class VelocityProfileType {
    TRAPEZOIDAL,      // 3段梯形曲线
    S_CURVE_5_SEG,    // 5段S曲线
    S_CURVE_7_SEG     // 7段S曲线
};

/**
 * @brief 速度规划配置
 */
struct VelocityProfileConfig {
    float32 max_velocity = 0.0f;         // mm/s
    float32 max_acceleration = 0.0f;     // mm/s^2
    float32 max_jerk = 0.0f;             // mm/s^3
    float32 time_step = 0.001f;          // s
};

/**
 * @brief 速度规划结果
 */
struct VelocityProfile {
    std::vector<float32> velocities;     // 速度序列
    std::vector<float32> accelerations;  // 加速度序列 (可选)
    float32 total_time = 0.0f;           // 总时间
    VelocityProfileType type = VelocityProfileType::S_CURVE_7_SEG;
};

/**
 * @brief 速度规划端口接口
 *
 * 定义速度曲线生成的抽象接口，支持多种速度规划算法。
 * 符合接口隔离原则(ISP) - 客户端仅依赖速度规划功能。
 */
class IVelocityProfilePort {
   public:
    virtual ~IVelocityProfilePort() = default;

    /**
     * @brief 生成速度曲线
     * @param distance 运动距离 (mm)
     * @param config 速度规划配置
     * @return 速度曲线结果
     */
    virtual Result<VelocityProfile> GenerateProfile(
        float32 distance,
        const VelocityProfileConfig& config) const noexcept = 0;

    /**
     * @brief 获取支持的曲线类型
     */
    virtual VelocityProfileType GetProfileType() const noexcept = 0;
};

}  // namespace Siligen::Domain::Motion::Ports
