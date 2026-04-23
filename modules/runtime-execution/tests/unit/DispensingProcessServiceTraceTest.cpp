#include <gtest/gtest.h>

#include "runtime_execution/contracts/dispensing/ProfileCompareExecutionCompiler.h"
#include "services/dispensing/DispensingProcessService.h"
#include "shared/types/Error.h"

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

using Siligen::RuntimeExecution::Application::Services::Dispensing::DispensingProcessService;
using Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver;
using Siligen::Domain::Dispensing::Ports::DispenserValveParams;
using Siligen::Domain::Dispensing::Ports::DispenserValveState;
using Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using Siligen::Domain::Dispensing::Ports::IProfileComparePort;
using Siligen::Domain::Dispensing::Ports::ProfileCompareArmRequest;
using Siligen::Domain::Dispensing::Ports::ProfileCompareStatus;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareExpectedTrace;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareExpectedTraceItem;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareProgramSpan;
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareTraceabilityMismatch;
using Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode;
using Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using Siligen::Domain::Motion::Ports::CoordinateSystemState;
using Siligen::Domain::Motion::Ports::CoordinateSystemStatus;
using Siligen::Domain::Motion::Ports::IInterpolationPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::Ports::InterpolationType;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::RuntimeExecution::Contracts::Dispensing::CompileProfileCompareExecutionSchedule;
using Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionCompileInput;
using Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionSchedule;
using Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareRuntimeContract;
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
                                                                                 float32,
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

class FakeValvePort final : public Siligen::Domain::Dispensing::Ports::IValvePort,
                            public IProfileComparePort {
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
        const Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams&) noexcept override {
        ++start_position_trigger_calls;
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

    Result<void> ArmProfileCompare(const ProfileCompareArmRequest& request) noexcept override {
        ++arm_profile_compare_calls;
        last_profile_compare_request = request;
        armed_requests.push_back(request);
        profile_compare_status.expected_trigger_count = request.expected_trigger_count;
        if (request.compare_positions_pulse.empty()) {
            profile_compare_status.armed = false;
            profile_compare_status.completed_trigger_count = request.expected_trigger_count;
            profile_compare_status.remaining_trigger_count = 0U;
        } else if (complete_profile_compare_immediately) {
            profile_compare_status.armed = false;
            profile_compare_status.completed_trigger_count = request.expected_trigger_count;
            profile_compare_status.remaining_trigger_count = 0U;
        } else {
            profile_compare_status.armed = true;
            profile_compare_status.completed_trigger_count = request.start_boundary_trigger_count;
            profile_compare_status.remaining_trigger_count =
                request.expected_trigger_count - request.start_boundary_trigger_count;
        }
        ApplyForcedProfileCompareStatus();
        profile_compare_status.error_message.clear();
        return Result<void>::Success();
    }

    Result<void> DisarmProfileCompare() noexcept override {
        ++disarm_profile_compare_calls;
        profile_compare_status.armed = false;
        profile_compare_status.remaining_trigger_count = 0U;
        return Result<void>::Success();
    }

    Result<ProfileCompareStatus> GetProfileCompareStatus() noexcept override {
        if (complete_profile_compare_immediately && profile_compare_status.armed) {
            profile_compare_status.armed = false;
            profile_compare_status.completed_trigger_count = profile_compare_status.expected_trigger_count;
            profile_compare_status.remaining_trigger_count = 0U;
        }
        ApplyForcedProfileCompareStatus();
        return Result<ProfileCompareStatus>::Success(profile_compare_status);
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
    int arm_profile_compare_calls = 0;
    int disarm_profile_compare_calls = 0;
    DispenserValveParams last_timed_params{};
    ProfileCompareArmRequest last_profile_compare_request{};
    std::vector<ProfileCompareArmRequest> armed_requests{};
    ProfileCompareStatus profile_compare_status{};
    bool complete_profile_compare_immediately = true;
    std::optional<uint32> forced_completed_trigger_count;
    std::optional<uint32> forced_remaining_trigger_count;
    std::optional<bool> forced_armed;
    DispenserValveState status{};

   private:
    void ApplyForcedProfileCompareStatus() noexcept {
        if (forced_completed_trigger_count.has_value()) {
            profile_compare_status.completed_trigger_count = forced_completed_trigger_count.value();
            if (!forced_remaining_trigger_count.has_value()) {
                profile_compare_status.remaining_trigger_count =
                    profile_compare_status.completed_trigger_count >= profile_compare_status.expected_trigger_count
                    ? 0U
                    : (profile_compare_status.expected_trigger_count - profile_compare_status.completed_trigger_count);
            }
        }
        if (forced_remaining_trigger_count.has_value()) {
            profile_compare_status.remaining_trigger_count = forced_remaining_trigger_count.value();
        }
        if (forced_armed.has_value()) {
            profile_compare_status.armed = forced_armed.value();
        }
    }
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
        all_added_segments.push_back(data);
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
        ++status_reads;
        if (!status_sequence.empty()) {
            const auto index =
                status_sequence_cursor < status_sequence.size() ? status_sequence_cursor : status_sequence.size() - 1U;
            const auto value = status_sequence[index];
            if (status_sequence_cursor + 1U < status_sequence.size()) {
                ++status_sequence_cursor;
            }
            return Result<CoordinateSystemStatus>::Success(value);
        }
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
    mutable int status_reads = 0;
    mutable std::size_t status_sequence_cursor = 0U;
    std::vector<CoordinateSystemStatus> status_sequence{};
    std::vector<InterpolationData> added_segments{};
    std::vector<InterpolationData> all_added_segments{};
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

class SpanAwareMotionStatePort final : public IMotionStatePort {
   public:
    SpanAwareMotionStatePort(std::shared_ptr<FakeInterpolationPort> interpolation_port,
                             std::vector<Point2D> stage_positions)
        : interpolation_port_(std::move(interpolation_port)),
          stage_positions_(std::move(stage_positions)) {}

    Result<Point2D> GetCurrentPosition() const override {
        if (stage_positions_.empty()) {
            return Result<Point2D>::Success(Point2D{});
        }

        std::size_t stage_index = 0U;
        if (interpolation_port_) {
            stage_index = static_cast<std::size_t>(std::max(0, interpolation_port_->start_calls));
        }
        stage_index = std::min(stage_index, stage_positions_.size() - 1U);
        return Result<Point2D>::Success(stage_positions_[stage_index]);
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
    std::shared_ptr<FakeInterpolationPort> interpolation_port_;
    std::vector<Point2D> stage_positions_{};
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

ProfileCompareProgramSpan MakeProgramSpan(
    uint32 trigger_begin_index,
    uint32 trigger_end_index,
    float32 start_profile_position_mm,
    float32 end_profile_position_mm,
    const Point2D& start_position_mm,
    const Point2D& end_position_mm,
    short compare_source_axis,
    uint32 start_boundary_trigger_count) {
    ProfileCompareProgramSpan span;
    span.trigger_begin_index = trigger_begin_index;
    span.trigger_end_index = trigger_end_index;
    span.compare_source_axis = compare_source_axis;
    span.start_boundary_trigger_count = start_boundary_trigger_count;
    span.start_profile_position_mm = start_profile_position_mm;
    span.end_profile_position_mm = end_profile_position_mm;
    span.start_position_mm = start_position_mm;
    span.end_position_mm = end_position_mm;
    return span;
}

DispensingExecutionPlan BuildProfileCompareExecutionPlan() {
    auto plan = BuildLinearExecutionPlan();
    plan.production_trigger_mode = ProductionTriggerMode::PROFILE_COMPARE;
    plan.profile_compare_program.expected_trigger_count = 2U;
    plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{0.0f, 0.0f}, 2000U},
        {1U, 10.0f, Point2D{10.0f, 0.0f}, 2000U},
    };
    plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 1U, 0.0f, 10.0f, Point2D{0.0f, 0.0f}, Point2D{10.0f, 0.0f}, 1, 1U),
    };
    plan.completion_contract.enabled = true;
    plan.completion_contract.final_target_position_mm = Point2D{10.0f, 0.0f};
    plan.completion_contract.final_position_tolerance_mm = 0.2f;
    plan.completion_contract.expected_trigger_count = 2U;
    return plan;
}

DispensingExecutionPlan BuildDiagonalProfileCompareExecutionPlan() {
    auto plan = BuildLinearExecutionPlan();

    plan.interpolation_segments[0].positions = {5.0f, 5.0f};
    plan.interpolation_segments[1].positions = {10.0f, 10.0f};
    plan.interpolation_points[1].position = Point2D{2.5f, 2.5f};
    plan.interpolation_points[2].position = Point2D{5.0f, 5.0f};
    plan.interpolation_points[3].position = Point2D{7.5f, 7.5f};
    plan.interpolation_points[4].position = Point2D{10.0f, 10.0f};
    plan.total_length_mm = 14.142136f;

    plan.production_trigger_mode = ProductionTriggerMode::PROFILE_COMPARE;
    plan.profile_compare_program.expected_trigger_count = 2U;
    plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{0.0f, 0.0f}, 2000U},
        {1U, 14.142136f, Point2D{10.0f, 10.0f}, 2000U},
    };
    plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 1U, 0.0f, 14.142136f, Point2D{0.0f, 0.0f}, Point2D{10.0f, 10.0f}, 1, 1U),
    };
    plan.completion_contract.enabled = true;
    plan.completion_contract.final_target_position_mm = Point2D{10.0f, 10.0f};
    plan.completion_contract.final_position_tolerance_mm = 0.2f;
    plan.completion_contract.expected_trigger_count = 2U;
    return plan;
}

DispensingExecutionPlan BuildPureYProfileCompareExecutionPlan() {
    auto plan = BuildLinearExecutionPlan();

    plan.interpolation_segments[0].positions = {0.0f, 5.0f};
    plan.interpolation_segments[1].positions = {0.0f, 10.0f};
    plan.interpolation_points[1].position = Point2D{0.0f, 2.5f};
    plan.interpolation_points[2].position = Point2D{0.0f, 5.0f};
    plan.interpolation_points[3].position = Point2D{0.0f, 7.5f};
    plan.interpolation_points[4].position = Point2D{0.0f, 10.0f};
    plan.total_length_mm = 10.0f;

    plan.production_trigger_mode = ProductionTriggerMode::PROFILE_COMPARE;
    plan.profile_compare_program.expected_trigger_count = 2U;
    plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{0.0f, 0.0f}, 2000U},
        {1U, 10.0f, Point2D{0.0f, 10.0f}, 2000U},
    };
    plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 1U, 0.0f, 10.0f, Point2D{0.0f, 0.0f}, Point2D{0.0f, 10.0f}, 2, 1U),
    };
    plan.completion_contract.enabled = true;
    plan.completion_contract.final_target_position_mm = Point2D{0.0f, 10.0f};
    plan.completion_contract.final_position_tolerance_mm = 0.2f;
    plan.completion_contract.expected_trigger_count = 2U;
    return plan;
}

DispensingExecutionPlan BuildOrthogonalTurnProfileCompareExecutionPlan() {
    DispensingExecutionPlan plan;

    InterpolationData first_segment;
    first_segment.type = InterpolationType::LINEAR;
    first_segment.positions = {10.0f, 0.0f};
    first_segment.velocity = 20.0f;
    first_segment.acceleration = 100.0f;
    first_segment.end_velocity = 20.0f;

    InterpolationData second_segment;
    second_segment.type = InterpolationType::LINEAR;
    second_segment.positions = {10.0f, 10.0f};
    second_segment.velocity = 20.0f;
    second_segment.acceleration = 100.0f;
    second_segment.end_velocity = 0.0f;

    plan.interpolation_segments = {first_segment, second_segment};

    const std::vector<Point2D> preview_positions{
        Point2D{0.0f, 0.0f},
        Point2D{10.0f, 0.0f},
        Point2D{10.0f, 10.0f}};
    plan.interpolation_points.reserve(preview_positions.size());
    for (std::size_t index = 0; index < preview_positions.size(); ++index) {
        Siligen::TrajectoryPoint point;
        point.position = preview_positions[index];
        point.sequence_id = static_cast<uint32>(index);
        point.timestamp = static_cast<float32>(index) * 0.5f;
        point.velocity = 20.0f;
        plan.interpolation_points.push_back(point);
    }

    plan.total_length_mm = 20.0f;
    plan.production_trigger_mode = ProductionTriggerMode::PROFILE_COMPARE;
    plan.profile_compare_program.expected_trigger_count = 3U;
    plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{0.0f, 0.0f}, 2000U},
        {1U, 10.0f, Point2D{10.0f, 0.0f}, 2000U},
        {2U, 20.0f, Point2D{10.0f, 10.0f}, 2000U},
    };
    plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 1U, 0.0f, 10.0f, Point2D{0.0f, 0.0f}, Point2D{10.0f, 0.0f}, 1, 1U),
        MakeProgramSpan(2U, 2U, 10.0f, 20.0f, Point2D{10.0f, 0.0f}, Point2D{10.0f, 10.0f}, 2, 0U),
    };
    plan.completion_contract.enabled = true;
    plan.completion_contract.final_target_position_mm = Point2D{10.0f, 10.0f};
    plan.completion_contract.final_position_tolerance_mm = 0.2f;
    plan.completion_contract.expected_trigger_count = 3U;
    return plan;
}

