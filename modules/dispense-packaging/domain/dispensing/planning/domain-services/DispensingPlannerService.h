#pragma once

#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/DispensingStrategy.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/Path.h"
#include "process_path/contracts/ProcessPath.h"
#include "motion_planning/contracts/MotionTrajectory.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "domain/dispensing/value-objects/DispenseCompensationProfile.h"
#include "domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Domain::Motion::DomainServices {
class VelocityProfileService;
}

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::DispensingStrategy;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::uint32;
using Siligen::TrajectoryPoint;
using Siligen::Domain::Motion::InterpolationAlgorithm;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::MotionPlanning::Contracts::MotionTrajectory;
using Siligen::ProcessPath::Contracts::Path;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile;

struct AuthorityTriggerPoint {
    Point2D position;
    float32 trigger_distance_mm = 0.0f;
    std::size_t segment_index = 0;
    bool short_segment_exception = false;
    bool shared_vertex = false;
};

struct SpacingValidationGroup {
    std::size_t segment_index = 0;
    std::vector<Point2D> points;
    float32 actual_spacing_mm = 0.0f;
    bool short_segment_exception = false;
    bool within_window = false;
};

struct DispensingPlanRequest {
    std::string dxf_filepath;
    Point2D dxf_offset{0.0f, 0.0f};
    bool bounds_check_enabled = false;
    float32 bounds_x_min = 0.0f;
    float32 bounds_x_max = 0.0f;
    float32 bounds_y_min = 0.0f;
    float32 bounds_y_max = 0.0f;
    bool optimize_path = false;
    float32 start_x = 0.0f;
    float32 start_y = 0.0f;
    bool approximate_splines = false;
    int two_opt_iterations = 0;
    float32 spline_max_step_mm = 0.0f;
    float32 spline_max_error_mm = 0.0f;
    float32 continuity_tolerance_mm = 0.0f;
    float32 curve_chain_angle_deg = 0.0f;
    float32 curve_chain_max_segment_mm = 0.0f;
    bool use_hardware_trigger = true;
    float32 dispensing_velocity = 0.0f;
    float32 acceleration = 500.0f;
    uint32 dispenser_interval_ms = 100;
    uint32 dispenser_duration_ms = 100;
    float32 trigger_spatial_interval_mm = 1.0f;
    float32 pulse_per_mm = 200.0f;
    float32 start_speed_factor = 0.5f;
    float32 end_speed_factor = 0.5f;
    float32 corner_speed_factor = 0.6f;
    float32 rapid_speed_factor = 1.0f;
    float32 sample_dt = 0.01f;
    float32 sample_ds = 0.0f;
    float32 arc_tolerance_mm = 0.0f;
    float32 max_jerk = 0.0f;
    bool use_interpolation_planner = false;
    InterpolationAlgorithm interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    DispensingStrategy dispensing_strategy = DispensingStrategy::BASELINE;
    int subsegment_count = 8;
    bool dispense_only_cruise = false;
    bool downgrade_on_violation = true;
    float32 valve_response_ms = 0.0f;
    float32 safety_margin_ms = 0.0f;
    float32 min_interval_ms = 0.0f;
    DispenseCompensationProfile compensation_profile{};
    bool Validate() const noexcept;
};

struct DispensingPlan {
    bool success = false;
    std::string error_message;
    Path path;
    ProcessPath process_path;
    MotionTrajectory motion_trajectory;
    std::vector<InterpolationData> interpolation_segments;
    std::vector<TrajectoryPoint> interpolation_points;
    std::vector<float32> trigger_distances_mm;
    uint32 trigger_interval_ms = 0;
    float32 trigger_interval_mm = 0.0f;
    bool trigger_downgrade_applied = false;
    float32 total_length_mm = 0.0f;
    float32 estimated_time_s = 0.0f;
    bool preview_authority_ready = false;
    bool preview_authority_shared_with_execution = false;
    bool preview_spacing_valid = false;
    bool preview_has_short_segment_exceptions = false;
    std::string preview_failure_reason;
    std::vector<AuthorityTriggerPoint> authority_trigger_points;
    std::vector<SpacingValidationGroup> spacing_validation_groups;
};

class DispensingPlanner {
   public:
    explicit DispensingPlanner(
        std::shared_ptr<Domain::Trajectory::Ports::IPathSourcePort> path_source,
        std::shared_ptr<Domain::Motion::DomainServices::VelocityProfileService> velocity_profile_service = nullptr);

    Result<DispensingPlan> Plan(const DispensingPlanRequest& request) const noexcept;
    std::vector<TrajectoryPoint> BuildPreviewPoints(const DispensingPlan& plan,
                                                    float32 spacing_mm,
                                                    size_t max_points) const;

   private:
    std::shared_ptr<Domain::Trajectory::Ports::IPathSourcePort> path_source_;
    std::shared_ptr<Domain::Motion::DomainServices::VelocityProfileService> velocity_profile_service_;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices

