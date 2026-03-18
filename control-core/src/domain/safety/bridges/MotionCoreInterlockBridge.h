#pragma once

#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/safety/ports/IInterlockSignalPort.h"
#include "domain/safety/value-objects/InterlockTypes.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "siligen/motion/safety/services/interlock_policy.h"

namespace Siligen::Domain::Safety::Bridges {

inline Siligen::SharedKernel::Error ToNewError(const Siligen::Shared::Types::Error& error) {
    return Siligen::SharedKernel::Error(
        static_cast<Siligen::SharedKernel::ErrorCode>(static_cast<int>(error.GetCode())),
        error.GetMessage(),
        error.GetModule());
}

inline Siligen::Shared::Types::Error ToLegacyError(const Siligen::SharedKernel::Error& error) {
    return Siligen::Shared::Types::Error(
        static_cast<Siligen::Shared::Types::ErrorCode>(static_cast<int>(error.GetCode())),
        error.GetMessage(),
        error.GetModule());
}

inline Siligen::Motion::Safety::InterlockSignals ToNewSignals(
    const Siligen::Domain::Safety::ValueObjects::InterlockSignals& signals) {
    Siligen::Motion::Safety::InterlockSignals converted;
    converted.emergency_stop_triggered = signals.emergency_stop_triggered;
    converted.safety_door_open = signals.safety_door_open;
    converted.pressure_abnormal = signals.pressure_abnormal;
    converted.temperature_abnormal = signals.temperature_abnormal;
    converted.voltage_abnormal = signals.voltage_abnormal;
    converted.servo_alarm = signals.servo_alarm;
    return converted;
}

inline Siligen::Motion::Safety::InterlockPolicyConfig ToNewConfig(
    const Siligen::Domain::Safety::ValueObjects::InterlockPolicyConfig& config) {
    Siligen::Motion::Safety::InterlockPolicyConfig converted;
    converted.enabled = config.enabled;
    converted.check_emergency_stop = config.check_emergency_stop;
    converted.check_servo_alarm = config.check_servo_alarm;
    converted.check_safety_door = config.check_safety_door;
    converted.check_pressure = config.check_pressure;
    converted.check_temperature = config.check_temperature;
    converted.check_voltage = config.check_voltage;
    return converted;
}

inline Siligen::Motion::Safety::AxisSafetyStatus ToNewAxisSafetyStatus(
    const Siligen::Domain::Motion::Ports::MotionStatus& status) {
    Siligen::Motion::Safety::AxisSafetyStatus converted;
    converted.estop_active = status.state == Siligen::Domain::Motion::Ports::MotionState::ESTOP;
    converted.moving = status.state == Siligen::Domain::Motion::Ports::MotionState::MOVING;
    converted.faulted = status.state == Siligen::Domain::Motion::Ports::MotionState::FAULT;
    converted.has_error = status.has_error;
    converted.following_error = status.following_error;
    converted.soft_limit_positive = status.soft_limit_positive;
    converted.soft_limit_negative = status.soft_limit_negative;
    converted.hard_limit_positive = status.hard_limit_positive;
    converted.hard_limit_negative = status.hard_limit_negative;
    converted.servo_alarm = status.servo_alarm;
    return converted;
}

inline Siligen::Domain::Safety::ValueObjects::InterlockDecision ToLegacyDecision(
    const Siligen::Motion::Safety::InterlockDecision& decision) {
    Siligen::Domain::Safety::ValueObjects::InterlockDecision converted;
    converted.triggered = decision.triggered;
    converted.priority = static_cast<Siligen::Domain::Safety::ValueObjects::InterlockPriority>(
        static_cast<int>(decision.priority));
    converted.cause = static_cast<Siligen::Domain::Safety::ValueObjects::InterlockCause>(
        static_cast<int>(decision.cause));
    converted.reason = decision.reason;
    return converted;
}

inline Siligen::Shared::Types::Result<Siligen::Domain::Safety::ValueObjects::InterlockDecision>
ToLegacyDecisionResult(
    const Siligen::SharedKernel::Result<Siligen::Motion::Safety::InterlockDecision>& result) {
    if (result.IsError()) {
        return Siligen::Shared::Types::Result<Siligen::Domain::Safety::ValueObjects::InterlockDecision>::Failure(
            ToLegacyError(result.GetError()));
    }
    return Siligen::Shared::Types::Result<Siligen::Domain::Safety::ValueObjects::InterlockDecision>::Success(
        ToLegacyDecision(result.Value()));
}

inline Siligen::Shared::Types::Result<void> ToLegacyVoidResult(const Siligen::SharedKernel::VoidResult& result) {
    if (result.IsSuccess()) {
        return Siligen::Shared::Types::Result<void>::Success();
    }
    return Siligen::Shared::Types::Result<void>::Failure(ToLegacyError(result.GetError()));
}

class InterlockSignalPortAdapter final : public Siligen::Motion::Safety::Ports::InterlockSignalPort {
   public:
    explicit InterlockSignalPortAdapter(const Siligen::Domain::Safety::Ports::IInterlockSignalPort& legacy) noexcept
        : legacy_(legacy) {}

