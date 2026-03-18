#include "ValidatedInterpolationPort.h"

#include <utility>

namespace Siligen::Domain::Motion::DomainServices {

ValidatedInterpolationPort::ValidatedInterpolationPort(
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> inner)
    : inner_(std::move(inner)) {
}

Result<void> ValidatedInterpolationPort::ConfigureCoordinateSystem(
    int16 coord_sys,
    const Siligen::Domain::Motion::Ports::CoordinateSystemConfig& config) noexcept {
    return inner_->ConfigureCoordinateSystem(coord_sys, config);
}

Result<void> ValidatedInterpolationPort::AddInterpolationData(
    int16 coord_sys,
    const Siligen::Domain::Motion::Ports::InterpolationData& data) noexcept {
    auto validation = validator_.ValidateInterpolationData(data);
    if (validation.IsError()) {
        return validation;
    }
    return inner_->AddInterpolationData(coord_sys, data);
}

Result<void> ValidatedInterpolationPort::ClearInterpolationBuffer(int16 coord_sys) noexcept {
    return inner_->ClearInterpolationBuffer(coord_sys);
}

Result<void> ValidatedInterpolationPort::FlushInterpolationData(int16 coord_sys) noexcept {
    return inner_->FlushInterpolationData(coord_sys);
}

Result<void> ValidatedInterpolationPort::StartCoordinateSystemMotion(uint32 coord_sys_mask) noexcept {
    auto validation = validator_.ValidateCoordinateSystemMask(coord_sys_mask);
    if (validation.IsError()) {
        return validation;
    }
    return inner_->StartCoordinateSystemMotion(coord_sys_mask);
}

Result<void> ValidatedInterpolationPort::StopCoordinateSystemMotion(uint32 coord_sys_mask) noexcept {
    auto validation = validator_.ValidateCoordinateSystemMask(coord_sys_mask);
    if (validation.IsError()) {
        return validation;
    }
    return inner_->StopCoordinateSystemMotion(coord_sys_mask);
}

Result<void> ValidatedInterpolationPort::SetCoordinateSystemVelocityOverride(
    int16 coord_sys,
    float32 override_percent) noexcept {
    auto validation = validator_.ValidateVelocityOverride(override_percent);
    if (validation.IsError()) {
        return validation;
    }
    return inner_->SetCoordinateSystemVelocityOverride(coord_sys, override_percent);
}

Result<void> ValidatedInterpolationPort::EnableCoordinateSystemSCurve(int16 coord_sys, float32 jerk) noexcept {
    auto validation = validator_.ValidateSCurveJerk(jerk);
    if (validation.IsError()) {
        return validation;
    }
    return inner_->EnableCoordinateSystemSCurve(coord_sys, jerk);
}

Result<void> ValidatedInterpolationPort::DisableCoordinateSystemSCurve(int16 coord_sys) noexcept {
    return inner_->DisableCoordinateSystemSCurve(coord_sys);
}

Result<void> ValidatedInterpolationPort::SetConstLinearVelocityMode(
    int16 coord_sys,
    bool enabled,
    uint32 rotate_axis_mask) noexcept {
    return inner_->SetConstLinearVelocityMode(coord_sys, enabled, rotate_axis_mask);
}

Result<uint32> ValidatedInterpolationPort::GetInterpolationBufferSpace(int16 coord_sys) const noexcept {
    return inner_->GetInterpolationBufferSpace(coord_sys);
}

Result<uint32> ValidatedInterpolationPort::GetLookAheadBufferSpace(int16 coord_sys) const noexcept {
    return inner_->GetLookAheadBufferSpace(coord_sys);
}

Result<Siligen::Domain::Motion::Ports::CoordinateSystemStatus>
ValidatedInterpolationPort::GetCoordinateSystemStatus(int16 coord_sys) const noexcept {
    return inner_->GetCoordinateSystemStatus(coord_sys);
}

}  // namespace Siligen::Domain::Motion::DomainServices
