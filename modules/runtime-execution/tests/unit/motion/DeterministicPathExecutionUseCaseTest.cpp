#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.h"
#include "domain/motion/ports/IInterpolationPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "shared/types/Error.h"

namespace {

using Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathExecutionRequest;
using Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathExecutionState;
using Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathExecutionUseCase;
using Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathSegment;
using Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathSegmentType;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemConfig;
using Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemStatus;
using Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;
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

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T>
Result<T> NotImplemented(const char* method) {
    return Result<T>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "DeterministicPathExecutionUseCaseTest"));
}

Result<void> NotImplementedVoid(const char* method) {
    return Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "DeterministicPathExecutionUseCaseTest"));
}

class FakeInterpolationPort final : public IInterpolationPort {
   public:
    Result<void> ConfigureCoordinateSystem(int16 coord_sys, const CoordinateSystemConfig& config) override {
        configure_calls++;
        last_coord_sys = coord_sys;
        last_config = config;
        return Result<void>::Success();
    }

    Result<void> AddInterpolationData(int16 coord_sys, const InterpolationData& data) override {
        add_calls++;
        last_add_coord_sys = coord_sys;
        staged_segments.push_back(data);
        segments.push_back(data);
        return Result<void>::Success();
    }

    Result<void> ClearInterpolationBuffer(int16 coord_sys) override {
        clear_calls++;
        last_clear_coord_sys = coord_sys;
        staged_segments.clear();
        queued_segments.clear();
        active_segment = false;
        return Result<void>::Success();
    }

    Result<void> FlushInterpolationData(int16 coord_sys) override {
        flush_calls++;
        last_flush_coord_sys = coord_sys;
        queued_segments.insert(queued_segments.end(), staged_segments.begin(), staged_segments.end());
        staged_segments.clear();
        return Result<void>::Success();
    }

    Result<void> StartCoordinateSystemMotion(uint32 coord_sys_mask) override {
        start_calls++;
        last_start_mask = coord_sys_mask;
        if (!active_segment && !queued_segments.empty()) {
            active_segment = true;
            queued_segments.erase(queued_segments.begin());
        }
        return Result<void>::Success();
    }

    Result<void> StopCoordinateSystemMotion(uint32 coord_sys_mask) override {
        stop_calls++;
        last_stop_mask = coord_sys_mask;
        staged_segments.clear();
        queued_segments.clear();
        active_segment = false;
        return Result<void>::Success();
    }

    Result<void> SetCoordinateSystemVelocityOverride(int16 /*coord_sys*/, float32 /*override_percent*/) override {
        return NotImplementedVoid("SetCoordinateSystemVelocityOverride");
    }

    Result<void> EnableCoordinateSystemSCurve(int16 /*coord_sys*/, float32 /*jerk*/) override {
        return NotImplementedVoid("EnableCoordinateSystemSCurve");
    }

    Result<void> DisableCoordinateSystemSCurve(int16 /*coord_sys*/) override {
        return NotImplementedVoid("DisableCoordinateSystemSCurve");
    }

    Result<void> SetConstLinearVelocityMode(int16 /*coord_sys*/, bool /*enabled*/, uint32 /*rotate_axis_mask*/) override {
        return NotImplementedVoid("SetConstLinearVelocityMode");
    }

    Result<uint32> GetInterpolationBufferSpace(int16 /*coord_sys*/) const override {
        const auto occupancy =
            staged_segments.size() + queued_segments.size() + (active_segment ? 1U : 0U);
        return Result<uint32>::Success(buffer_capacity > occupancy ? static_cast<uint32>(buffer_capacity - occupancy) : 0U);
    }

    Result<uint32> GetLookAheadBufferSpace(int16 /*coord_sys*/) const override {
        const auto occupied = queued_segments.size();
        return Result<uint32>::Success(
            lookahead_capacity > occupied ? static_cast<uint32>(lookahead_capacity - occupied) : 0U);
    }

    Result<CoordinateSystemStatus> GetCoordinateSystemStatus(int16 /*coord_sys*/) const override {
        CoordinateSystemStatus status;
        status.is_moving = active_segment;
        status.state = active_segment
            ? Siligen::Domain::Motion::Ports::CoordinateSystemState::MOVING
            : Siligen::Domain::Motion::Ports::CoordinateSystemState::IDLE;
        status.remaining_segments = static_cast<uint32>(queued_segments.size() + (active_segment ? 1U : 0U));
        status.raw_segment = static_cast<int32>(status.remaining_segments);
        return Result<CoordinateSystemStatus>::Success(status);
    }

