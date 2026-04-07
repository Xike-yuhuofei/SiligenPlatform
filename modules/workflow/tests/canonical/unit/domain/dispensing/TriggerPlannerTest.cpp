#include "domain/dispensing/domain-services/TriggerPlanner.h"

#include <gtest/gtest.h>

TEST(TriggerPlannerTest, AppliesSafetyBoundaryWhenDowngradeEnabled) {
    using Siligen::Domain::Dispensing::DomainServices::TriggerPlanner;
    using Siligen::Domain::Dispensing::ValueObjects::TriggerPlan;
    using Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile;

    TriggerPlanner planner;
    TriggerPlan plan;
    plan.safety.duration_ms = 20;
    plan.safety.valve_response_ms = 10;
    plan.safety.margin_ms = 0;
    plan.safety.min_interval_ms = 0;
    plan.safety.downgrade_on_violation = true;

    DispenseCompensationProfile compensation;

    auto result = planner.Plan(100.0f,
                               10.0f,
                               100.0f,
                               0.0f,
                               1,
                               0.0f,
                               plan,
                               compensation);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_GE(result.Value().interval_ms, 30u);
    EXPECT_TRUE(result.Value().downgrade_applied);
}
