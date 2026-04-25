#pragma once

#include "motion_planning/contracts/IVelocityProfilePort.h"

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Domain::Motion::Ports::IVelocityProfilePort;
using Siligen::Domain::Motion::Ports::VelocityProfile;
using Siligen::Domain::Motion::Ports::VelocityProfileConfig;
using Siligen::Domain::Motion::Ports::VelocityProfileType;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::Result;

/**
 * @brief 7段S曲线速度规划器
 *
 * 纯计算逻辑，无任何外部资源依赖。
 */
class SevenSegmentSCurveProfile : public IVelocityProfilePort {
   public:
    SevenSegmentSCurveProfile() = default;
    ~SevenSegmentSCurveProfile() override = default;

    Result<VelocityProfile> GenerateProfile(
        float32 distance,
        const VelocityProfileConfig& config) const noexcept override;

    VelocityProfileType GetProfileType() const noexcept override {
        return VelocityProfileType::S_CURVE_7_SEG;
    }
};

}  // namespace Siligen::Domain::Motion::DomainServices
