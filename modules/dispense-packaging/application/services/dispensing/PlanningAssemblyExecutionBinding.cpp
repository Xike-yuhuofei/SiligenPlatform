#include "application/services/dispensing/PlanningAssemblyInternals.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/TrajectoryTriggerUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace Siligen::Application::Services::Dispensing::Internal {

using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayoutState;
using Siligen::Domain::Dispensing::ValueObjects::InterpolationTriggerBinding;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerSourceKind;

namespace {

struct TriggerBindingCandidate {
    bool found = false;
    std::size_t enabled_position = 0;
    std::size_t interpolation_index = 0;
    float32 distance_error_mm = std::numeric_limits<float32>::max();
    float32 match_error_mm = std::numeric_limits<float32>::max();
    bool monotonic = true;
};

struct TriggerBindingTraceRow {
    std::size_t trigger_index = 0;
    std::string trigger_id;
    std::string span_ref;
    std::size_t source_segment_index = 0;
    LayoutTriggerSourceKind source_kind = LayoutTriggerSourceKind::Generated;
    Point2D trigger_position;
    float32 authority_distance_mm = 0.0f;
    bool matched = false;
    std::size_t interpolation_index = 0;
    uint32 execution_sequence_id = 0;
    Point2D execution_position;
    float32 execution_trigger_position_mm = 0.0f;
    float32 distance_delta_mm = 0.0f;
    float32 position_delta_mm = 0.0f;
    bool monotonic = false;
    Siligen::SegmentType execution_segment_type = Siligen::SegmentType::LINEAR;
};

std::string FormatPoint(const Point2D& point) {
    std::ostringstream oss;
    oss << '(' << point.x << ',' << point.y << ')';
    return oss.str();
}

const char* ToString(LayoutTriggerSourceKind source_kind) {
    switch (source_kind) {
        case LayoutTriggerSourceKind::Anchor:
            return "anchor";
        case LayoutTriggerSourceKind::Generated:
            return "generated";
        case LayoutTriggerSourceKind::SharedVertex:
            return "shared_vertex";
    }
    return "generated";
}

const char* ToString(Siligen::SegmentType segment_type) {
    switch (segment_type) {
        case Siligen::SegmentType::LINEAR:
            return "linear";
        case Siligen::SegmentType::ARC_CW:
            return "arc_cw";
        case Siligen::SegmentType::ARC_CCW:
            return "arc_ccw";
    }
    return "linear";
}

void LogBindingTraceWindow(
    const AuthorityTriggerLayout& layout,
    const std::vector<TriggerBindingTraceRow>& rows,
    std::size_t failure_index) {
    if (rows.empty()) {
        return;
    }

    const std::size_t window_start = 0;
    const std::size_t requested_end = std::max<std::size_t>(failure_index + 1, 30);
    const std::size_t window_end = std::min(rows.size(), requested_end);

    {
        std::ostringstream oss;
        oss << "preview_binding_trace_window"
            << " layout_id=" << layout.layout_id
            << " failed_trigger_index=" << failure_index
            << " row_start=" << window_start
            << " row_end=" << window_end
            << " available_rows=" << rows.size()
            << " total_triggers=" << layout.trigger_points.size();
        SILIGEN_LOG_INFO(oss.str());
    }

    for (std::size_t row_index = window_start; row_index < window_end; ++row_index) {
        const auto& row = rows[row_index];
        std::ostringstream oss;
        oss << "preview_binding_trace"
            << " trigger_index=" << row.trigger_index
            << " trigger_id=" << row.trigger_id
            << " span_ref=" << row.span_ref
            << " source_segment_index=" << row.source_segment_index
            << " source_kind=" << ToString(row.source_kind)
            << " trigger_position=" << FormatPoint(row.trigger_position)
            << " authority_distance_mm=" << row.authority_distance_mm
            << " matched=" << (row.matched ? 1 : 0);
        if (row.matched) {
            oss << " execution_index=" << row.interpolation_index
                << " execution_sequence_id=" << row.execution_sequence_id
                << " execution_position=" << FormatPoint(row.execution_position)
                << " execution_trigger_position_mm=" << row.execution_trigger_position_mm
                << " distance_delta_mm=" << row.distance_delta_mm
                << " position_delta_mm=" << row.position_delta_mm
                << " monotonic=" << (row.monotonic ? 1 : 0)
                << " execution_segment_type=" << ToString(row.execution_segment_type);
        }
        SILIGEN_LOG_INFO(oss.str());
    }
}

bool IsBetterTriggerBindingCandidate(
    const TriggerBindingCandidate& candidate,
    const TriggerBindingCandidate& current) {
    if (!candidate.found) {
        return false;
    }
    if (!current.found) {
        return true;
    }

    if (candidate.distance_error_mm + Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm <
        current.distance_error_mm) {
        return true;
    }
    if (std::fabs(candidate.distance_error_mm - current.distance_error_mm) >
        Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm) {
        return false;
    }

    if (candidate.match_error_mm + Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm <
        current.match_error_mm) {
        return true;
    }
    if (std::fabs(candidate.match_error_mm - current.match_error_mm) >
        Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm) {
        return false;
    }

    if (candidate.monotonic != current.monotonic) {
        return candidate.monotonic;
    }

    return candidate.interpolation_index < current.interpolation_index;
}

bool SupportsClosedLoopCyclicBinding(const AuthorityTriggerLayout& layout) {
    return layout.spans.size() == 1U && layout.spans.front().closed;
}

void RotateEnabledTriggerIndicesForClosedLoopBinding(
    const TriggerArtifacts& artifacts,
    const std::vector<TrajectoryPoint>& execution_trajectory,
    std::vector<std::size_t>& enabled_trigger_indices,
    float32 distance_tolerance_mm,
    float32 match_tolerance_mm) {
    if (!SupportsClosedLoopCyclicBinding(artifacts.authority_trigger_layout) ||
        enabled_trigger_indices.empty() ||
        artifacts.authority_trigger_layout.trigger_points.empty()) {
        return;
    }

    const auto& first_trigger = artifacts.authority_trigger_layout.trigger_points.front();
    TriggerBindingCandidate best_candidate;
    for (std::size_t enabled_position = 0; enabled_position < enabled_trigger_indices.size(); ++enabled_position) {
        const auto& trajectory_point = execution_trajectory[enabled_trigger_indices[enabled_position]];
        const float32 distance_error_mm =
            std::fabs(trajectory_point.trigger_position_mm - first_trigger.distance_mm_global);
        const float32 match_error_mm = trajectory_point.position.DistanceTo(first_trigger.position);
        if (distance_error_mm > distance_tolerance_mm || match_error_mm > match_tolerance_mm) {
            continue;
        }

        TriggerBindingCandidate candidate;
        candidate.found = true;
        candidate.enabled_position = enabled_position;
        candidate.interpolation_index = enabled_trigger_indices[enabled_position];
        candidate.distance_error_mm = distance_error_mm;
        candidate.match_error_mm = match_error_mm;
        candidate.monotonic = true;
        if (IsBetterTriggerBindingCandidate(candidate, best_candidate)) {
            best_candidate = candidate;
        }
    }

    if (best_candidate.found && best_candidate.enabled_position > 0) {
        std::rotate(
            enabled_trigger_indices.begin(),
            enabled_trigger_indices.begin() + static_cast<std::ptrdiff_t>(best_candidate.enabled_position),
            enabled_trigger_indices.end());
    }
}

}  // namespace

