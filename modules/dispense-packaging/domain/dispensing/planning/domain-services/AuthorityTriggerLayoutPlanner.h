#pragma once

#include "domain/dispensing/value-objects/AuthorityTriggerLayout.h"
#include "domain/dispensing/value-objects/DispenseCompensationProfile.h"
#include "domain/trajectory/value-objects/ProcessPath.h"
#include "shared/types/DispensingStrategy.h"
#include "shared/types/Result.h"

#include <string>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;
using Siligen::Shared::Types::DispensingStrategy;
using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;
using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout;
using Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile;

struct AuthorityTriggerLayoutPlannerRequest {
    ProcessPath process_path;
    std::string plan_id;
    std::string plan_fingerprint;
    std::string layout_id_seed;
    float32 target_spacing_mm = 0.0f;
    float32 min_spacing_mm = 0.0f;
    float32 max_spacing_mm = 0.0f;
    float32 dispensing_velocity = 0.0f;
    float32 acceleration = 0.0f;
    uint32 dispenser_interval_ms = 0;
    uint32 dispenser_duration_ms = 0;
    float32 valve_response_ms = 0.0f;
    float32 safety_margin_ms = 0.0f;
    float32 min_interval_ms = 0.0f;
    bool downgrade_on_violation = true;
    DispensingStrategy dispensing_strategy = DispensingStrategy::BASELINE;
    int subsegment_count = 8;
    bool dispense_only_cruise = false;
    bool enable_branch_revisit_split = true;
    bool enable_closed_loop_corner_anchors = true;
    bool emit_topology_diagnostics = true;
    float32 topology_vertex_tolerance_mm = 0.0f;
    float32 closed_loop_corner_angle_threshold_deg = 30.0f;
    float32 closed_loop_corner_cluster_distance_mm = 0.0f;
    float32 closed_loop_anchor_tolerance_mm = 0.0f;
    DispenseCompensationProfile compensation_profile{};
    float32 spline_max_error_mm = 0.0f;
    float32 spline_max_step_mm = 0.0f;
};

class AuthorityTriggerLayoutPlanner {
   public:
    Result<AuthorityTriggerLayout> Plan(const AuthorityTriggerLayoutPlannerRequest& request) const;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices
