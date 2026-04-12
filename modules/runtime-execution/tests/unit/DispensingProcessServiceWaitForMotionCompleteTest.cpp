#include <gtest/gtest.h>

#include <atomic>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "services/dispensing/DispensingProcessService.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "shared/types/Error.h"

namespace {

using Siligen::RuntimeExecution::Application::Services::Dispensing::DispensingProcessService;
using Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver;
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
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using int16 = std::int16_t;
using int32 = std::int32_t;
using uint32 = std::uint32_t;

template <typename Tag, typename Tag::type Member>
struct PrivateMemberAccessor {
    friend typename Tag::type GetPrivateMember(Tag) {
        return Member;
    }
};

struct WaitForMotionCompleteTag {
    using type = Result<void> (DispensingProcessService::*)(int32,
                                                            std::atomic<bool>*,
                                                            std::atomic<bool>*,
                                                            std::atomic<bool>*,
                                                            const Point2D*,
                                                            float32,
                                                            uint32,
                                                            bool,
                                                            IDispensingExecutionObserver*) noexcept;
    friend type GetPrivateMember(WaitForMotionCompleteTag);
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
    WaitForMotionCompleteTag,
    &DispensingProcessService::WaitForMotionComplete>;
template struct PrivateMemberAccessor<
    ExecutePlanInternalTag,
    &DispensingProcessService::ExecutePlanInternal>;

template <typename T>
Result<T> NotImplemented(const char* method) {
    return Result<T>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "DispensingProcessServiceWaitTest"));
}

Result<void> NotImplementedVoid(const char* method) {
    return Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "DispensingProcessServiceWaitTest"));
}

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
        return NotImplementedVoid("EnableCoordinateSystemSCurve");
    }

    Result<void> DisableCoordinateSystemSCurve(int16) override {
        return NotImplementedVoid("DisableCoordinateSystemSCurve");
    }

    Result<void> SetConstLinearVelocityMode(int16, bool, uint32) override {
        return NotImplementedVoid("SetConstLinearVelocityMode");
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
        CoordinateSystemState::MOVING,
        true,
        1U,
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
    explicit SequencedMotionStatePort(std::vector<Point2D> positions_in) : positions(std::move(positions_in)) {}

    Result<Point2D> GetCurrentPosition() const override {
        if (positions.empty()) {
            return Result<Point2D>::Success(Point2D{});
        }
        const auto index = current_index < positions.size() ? current_index : positions.size() - 1;
        const auto value = positions[index];
        if (current_index + 1 < positions.size()) {
            ++current_index;
        }
        last_position = value;
        return Result<Point2D>::Success(value);
    }

    Result<float32> GetAxisPosition(LogicalAxisId axis) const override {
        if (axis == LogicalAxisId::X) {
            return Result<float32>::Success(last_position.x);
        }
        if (axis == LogicalAxisId::Y) {
            return Result<float32>::Success(last_position.y);
        }
        return Result<float32>::Success(0.0f);
    }

    Result<float32> GetAxisVelocity(LogicalAxisId) const override {
        return Result<float32>::Success(0.0f);
    }

    Result<MotionStatus> GetAxisStatus(LogicalAxisId) const override {
        MotionStatus status;
        status.state = MotionState::IDLE;
        status.position = last_position;
        status.velocity = 0.0f;
        return Result<MotionStatus>::Success(status);
    }

    Result<bool> IsAxisMoving(LogicalAxisId) const override {
        return Result<bool>::Success(false);
    }

    Result<bool> IsAxisInPosition(LogicalAxisId) const override {
        return Result<bool>::Success(true);
    }

    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        return Result<std::vector<MotionStatus>>::Success({});
    }

    mutable std::size_t current_index = 0;
    mutable Point2D last_position{};
    std::vector<Point2D> positions{};
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

TEST(DispensingProcessServiceWaitForMotionCompleteTest, StablePositionAwayFromTargetTimesOut) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{92.305f, 0.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        180,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        false,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::MOTION_TIMEOUT);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, NearTargetLowVelocityFallsBackToSuccess) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::IDLE;
    interpolation_port->status.is_moving = false;
    interpolation_port->status.remaining_segments = 0U;
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f},
                             Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        800,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        false,
        nullptr);

    ASSERT_TRUE(result.IsSuccess());
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, FifoFinishedWithLatchedRunBitFallsBackToSuccess) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::MOVING;
    interpolation_port->status.is_moving = true;
    interpolation_port->status.remaining_segments = std::numeric_limits<uint32>::max();
    interpolation_port->status.current_velocity = 0.0f;
    interpolation_port->status.raw_status_word = 0x11;
    interpolation_port->status.raw_segment = -858993460;
    interpolation_port->status.mc_status_ret = 0;

    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{100.0f, 100.0f}, Point2D{100.0f, 100.0f},
                             Point2D{100.0f, 100.0f}, Point2D{100.0f, 100.0f}, Point2D{100.0f, 100.0f},
                             Point2D{100.0f, 100.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        800,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        false,
        nullptr);

    ASSERT_TRUE(result.IsSuccess());
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, IdleCoordinateSystemAwayFromTargetStillTimesOut) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::IDLE;
    interpolation_port->status.is_moving = false;
    interpolation_port->status.remaining_segments = 0U;

    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{92.305f, 0.0f}, Point2D{92.305f, 0.0f},
                             Point2D{92.305f, 0.0f}, Point2D{92.305f, 0.0f}, Point2D{92.305f, 0.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        180,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        false,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::MOTION_TIMEOUT);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, ExecutePlanInternalKeepsDispatchOrderWithPreviewTraceEnabled) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{10.0f, 0.0f}, Point2D{10.0f, 0.0f}, Point2D{10.0f, 0.0f}});
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

}  // namespace
