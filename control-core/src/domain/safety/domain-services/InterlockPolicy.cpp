#include "InterlockPolicy.h"
#include "domain/safety/bridges/MotionCoreInterlockBridge.h"

namespace Siligen {
namespace Domain {
namespace Safety {
namespace DomainServices {

Result<InterlockDecision> InterlockPolicy::EvaluateSignals(const InterlockSignals& signals,
                                                           const InterlockPolicyConfig& config) noexcept {
    return Bridges::EvaluateSignalsWithMotionCore(signals, config);
}

Result<InterlockDecision> InterlockPolicy::Evaluate(const IInterlockSignalPort& port,
                                                    const InterlockPolicyConfig& config) noexcept {
    return Bridges::EvaluateWithMotionCore(port, config);
}

Result<void> InterlockPolicy::CheckAxisForJog(const MotionStatus& status,
                                              int16_t direction,
                                              const char* error_source) noexcept {
    return Bridges::CheckAxisForJogWithMotionCore(status, direction, error_source);
}

Result<void> InterlockPolicy::CheckAxisForHoming(const MotionStatus& status,
                                                 const char* axis_name,
                                                 const char* error_source) noexcept {
    return Bridges::CheckAxisForHomingWithMotionCore(status, axis_name, error_source);
}

bool InterlockPolicy::IsHardLimitTriggered(bool positive_limit, bool negative_limit) noexcept {
    return Bridges::IsHardLimitTriggeredWithMotionCore(positive_limit, negative_limit);
}

bool InterlockPolicy::IsSoftLimitTriggered(const MotionStatus& status,
                                           bool* positive_limit,
                                           bool* negative_limit) noexcept {
    return Bridges::IsSoftLimitTriggeredWithMotionCore(status, positive_limit, negative_limit);
}

}  // namespace DomainServices
}  // namespace Safety
}  // namespace Domain
}  // namespace Siligen
