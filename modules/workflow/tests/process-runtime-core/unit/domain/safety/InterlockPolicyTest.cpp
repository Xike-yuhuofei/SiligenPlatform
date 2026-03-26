#include "domain/safety/domain-services/InterlockPolicy.h"
#include "domain/safety/ports/IInterlockSignalPort.h"

#include <gtest/gtest.h>

namespace {
using Siligen::Domain::Safety::DomainServices::InterlockPolicy;
using Siligen::Domain::Safety::Ports::IInterlockSignalPort;
using Siligen::Domain::Safety::ValueObjects::InterlockCause;
using Siligen::Domain::Safety::ValueObjects::InterlockDecision;
using Siligen::Domain::Safety::ValueObjects::InterlockPolicyConfig;
using Siligen::Domain::Safety::ValueObjects::InterlockPriority;
using Siligen::Domain::Safety::ValueObjects::InterlockSignals;
using Siligen::Shared::Types::Result;

class FakeInterlockPort : public IInterlockSignalPort {
   public:
    explicit FakeInterlockPort(const InterlockSignals& signals) : signals_(signals) {}

    Result<InterlockSignals> ReadSignals() const noexcept override {
        return Result<InterlockSignals>::Success(signals_);
    }

   private:
    InterlockSignals signals_;
};
}  // namespace

TEST(InterlockPolicyTest, EmergencyStopHasHighestPriority) {
    InterlockSignals signals;
    signals.emergency_stop_triggered = true;
    signals.safety_door_open = true;
    signals.pressure_abnormal = true;

    InterlockPolicyConfig config;
    auto result = InterlockPolicy::EvaluateSignals(signals, config);

    ASSERT_TRUE(result.IsSuccess());
    const auto& decision = result.Value();
    EXPECT_TRUE(decision.triggered);
    EXPECT_EQ(decision.priority, InterlockPriority::CRITICAL);
    EXPECT_EQ(decision.cause, InterlockCause::EMERGENCY_STOP);
    EXPECT_STREQ(decision.reason, "急停按钮触发");
}

TEST(InterlockPolicyTest, SafetyDoorTriggersHighPriority) {
    InterlockSignals signals;
    signals.safety_door_open = true;

    InterlockPolicyConfig config;
    auto result = InterlockPolicy::EvaluateSignals(signals, config);

    ASSERT_TRUE(result.IsSuccess());
    const auto& decision = result.Value();
    EXPECT_TRUE(decision.triggered);
    EXPECT_EQ(decision.priority, InterlockPriority::HIGH);
    EXPECT_EQ(decision.cause, InterlockCause::SAFETY_DOOR_OPEN);
}

TEST(InterlockPolicyTest, PortBasedEvaluationUsesSignals) {
    InterlockSignals signals;
    signals.voltage_abnormal = true;

    FakeInterlockPort port(signals);
    InterlockPolicyConfig config;

    auto result = InterlockPolicy::Evaluate(port, config);
    ASSERT_TRUE(result.IsSuccess());
    const auto& decision = result.Value();
    EXPECT_TRUE(decision.triggered);
    EXPECT_EQ(decision.priority, InterlockPriority::LOW);
    EXPECT_EQ(decision.cause, InterlockCause::VOLTAGE_ABNORMAL);
}

TEST(InterlockPolicyTest, DisabledPolicySkipsTriggers) {
    InterlockSignals signals;
    signals.emergency_stop_triggered = true;

    InterlockPolicyConfig config;
    config.enabled = false;

    auto result = InterlockPolicy::EvaluateSignals(signals, config);
    ASSERT_TRUE(result.IsSuccess());
    const auto& decision = result.Value();
    EXPECT_FALSE(decision.triggered);
    EXPECT_EQ(decision.cause, InterlockCause::NONE);
}

TEST(InterlockPolicyTest, JogBlockedByPositiveHardLimit) {
    Siligen::Domain::Motion::Ports::MotionStatus status;
    status.hard_limit_positive = true;

    auto result = InterlockPolicy::CheckAxisForJog(status, 1, "InterlockPolicyTest");
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::POSITION_OUT_OF_RANGE);
}

TEST(InterlockPolicyTest, HomingBlockedByServoAlarm) {
    Siligen::Domain::Motion::Ports::MotionStatus status;
    status.servo_alarm = true;

    auto result = InterlockPolicy::CheckAxisForHoming(status, "X", "InterlockPolicyTest");
    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::HARDWARE_ERROR);
}