    void completeActiveSegment() {
        active_segment = false;
    }

    int configure_calls = 0;
    int add_calls = 0;
    int clear_calls = 0;
    int flush_calls = 0;
    int start_calls = 0;
    int stop_calls = 0;
    int16 last_coord_sys = 0;
    int16 last_add_coord_sys = 0;
    int16 last_clear_coord_sys = 0;
    int16 last_flush_coord_sys = 0;
    uint32 last_start_mask = 0;
    uint32 last_stop_mask = 0;
    CoordinateSystemConfig last_config{};
    std::vector<InterpolationData> segments{};
    std::vector<InterpolationData> staged_segments{};
    std::vector<InterpolationData> queued_segments{};
    std::size_t buffer_capacity = 8U;
    std::size_t lookahead_capacity = 4U;
    bool active_segment = false;
};

class FakeMotionStatePort final : public IMotionStatePort {
   public:
    Result<Point2D> GetCurrentPosition() const override {
        return Result<Point2D>::Success(position);
    }

    Result<float32> GetAxisPosition(LogicalAxisId axis) const override {
        if (axis == LogicalAxisId::X) {
            return Result<float32>::Success(position.x);
        }
        if (axis == LogicalAxisId::Y) {
            return Result<float32>::Success(position.y);
        }
        return Result<float32>::Success(0.0f);
    }

    Result<float32> GetAxisVelocity(LogicalAxisId /*axis*/) const override {
        return Result<float32>::Success(0.0f);
    }

    Result<MotionStatus> GetAxisStatus(LogicalAxisId axis) const override {
        if (axis == LogicalAxisId::X) {
            return Result<MotionStatus>::Success(axis_statuses[0]);
        }
        if (axis == LogicalAxisId::Y) {
            return Result<MotionStatus>::Success(axis_statuses[1]);
        }
        return Result<MotionStatus>::Success(MotionStatus{});
    }

    Result<bool> IsAxisMoving(LogicalAxisId axis) const override {
        auto status_result = GetAxisStatus(axis);
        if (status_result.IsError()) {
            return Result<bool>::Failure(status_result.GetError());
        }
        return Result<bool>::Success(status_result.Value().state == MotionState::MOVING);
    }

    Result<bool> IsAxisInPosition(LogicalAxisId /*axis*/) const override {
        return Result<bool>::Success(true);
    }

    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        return Result<std::vector<MotionStatus>>::Success(axis_statuses);
    }

    void setMoving(bool moving) {
        axis_statuses[0].state = moving ? MotionState::MOVING : MotionState::IDLE;
        axis_statuses[1].state = moving ? MotionState::MOVING : MotionState::IDLE;
    }

    Point2D position{0.0f, 0.0f};
    std::vector<MotionStatus> axis_statuses{MotionStatus{}, MotionStatus{}};
};

DeterministicPathExecutionRequest makeRequest() {
    DeterministicPathExecutionRequest request;
    request.max_velocity_mm_s = 20.0f;
    request.max_acceleration_mm_s2 = 200.0f;
    request.sample_dt_s = 0.001f;
    request.axis_map = {LogicalAxisId::X, LogicalAxisId::Y};
    request.segments = {
        DeterministicPathSegment{
            DeterministicPathSegmentType::LINE,
            Point2D{10.0f, 0.0f},
            Point2D{0.0f, 0.0f}},
        DeterministicPathSegment{
            DeterministicPathSegmentType::LINE,
            Point2D{10.0f, 5.0f},
            Point2D{0.0f, 0.0f}},
    };
    return request;
}