DispensingExecutionPlan BuildLeadInGapProfileCompareExecutionPlan() {
    DispensingExecutionPlan plan;

    InterpolationData first_segment;
    first_segment.type = InterpolationType::LINEAR;
    first_segment.positions = {10.0f, 0.0f};
    first_segment.velocity = 20.0f;
    first_segment.acceleration = 100.0f;
    first_segment.end_velocity = 20.0f;

    InterpolationData second_segment;
    second_segment.type = InterpolationType::LINEAR;
    second_segment.positions = {20.0f, 0.0f};
    second_segment.velocity = 5.0f;
    second_segment.acceleration = 100.0f;
    second_segment.end_velocity = 5.0f;

    InterpolationData third_segment;
    third_segment.type = InterpolationType::LINEAR;
    third_segment.positions = {22.0f, 0.0f};
    third_segment.velocity = 5.0f;
    third_segment.acceleration = 100.0f;
    third_segment.end_velocity = 0.0f;

    plan.interpolation_segments = {first_segment, second_segment, third_segment};

    const std::vector<Point2D> preview_positions{
        Point2D{0.0f, 0.0f},
        Point2D{10.0f, 0.0f},
        Point2D{20.0f, 0.0f},
        Point2D{22.0f, 0.0f}};
    const std::vector<float32> preview_timestamps{0.0f, 0.5f, 1.5f, 1.7f};
    const std::vector<float32> preview_velocities{20.0f, 20.0f, 5.0f, 5.0f};
    plan.interpolation_points.reserve(preview_positions.size());
    for (std::size_t index = 0; index < preview_positions.size(); ++index) {
        Siligen::TrajectoryPoint point;
        point.position = preview_positions[index];
        point.sequence_id = static_cast<uint32>(index);
        point.timestamp = preview_timestamps[index];
        point.velocity = preview_velocities[index];
        plan.interpolation_points.push_back(point);
    }

    plan.total_length_mm = 22.0f;
    plan.production_trigger_mode = ProductionTriggerMode::PROFILE_COMPARE;
    plan.profile_compare_program.expected_trigger_count = 3U;
    plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{0.0f, 0.0f}, 2000U},
        {1U, 10.0f, Point2D{10.0f, 0.0f}, 2000U},
        {2U, 22.0f, Point2D{22.0f, 0.0f}, 2000U},
    };
    plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 1U, 0.0f, 10.0f, Point2D{0.0f, 0.0f}, Point2D{10.0f, 0.0f}, 1, 1U),
        MakeProgramSpan(2U, 2U, 20.0f, 22.0f, Point2D{20.0f, 0.0f}, Point2D{22.0f, 0.0f}, 1, 0U),
    };
    plan.completion_contract.enabled = true;
    plan.completion_contract.final_target_position_mm = Point2D{22.0f, 0.0f};
    plan.completion_contract.final_position_tolerance_mm = 0.2f;
    plan.completion_contract.expected_trigger_count = 3U;
    return plan;
}

DispensingExecutionPlan BuildClosedLoopBranchRevisitProfileCompareExecutionPlan() {
    DispensingExecutionPlan plan;

    InterpolationData segment1;
    segment1.type = InterpolationType::LINEAR;
    segment1.positions = {10.0f, 0.0f};
    segment1.velocity = 20.0f;
    segment1.acceleration = 100.0f;
    segment1.end_velocity = 20.0f;

    InterpolationData segment2;
    segment2.type = InterpolationType::LINEAR;
    segment2.positions = {10.0f, 10.0f};
    segment2.velocity = 20.0f;
    segment2.acceleration = 100.0f;
    segment2.end_velocity = 20.0f;

    InterpolationData segment3;
    segment3.type = InterpolationType::LINEAR;
    segment3.positions = {0.0f, 10.0f};
    segment3.velocity = 20.0f;
    segment3.acceleration = 100.0f;
    segment3.end_velocity = 20.0f;

    InterpolationData segment4;
    segment4.type = InterpolationType::LINEAR;
    segment4.positions = {0.0f, 0.0f};
    segment4.velocity = 20.0f;
    segment4.acceleration = 100.0f;
    segment4.end_velocity = 20.0f;

    InterpolationData segment5;
    segment5.type = InterpolationType::LINEAR;
    segment5.positions = {10.0f, 10.0f};
    segment5.velocity = 20.0f;
    segment5.acceleration = 100.0f;
    segment5.end_velocity = 0.0f;

    plan.interpolation_segments = {segment1, segment2, segment3, segment4, segment5};

    const std::vector<Point2D> preview_positions{
        Point2D{0.0f, 0.0f},
        Point2D{10.0f, 0.0f},
        Point2D{10.0f, 10.0f},
        Point2D{0.0f, 10.0f},
        Point2D{0.0f, 0.0f},
        Point2D{10.0f, 10.0f}};
    plan.interpolation_points.reserve(preview_positions.size());
    for (std::size_t index = 0; index < preview_positions.size(); ++index) {
        Siligen::TrajectoryPoint point;
        point.position = preview_positions[index];
        point.sequence_id = static_cast<uint32>(index);
        point.timestamp = static_cast<float32>(index) * 0.1f;
        point.velocity = 20.0f;
        plan.interpolation_points.push_back(point);
    }

    plan.total_length_mm = 54.142136f;
    plan.production_trigger_mode = ProductionTriggerMode::PROFILE_COMPARE;
    plan.profile_compare_program.expected_trigger_count = static_cast<uint32>(preview_positions.size());
    plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{0.0f, 0.0f}, 2000U},
        {1U, 10.0f, Point2D{10.0f, 0.0f}, 2000U},
        {2U, 20.0f, Point2D{10.0f, 10.0f}, 2000U},
        {3U, 30.0f, Point2D{0.0f, 10.0f}, 2000U},
        {4U, 40.0f, Point2D{0.0f, 0.0f}, 2000U},
        {5U, 54.142136f, Point2D{10.0f, 10.0f}, 2000U},
    };
    plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 5U, 0.0f, 54.142136f, Point2D{0.0f, 0.0f}, Point2D{10.0f, 10.0f}, 1, 1U),
    };
    plan.completion_contract.enabled = true;
    plan.completion_contract.final_target_position_mm = Point2D{10.0f, 10.0f};
    plan.completion_contract.final_position_tolerance_mm = 0.2f;
    plan.completion_contract.expected_trigger_count = plan.profile_compare_program.expected_trigger_count;
    return plan;
}

DispensingExecutionPlan BuildInterpolationAnchoredProfileCompareExecutionPlan() {
    DispensingExecutionPlan plan;

    InterpolationData segment1;
    segment1.type = InterpolationType::LINEAR;
    segment1.positions = {10.0f, 0.0f};
    segment1.velocity = 20.0f;
    segment1.acceleration = 100.0f;
    segment1.end_velocity = 0.0f;
    plan.interpolation_segments = {segment1};

    const std::vector<Point2D> preview_positions{
        Point2D{10.0f, 0.0f},
        Point2D{15.0f, 0.0f},
        Point2D{20.0f, 0.0f}};
    plan.interpolation_points.reserve(preview_positions.size());
    for (std::size_t index = 0; index < preview_positions.size(); ++index) {
        Siligen::TrajectoryPoint point;
        point.position = preview_positions[index];
        point.sequence_id = static_cast<uint32>(index);
        point.timestamp = static_cast<float32>(index) * 0.1f;
        point.velocity = 20.0f;
        plan.interpolation_points.push_back(point);
    }

    Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint motion_start;
    motion_start.t = 0.0f;
    motion_start.position = {0.0f, 0.0f, 0.0f};
    motion_start.velocity = {20.0f, 0.0f, 0.0f};
    plan.motion_trajectory.points.push_back(motion_start);

    Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint motion_end;
    motion_end.t = 0.2f;
    motion_end.position = {20.0f, 0.0f, 0.0f};
    motion_end.velocity = {0.0f, 0.0f, 0.0f};
    plan.motion_trajectory.points.push_back(motion_end);
    plan.motion_trajectory.total_length = 20.0f;
    plan.total_length_mm = 10.0f;

    plan.production_trigger_mode = ProductionTriggerMode::PROFILE_COMPARE;
    plan.profile_compare_program.expected_trigger_count = 3U;
    plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{10.0f, 0.0f}, 2000U},
        {1U, 5.0f, Point2D{15.0f, 0.0f}, 2000U},
        {2U, 10.0f, Point2D{20.0f, 0.0f}, 2000U},
    };
    plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 2U, 0.0f, 10.0f, Point2D{10.0f, 0.0f}, Point2D{20.0f, 0.0f}, 1, 1U),
    };
    plan.completion_contract.enabled = true;
    plan.completion_contract.final_target_position_mm = Point2D{20.0f, 0.0f};
    plan.completion_contract.final_position_tolerance_mm = 0.2f;
    plan.completion_contract.expected_trigger_count = 3U;
    return plan;
}

DispensingExecutionPlan BuildClosedLoopDescendingQuadrantProfileCompareExecutionPlan() {
    DispensingExecutionPlan plan;

    const std::vector<Point2D> preview_positions{
        Point2D{0.0f, 0.0f},
        Point2D{5.0f, 5.0f},
        Point2D{10.0f, 0.0f},
        Point2D{5.0f, -5.0f},
        Point2D{0.0f, 0.0f}};
    plan.interpolation_points.reserve(preview_positions.size());

    float32 cumulative_length_mm = 0.0f;
    for (std::size_t index = 0; index < preview_positions.size(); ++index) {
        if (index > 0U) {
            cumulative_length_mm +=
                static_cast<float32>(preview_positions[index - 1U].DistanceTo(preview_positions[index]));

            InterpolationData segment;
            segment.type = InterpolationType::LINEAR;
            segment.positions = {preview_positions[index].x, preview_positions[index].y};
            segment.velocity = 20.0f;
            segment.acceleration = 100.0f;
            segment.end_velocity = index + 1U == preview_positions.size() ? 0.0f : 20.0f;
            plan.interpolation_segments.push_back(segment);
        }

        Siligen::TrajectoryPoint point;
        point.position = preview_positions[index];
        point.sequence_id = static_cast<uint32>(index);
        point.timestamp = static_cast<float32>(index) * 0.1f;
        point.velocity = 20.0f;
        plan.interpolation_points.push_back(point);
    }

    plan.total_length_mm = cumulative_length_mm;
    plan.production_trigger_mode = ProductionTriggerMode::PROFILE_COMPARE;
    plan.profile_compare_program.expected_trigger_count = static_cast<uint32>(preview_positions.size());
    plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{0.0f, 0.0f}, 2000U},
        {1U, 7.0710678f, Point2D{5.0f, 5.0f}, 2000U},
        {2U, 14.1421356f, Point2D{10.0f, 0.0f}, 2000U},
        {3U, 21.2132034f, Point2D{5.0f, -5.0f}, 2000U},
        {4U, 28.2842712f, Point2D{0.0f, 0.0f}, 2000U},
    };
    plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 4U, 0.0f, 28.2842712f, Point2D{0.0f, 0.0f}, Point2D{0.0f, 0.0f}, 1, 1U),
    };
    plan.completion_contract.enabled = true;
    plan.completion_contract.final_target_position_mm = Point2D{0.0f, 0.0f};
    plan.completion_contract.final_position_tolerance_mm = 0.2f;
    plan.completion_contract.expected_trigger_count = plan.profile_compare_program.expected_trigger_count;
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

std::shared_ptr<ProfileCompareExpectedTrace> BuildExpectedTraceFromPlanAndSchedule(
    const DispensingExecutionPlan& plan,
    const ProfileCompareExecutionSchedule& schedule) {
    auto trace = std::make_shared<ProfileCompareExpectedTrace>();
    trace->items.reserve(plan.profile_compare_program.trigger_points.size());

    for (const auto& schedule_span : schedule.spans) {
        const auto future_compare_offset = schedule_span.trigger_begin_index + schedule_span.start_boundary_trigger_count;
        for (uint32 trigger_index = schedule_span.trigger_begin_index;
             trigger_index <= schedule_span.trigger_end_index;
             ++trigger_index) {
            const auto& owner_trigger = plan.profile_compare_program.trigger_points[trigger_index];
            ProfileCompareExpectedTraceItem item;
            item.cycle_index = 1U;
            item.trigger_sequence_id = owner_trigger.sequence_index;
            item.authority_trigger_index = owner_trigger.sequence_index;
            item.span_index = schedule_span.span_index;
            item.execution_interpolation_index = plan.interpolation_points.empty()
                ? 0U
                : std::min<uint32>(
                      trigger_index,
                      static_cast<uint32>(plan.interpolation_points.size() - 1U));
            item.authority_distance_mm = owner_trigger.profile_position_mm;
            item.execution_profile_position_mm = owner_trigger.profile_position_mm;
            item.execution_position_mm = owner_trigger.trigger_position_mm;
            item.execution_trigger_position_mm = owner_trigger.trigger_position_mm;
            item.compare_source_axis = schedule_span.compare_source_axis;
            item.pulse_width_us = owner_trigger.pulse_width_us;
            item.authority_trigger_ref = "trigger-" + std::to_string(owner_trigger.sequence_index);
            item.authority_span_ref = "span-" + std::to_string(schedule_span.span_index);
            if (trigger_index == schedule_span.trigger_begin_index &&
                schedule_span.start_boundary_trigger_count == 1U) {
                item.trigger_mode = "start_boundary";
                item.compare_position_pulse = 0L;
            } else {
                const auto future_compare_index =
                    static_cast<std::size_t>(trigger_index - future_compare_offset);
                item.trigger_mode = "future_compare";
                item.compare_position_pulse = schedule_span.compare_positions_pulse[future_compare_index];
            }
            trace->items.push_back(std::move(item));
        }
    }

    return trace;
}

Result<ProfileCompareExecutionSchedule> CompileProfileCompareScheduleForTest(
    const DispensingExecutionPlan& plan,
    const DispensingRuntimeParams& params,
    int compare_axis_mask = 0x03,
    uint32 max_future_compare_count = 1000U) {
    ProfileCompareExecutionCompileInput input{
        plan,
        ProfileCompareRuntimeContract{
            params.pulse_per_mm,
            compare_axis_mask,
            max_future_compare_count,
        },
    };
    return CompileProfileCompareExecutionSchedule(input);
}

