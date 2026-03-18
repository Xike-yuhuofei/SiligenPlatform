#pragma once

#include "domain/motion/ports/IVelocityProfilePort.h"

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

   private:
    struct SegmentTimes {
        float32 t1 = 0;
        float32 t2 = 0;
        float32 t3 = 0;
        float32 t4 = 0;
        float32 t5 = 0;
        float32 t6 = 0;
        float32 t7 = 0;

        float32 Total() const noexcept {
            return t1 + t2 + t3 + t4 + t5 + t6 + t7;
        }
    };

    SegmentTimes CalculateSegmentTimes(
        float32 distance,
        float32 v_max,
        float32 a_max,
        float32 j_max) const noexcept;

    float32 CalculateVelocityAt(
        float32 t,
        const SegmentTimes& times,
        float32 v_max,
        float32 a_max,
        float32 j_max) const noexcept;

    float32 CalculateAccelerationAt(
        float32 t,
        const SegmentTimes& times,
        float32 a_max,
        float32 j_max) const noexcept;
};

}  // namespace Siligen::Domain::Motion::DomainServices