void testStartAndAdvanceAreDeterministic() {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();

    DeterministicPathExecutionUseCase use_case(interpolation_port, motion_state_port);
    const auto start_result = use_case.Start(makeRequest());

    require(start_result.IsSuccess(), "start should succeed");
    const auto start_status = start_result.Value();
    require(start_status.state == DeterministicPathExecutionState::READY, "start should be non-blocking");
    require(start_status.total_segments == 2, "expected two planned segments");
    require(start_status.dispatched_segments == 0, "start should not dispatch segments");
    require(interpolation_port->clear_calls == 1, "start should clear any stale interpolation buffer");
    require(interpolation_port->add_calls == 0, "start must not inject interpolation data");

    auto advance_result = use_case.Advance();
    require(advance_result.IsSuccess(), "first advance should succeed");
    require(advance_result.Value().state == DeterministicPathExecutionState::RUNNING, "first advance should run");
    require(advance_result.Value().dispatched_segments == 2, "first advance should prefetch the buffered program");
    require(interpolation_port->clear_calls == 2, "first advance should reset the coordinate-system buffer once more");
    require(interpolation_port->add_calls == 2, "first advance should add both interpolation segments");
    require(interpolation_port->flush_calls == 2, "first advance should flush each buffered segment");
    require(interpolation_port->start_calls == 2, "first advance should service start and dispatch the first segment");
    require(interpolation_port->segments.size() == 2, "first advance should record the buffered segments");
    require(interpolation_port->segments[0].positions.size() == 2, "segment should keep XY positions");
    require(interpolation_port->segments[0].positions[0] == 10.0f, "first segment X mismatch");
    require(interpolation_port->segments[0].positions[1] == 0.0f, "first segment Y mismatch");
    require(interpolation_port->segments[1].positions[0] == 10.0f, "second segment X mismatch");
    require(interpolation_port->segments[1].positions[1] == 5.0f, "second segment Y mismatch");

    motion_state_port->setMoving(true);
    advance_result = use_case.Advance();
    require(advance_result.IsSuccess(), "advance while moving should succeed");
    require(advance_result.Value().dispatched_segments == 2, "advance while moving should keep both segments buffered");
    require(interpolation_port->add_calls == 2, "advance while moving must not add more segments");

    motion_state_port->setMoving(false);
    interpolation_port->completeActiveSegment();
    advance_result = use_case.Advance();
    require(advance_result.IsSuccess(), "buffered dispatch should succeed");
    require(advance_result.Value().dispatched_segments == 2, "buffered dispatch should not change segment counter");
    require(interpolation_port->add_calls == 2, "buffered dispatch should reuse queued data");

    interpolation_port->completeActiveSegment();
    advance_result = use_case.Advance();
    require(advance_result.IsSuccess(), "completion advance should succeed");
    require(advance_result.Value().state == DeterministicPathExecutionState::COMPLETED, "path should complete");
}

void testCancelStopsCoordinateSystem() {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();

    DeterministicPathExecutionUseCase use_case(interpolation_port, motion_state_port);
    const auto start_result = use_case.Start(makeRequest());
    require(start_result.IsSuccess(), "cancel test start should succeed");
    require(use_case.Advance().IsSuccess(), "cancel test first advance should succeed");

    const auto cancel_result = use_case.Cancel();
    require(cancel_result.IsSuccess(), "cancel should succeed");
    const auto status = use_case.Status();
    require(status.state == DeterministicPathExecutionState::CANCELLED, "cancel should update terminal state");
    require(interpolation_port->stop_calls == 1, "cancel should stop coordinate system motion");
    require(interpolation_port->last_stop_mask == 1U, "cancel should stop coordinate system 1");
}

void testFailureStopsCoordinateSystem() {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();

    DeterministicPathExecutionUseCase use_case(interpolation_port, motion_state_port);
    const auto start_result = use_case.Start(makeRequest());
    require(start_result.IsSuccess(), "failure test start should succeed");
    require(use_case.Advance().IsSuccess(), "failure test first advance should succeed");

    motion_state_port->axis_statuses[0].has_error = true;
    motion_state_port->axis_statuses[0].error_code = 42;

    const auto advance_result = use_case.Advance();
    require(advance_result.IsError(), "advance on axis error should fail");
    require(use_case.Status().state == DeterministicPathExecutionState::FAILED, "status should move to failed");
    require(interpolation_port->stop_calls == 1, "failure should stop coordinate system motion");
    require(interpolation_port->last_stop_mask == 1U, "failure should stop coordinate system 1");
}

}  // namespace

int main() {
    try {
        testStartAndAdvanceAreDeterministic();
        testCancelStopsCoordinateSystem();
        testFailureStopsCoordinateSystem();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
