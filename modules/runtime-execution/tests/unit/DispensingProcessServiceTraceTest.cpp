#include <gtest/gtest.h>

#include "services/dispensing/DispensingProcessService.h"
#include "shared/types/Error.h"

#include <atomic>
#include <memory>
#include <vector>

namespace {

using Siligen::RuntimeExecution::Application::Services::Dispensing::DispensingProcessService;
using Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver;
using Siligen::Domain::Dispensing::Ports::DispenserValveParams;
using Siligen::Domain::Dispensing::Ports::DispenserValveState;
using Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams;
using Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using Siligen::Domain::Motion::Ports::CoordinateSystemState;
using Siligen::Domain::Motion::Ports::CoordinateSystemStatus;
using Siligen::Domain::Motion::Ports::IInterpolationPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::Ports::InterpolationType;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::DispensingExecutionGeometryKind;
using Siligen::Shared::Types::DispensingExecutionStrategy;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;

template <typename Tag, typename Tag::type M>
struct PrivateMemberAccessor {
    friend typename Tag::type GetPrivateMember(Tag) {
        return M;
    }
};

struct ExecutePlanInternalTag {
    using type = Result<DispensingExecutionReport> (DispensingProcessService::*)(const DispensingExecutionPlan&,
                                                                                 const DispensingRuntimeParams&,
                                                                                 const DispensingExecutionOptions&,
                                                                                 std::atomic<bool>*,
                                                                                 std::atomic<bool>*,
                                                                                 std::atomic<bool>*,
                                                                                 IDispensingExecutionObserver*)
        noexcept;
    friend type GetPrivateMember(ExecutePlanInternalTag);
};

template struct PrivateMemberAccessor<
    ExecutePlanInternalTag,
    &DispensingProcessService::ExecutePlanInternal>;

template <typename T>
Result<T> NotImplemented(const char* method) {
    return Result<T>::Failure(Error(ErrorCode::PORT_NOT_INITIALIZED, method));
}

Result<void> NotImplementedVoid(const char* method) {
    return Result<void>::Failure(Error(ErrorCode::PORT_NOT_INITIALIZED, method));
}

class FakeValvePort final : public Siligen::Domain::Dispensing::Ports::IValvePort {
   public:
    Result<DispenserValveState> StartDispenser(const DispenserValveParams& params) noexcept override {
        ++start_timed_calls;
        last_timed_params = params;
        status.status = DispenserValveStatus::Idle;
        status.totalCount = params.count;
        status.completedCount = params.count;
        status.remainingCount = 0U;
        return Result<DispenserValveState>::Success(status);
    }

    Result<DispenserValveState> OpenDispenser() noexcept override {
        return Result<DispenserValveState>::Failure(Error(ErrorCode::PORT_NOT_INITIALIZED, "OpenDispenser"));
    }

    Result<void> CloseDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<DispenserValveState> StartPositionTriggeredDispenser(
        const PositionTriggeredDispenserParams& params) noexcept override {
        ++start_position_trigger_calls;
        last_position_trigger_params = params;
        if (use_position_trigger_status_override) {
            status = position_trigger_status_override;
            if (status.remainingCount == 0U && status.totalCount >= status.completedCount) {
                status.remainingCount = status.totalCount - status.completedCount;
            }
        } else {
            status.status = DispenserValveStatus::Idle;
            status.totalCount = static_cast<uint32>(params.trigger_events.size());
            status.completedCount = status.totalCount;
            status.remainingCount = 0U;
            status.errorMessage.clear();
        }
        return Result<DispenserValveState>::Success(status);
    }

    Result<void> StopDispenser() noexcept override {
        ++stop_calls;
        return Result<void>::Success();
    }

    Result<void> PauseDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<void> ResumeDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<DispenserValveState> GetDispenserStatus() noexcept override {
        return Result<DispenserValveState>::Success(status);
    }

    Result<Siligen::Domain::Dispensing::Ports::SupplyValveState> OpenSupply() noexcept override {
        ++open_supply_calls;
        return Result<Siligen::Domain::Dispensing::Ports::SupplyValveState>::Success(
            Siligen::Domain::Dispensing::Ports::SupplyValveState::Open);
    }

