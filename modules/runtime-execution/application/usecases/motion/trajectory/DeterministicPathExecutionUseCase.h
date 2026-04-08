#pragma once

#include "runtime_execution/application/usecases/motion/coordination/MotionCoordinationUseCase.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Motion::Trajectory {

using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int16;
using Siligen::Shared::Types::int32;

enum class DeterministicPathSegmentType {
    LINE,
    ARC_CW,
    ARC_CCW
};

struct DeterministicPathSegment {
    DeterministicPathSegmentType type = DeterministicPathSegmentType::LINE;
    Point2D end_point{0, 0};
    Point2D center_point{0, 0};
};

struct DeterministicPathExecutionRequest {
    std::vector<DeterministicPathSegment> segments;
    std::vector<LogicalAxisId> axis_map{LogicalAxisId::X, LogicalAxisId::Y};
    float32 max_velocity_mm_s = 0.0f;
    float32 max_acceleration_mm_s2 = 0.0f;
    float32 sample_dt_s = 0.0f;
    int16 coord_sys = 1;

    bool Validate() const noexcept;
};

enum class DeterministicPathExecutionState {
    IDLE,
    READY,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED
};

struct DeterministicPathExecutionStatus {
    DeterministicPathExecutionState state = DeterministicPathExecutionState::IDLE;
    int32 total_segments = 0;
    int32 dispatched_segments = 0;
    bool segment_in_flight = false;
    std::string detail;
};

class DeterministicPathExecutionUseCase {
   public:
    DeterministicPathExecutionUseCase(
        std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port,
        std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port);

    ~DeterministicPathExecutionUseCase() = default;

    DeterministicPathExecutionUseCase(const DeterministicPathExecutionUseCase&) = delete;
    DeterministicPathExecutionUseCase& operator=(const DeterministicPathExecutionUseCase&) = delete;
    DeterministicPathExecutionUseCase(DeterministicPathExecutionUseCase&&) = delete;
    DeterministicPathExecutionUseCase& operator=(DeterministicPathExecutionUseCase&&) = delete;

    Result<DeterministicPathExecutionStatus> Start(const DeterministicPathExecutionRequest& request);
    Result<DeterministicPathExecutionStatus> Advance();
    DeterministicPathExecutionStatus Status() const noexcept;
    Result<void> Cancel();
    bool HasActiveExecution() const noexcept;

   private:
    struct ActiveExecution {
        std::vector<Siligen::RuntimeExecution::Contracts::Motion::InterpolationData> program{};
        std::vector<LogicalAxisId> axis_map{};
        std::size_t next_segment_index{0};
        int16 coord_sys{1};
    };

    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port_;
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
    Coordination::MotionCoordinationUseCase coordination_use_case_;
    std::optional<ActiveExecution> active_execution_{};
    DeterministicPathExecutionStatus status_{};

    Result<ActiveExecution> BuildExecution(
        const DeterministicPathExecutionRequest& request,
        const Point2D& start_point) const;
    Result<void> DispatchNextSegment(ActiveExecution& execution);
    Result<Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemStatus>
        ReadCoordinateSystemStatus(int16 coord_sys) const;
    Result<std::vector<Domain::Motion::Ports::MotionStatus>> ReadMotionStatuses() const;
    Result<void> FailActiveExecution(const std::string& detail);
};

}  // namespace Siligen::Application::UseCases::Motion::Trajectory