void BindAuthorityLayoutToExecutionTrajectory(
    TriggerArtifacts& artifacts,
    const std::vector<TrajectoryPoint>* execution_trajectory) {
    artifacts.authority_trigger_layout.bindings.clear();
    artifacts.binding_ready = false;
    artifacts.authority_trigger_layout.binding_ready = false;

    if (execution_trajectory == nullptr || artifacts.authority_trigger_layout.trigger_points.empty()) {
        if (execution_trajectory == nullptr && !artifacts.authority_trigger_layout.trigger_points.empty() &&
            artifacts.failure_reason.empty()) {
            artifacts.failure_reason = "execution trajectory unavailable for preview binding";
        }
        std::ostringstream oss;
        oss << "preview_binding_stage=skipped"
            << " layout_id=" << artifacts.authority_trigger_layout.layout_id
            << " trigger_points=" << artifacts.authority_trigger_layout.trigger_points.size()
            << " execution_points=" << (execution_trajectory ? execution_trajectory->size() : 0U);
        SILIGEN_LOG_INFO(oss.str());
        return;
    }

    const auto started_at = std::chrono::steady_clock::now();
    std::vector<std::size_t> enabled_trigger_indices;
    enabled_trigger_indices.reserve(artifacts.authority_trigger_layout.trigger_points.size());
    for (std::size_t interpolation_index = 0; interpolation_index < execution_trajectory->size();
         ++interpolation_index) {
        if ((*execution_trajectory)[interpolation_index].enable_position_trigger) {
            enabled_trigger_indices.push_back(interpolation_index);
        }
    }

    {
        std::ostringstream oss;
        oss << "preview_binding_stage=start"
            << " layout_id=" << artifacts.authority_trigger_layout.layout_id
            << " trigger_points=" << artifacts.authority_trigger_layout.trigger_points.size()
            << " execution_points=" << execution_trajectory->size()
            << " enabled_trigger_points=" << enabled_trigger_indices.size();
        SILIGEN_LOG_INFO(oss.str());
    }

    std::size_t last_interpolation_index = 0;
    std::size_t preferred_enabled_position = 0;
    std::vector<bool> consumed_enabled(enabled_trigger_indices.size(), false);
    std::vector<TriggerBindingTraceRow> binding_trace_rows;
    binding_trace_rows.reserve(artifacts.authority_trigger_layout.trigger_points.size());
    artifacts.authority_trigger_layout.bindings.reserve(artifacts.authority_trigger_layout.trigger_points.size());

    const float32 distance_tolerance_mm = Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm;
    const float32 match_tolerance_mm = std::max(kGluePointDedupEpsilonMm, distance_tolerance_mm);
    RotateEnabledTriggerIndicesForClosedLoopBinding(
        artifacts,
        *execution_trajectory,
        enabled_trigger_indices,
        distance_tolerance_mm,
        match_tolerance_mm);

    auto bind_candidate = [&](std::size_t trigger_index,
                              const auto& trigger,
                              const TriggerBindingCandidate& candidate,
                              TriggerBindingTraceRow&& trace_row) {
        const auto& trajectory_point = (*execution_trajectory)[candidate.interpolation_index];
        trace_row.matched = true;
        trace_row.interpolation_index = candidate.interpolation_index;
        trace_row.execution_sequence_id = trajectory_point.sequence_id;
        trace_row.execution_position = trajectory_point.position;
        trace_row.execution_trigger_position_mm = trajectory_point.trigger_position_mm;
        trace_row.distance_delta_mm = trajectory_point.trigger_position_mm - trigger.distance_mm_global;
        trace_row.position_delta_mm = trajectory_point.position.DistanceTo(trigger.position);
        trace_row.monotonic = candidate.monotonic;
        trace_row.execution_segment_type = trajectory_point.segment_type;
        binding_trace_rows.push_back(std::move(trace_row));

        InterpolationTriggerBinding binding;
        binding.binding_id = artifacts.authority_trigger_layout.layout_id + "-binding-" + std::to_string(trigger_index);
        binding.layout_ref = artifacts.authority_trigger_layout.layout_id;
        binding.trigger_ref = trigger.trigger_id;
        binding.interpolation_index = candidate.interpolation_index;
        binding.execution_position = trajectory_point.position;
        binding.match_error_mm = candidate.match_error_mm;
        binding.monotonic = candidate.monotonic;
        binding.bound = true;
        artifacts.authority_trigger_layout.bindings.push_back(std::move(binding));
        last_interpolation_index = candidate.interpolation_index;
    };

    std::size_t last_enabled_position = 0;
    bool has_previous_binding = false;

    for (std::size_t trigger_index = 0; trigger_index < artifacts.authority_trigger_layout.trigger_points.size();
         ++trigger_index) {
        const auto& trigger = artifacts.authority_trigger_layout.trigger_points[trigger_index];
        TriggerBindingTraceRow trace_row;
        trace_row.trigger_index = trigger_index;
        trace_row.trigger_id = trigger.trigger_id;
        trace_row.span_ref = trigger.span_ref;
        trace_row.source_segment_index = trigger.source_segment_index;
        trace_row.source_kind = trigger.source_kind;
        trace_row.trigger_position = trigger.position;
        trace_row.authority_distance_mm = trigger.distance_mm_global;

        TriggerBindingCandidate best_candidate;
        std::size_t nearest_distance_enabled_position = enabled_trigger_indices.size();
        std::size_t nearest_match_enabled_position = enabled_trigger_indices.size();
        float32 nearest_distance_error_mm = std::numeric_limits<float32>::max();
        float32 nearest_match_error_mm = std::numeric_limits<float32>::max();

        auto evaluate_candidate = [&](std::size_t enabled_position) {
            if (enabled_position >= enabled_trigger_indices.size() || consumed_enabled[enabled_position]) {
                return;
            }

            const std::size_t interpolation_index = enabled_trigger_indices[enabled_position];
            const auto& trajectory_point = (*execution_trajectory)[interpolation_index];
            const float32 distance_error_mm =
                std::fabs(trajectory_point.trigger_position_mm - trigger.distance_mm_global);
            const float32 match_error_mm = trajectory_point.position.DistanceTo(trigger.position);
            if (distance_error_mm < nearest_distance_error_mm) {
                nearest_distance_error_mm = distance_error_mm;
                nearest_distance_enabled_position = enabled_position;
            }
            if (match_error_mm < nearest_match_error_mm) {
                nearest_match_error_mm = match_error_mm;
                nearest_match_enabled_position = enabled_position;
            }
            if (distance_error_mm > distance_tolerance_mm || match_error_mm > match_tolerance_mm) {
                return;
            }

            TriggerBindingCandidate candidate;
            candidate.found = true;
            candidate.enabled_position = enabled_position;
            candidate.interpolation_index = interpolation_index;
            candidate.distance_error_mm = distance_error_mm;
            candidate.match_error_mm = match_error_mm;
            candidate.monotonic = !has_previous_binding || enabled_position >= last_enabled_position;
            if (IsBetterTriggerBindingCandidate(candidate, best_candidate)) {
                best_candidate = candidate;
            }
        };

        while (preferred_enabled_position < enabled_trigger_indices.size()) {
            if (consumed_enabled[preferred_enabled_position]) {
                ++preferred_enabled_position;
                continue;
            }
            const auto& trajectory_point = (*execution_trajectory)[enabled_trigger_indices[preferred_enabled_position]];
            if (trajectory_point.trigger_position_mm + distance_tolerance_mm < trigger.distance_mm_global) {
                ++preferred_enabled_position;
                continue;
            }
            break;
        }

        for (std::size_t enabled_position = preferred_enabled_position;
             enabled_position < enabled_trigger_indices.size();
             ++enabled_position) {
            if (consumed_enabled[enabled_position]) {
                continue;
            }

            const auto& trajectory_point = (*execution_trajectory)[enabled_trigger_indices[enabled_position]];
            evaluate_candidate(enabled_position);

            if (trajectory_point.trigger_position_mm > trigger.distance_mm_global + distance_tolerance_mm) {
                break;
            }
        }

        if (!best_candidate.found) {
            for (std::size_t enabled_position = 0; enabled_position < enabled_trigger_indices.size(); ++enabled_position) {
                evaluate_candidate(enabled_position);
            }
        }

        if (!best_candidate.found) {
            std::ostringstream oss;
            oss << "preview_binding_stage=failed_first_trigger"
                << " layout_id=" << artifacts.authority_trigger_layout.layout_id
                << " trigger_index=" << trigger_index
                << " trigger_id=" << trigger.trigger_id
                << " trigger_distance_mm=" << trigger.distance_mm_global
                << " trigger_position=" << FormatPoint(trigger.position)
                << " execution_points=" << execution_trajectory->size()
                << " enabled_trigger_points=" << enabled_trigger_indices.size()
                << " last_interpolation_index=" << last_interpolation_index
                << " distance_tolerance_mm=" << distance_tolerance_mm
                << " match_tolerance_mm=" << match_tolerance_mm
                << " elapsed_ms=" << ElapsedMs(started_at);
            if (nearest_distance_enabled_position < enabled_trigger_indices.size()) {
                const std::size_t nearest_distance_index =
                    enabled_trigger_indices[nearest_distance_enabled_position];
                const auto& nearest_distance_point = (*execution_trajectory)[nearest_distance_index];
                oss << " nearest_distance={index=" << nearest_distance_index
                    << ",trigger_position_mm=" << nearest_distance_point.trigger_position_mm
                    << ",distance_error_mm=" << nearest_distance_error_mm
                    << ",match_error_mm=" << nearest_distance_point.position.DistanceTo(trigger.position)
                    << ",position=" << FormatPoint(nearest_distance_point.position)
                    << ",monotonic=" << (nearest_distance_index >= last_interpolation_index ? 1 : 0)
                    << '}';
            }
            if (nearest_match_enabled_position < enabled_trigger_indices.size()) {
                const std::size_t nearest_match_index = enabled_trigger_indices[nearest_match_enabled_position];
                const auto& nearest_match_point = (*execution_trajectory)[nearest_match_index];
                oss << " nearest_match={index=" << nearest_match_index
                    << ",trigger_position_mm=" << nearest_match_point.trigger_position_mm
                    << ",distance_error_mm="
                    << std::fabs(nearest_match_point.trigger_position_mm - trigger.distance_mm_global)
                    << ",match_error_mm=" << nearest_match_error_mm
                    << ",position=" << FormatPoint(nearest_match_point.position)
                    << ",monotonic=" << (nearest_match_index >= last_interpolation_index ? 1 : 0)
                    << '}';
            }
            SILIGEN_LOG_WARNING(oss.str());
            binding_trace_rows.push_back(std::move(trace_row));
            LogBindingTraceWindow(artifacts.authority_trigger_layout, binding_trace_rows, trigger_index);
            artifacts.failure_reason = "authority trigger binding unavailable";
            return;
        }

        if (!best_candidate.monotonic) {
            const auto& trajectory_point = (*execution_trajectory)[best_candidate.interpolation_index];
            trace_row.matched = true;
            trace_row.interpolation_index = best_candidate.interpolation_index;
            trace_row.execution_sequence_id = trajectory_point.sequence_id;
            trace_row.execution_position = trajectory_point.position;
            trace_row.execution_trigger_position_mm = trajectory_point.trigger_position_mm;
            trace_row.distance_delta_mm = trajectory_point.trigger_position_mm - trigger.distance_mm_global;
            trace_row.position_delta_mm = trajectory_point.position.DistanceTo(trigger.position);
            trace_row.monotonic = false;
            trace_row.execution_segment_type = trajectory_point.segment_type;
            binding_trace_rows.push_back(std::move(trace_row));

            std::ostringstream oss;
            oss << "preview_binding_stage=failed_non_monotonic"
                << " layout_id=" << artifacts.authority_trigger_layout.layout_id
                << " trigger_index=" << trigger_index
                << " trigger_id=" << trigger.trigger_id
                << " trigger_distance_mm=" << trigger.distance_mm_global
                << " trigger_position=" << FormatPoint(trigger.position)
                << " execution_index=" << best_candidate.interpolation_index
                << " execution_sequence_id=" << trajectory_point.sequence_id
                << " execution_trigger_position_mm=" << trajectory_point.trigger_position_mm
                << " execution_position=" << FormatPoint(trajectory_point.position)
                << " last_interpolation_index=" << last_interpolation_index
                << " elapsed_ms=" << ElapsedMs(started_at);
            SILIGEN_LOG_WARNING(oss.str());
            LogBindingTraceWindow(artifacts.authority_trigger_layout, binding_trace_rows, trigger_index);
            artifacts.failure_reason = "authority trigger binding non-monotonic";
            artifacts.binding_ready = false;
            artifacts.authority_trigger_layout.binding_ready = false;
            return;
        }

        bind_candidate(trigger_index, trigger, best_candidate, std::move(trace_row));
        consumed_enabled[best_candidate.enabled_position] = true;
        last_enabled_position = best_candidate.enabled_position;
        has_previous_binding = true;
        preferred_enabled_position = std::max(preferred_enabled_position, best_candidate.enabled_position + 1U);
    }

    artifacts.binding_ready =
        artifacts.authority_trigger_layout.bindings.size() == artifacts.authority_trigger_layout.trigger_points.size();
    artifacts.authority_trigger_layout.binding_ready = artifacts.binding_ready;
    if (artifacts.binding_ready) {
        artifacts.authority_trigger_layout.state = AuthorityTriggerLayoutState::BindingReady;
        std::ostringstream oss;
        oss << "preview_binding_stage=complete"
            << " layout_id=" << artifacts.authority_trigger_layout.layout_id
            << " bindings=" << artifacts.authority_trigger_layout.bindings.size()
            << " trigger_points=" << artifacts.authority_trigger_layout.trigger_points.size()
            << " enabled_trigger_points=" << enabled_trigger_indices.size()
            << " elapsed_ms=" << ElapsedMs(started_at);
        SILIGEN_LOG_INFO(oss.str());
    } else if (artifacts.failure_reason.empty()) {
        artifacts.failure_reason = "authority trigger binding unavailable";
    }
}

}  // namespace Siligen::Application::Services::Dispensing::Internal