    Result<Siligen::Domain::Dispensing::Ports::SupplyValveState> CloseSupply() noexcept override {
        ++close_supply_calls;
        return Result<Siligen::Domain::Dispensing::Ports::SupplyValveState>::Success(
            Siligen::Domain::Dispensing::Ports::SupplyValveState::Closed);
    }

    Result<Siligen::Domain::Dispensing::Ports::SupplyValveStatusDetail> GetSupplyStatus() noexcept override {
        return Result<Siligen::Domain::Dispensing::Ports::SupplyValveStatusDetail>::Success({});
    }

    int start_timed_calls = 0;
    int start_position_trigger_calls = 0;
    int stop_calls = 0;
    int open_supply_calls = 0;
    int close_supply_calls = 0;
    DispenserValveParams last_timed_params{};
    PositionTriggeredDispenserParams last_position_trigger_params{};
    DispenserValveState status{};
    bool use_position_trigger_status_override = false;
    DispenserValveState position_trigger_status_override{};
};

class FakeInterpolationPort final : public IInterpolationPort {
   public:
    Result<void> ConfigureCoordinateSystem(int16, const CoordinateSystemConfig&) override {
        ++configure_calls;
        return Result<void>::Success();
    }

    Result<void> AddInterpolationData(int16, const InterpolationData& data) override {
        ++add_calls;
        added_segments.push_back(data);
        return Result<void>::Success();
    }

    Result<void> ClearInterpolationBuffer(int16) override {
        ++clear_calls;
        added_segments.clear();
        return Result<void>::Success();
    }

    Result<void> FlushInterpolationData(int16) override {
        ++flush_calls;
        return Result<void>::Success();
    }

    Result<void> StartCoordinateSystemMotion(uint32) override {
        ++start_calls;
        status.state = CoordinateSystemState::IDLE;
        status.is_moving = false;
        status.remaining_segments = 0U;
        status.raw_segment = 0;
        return Result<void>::Success();
    }

    Result<void> StopCoordinateSystemMotion(uint32) override {
        ++stop_calls;
        return Result<void>::Success();
    }

    Result<void> SetCoordinateSystemVelocityOverride(int16, float32) override {
        return Result<void>::Success();
    }

    Result<void> EnableCoordinateSystemSCurve(int16, float32) override {
        return Result<void>::Success();
    }

    Result<void> DisableCoordinateSystemSCurve(int16) override {
        return Result<void>::Success();
    }

    Result<void> SetConstLinearVelocityMode(int16, bool, uint32) override {
        return Result<void>::Success();
    }

    Result<uint32> GetInterpolationBufferSpace(int16) const override {
        return Result<uint32>::Success(buffer_space);
    }

    Result<uint32> GetLookAheadBufferSpace(int16) const override {
        return Result<uint32>::Success(lookahead_space);
    }

    Result<CoordinateSystemStatus> GetCoordinateSystemStatus(int16) const override {
        return Result<CoordinateSystemStatus>::Success(status);
    }

    CoordinateSystemStatus status{
        CoordinateSystemState::IDLE,
        false,
        0,
        0.0f,
        0,
        1,
        0};
    uint32 buffer_space = 64U;
    uint32 lookahead_space = 0U;
    int configure_calls = 0;
    int add_calls = 0;
    int clear_calls = 0;
    int flush_calls = 0;
    int start_calls = 0;
    int stop_calls = 0;
    std::vector<InterpolationData> added_segments{};
};

class SequencedMotionStatePort final : public IMotionStatePort {
   public:
    explicit SequencedMotionStatePort(std::vector<Point2D> positions)
        : positions_(std::move(positions)) {}

    Result<Point2D> GetCurrentPosition() const override {
        if (positions_.empty()) {
            return Result<Point2D>::Success(Point2D{});
        }
        const auto index = std::min(cursor_, positions_.size() - 1U);
        const auto position = positions_[index];
        if (cursor_ + 1U < positions_.size()) {
            ++cursor_;
        }
        return Result<Point2D>::Success(position);
    }

    Result<float32> GetAxisPosition(LogicalAxisId axis) const override {
        auto position_result = GetCurrentPosition();
        if (position_result.IsError()) {
            return Result<float32>::Failure(position_result.GetError());
        }
        const auto& position = position_result.Value();
        return Result<float32>::Success(axis == LogicalAxisId::X ? position.x : position.y);
    }

    Result<float32> GetAxisVelocity(LogicalAxisId) const override {
        return Result<float32>::Success(0.0f);
    }

    Result<MotionStatus> GetAxisStatus(LogicalAxisId axis) const override {
        auto position_result = GetAxisPosition(axis);
        if (position_result.IsError()) {
            return Result<MotionStatus>::Failure(position_result.GetError());
        }

        MotionStatus status;
        status.state = MotionState::IDLE;
        status.in_position = true;
        status.enabled = true;
        status.axis_position_mm = position_result.Value();
        status.profile_position_mm = position_result.Value();
        status.encoder_position_mm = position_result.Value();
        status.selected_feedback_source = "profile";
        return Result<MotionStatus>::Success(status);
    }

    Result<bool> IsAxisMoving(LogicalAxisId) const override {
        return Result<bool>::Success(false);
    }

    Result<bool> IsAxisInPosition(LogicalAxisId) const override {
        return Result<bool>::Success(true);
    }

    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        return Result<std::vector<MotionStatus>>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "GetAllAxesStatus"));
    }

   private:
    mutable std::size_t cursor_ = 0U;
    std::vector<Point2D> positions_{};
};

DispensingExecutionPlan BuildLinearExecutionPlan() {
    DispensingExecutionPlan plan;

    InterpolationData first_segment;
    first_segment.type = InterpolationType::LINEAR;
    first_segment.positions = {5.0f, 0.0f};
    first_segment.velocity = 20.0f;
    first_segment.acceleration = 100.0f;
    first_segment.end_velocity = 20.0f;

    InterpolationData second_segment;
    second_segment.type = InterpolationType::LINEAR;
    second_segment.positions = {10.0f, 0.0f};
    second_segment.velocity = 20.0f;
    second_segment.acceleration = 100.0f;
    second_segment.end_velocity = 0.0f;

    plan.interpolation_segments = {first_segment, second_segment};

    const std::vector<Point2D> preview_positions{
        Point2D{0.0f, 0.0f},
        Point2D{2.5f, 0.0f},
        Point2D{5.0f, 0.0f},
        Point2D{7.5f, 0.0f},
        Point2D{10.0f, 0.0f}};
    plan.interpolation_points.reserve(preview_positions.size());
    for (std::size_t index = 0; index < preview_positions.size(); ++index) {
        Siligen::TrajectoryPoint point;
        point.position = preview_positions[index];
        point.sequence_id = static_cast<uint32>(index);
        point.timestamp = static_cast<float32>(index) * 0.1f;
        point.velocity = 20.0f;
        plan.interpolation_points.push_back(point);
    }

    plan.total_length_mm = 10.0f;
    return plan;
}

DispensingExecutionPlan BuildStationaryPointExecutionPlan() {
    DispensingExecutionPlan plan;
    plan.geometry_kind = DispensingExecutionGeometryKind::POINT;
    plan.execution_strategy = DispensingExecutionStrategy::STATIONARY_SHOT;

    Siligen::TrajectoryPoint point;
    point.position = Point2D{5.0f, 5.0f};
    point.sequence_id = 0U;
    point.enable_position_trigger = true;
    point.trigger_position_mm = 0.0f;
    plan.interpolation_points.push_back(point);

    Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint motion_point;
    motion_point.t = 0.0f;
    motion_point.position = {5.0f, 5.0f, 0.0f};
    motion_point.velocity = {0.0f, 0.0f, 0.0f};
    plan.motion_trajectory.points.push_back(motion_point);
    plan.total_length_mm = 0.0f;
    return plan;
}

DispensingRuntimeParams BuildRuntimeParams() {
    DispensingRuntimeParams params;
    params.dispensing_velocity = 20.0f;
    params.acceleration = 100.0f;
    params.pulse_per_mm = 200.0f;
    params.dispenser_interval_ms = 100U;
    params.dispenser_duration_ms = 50U;
    params.trigger_spatial_interval_mm = 2.0f;
    return params;
}

