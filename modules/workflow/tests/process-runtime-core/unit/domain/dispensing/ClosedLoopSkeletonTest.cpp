#include "domain/dispensing/compensation/TriggerStrategy.h"
#include "domain/dispensing/model/ModelService.h"
#include "domain/dispensing/simulation/SimulationRecordStore.h"

#include <gtest/gtest.h>

TEST(ClosedLoopSkeletonTest, RecordsFeedModelAndStrategy) {
    using Siligen::Domain::Dispensing::Compensation::FixedTriggerStrategy;
    using Siligen::Domain::Dispensing::Compensation::TriggerCompensation;
    using Siligen::Domain::Dispensing::Compensation::TriggerContext;
    using Siligen::Domain::Dispensing::Model::SimpleModelService;
    using Siligen::Domain::Dispensing::Simulation::InMemorySimulationRecordStore;
    using Siligen::Domain::Dispensing::Simulation::SimulationRecord;

    InMemorySimulationRecordStore store;

    SimulationRecord record_a;
    record_a.sequence_id = 1;
    record_a.timestamp_ms = 100;
    record_a.target_volume_ul = 10.0f;
    record_a.measured_volume_ul = 9.5f;
    record_a.position_mm = 1.0f;

    SimulationRecord record_b;
    record_b.sequence_id = 2;
    record_b.timestamp_ms = 120;
    record_b.target_volume_ul = 11.0f;
    record_b.measured_volume_ul = 10.0f;
    record_b.position_mm = 2.0f;

    ASSERT_TRUE(store.Append(record_a).IsSuccess());
    ASSERT_TRUE(store.Append(record_b).IsSuccess());

    auto records_result = store.List();
    ASSERT_TRUE(records_result.IsSuccess());
    ASSERT_EQ(records_result.Value().size(), 2u);
    EXPECT_EQ(records_result.Value()[0].sequence_id, 1u);
    EXPECT_EQ(records_result.Value()[1].sequence_id, 2u);

    SimpleModelService model;
    ASSERT_TRUE(model.UpdateFromRecords(records_result.Value()).IsSuccess());

    auto snapshot_result = model.GetSnapshot();
    ASSERT_TRUE(snapshot_result.IsSuccess());
    EXPECT_TRUE(snapshot_result.Value().valid);
    EXPECT_FLOAT_EQ(snapshot_result.Value().bias, -0.75f);
    EXPECT_FLOAT_EQ(snapshot_result.Value().last_error, -1.0f);

    TriggerCompensation compensation;
    compensation.enabled = true;
    compensation.interval_scale = 0.9f;
    compensation.pulse_width_scale = 1.1f;
    compensation.open_comp_delta_ms = 0.2f;
    compensation.close_comp_delta_ms = -0.1f;

    FixedTriggerStrategy strategy(compensation);

    TriggerContext context;
    context.nominal_interval_mm = 0.5f;
    context.nominal_pulse_width_ms = 2.0f;

    auto decision = strategy.Evaluate(snapshot_result.Value(), context);
    ASSERT_TRUE(decision.IsSuccess());
    EXPECT_TRUE(decision.Value().enabled);
    EXPECT_FLOAT_EQ(decision.Value().interval_scale, 0.9f);
    EXPECT_FLOAT_EQ(decision.Value().pulse_width_scale, 1.1f);
}