    Siligen::SharedKernel::Result<Siligen::Motion::Safety::InterlockSignals> ReadSignals() const noexcept override {
        const auto result = legacy_.ReadSignals();
        if (result.IsError()) {
            return Siligen::SharedKernel::Result<Siligen::Motion::Safety::InterlockSignals>::Failure(
                ToNewError(result.GetError()));
        }
        return Siligen::SharedKernel::Result<Siligen::Motion::Safety::InterlockSignals>::Success(
            ToNewSignals(result.Value()));
    }

   private:
    const Siligen::Domain::Safety::Ports::IInterlockSignalPort& legacy_;
};

inline Siligen::Shared::Types::Result<Siligen::Domain::Safety::ValueObjects::InterlockDecision>
EvaluateSignalsWithMotionCore(
    const Siligen::Domain::Safety::ValueObjects::InterlockSignals& signals,
    const Siligen::Domain::Safety::ValueObjects::InterlockPolicyConfig& config) {
    return ToLegacyDecisionResult(
        Siligen::Motion::Safety::Services::InterlockPolicy::EvaluateSignals(ToNewSignals(signals), ToNewConfig(config)));
}

inline Siligen::Shared::Types::Result<Siligen::Domain::Safety::ValueObjects::InterlockDecision>
EvaluateWithMotionCore(
    const Siligen::Domain::Safety::Ports::IInterlockSignalPort& port,
    const Siligen::Domain::Safety::ValueObjects::InterlockPolicyConfig& config) {
    InterlockSignalPortAdapter adapter(port);
    return ToLegacyDecisionResult(
        Siligen::Motion::Safety::Services::InterlockPolicy::Evaluate(adapter, ToNewConfig(config)));
}

inline Siligen::Shared::Types::Result<void> CheckAxisForJogWithMotionCore(
    const Siligen::Domain::Motion::Ports::MotionStatus& status,
    std::int16_t direction,
    const char* error_source = "InterlockPolicy") {
    return ToLegacyVoidResult(
        Siligen::Motion::Safety::Services::InterlockPolicy::CheckAxisForJog(
            ToNewAxisSafetyStatus(status),
            direction,
            error_source));
}

inline Siligen::Shared::Types::Result<void> CheckAxisForHomingWithMotionCore(
    const Siligen::Domain::Motion::Ports::MotionStatus& status,
    const char* axis_name,
    const char* error_source = "InterlockPolicy") {
    return ToLegacyVoidResult(
        Siligen::Motion::Safety::Services::InterlockPolicy::CheckAxisForHoming(
            ToNewAxisSafetyStatus(status),
            axis_name,
            error_source));
}

inline bool IsHardLimitTriggeredWithMotionCore(bool positive_limit, bool negative_limit) {
    return Siligen::Motion::Safety::Services::InterlockPolicy::IsHardLimitTriggered(
        positive_limit,
        negative_limit);
}

inline bool IsSoftLimitTriggeredWithMotionCore(
    const Siligen::Domain::Motion::Ports::MotionStatus& status,
    bool* positive_limit = nullptr,
    bool* negative_limit = nullptr) {
    return Siligen::Motion::Safety::Services::InterlockPolicy::IsSoftLimitTriggered(
        ToNewAxisSafetyStatus(status),
        positive_limit,
        negative_limit);
}

}  // namespace Siligen::Domain::Safety::Bridges