DispensingExecutionOptions BuildExecutionOptions() {
    DispensingExecutionOptions options;
    options.dispense_enabled = false;
    options.use_hardware_trigger = false;
    options.guard_decision.allow_motion = true;
    options.guard_decision.allow_valve = false;
    options.guard_decision.allow_supply = false;
    options.guard_decision.allow_cmp = false;
    return options;
}

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalKeepsDispatchOrderWithPreviewTraceEnabled) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildLinearExecutionPlan();
    auto params = BuildRuntimeParams();
    auto options = BuildExecutionOptions();
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().executed_segments, 2U);
    EXPECT_FLOAT_EQ(result.Value().total_distance, 10.0f);
    EXPECT_EQ(interpolation_port->clear_calls, 1);
    EXPECT_EQ(interpolation_port->add_calls, 2);
    EXPECT_EQ(interpolation_port->flush_calls, 1);
    EXPECT_EQ(interpolation_port->start_calls, 1);
    ASSERT_EQ(interpolation_port->added_segments.size(), 2U);
    ASSERT_GE(interpolation_port->added_segments[0].positions.size(), 2U);
    ASSERT_GE(interpolation_port->added_segments[1].positions.size(), 2U);
    EXPECT_FLOAT_EQ(interpolation_port->added_segments[0].positions[0], 5.0f);
    EXPECT_FLOAT_EQ(interpolation_port->added_segments[1].positions[0], 10.0f);
}

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalRunsStationaryPointShotWithoutCmpPath) {
    auto valve_port = std::make_shared<FakeValvePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{5.0f, 5.0f}, Point2D{5.0f, 5.0f}});
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildStationaryPointExecutionPlan();
    auto params = BuildRuntimeParams();
    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().executed_segments, 1U);
    EXPECT_EQ(valve_port->start_timed_calls, 1);
    EXPECT_EQ(valve_port->start_position_trigger_calls, 0);
    EXPECT_EQ(valve_port->open_supply_calls, 1);
    EXPECT_EQ(interpolation_port->add_calls, 1);
    EXPECT_EQ(interpolation_port->start_calls, 1);
}

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalFailsWhenPathTriggerCountsAreIncomplete) {
    auto valve_port = std::make_shared<FakeValvePort>();
    valve_port->use_position_trigger_status_override = true;
    valve_port->position_trigger_status_override.status = DispenserValveStatus::Idle;
    valve_port->position_trigger_status_override.completedCount = 3U;
    valve_port->position_trigger_status_override.totalCount = 5U;
    valve_port->position_trigger_status_override.remainingCount = 2U;

    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f}});
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildLinearExecutionPlan();
    auto params = BuildRuntimeParams();
    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.guard_decision.allow_valve = true;
    options.guard_decision.allow_supply = true;
    options.guard_decision.allow_cmp = true;
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::DISPENSER_TRIGGER_INCOMPLETE);
    EXPECT_EQ(valve_port->start_position_trigger_calls, 1);
    EXPECT_EQ(valve_port->stop_calls, 1);
    EXPECT_NE(result.GetError().GetMessage().find("failure_stage=path_trigger_reconcile"), std::string::npos);
    EXPECT_NE(result.GetError().GetMessage().find("completed_count=3"), std::string::npos);
    EXPECT_NE(result.GetError().GetMessage().find("total_count=5"), std::string::npos);
    EXPECT_NE(result.GetError().GetMessage().find("expected_trigger_count="), std::string::npos);
}

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalFailsWhenPathTriggerStatusHasAdapterError) {
    auto valve_port = std::make_shared<FakeValvePort>();
    valve_port->use_position_trigger_status_override = true;
    valve_port->position_trigger_status_override.status = DispenserValveStatus::Error;
    valve_port->position_trigger_status_override.completedCount = 5U;
    valve_port->position_trigger_status_override.totalCount = 5U;
    valve_port->position_trigger_status_override.errorMessage = "adapter-path-trigger-error";

    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f}});
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildLinearExecutionPlan();
    auto params = BuildRuntimeParams();
    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.guard_decision.allow_valve = true;
    options.guard_decision.allow_supply = true;
    options.guard_decision.allow_cmp = true;
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::DISPENSER_TRIGGER_INCOMPLETE);
    EXPECT_NE(result.GetError().GetMessage().find("adapter_error_message=adapter-path-trigger-error"), std::string::npos);
}

}  // namespace
