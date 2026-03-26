#include "ExecuteTrajectoryUseCase.h"

#include "shared/types/Error.h"
#include "shared/util/PortCheck.h"

#include <chrono>
#include <cmath>
#include <thread>
#include <utility>

namespace Siligen::Application::UseCases::Motion::Trajectory {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;
using Siligen::Domain::Dispensing::Ports::TriggerConfig;
using Siligen::Domain::Dispensing::Ports::TriggerMode;

namespace {
constexpr float32 kMinAngleStep = 0.1f;
constexpr int kMinArcSegments = 10;
constexpr double kPi = 3.14159265358979323846;

class TriggerCleanupGuard {
   public:
    TriggerCleanupGuard(const std::shared_ptr<Siligen::Domain::Dispensing::Ports::ITriggerControllerPort>& port,
                        LogicalAxisId axis)
        : port_(port), axis_(axis) {}

    void Arm() { armed_ = true; }

    ~TriggerCleanupGuard() {
        if (!armed_ || !port_) {
            return;
        }
        (void)port_->ClearTrigger(axis_);
    }

    TriggerCleanupGuard(const TriggerCleanupGuard&) = delete;
    TriggerCleanupGuard& operator=(const TriggerCleanupGuard&) = delete;

   private:
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::ITriggerControllerPort> port_;
    LogicalAxisId axis_;
    bool armed_ = false;
};

float32 ComputeLineLength(const TrajectorySegment& segment) {
    return segment.start_point.DistanceTo(segment.end_point);
}

float32 ComputeArcLength(const TrajectorySegment& segment) {
    const auto radius = segment.start_point.DistanceTo(segment.center_point);
    if (radius <= 0.0f) {
        return 0.0f;
    }

    const auto start_angle = std::atan2(segment.start_point.y - segment.center_point.y,
                                        segment.start_point.x - segment.center_point.x);
    const auto end_angle = std::atan2(segment.end_point.y - segment.center_point.y,
                                      segment.end_point.x - segment.center_point.x);

    double delta = end_angle - start_angle;
    if (segment.type == TrajectorySegmentType::ARC_CW && delta > 0) {
        delta -= 2.0 * kPi;
    } else if (segment.type == TrajectorySegmentType::ARC_CCW && delta < 0) {
        delta += 2.0 * kPi;
    }

    return static_cast<float32>(std::abs(delta) * radius);
}

int DetermineArcSegments(float32 radius, float32 angle_span) {
    if (radius <= 0.0f) {
        return kMinArcSegments;
    }

    int segments = static_cast<int>(std::ceil(std::abs(angle_span) / kMinAngleStep));
    return std::max(segments, kMinArcSegments);
}

LogicalAxisId ResolveCompletionAxis(const TrajectorySegment& segment) {
    const float32 dx = std::abs(segment.end_point.x - segment.start_point.x);
    const float32 dy = std::abs(segment.end_point.y - segment.start_point.y);
    return (dy > dx) ? LogicalAxisId::Y : LogicalAxisId::X;
}

Result<void> EnsurePositionControlPort(
    const std::shared_ptr<Siligen::Domain::Motion::Ports::IPositionControlPort>& port,
    const char* source) {
    return Shared::Util::EnsurePort(port, "Position control port not initialized", source);
}
}  // namespace

ExecuteTrajectoryUseCase::ExecuteTrajectoryUseCase(
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port,
                                                   std::shared_ptr<Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port,
                                                   std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port)
    : position_control_port_(std::move(position_control_port)),
      trigger_port_(std::move(trigger_port)),
      event_port_(std::move(event_port)),
      is_paused_(false),
      is_cancelled_(false) {}

Result<ExecuteTrajectoryResponse> ExecuteTrajectoryUseCase::Execute(const ExecuteTrajectoryRequest& request) {
    is_paused_ = false;
    is_cancelled_ = false;

    auto validation = ValidateTrajectory(request);
    if (validation.IsError()) {
        return Result<ExecuteTrajectoryResponse>::Failure(validation.GetError());
    }

    ExecuteTrajectoryResponse response;
    response.total_segments = static_cast<int32>(request.trajectory_segments.size());

    auto start_time = std::chrono::steady_clock::now();

    for (const auto& segment : request.trajectory_segments) {
        if (is_cancelled_) {
            response.status_message = "Execution cancelled";
            break;
        }

        while (is_paused_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        auto segment_result = ExecuteSegment(segment, response, request.timeout_ms);
        if (segment_result.IsError()) {
            return Result<ExecuteTrajectoryResponse>::Failure(segment_result.GetError());
        }

        response.segments_executed++;

        if (segment.type == TrajectorySegmentType::LINEAR) {
            response.total_distance_mm += ComputeLineLength(segment);
        } else {
            response.total_distance_mm += ComputeArcLength(segment);
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    response.execution_time_ms = static_cast<int32>(elapsed.count());
    response.completed_successfully =
        (response.segments_executed == response.total_segments) && !is_cancelled_;
    response.status_message = response.completed_successfully ? "Trajectory completed" : response.status_message;

    PublishTrajectoryEvent("trajectory_completed", response);

    return Result<ExecuteTrajectoryResponse>::Success(response);
}

Result<void> ExecuteTrajectoryUseCase::Pause() {
    is_paused_ = true;
    if (position_control_port_) {
        return position_control_port_->StopAllAxes(false);
    }
    return Result<void>::Success();
}

Result<void> ExecuteTrajectoryUseCase::Resume() {
    is_paused_ = false;
    return Result<void>::Success();
}

Result<void> ExecuteTrajectoryUseCase::Cancel() {
    is_cancelled_ = true;
    if (position_control_port_) {
        return position_control_port_->StopAllAxes(true);
    }
    return Result<void>::Success();
}

Result<void> ExecuteTrajectoryUseCase::ValidateTrajectory(const ExecuteTrajectoryRequest& request) {
    if (!request.Validate()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid trajectory request", "ExecuteTrajectoryUseCase"));
    }
    return Result<void>::Success();
}

Result<void> ExecuteTrajectoryUseCase::ExecuteSegment(const TrajectorySegment& segment,
                                                      ExecuteTrajectoryResponse& response,
                                                      int32 timeout_ms) {
    TriggerCleanupGuard trigger_guard(trigger_port_, LogicalAxisId::X);
    if (segment.enable_trigger && trigger_port_) {
        trigger_guard.Arm();
    }

    auto trigger_result = ConfigureTriggerForSegment(segment);
    if (trigger_result.IsError()) {
        return trigger_result;
    }

    Result<void> result;
    if (segment.type == TrajectorySegmentType::LINEAR) {
        result = ExecuteLinearSegment(segment);
    } else {
        result = ExecuteArcSegment(segment, timeout_ms);
    }

    if (result.IsSuccess() && segment.enable_trigger) {
        response.triggers_activated++;
    }

    auto port_check = EnsurePositionControlPort(position_control_port_, "ExecuteTrajectoryUseCase::ExecuteSegment");
    if (port_check.IsError()) {
        return port_check;
    }

    const auto wait_axis = ResolveCompletionAxis(segment);
    auto wait_result = position_control_port_->WaitForMotionComplete(wait_axis, timeout_ms);
    if (wait_result.IsError()) {
        return wait_result;
    }

    response.final_position = segment.end_point;
    return result;
}

Result<void> ExecuteTrajectoryUseCase::ExecuteLinearSegment(const TrajectorySegment& segment) {
    auto port_check =
        EnsurePositionControlPort(position_control_port_, "ExecuteTrajectoryUseCase::ExecuteLinearSegment");
    if (port_check.IsError()) {
        return port_check;
    }

    return position_control_port_->MoveToPosition(segment.end_point, segment.velocity);
}

Result<void> ExecuteTrajectoryUseCase::ExecuteArcSegment(const TrajectorySegment& segment, int32 timeout_ms) {
    auto port_check = EnsurePositionControlPort(position_control_port_, "ExecuteTrajectoryUseCase::ExecuteArcSegment");
    if (port_check.IsError()) {
        return port_check;
    }

    const auto radius = segment.start_point.DistanceTo(segment.center_point);
    if (radius <= 0.0f) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid arc radius", "ExecuteTrajectoryUseCase"));
    }

    const auto start_angle = std::atan2(segment.start_point.y - segment.center_point.y,
                                        segment.start_point.x - segment.center_point.x);
    const auto end_angle = std::atan2(segment.end_point.y - segment.center_point.y,
                                      segment.end_point.x - segment.center_point.x);
    double delta = end_angle - start_angle;
    if (segment.type == TrajectorySegmentType::ARC_CW && delta > 0) {
        delta -= 2.0 * kPi;
    } else if (segment.type == TrajectorySegmentType::ARC_CCW && delta < 0) {
        delta += 2.0 * kPi;
    }

    const int steps = DetermineArcSegments(radius, static_cast<float32>(delta));
    for (int i = 1; i <= steps; ++i) {
        if (is_cancelled_) {
            break;
        }

        const double ratio = static_cast<double>(i) / static_cast<double>(steps);
        const double angle = start_angle + delta * ratio;

        Point2D point;
        point.x = segment.center_point.x + static_cast<float32>(radius * std::cos(angle));
        point.y = segment.center_point.y + static_cast<float32>(radius * std::sin(angle));

        auto result = position_control_port_->MoveToPosition(point, segment.velocity);
        if (result.IsError()) {
            return result;
        }

        auto wait_result = position_control_port_->WaitForMotionComplete(ResolveCompletionAxis(segment), timeout_ms);
        if (wait_result.IsError()) {
            return wait_result;
        }
    }

    return Result<void>::Success();
}

Result<void> ExecuteTrajectoryUseCase::ConfigureTriggerForSegment(const TrajectorySegment& segment) {
    if (!segment.enable_trigger) {
        return Result<void>::Success();
    }

    if (!trigger_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Trigger port not initialized", "ExecuteTrajectoryUseCase"));
    }

    TriggerConfig config;
    config.mode = TriggerMode::CONTINUOUS;
    auto config_result = trigger_port_->ConfigureTrigger(config);
    if (config_result.IsError()) {
        return config_result;
    }

    if (segment.trigger_interval_mm > 0.0f) {
        return trigger_port_->SetContinuousTrigger(LogicalAxisId::X,
                                                   segment.start_point.x,
                                                   segment.end_point.x,
                                                   segment.trigger_interval_mm,
                                                   config.pulse_width_us);
    }

    return Result<void>::Success();
}

void ExecuteTrajectoryUseCase::PublishTrajectoryEvent(const std::string& event_type,
                                                      const ExecuteTrajectoryResponse& response) {
    if (!event_port_) {
        return;
    }

    Domain::System::Ports::DomainEvent event;
    event.type = Domain::System::Ports::EventType::MOTION_COMPLETED;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    event.source = "ExecuteTrajectoryUseCase";
    event.message = event_type + ": segments=" + std::to_string(response.segments_executed);

    event_port_->PublishAsync(event);
}

}  // namespace Siligen::Application::UseCases::Motion::Trajectory



