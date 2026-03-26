#include <gtest/gtest.h>

#include "domain/safety/domain-services/SafetyOutputGuard.h"

namespace {

using Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode;
using Siligen::Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Siligen::Domain::Machine::ValueObjects::MachineMode;
using Siligen::Domain::Safety::DomainServices::SafetyOutputGuard;
using Siligen::Shared::Types::ErrorCode;

}  // namespace

TEST(SafetyOutputGuardTest, ValidationDryCycleInhibitsAllProcessOutputsButAllowsMotion) {
    auto result = SafetyOutputGuard::Evaluate(
        MachineMode::Test,
        JobExecutionMode::ValidationDryCycle,
        ProcessOutputPolicy::Inhibited);

    ASSERT_TRUE(result.IsSuccess());
    const auto decision = result.Value();
    EXPECT_TRUE(decision.allow_motion);
    EXPECT_FALSE(decision.allow_valve);
    EXPECT_FALSE(decision.allow_supply);
    EXPECT_FALSE(decision.allow_cmp);
}

TEST(SafetyOutputGuardTest, ProductionModeEnablesProcessOutputs) {
    auto result = SafetyOutputGuard::Evaluate(
        MachineMode::Production,
        JobExecutionMode::Production,
        ProcessOutputPolicy::Enabled);

    ASSERT_TRUE(result.IsSuccess());
    const auto decision = result.Value();
    EXPECT_TRUE(decision.allow_motion);
    EXPECT_TRUE(decision.allow_valve);
    EXPECT_TRUE(decision.allow_supply);
    EXPECT_TRUE(decision.allow_cmp);
}

TEST(SafetyOutputGuardTest, RejectsValidationDryCycleWithEnabledOutputs) {
    auto result = SafetyOutputGuard::Evaluate(
        MachineMode::Test,
        JobExecutionMode::ValidationDryCycle,
        ProcessOutputPolicy::Enabled);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
}