std::shared_ptr<const ProfileCompareExpectedTrace> BuildExpectedTraceForTest(
    const DispensingExecutionPlan& plan,
    const ProfileCompareExecutionSchedule& schedule) {
    auto trace = std::make_shared<ProfileCompareExpectedTrace>();
    trace->items.reserve(plan.profile_compare_program.trigger_points.size());

    for (const auto& span : schedule.spans) {
        const auto future_compare_offset = span.trigger_begin_index + span.start_boundary_trigger_count;
        for (uint32 trigger_index = span.trigger_begin_index; trigger_index <= span.trigger_end_index; ++trigger_index) {
            const auto& trigger = plan.profile_compare_program.trigger_points[trigger_index];

            ProfileCompareExpectedTraceItem item;
            item.cycle_index = 1U;
            item.trigger_sequence_id = trigger.sequence_index;
            item.span_index = span.span_index;
            item.compare_source_axis = span.compare_source_axis;
            item.authority_trigger_ref = "trigger-" + std::to_string(trigger_index);
            if (trigger_index == span.trigger_begin_index && span.start_boundary_trigger_count == 1U) {
                item.trigger_mode = "start_boundary";
                item.compare_position_pulse = 0L;
            } else {
                const auto future_compare_index =
                    static_cast<std::size_t>(trigger_index - future_compare_offset);
                item.trigger_mode = "future_compare";
                item.compare_position_pulse = span.compare_positions_pulse[future_compare_index];
            }
            trace->items.push_back(std::move(item));
        }
    }

    return trace;
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
        0.0f,
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
        0.0f,
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

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalUsesProfileCompareSingleTrackForDispensingPath) {
    auto valve_port = std::make_shared<FakeValvePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
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

    auto plan = BuildProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    auto schedule_result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(schedule_result.IsSuccess()) << schedule_result.GetError().GetMessage();
    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.profile_compare_schedule = std::make_shared<ProfileCompareExecutionSchedule>(schedule_result.Value());
    options.expected_trace = BuildExpectedTraceForTest(plan, schedule_result.Value());
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().executed_segments, 2U);
    EXPECT_EQ(valve_port->open_supply_calls, 1);
    EXPECT_EQ(valve_port->arm_profile_compare_calls, 1);
    EXPECT_EQ(valve_port->disarm_profile_compare_calls, 1);
    EXPECT_EQ(valve_port->start_position_trigger_calls, 0);
    EXPECT_EQ(valve_port->last_profile_compare_request.expected_trigger_count, 2U);
    EXPECT_EQ(valve_port->last_profile_compare_request.start_boundary_trigger_count, 1U);
    ASSERT_EQ(valve_port->last_profile_compare_request.compare_positions_pulse.size(), 1U);
    EXPECT_EQ(valve_port->last_profile_compare_request.compare_positions_pulse[0], 2000);
    EXPECT_EQ(valve_port->last_profile_compare_request.compare_source_axis, 1);
    ASSERT_EQ(result.Value().actual_trace.size(), 2U);
    EXPECT_EQ(result.Value().traceability_verdict, "passed");
    EXPECT_TRUE(result.Value().traceability_verdict_reason.empty());
    EXPECT_TRUE(result.Value().strict_one_to_one_proven);
    EXPECT_TRUE(result.Value().traceability_mismatches.empty());
    EXPECT_EQ(result.Value().actual_trace.front().trigger_mode, "start_boundary");
    EXPECT_EQ(result.Value().actual_trace.front().compare_position_pulse, 0L);
    EXPECT_EQ(result.Value().actual_trace.back().trigger_mode, "future_compare");
    EXPECT_EQ(result.Value().actual_trace.back().compare_position_pulse, 2000L);
}

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalMapsDiagonalProfileCompareToAxisPositionInsteadOfPathLength) {
    auto valve_port = std::make_shared<FakeValvePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 10.0f},
                             Point2D{10.0f, 10.0f},
                             Point2D{10.0f, 10.0f},
                             Point2D{10.0f, 10.0f},
                             Point2D{10.0f, 10.0f}});
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildDiagonalProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    auto schedule_result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(schedule_result.IsSuccess()) << schedule_result.GetError().GetMessage();
    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.profile_compare_schedule = std::make_shared<ProfileCompareExecutionSchedule>(schedule_result.Value());
    options.expected_trace = BuildExpectedTraceForTest(plan, schedule_result.Value());
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(valve_port->arm_profile_compare_calls, 1);
    EXPECT_EQ(valve_port->last_profile_compare_request.compare_source_axis, 1);
    ASSERT_EQ(valve_port->last_profile_compare_request.compare_positions_pulse.size(), 1U);
    EXPECT_EQ(valve_port->last_profile_compare_request.compare_positions_pulse[0], 2000);
}

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalRejectsProfileCompareWithoutExpectedTrace) {
    auto valve_port = std::make_shared<FakeValvePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{10.0f, 0.0f}});
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    auto schedule_result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(schedule_result.IsSuccess()) << schedule_result.GetError().GetMessage();

    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.profile_compare_schedule = std::make_shared<ProfileCompareExecutionSchedule>(schedule_result.Value());
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
    EXPECT_NE(result.GetError().GetMessage().find("expected trace"), std::string::npos);
}

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalSelectsYAxisForPureYProfileComparePath) {
    auto valve_port = std::make_shared<FakeValvePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
                             Point2D{0.0f, 10.0f},
                             Point2D{0.0f, 10.0f},
                             Point2D{0.0f, 10.0f},
                             Point2D{0.0f, 10.0f},
                             Point2D{0.0f, 10.0f}});
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildPureYProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    auto schedule_result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(schedule_result.IsSuccess()) << schedule_result.GetError().GetMessage();
    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.profile_compare_schedule = std::make_shared<ProfileCompareExecutionSchedule>(schedule_result.Value());
    options.expected_trace = BuildExpectedTraceForTest(plan, schedule_result.Value());
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(valve_port->arm_profile_compare_calls, 1);
    EXPECT_EQ(valve_port->last_profile_compare_request.compare_source_axis, 2);
    ASSERT_EQ(valve_port->last_profile_compare_request.compare_positions_pulse.size(), 1U);
    EXPECT_EQ(valve_port->last_profile_compare_request.compare_positions_pulse[0], 2000);
}

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalUsesMultipleProfileCompareSpansForAxisSwitchPath) {
    auto valve_port = std::make_shared<FakeValvePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SpanAwareMotionStatePort>(
        interpolation_port,
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 10.0f}});
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildOrthogonalTurnProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    auto schedule_result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(schedule_result.IsSuccess()) << schedule_result.GetError().GetMessage();
    const auto& schedule = schedule_result.Value();
    ASSERT_EQ(schedule.spans.size(), 2U);

    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.profile_compare_schedule = std::make_shared<ProfileCompareExecutionSchedule>(schedule);
    options.expected_trace = BuildExpectedTraceForTest(plan, schedule);
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().executed_segments, 2U);
    EXPECT_EQ(valve_port->arm_profile_compare_calls, static_cast<int>(schedule.spans.size()));
    EXPECT_EQ(valve_port->disarm_profile_compare_calls, static_cast<int>(schedule.spans.size()));
    ASSERT_EQ(valve_port->armed_requests.size(), schedule.spans.size());
    EXPECT_EQ(valve_port->armed_requests[0].compare_source_axis, 1);
    EXPECT_EQ(valve_port->armed_requests[0].expected_trigger_count, 2U);
    EXPECT_EQ(valve_port->armed_requests[0].start_boundary_trigger_count, 1U);
    EXPECT_EQ(valve_port->armed_requests[1].compare_source_axis, 2);
    EXPECT_EQ(valve_port->armed_requests[1].expected_trigger_count, 1U);
    EXPECT_EQ(valve_port->armed_requests[1].start_boundary_trigger_count, 0U);
    EXPECT_EQ(interpolation_port->clear_calls, static_cast<int>(schedule.spans.size()));
    EXPECT_EQ(interpolation_port->start_calls, static_cast<int>(schedule.spans.size()));
    EXPECT_EQ(interpolation_port->add_calls, static_cast<int>(plan.interpolation_segments.size()));
    EXPECT_EQ(interpolation_port->all_added_segments.size(), plan.interpolation_segments.size());
}

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalProducesStrictTraceabilityForOrthogonalTurnProfileComparePath) {
    auto valve_port = std::make_shared<FakeValvePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SpanAwareMotionStatePort>(
        interpolation_port,
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
                             Point2D{10.0f, 0.0f},
                             Point2D{10.0f, 10.0f}});
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildOrthogonalTurnProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    auto schedule_result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(schedule_result.IsSuccess()) << schedule_result.GetError().GetMessage();
    const auto& schedule = schedule_result.Value();

    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.profile_compare_schedule = std::make_shared<ProfileCompareExecutionSchedule>(schedule);
    options.expected_trace = BuildExpectedTraceFromPlanAndSchedule(plan, schedule);
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    ASSERT_EQ(result.Value().actual_trace.size(), 3U);
    EXPECT_EQ(result.Value().traceability_mismatches.size(), 0U);
    EXPECT_EQ(result.Value().traceability_verdict, "passed");
    EXPECT_TRUE(result.Value().traceability_verdict_reason.empty());
    EXPECT_TRUE(result.Value().strict_one_to_one_proven);

    const auto& actual_trace = result.Value().actual_trace;
    EXPECT_EQ(actual_trace[0].trigger_sequence_id, 0U);
    EXPECT_EQ(actual_trace[0].span_index, schedule.spans[0].span_index);
    EXPECT_EQ(actual_trace[0].completion_sequence, 1U);
    EXPECT_EQ(actual_trace[0].local_completed_trigger_count, 1U);
    EXPECT_EQ(actual_trace[0].trigger_mode, "start_boundary");

    EXPECT_EQ(actual_trace[1].trigger_sequence_id, 1U);
    EXPECT_EQ(actual_trace[1].span_index, schedule.spans[0].span_index);
    EXPECT_EQ(actual_trace[1].completion_sequence, 2U);
    EXPECT_EQ(actual_trace[1].local_completed_trigger_count, 2U);
    EXPECT_EQ(actual_trace[1].trigger_mode, "future_compare");

    EXPECT_EQ(actual_trace[2].trigger_sequence_id, 2U);
    EXPECT_EQ(actual_trace[2].span_index, schedule.spans[1].span_index);
    EXPECT_EQ(actual_trace[2].completion_sequence, 3U);
    EXPECT_EQ(actual_trace[2].local_completed_trigger_count, 1U);
    EXPECT_EQ(actual_trace[2].trigger_mode, "future_compare");
}

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalFailsClosedWhenSpanCompletedTriggerCountOverflows) {
    auto valve_port = std::make_shared<FakeValvePort>();
    valve_port->forced_completed_trigger_count = 3U;
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
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

    auto plan = BuildProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    auto schedule_result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(schedule_result.IsSuccess()) << schedule_result.GetError().GetMessage();
    const auto& schedule = schedule_result.Value();

    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.profile_compare_schedule = std::make_shared<ProfileCompareExecutionSchedule>(schedule);
    options.expected_trace = BuildExpectedTraceFromPlanAndSchedule(plan, schedule);
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::CMP_TRIGGER_SETUP_FAILED);
    EXPECT_NE(
        result.GetError().GetMessage().find("completed_trigger_count exceeded span expected_trigger_count"),
        std::string::npos);
}

