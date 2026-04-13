#pragma once

#include "motion_planning/contracts/IVelocityProfilePort.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <memory>

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Domain::Motion::Ports::IVelocityProfilePort;
using Siligen::Domain::Motion::Ports::VelocityProfile;
using Siligen::Domain::Motion::Ports::VelocityProfileConfig;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::Result;

/**
 * @brief 速度规划领域服务
 *
 * 职责：协调速度曲线生成，应用业务规则
 *
 * 业务规则：
 * - 距离过短时自动降级曲线类型
 * - 验证配置参数合法性
 * - 确保生成的曲线满足运动学约束
 */
class VelocityProfileService {
   public:
    explicit VelocityProfileService(
        std::shared_ptr<IVelocityProfilePort> profile_port) noexcept;

    ~VelocityProfileService() = default;

    // 禁止拷贝和移动
    VelocityProfileService(const VelocityProfileService&) = delete;
    VelocityProfileService& operator=(const VelocityProfileService&) = delete;

    /**
     * @brief 生成速度曲线（带业务规则验证）
     * @param distance 运动距离 (mm)
     * @param config 速度规划配置
     * @return 速度曲线结果
     */
    Result<VelocityProfile> GenerateProfile(
        float32 distance,
        const VelocityProfileConfig& config) const noexcept;

    /**
     * @brief 验证配置参数
     * @param config 速度规划配置
     * @return 验证结果
     */
    Result<void> ValidateConfig(const VelocityProfileConfig& config) const noexcept;

    /**
     * @brief 计算最小距离阈值（低于此距离需要降级曲线类型）
     * @param config 速度规划配置
     * @return 最小距离 (mm)
     */
    float32 CalculateMinDistanceFor7Seg(const VelocityProfileConfig& config) const noexcept;

   private:
    std::shared_ptr<IVelocityProfilePort> profile_port_;
};

}  // namespace Siligen::Domain::Motion::DomainServices
