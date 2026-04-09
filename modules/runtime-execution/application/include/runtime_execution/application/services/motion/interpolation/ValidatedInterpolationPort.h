#pragma once

#include "runtime_execution/contracts/motion/IInterpolationPort.h"

#include <memory>

namespace Siligen::Domain::Motion::DomainServices {
class InterpolationCommandValidator;
}

namespace Siligen::RuntimeExecution::Application::Services::Motion {

class ValidatedInterpolationPort final : public Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort {
   public:
    explicit ValidatedInterpolationPort(
        std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> inner);
    ~ValidatedInterpolationPort() override;

    Result<void> ConfigureCoordinateSystem(
        int16 coord_sys,
        const Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemConfig& config) noexcept override;

    Result<void> AddInterpolationData(
        int16 coord_sys,
        const Siligen::RuntimeExecution::Contracts::Motion::InterpolationData& data) noexcept override;

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
    Result<Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemStatus> GetCoordinateSystemStatus(
        int16 coord_sys) const noexcept override;

   private:
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> inner_;
    std::unique_ptr<Siligen::Domain::Motion::DomainServices::InterpolationCommandValidator> validator_;
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
