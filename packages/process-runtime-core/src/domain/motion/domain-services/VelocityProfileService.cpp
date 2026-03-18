#include "VelocityProfileService.h"

#include <cmath>

namespace Siligen::Domain::Motion::DomainServices {

VelocityProfileService::VelocityProfileService(
    std::shared_ptr<IVelocityProfilePort> profile_port) noexcept
    : profile_port_(std::move(profile_port)) {}

Result<VelocityProfile> VelocityProfileService::GenerateProfile(
    float32 distance,
    const VelocityProfileConfig& config) const noexcept {

    // 1. 验证配置
    auto validate_result = ValidateConfig(config);
    if (!validate_result.IsSuccess()) {
        return Result<VelocityProfile>::Failure(validate_result.GetError());
    }

    // 2. 验证距离
    if (distance <= 0.0f) {
        return Result<VelocityProfile>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "Distance must be positive"));
    }

    // 3. 委托给端口实现
    if (!profile_port_) {
        return Result<VelocityProfile>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED,
                                 "Velocity profile port not initialized"));
    }

    return profile_port_->GenerateProfile(distance, config);
}

Result<void> VelocityProfileService::ValidateConfig(
    const VelocityProfileConfig& config) const noexcept {

    if (config.max_velocity <= 0.0f) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "max_velocity must be positive"));
    }

    if (config.max_acceleration <= 0.0f) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "max_acceleration must be positive"));
    }

    if (config.max_jerk <= 0.0f) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "max_jerk must be positive"));
    }

    if (config.time_step <= 0.0f) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "time_step must be positive"));
    }

    return Result<void>::Success();
}

float32 VelocityProfileService::CalculateMinDistanceFor7Seg(
    const VelocityProfileConfig& config) const noexcept {

    // 7段S曲线最小距离 = 2 * (加速段距离)
    // 加速段距离 = v^2 / (2*a) + v*a/j (简化估算)
    float32 v = config.max_velocity;
    float32 a = config.max_acceleration;
    float32 j = config.max_jerk;

    // 加速时间 t_a = a / j
    float32 t_a = a / j;
    // 加速段距离估算
    float32 s_acc = v * v / (2.0f * a) + v * t_a;

    return 2.0f * s_acc;
}

}  // namespace Siligen::Domain::Motion::DomainServices