TEST(DispensingProcessServiceTraceTest, ExecutePlanInternalFailsClosedWhenExpectedTraceDriftsOutsideCurrentSpan) {
    auto valve_port = std::make_shared<FakeValvePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f},
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

    auto plan = BuildProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    auto schedule_result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(schedule_result.IsSuccess()) << schedule_result.GetError().GetMessage();
    const auto& schedule = schedule_result.Value();

    auto expected_trace = BuildExpectedTraceFromPlanAndSchedule(plan, schedule);
    ASSERT_EQ(expected_trace->items.size(), 2U);
    expected_trace->items[1].span_index = schedule.spans[0].span_index + 1U;

    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.profile_compare_schedule = std::make_shared<ProfileCompareExecutionSchedule>(schedule);
    options.expected_trace = expected_trace;
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::CMP_TRIGGER_SETUP_FAILED);
    EXPECT_NE(
        result.GetError().GetMessage().find("completed trigger advanced outside current span"),
        std::string::npos);
}

TEST(DispensingProcessServiceTraceTest,
     ExecutePlanInternalUsesControllerSegmentDurationForSlowSecondProfileCompareSpan) {
    auto valve_port = std::make_shared<FakeValvePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();

    const CoordinateSystemStatus settled_status{
        CoordinateSystemState::IDLE,
        false,
        0U,
        0.0f,
        0x10,
        1,
        0};
    const CoordinateSystemStatus moving_status{
        CoordinateSystemState::MOVING,
        true,
        1U,
        1.0f,
        0x01,
        1,
        1};
    auto append_status = [&](const CoordinateSystemStatus& status, int count) {
        for (int index = 0; index < count; ++index) {
            interpolation_port->status_sequence.push_back(status);
        }
    };

    append_status(settled_status, 6);
    append_status(moving_status, 4);
    append_status(settled_status, 6);
    append_status(settled_status, 6);
    append_status(moving_status, 115);
    append_status(settled_status, 6);

    std::vector<Point2D> positions;
    positions.reserve(11U + 122U);
    positions.push_back(Point2D{0.0f, 0.0f});
    for (int index = 1; index <= 4; ++index) {
        positions.push_back(Point2D{10.0f * static_cast<float32>(index) / 4.0f, 0.0f});
    }
    for (int index = 0; index < 6; ++index) {
        positions.push_back(Point2D{10.0f, 0.0f});
    }
    positions.push_back(Point2D{10.0f, 0.0f});
    for (int index = 1; index <= 115; ++index) {
        positions.push_back(Point2D{10.0f, 10.0f * static_cast<float32>(index) / 115.0f});
    }
    for (int index = 0; index < 6; ++index) {
        positions.push_back(Point2D{10.0f, 10.0f});
    }

    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(std::move(positions));
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildOrthogonalTurnProfileCompareExecutionPlan();
    plan.interpolation_points[1].velocity = 20.0f;
    plan.interpolation_points[2].velocity = 1.0f;
    auto params = BuildRuntimeParams();
    auto schedule_result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(schedule_result.IsSuccess()) << schedule_result.GetError().GetMessage();
    const auto& schedule = schedule_result.Value();
    ASSERT_EQ(schedule.spans.size(), 2U);

    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.profile_compare_schedule = std::make_shared<ProfileCompareExecutionSchedule>(schedule);
    options.expected_trace = BuildExpectedTraceForTest(plan, schedule);
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().executed_segments, 2U);
    EXPECT_EQ(valve_port->arm_profile_compare_calls, static_cast<int>(schedule.spans.size()));
    EXPECT_EQ(valve_port->disarm_profile_compare_calls, static_cast<int>(schedule.spans.size()));
    EXPECT_EQ(interpolation_port->clear_calls, static_cast<int>(schedule.spans.size()));
    EXPECT_EQ(interpolation_port->start_calls, static_cast<int>(schedule.spans.size()));
}

TEST(DispensingProcessServiceTraceTest,
     ExecutePlanInternalUsesPreviousSpanEndForProfileCompareLeadInDurationEstimate) {
    auto valve_port = std::make_shared<FakeValvePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();

    const CoordinateSystemStatus settled_status{
        CoordinateSystemState::IDLE,
        false,
        0U,
        0.0f,
        0x10,
        1,
        0};
    const CoordinateSystemStatus moving_status{
        CoordinateSystemState::MOVING,
        true,
        1U,
        1.0f,
        0x01,
        1,
        1};
    auto append_status = [&](const CoordinateSystemStatus& status, int count) {
        for (int index = 0; index < count; ++index) {
            interpolation_port->status_sequence.push_back(status);
        }
    };

    append_status(settled_status, 6);
    append_status(moving_status, 4);
    append_status(settled_status, 6);
    constexpr int kSecondSpanMovingPolls = 100;
    constexpr int kSecondSpanRampPolls = 20;
    constexpr int kSecondSpanSettledPolls = 10;

    append_status(settled_status, kSecondSpanSettledPolls);
    append_status(moving_status, kSecondSpanMovingPolls);
    append_status(settled_status, 6);

    std::vector<Point2D> positions;
    positions.reserve(19U + static_cast<std::size_t>(kSecondSpanMovingPolls + kSecondSpanSettledPolls));
    positions.push_back(Point2D{0.0f, 0.0f});
    positions.push_back(Point2D{0.0f, 0.0f});
    for (int index = 1; index <= 4; ++index) {
        positions.push_back(Point2D{10.0f * static_cast<float32>(index) / 4.0f, 0.0f});
    }
    for (int index = 0; index < 6; ++index) {
        positions.push_back(Point2D{10.0f, 0.0f});
    }
    positions.push_back(Point2D{10.0f, 0.0f});
    for (int index = 1; index <= kSecondSpanRampPolls; ++index) {
        positions.push_back(Point2D{
            10.0f + 12.0f * static_cast<float32>(index) / static_cast<float32>(kSecondSpanRampPolls),
            0.0f});
    }
    for (int index = kSecondSpanRampPolls; index < kSecondSpanMovingPolls; ++index) {
        positions.push_back(Point2D{22.0f, 0.0f});
    }
    for (int index = 0; index < kSecondSpanSettledPolls; ++index) {
        positions.push_back(Point2D{22.0f, 0.0f});
    }

    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(std::move(positions));
    DispensingProcessService service(valve_port,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    auto plan = BuildLeadInGapProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    auto schedule_result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(schedule_result.IsSuccess()) << schedule_result.GetError().GetMessage();
    const auto& schedule = schedule_result.Value();
    ASSERT_EQ(schedule.spans.size(), 2U);
    EXPECT_FLOAT_EQ(schedule.spans[0].end_position_mm.x, 10.0f);
    EXPECT_EQ(schedule.spans[1].start_boundary_trigger_count, 0U);
    EXPECT_FLOAT_EQ(schedule.spans[1].start_position_mm.x, 20.0f);
    EXPECT_FLOAT_EQ(schedule.spans[1].end_position_mm.x, 22.0f);

    auto options = BuildExecutionOptions();
    options.dispense_enabled = true;
    options.use_hardware_trigger = true;
    options.profile_compare_schedule = std::make_shared<ProfileCompareExecutionSchedule>(schedule);
    options.expected_trace = BuildExpectedTraceForTest(plan, schedule);
    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    const auto execute_plan_internal = GetPrivateMember(ExecutePlanInternalTag{});

    const auto result = (service.*execute_plan_internal)(
        plan,
        0.0f,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(interpolation_port->start_calls, 2);
    EXPECT_EQ(interpolation_port->clear_calls, 2);
    EXPECT_EQ(valve_port->arm_profile_compare_calls, 2);
    EXPECT_EQ(valve_port->disarm_profile_compare_calls, 2);
    ASSERT_EQ(interpolation_port->all_added_segments.size(), 2U);
    ASSERT_EQ(interpolation_port->all_added_segments[1].positions.size(), 2U);
    EXPECT_FLOAT_EQ(interpolation_port->all_added_segments[1].positions[0], 22.0f);
    EXPECT_FLOAT_EQ(interpolation_port->all_added_segments[1].positions[1], 0.0f);
    EXPECT_EQ(result.Value().executed_segments, static_cast<uint32>(plan.interpolation_segments.size()));
}

TEST(DispensingProcessServiceTraceTest, CompileProfileCompareExecutionScheduleRejectsMultipleTriggersCollapsingToStartBoundary) {
    auto plan = BuildProfileCompareExecutionPlan();
    plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{0.0f, 0.0f}, 2000U},
        {1U, 0.001f, Point2D{0.001f, 0.0f}, 2000U},
    };
    auto params = BuildRuntimeParams();
    const auto result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::CMP_TRIGGER_SETUP_FAILED);
    EXPECT_NE(result.GetError().GetMessage().find("boundary"), std::string::npos);
}

TEST(DispensingProcessServiceTraceTest, CompileProfileCompareExecutionScheduleRejectsDuplicateFutureCompareAfterPulseNormalization) {
    auto plan = BuildProfileCompareExecutionPlan();
    plan.profile_compare_program.trigger_points = {
        {0U, 5.001f, Point2D{5.001f, 0.0f}, 2000U},
        {1U, 5.002f, Point2D{5.002f, 0.0f}, 2000U},
    };
    auto params = BuildRuntimeParams();
    const auto result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::CMP_TRIGGER_SETUP_FAILED);
    EXPECT_NE(result.GetError().GetMessage().find("严格递增"), std::string::npos);
}

TEST(DispensingProcessServiceTraceTest, CompileProfileCompareExecutionScheduleRejectsMissingOwnerSpanContract) {
    auto plan = BuildProfileCompareExecutionPlan();
    plan.profile_compare_program.spans.clear();
    auto params = BuildRuntimeParams();
    const auto result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
    EXPECT_NE(result.GetError().GetMessage().find("owner formal span contract"), std::string::npos);
}

TEST(DispensingProcessServiceTraceTest, CompileProfileCompareExecutionScheduleRejectsClosedLoopBranchRevisitProfileComparePath) {
    auto plan = BuildClosedLoopBranchRevisitProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    const auto result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::CMP_TRIGGER_SETUP_FAILED);
    EXPECT_NE(result.GetError().GetMessage().find("无法编译正式 execution schedule"), std::string::npos);
    EXPECT_NE(result.GetError().GetMessage().find("trigger_begin_index="), std::string::npos);
}

TEST(DispensingProcessServiceTraceTest, CompileProfileCompareExecutionScheduleUsesInterpolationStartAsFormalOrigin) {
    auto plan = BuildInterpolationAnchoredProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    const auto result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    ASSERT_EQ(result.Value().spans.size(), 1U);
    const auto& span = result.Value().spans.front();
    EXPECT_EQ(span.compare_source_axis, 1);
    EXPECT_EQ(span.start_boundary_trigger_count, 1U);
    EXPECT_EQ(span.expected_trigger_count, 3U);
    EXPECT_EQ(span.start_position_mm.x, 10.0f);
    EXPECT_EQ(span.start_position_mm.y, 0.0f);
    ASSERT_EQ(span.compare_positions_pulse.size(), 2U);
    EXPECT_EQ(span.compare_positions_pulse[0], 1000);
    EXPECT_EQ(span.compare_positions_pulse[1], 2000);
}

TEST(DispensingProcessServiceTraceTest, CompileProfileCompareExecutionScheduleRejectsClosedLoopDescendingQuadrantWithFormalCompareContractHint) {
    auto plan = BuildClosedLoopDescendingQuadrantProfileCompareExecutionPlan();
    auto params = BuildRuntimeParams();
    const auto result = CompileProfileCompareScheduleForTest(plan, params);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::CMP_TRIGGER_SETUP_FAILED);
    EXPECT_NE(result.GetError().GetMessage().find("无法编译正式 execution schedule"), std::string::npos);
    EXPECT_NE(result.GetError().GetMessage().find("trigger_begin_index="), std::string::npos);
    EXPECT_NE(result.GetError().GetMessage().find("递增 compare 位置"), std::string::npos);
}

}  // namespace
