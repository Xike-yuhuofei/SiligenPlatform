#pragma once

#include "domain/motion/ports/IInterpolationPort.h"
#include "InterpolationCommandValidator.h"

#include <memory>

namespace Siligen::Domain::Motion::DomainServices {

class ValidatedInterpolationPort : public Siligen::Domain::Motion::Ports::IInterpolationPort {
public:
    explicit ValidatedInterpolationPort(std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> inner);
    ~ValidatedInterpolationPort() override = default;

    Result<void> ConfigureCoordinateSystem(int16 coord_sys,
                                           const Siligen::Domain::Motion::Ports::CoordinateSystemConfig& config) noexcept override;

    Result<void> AddInterpolationData(int16 coord_sys,
                                      const Siligen::Domain::Motion::Ports::InterpolationData& data) noexcept override;

    Result<void> ClearInterpolationBuffer(int16 coord_sys) noexcept override;

    Result<void> FlushInterpolationData(int16 coord_sys) noexcept override;

    Result<void> StartCoordinateSystemMotion(uint32 coord_sys_mask) noexcept override;

    Result<void> StopCoordinateSystemMotion(uint32 coord_sys_mask) noexcept override;

    Result<void> SetCoordinateSystemVelocityOverride(int16 coord_sys, float32 override_percent) noexcept override;

    Result<void> EnableCoordinateSystemSCurve(int16 coord_sys, float32 jerk) noexcept override;

    Result<void> DisableCoordinateSystemSCurve(int16 coord_sys) noexcept override;

    Result<void> SetConstLinearVelocityMode(int16 coord_sys, bool enabled, uint32 rotate_axis_mask) noexcept override;

    Result<uint32> GetInterpolationBufferSpace(int16 coord_sys) const noexcept override;

    Result<uint32> GetLookAheadBufferSpace(int16 coord_sys) const noexcept override;

    Result<Siligen::Domain::Motion::Ports::CoordinateSystemStatus> GetCoordinateSystemStatus(int16 coord_sys) const noexcept override;

private:
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> inner_;
    InterpolationCommandValidator validator_{};
};

}  // namespace Siligen::Domain::Motion::DomainServices
