#define MODULE_NAME "DispensingProcessService"

#include "DispensingProcessService.h"

#include "domain/dispensing/domain-services/DispenseCompensationService.h"
#include "domain/dispensing/domain-services/SupplyStabilizationPolicy.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int16;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;

namespace {
constexpr float32 kTraceEpsilonMm = 1e-4f;
constexpr float32 kTraceRadiansPerDegree = 0.017453292519943295f;
constexpr float32 kTraceFullCirclePositionToleranceMm = 0.1f;
constexpr float32 kDefaultAcceleration = 100.0f;
constexpr float32 kDefaultPulsePerMm = 200.0f;
constexpr float32 kPathTriggerPositionToleranceMm = 0.1f;
constexpr uint32 kDefaultDispenserIntervalMs = 100;
constexpr uint32 kDefaultDispenserDurationMs = 100;
constexpr float32 kDefaultTriggerSpatialIntervalMm = 3.0f;
constexpr uint32 kStatusPollIntervalMs = 50;
constexpr uint32 kMotionCompletionGraceMinMs = 5000;
constexpr uint32 kMotionCompletionGraceMaxMs = 30000;
constexpr float32 kMotionCompletionGraceRatio = 0.15f;
constexpr float32 kCompletionVelocityToleranceMmS = 0.5f;
constexpr float32 kCompletionPositionToleranceFloorMm = 0.2f;
constexpr auto kCompletionSettleWindow = std::chrono::milliseconds(250);
constexpr int32 kCrdStatusFifoFinish0 = 0x00000010;
constexpr int16 kCoordinateSystem = 1;
constexpr uint32 kCoordinateSystemMask = 0x01;

bool IsCoordinateSystemCompleted(const Motion::Ports::CoordinateSystemStatus& status) {
    if (status.state == Motion::Ports::CoordinateSystemState::ERROR_STATE) {
        return false;
    }

    if (status.state == Motion::Ports::CoordinateSystemState::IDLE) {
        return true;
    }

    if (!status.is_moving && status.remaining_segments == 0U) {
        return true;
    }

    const bool fifo_finished = (status.raw_status_word & kCrdStatusFifoFinish0) != 0;
    const bool coord_velocity_idle = std::fabs(status.current_velocity) <= kCompletionVelocityToleranceMmS;
    const bool segments_drained = status.remaining_segments == 0U || status.raw_segment <= 0;
    return fifo_finished && coord_velocity_idle && segments_drained;
}

std::optional<uint32> TryResolveExecutedSegmentsForProgress(const Motion::Ports::CoordinateSystemStatus& status,
                                                            uint32 total_segments) {
    if (total_segments == 0U) {
        return 0U;
    }

    if (IsCoordinateSystemCompleted(status)) {
        return total_segments;
    }

    if (status.raw_segment <= 0) {
        return std::nullopt;
    }

    const auto remaining_segments =
        std::min<uint32>(total_segments, static_cast<uint32>(status.raw_segment));
    return total_segments - remaining_segments;
}

struct SupplyValveGuard {
    std::shared_ptr<IValvePort> port;
    bool opened = false;

    explicit SupplyValveGuard(std::shared_ptr<IValvePort> p)
        : port(std::move(p)) {}

    Result<void> Open() {
        if (!port) {
            return Result<void>::Failure(Error(ErrorCode::PORT_NOT_INITIALIZED, "供胶阀端口未初始化"));
        }
        auto result = port->OpenSupply();
        if (!result.IsSuccess()) {
            return Result<void>::Failure(result.GetError());
        }
        opened = true;
        return Result<void>::Success();
    }

    void Close() {
        if (!port || !opened) {
            return;
        }
        auto result = port->CloseSupply();
        if (!result.IsSuccess()) {
            SILIGEN_LOG_WARNING("关闭供胶阀失败: " + result.GetError().GetMessage());
        }
        opened = false;
    }

    ~SupplyValveGuard() {
        Close();
    }

    SupplyValveGuard(const SupplyValveGuard&) = delete;
    SupplyValveGuard& operator=(const SupplyValveGuard&) = delete;
};

class DispenserOperationGuard {
   public:
    explicit DispenserOperationGuard(std::shared_ptr<IValvePort> port)
        : port_(std::move(port)), started_(false) {}

    Result<void> Start(const Ports::PositionTriggeredDispenserParams& params) {
        auto result = port_->StartPositionTriggeredDispenser(params);
        if (result.IsSuccess()) {
            started_ = true;
            return Result<void>::Success();
        }
        return Result<void>::Failure(result.GetError());
    }

    ~DispenserOperationGuard() {
        if (started_) {
            SILIGEN_LOG_DEBUG("[DispenserGuard] 段执行完成,调用 StopDispenser 关闭点胶阀");
            auto stop_result = port_->StopDispenser();
            if (!stop_result.IsSuccess()) {
                SILIGEN_LOG_WARNING("[DispenserGuard] StopDispenser 失败: " + stop_result.GetError().GetMessage());
            } else {
                SILIGEN_LOG_DEBUG("[DispenserGuard] StopDispenser 成功,阀门已关闭");
            }
        }
    }

    DispenserOperationGuard(const DispenserOperationGuard&) = delete;
    DispenserOperationGuard& operator=(const DispenserOperationGuard&) = delete;

   private:
    std::shared_ptr<IValvePort> port_;
    bool started_;
};

class CoordinateSystemStopGuard {
   public:
    CoordinateSystemStopGuard(std::shared_ptr<IInterpolationPort> port, uint32 mask)
        : port_(std::move(port)), mask_(mask) {}

    void Arm() { armed_ = true; }

    ~CoordinateSystemStopGuard() {
        if (!armed_ || !port_) {
            return;
        }
        auto result = port_->StopCoordinateSystemMotion(mask_);
        if (result.IsError()) {
            SILIGEN_LOG_WARNING("坐标系停止失败: " + result.GetError().GetMessage());
        }
    }

    CoordinateSystemStopGuard(const CoordinateSystemStopGuard&) = delete;
    CoordinateSystemStopGuard& operator=(const CoordinateSystemStopGuard&) = delete;

   private:
    std::shared_ptr<IInterpolationPort> port_;
    uint32 mask_;
    bool armed_ = false;
};

class MotionObserverGuard {
   public:
    explicit MotionObserverGuard(IDispensingExecutionObserver* observer)
        : observer_(observer) {}

    void Start() noexcept {
        if (!observer_) {
            return;
        }
        observer_->OnMotionStart();
        started_ = true;
    }

    ~MotionObserverGuard() {
        if (!observer_ || !started_) {
            return;
        }
        observer_->OnMotionStop();
    }

    MotionObserverGuard(const MotionObserverGuard&) = delete;
    MotionObserverGuard& operator=(const MotionObserverGuard&) = delete;

   private:
    IDispensingExecutionObserver* observer_;
    bool started_ = false;
};

int32 BuildMotionCompletionTimeoutMs(float32 estimated_motion_time_ms) noexcept {
    const auto base_timeout = static_cast<int32>(std::max(0.0f, estimated_motion_time_ms));
    if (base_timeout <= 0) {
        return static_cast<int32>(kMotionCompletionGraceMinMs);
    }

    const auto ratio_grace = static_cast<int32>(std::ceil(estimated_motion_time_ms * kMotionCompletionGraceRatio));
    const auto grace_ms = std::clamp(ratio_grace,
                                     static_cast<int32>(kMotionCompletionGraceMinMs),
                                     static_cast<int32>(kMotionCompletionGraceMaxMs));
    return base_timeout + grace_ms;
}

long ResolvePathTriggerTolerancePulse(float32 pulse_per_mm) noexcept {
    if (pulse_per_mm <= kTraceEpsilonMm) {
        return 0;
    }
    return std::max<long>(
        1L,
        static_cast<long>(std::llround(kPathTriggerPositionToleranceMm * pulse_per_mm)));
}

const char* ToString(Motion::Ports::InterpolationType type) noexcept {
    switch (type) {
        case Motion::Ports::InterpolationType::LINEAR:
            return "linear";
        case Motion::Ports::InterpolationType::CIRCULAR_CW:
            return "arc_cw";
        case Motion::Ports::InterpolationType::CIRCULAR_CCW:
            return "arc_ccw";
        case Motion::Ports::InterpolationType::SPIRAL:
            return "spiral";
        case Motion::Ports::InterpolationType::BUFFER_STOP:
            return "buffer_stop";
    }
    return "unknown";
}

std::optional<Point2D> ResolveInterpolationEndPoint(
    const Motion::Ports::InterpolationData& segment) noexcept {
    if (segment.positions.size() < 2U) {
        return std::nullopt;
    }
    return Point2D(segment.positions[0], segment.positions[1]);
}

float32 NormalizePositiveDegrees(float32 degrees) noexcept {
    auto normalized = std::fmod(degrees, 360.0f);
    if (normalized < 0.0f) {
        normalized += 360.0f;
    }
    return normalized;
}

float32 ComputeArcSweepDegrees(const Point2D& center,
                               const Point2D& start_point,
                               const Point2D& end_point,
                               bool clockwise) noexcept {
    const auto start_angle = static_cast<float32>(
        std::atan2(start_point.y - center.y, start_point.x - center.x) / kTraceRadiansPerDegree);
    const auto end_angle = static_cast<float32>(
        std::atan2(end_point.y - center.y, end_point.x - center.x) / kTraceRadiansPerDegree);
    auto sweep = clockwise
                     ? NormalizePositiveDegrees(start_angle - end_angle)
                     : NormalizePositiveDegrees(end_angle - start_angle);
    if (sweep <= kTraceEpsilonMm &&
        start_point.DistanceTo(end_point) <= kTraceFullCirclePositionToleranceMm) {
        sweep = 360.0f;
    }
    return sweep;
}

float32 ResolveControllerReadySegmentLengthMm(const Point2D& start_point,
                                              const Motion::Ports::InterpolationData& segment) noexcept {
    const auto end_point = ResolveInterpolationEndPoint(segment);
    if (!end_point.has_value()) {
        return 0.0f;
    }

    if (segment.type == Motion::Ports::InterpolationType::CIRCULAR_CW ||
        segment.type == Motion::Ports::InterpolationType::CIRCULAR_CCW) {
        const Point2D center(segment.center_x, segment.center_y);
        const auto radius = std::max(center.DistanceTo(start_point), center.DistanceTo(end_point.value()));
        if (radius <= kTraceEpsilonMm) {
            return start_point.DistanceTo(end_point.value());
        }

        const auto sweep_deg = ComputeArcSweepDegrees(
            center,
            start_point,
            end_point.value(),
            segment.type == Motion::Ports::InterpolationType::CIRCULAR_CW);
        if (sweep_deg <= kTraceEpsilonMm) {
            return start_point.DistanceTo(end_point.value());
        }
        return radius * sweep_deg * kTraceRadiansPerDegree;
    }

    return start_point.DistanceTo(end_point.value());
}

std::vector<float32> BuildPreviewPointArcLengths(
    const std::vector<Siligen::TrajectoryPoint>& preview_points) {
    std::vector<float32> arc_lengths;
    if (preview_points.empty()) {
        return arc_lengths;
    }

    arc_lengths.reserve(preview_points.size());
    float32 cumulative_length = 0.0f;
    arc_lengths.push_back(cumulative_length);
    for (std::size_t index = 1; index < preview_points.size(); ++index) {
        cumulative_length += preview_points[index - 1].position.DistanceTo(preview_points[index].position);
        arc_lengths.push_back(cumulative_length);
    }
    return arc_lengths;
}

std::size_t ResolvePreviewPointFloorIndex(const std::vector<float32>& arc_lengths, float32 distance_mm) noexcept {
    if (arc_lengths.empty()) {
        return 0U;
    }

    const auto it = std::upper_bound(arc_lengths.begin(), arc_lengths.end(), distance_mm + kTraceEpsilonMm);
    if (it == arc_lengths.begin()) {
        return 0U;
    }
    return static_cast<std::size_t>(std::distance(arc_lengths.begin(), it) - 1);
}

std::size_t ResolvePreviewPointCeilIndex(const std::vector<float32>& arc_lengths, float32 distance_mm) noexcept {
    if (arc_lengths.empty()) {
        return 0U;
    }

    const auto it = std::lower_bound(arc_lengths.begin(), arc_lengths.end(), distance_mm - kTraceEpsilonMm);
    if (it == arc_lengths.end()) {
        return arc_lengths.size() - 1U;
    }
    return static_cast<std::size_t>(std::distance(arc_lengths.begin(), it));
}

struct ControllerReadySegmentTraceRow {
    std::size_t segment_index = 0U;
    Motion::Ports::InterpolationType type = Motion::Ports::InterpolationType::LINEAR;
    Point2D start_point{};
    Point2D end_point{};
    Point2D preview_start_point{};
    Point2D preview_end_point{};
    std::size_t preview_start_index = 0U;
    std::size_t preview_end_index = 0U;
    uint32 preview_start_sequence_id = 0U;
    uint32 preview_end_sequence_id = 0U;
    float32 segment_length_mm = 0.0f;
    float32 s_start_mm = 0.0f;
    float32 s_end_mm = 0.0f;
    float32 preview_start_arc_mm = 0.0f;
    float32 preview_end_arc_mm = 0.0f;
    float32 start_match_mm = 0.0f;
    float32 end_match_mm = 0.0f;
};

struct ControllerReadyTracePlan {
    std::vector<ControllerReadySegmentTraceRow> rows;
    float32 controller_total_length_mm = 0.0f;
    float32 preview_total_length_mm = 0.0f;
};

ControllerReadyTracePlan BuildControllerReadyTracePlan(const DispensingExecutionPlan& plan) {
    ControllerReadyTracePlan trace_plan;
    if (plan.interpolation_segments.empty() || plan.interpolation_points.size() < 2U) {
        return trace_plan;
    }

    const auto arc_lengths = BuildPreviewPointArcLengths(plan.interpolation_points);
    if (arc_lengths.empty()) {
        return trace_plan;
    }

    trace_plan.preview_total_length_mm = arc_lengths.back();
    trace_plan.rows.reserve(plan.interpolation_segments.size());

    Point2D segment_start = plan.interpolation_points.front().position;
    float32 s_cursor = 0.0f;
    for (std::size_t segment_index = 0; segment_index < plan.interpolation_segments.size(); ++segment_index) {
        const auto& segment = plan.interpolation_segments[segment_index];
        const auto segment_end = ResolveInterpolationEndPoint(segment);
        if (!segment_end.has_value()) {
            continue;
        }

        const auto segment_length_mm = ResolveControllerReadySegmentLengthMm(segment_start, segment);
        const auto s_start_mm = s_cursor;
        const auto s_end_mm = s_start_mm + segment_length_mm;
        const auto preview_start_index = ResolvePreviewPointFloorIndex(arc_lengths, s_start_mm);
        auto preview_end_index = ResolvePreviewPointCeilIndex(arc_lengths, s_end_mm);
        if (preview_end_index < preview_start_index) {
            preview_end_index = preview_start_index;
        }

        ControllerReadySegmentTraceRow row;
        row.segment_index = segment_index;
        row.type = segment.type;
        row.start_point = segment_start;
        row.end_point = segment_end.value();
        row.preview_start_index = preview_start_index;
        row.preview_end_index = preview_end_index;
        row.preview_start_sequence_id = plan.interpolation_points[preview_start_index].sequence_id;
        row.preview_end_sequence_id = plan.interpolation_points[preview_end_index].sequence_id;
        row.preview_start_point = plan.interpolation_points[preview_start_index].position;
        row.preview_end_point = plan.interpolation_points[preview_end_index].position;
        row.segment_length_mm = segment_length_mm;
        row.s_start_mm = s_start_mm;
        row.s_end_mm = s_end_mm;
        row.preview_start_arc_mm = arc_lengths[preview_start_index];
        row.preview_end_arc_mm = arc_lengths[preview_end_index];
        row.start_match_mm = row.start_point.DistanceTo(row.preview_start_point);
        row.end_match_mm = row.end_point.DistanceTo(row.preview_end_point);
        trace_plan.rows.push_back(row);

        segment_start = segment_end.value();
        s_cursor = s_end_mm;
    }

    trace_plan.controller_total_length_mm = s_cursor;
    return trace_plan;
}

void LogControllerReadyTraceRow(const ControllerReadySegmentTraceRow& row) {
    SILIGEN_LOG_INFO_FMT_HELPER(
        "controller_ready_dispatch segment_index=%zu preview_point_range=[%zu,%zu] "
        "preview_sequence_range=[%u,%u] s_range_mm=[%.3f,%.3f] segment_length_mm=%.3f type=%s "
        "start=(%.3f,%.3f) end=(%.3f,%.3f) preview_start=(%.3f,%.3f) preview_end=(%.3f,%.3f) "
        "start_match_mm=%.3f end_match_mm=%.3f",
        row.segment_index,
        row.preview_start_index,
        row.preview_end_index,
        row.preview_start_sequence_id,
        row.preview_end_sequence_id,
        row.s_start_mm,
        row.s_end_mm,
        row.segment_length_mm,
        ToString(row.type),
        row.start_point.x,
        row.start_point.y,
        row.end_point.x,
        row.end_point.y,
        row.preview_start_point.x,
        row.preview_start_point.y,
        row.preview_end_point.x,
        row.preview_end_point.y,
        row.start_match_mm,
        row.end_match_mm);
}

Point2D ResolveFinalTargetPosition(const DispensingExecutionPlan& plan) noexcept {
    if (!plan.motion_trajectory.points.empty()) {
        return Point2D(plan.motion_trajectory.points.back().position);
    }
    if (!plan.interpolation_points.empty()) {
        return plan.interpolation_points.back().position;
    }
    if (!plan.interpolation_segments.empty()) {
        const auto& positions = plan.interpolation_segments.back().positions;
        if (positions.size() >= 2) {
            return Point2D(positions[0], positions[1]);
        }
    }
    return Point2D{};
}

float32 ResolveCompletionPositionToleranceMm(const std::shared_ptr<IConfigurationPort>& config_port) noexcept {
    float32 tolerance = kCompletionPositionToleranceFloorMm;
    if (!config_port) {
        return tolerance;
    }

    auto machine_result = config_port->GetMachineConfig();
    if (!machine_result.IsSuccess()) {
        return tolerance;
    }

    return std::max(machine_result.Value().positioning_tolerance, tolerance);
}

}  // namespace

DispensingProcessService::DispensingProcessService(std::shared_ptr<IValvePort> valve_port,
                                                   std::shared_ptr<IInterpolationPort> interpolation_port,
                                                   std::shared_ptr<IMotionStatePort> motion_state_port,
                                                   std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
                                                   std::shared_ptr<IConfigurationPort> config_port) noexcept
    : valve_port_(std::move(valve_port)),
      interpolation_port_(std::move(interpolation_port)),
      motion_state_port_(std::move(motion_state_port)),
      connection_port_(std::move(connection_port)),
      config_port_(std::move(config_port)) {}

Result<void> DispensingProcessService::ValidateHardwareConnection() noexcept {
    if (!connection_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "硬件连接端口未初始化", "DispensingProcessService"));
    }
    auto conn_info = connection_port_->ReadConnection();
    if (conn_info.IsError()) {
        return Result<void>::Failure(conn_info.GetError());
    }
    if (conn_info.Value().state != Siligen::Device::Contracts::State::DeviceConnectionState::Connected) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "硬件未连接", "DispensingProcessService"));
    }
    return Result<void>::Success();
}

Result<DispensingRuntimeParams> DispensingProcessService::BuildRuntimeParams(
    const DispensingRuntimeOverrides& overrides) noexcept {
    auto override_validation = overrides.Validate();
    if (override_validation.IsError()) {
        return Result<DispensingRuntimeParams>::Failure(override_validation.GetError());
    }

    DispensingRuntimeParams params{};
    float32 safety_limit_velocity = 0.0f;
    float32 default_velocity = 0.0f;

    if (config_port_) {
        auto config_result = config_port_->LoadConfiguration();
        if (config_result.IsSuccess()) {
            const auto& config = config_result.Value();

            if (config.machine.max_speed > 0.0f) {
                safety_limit_velocity = config.machine.max_speed;
            }
            if (config.machine.max_acceleration > 0.0f) {
                params.acceleration = config.machine.max_acceleration;
            }
            if (config.machine.pulse_per_mm > 0.0f) {
                params.pulse_per_mm = config.machine.pulse_per_mm;
            }

            if (config.dispensing.dot_diameter_target_mm > 0.0f) {
                float32 interval = config.dispensing.dot_diameter_target_mm;
                if (config.dispensing.dot_edge_gap_mm > 0.0f) {
                    interval += config.dispensing.dot_edge_gap_mm;
                }
                params.trigger_spatial_interval_mm = interval;
            }

            params.compensation_profile.open_comp_ms = config.dispensing.open_comp_ms;
            params.compensation_profile.close_comp_ms = config.dispensing.close_comp_ms;
            params.compensation_profile.retract_enabled = config.dispensing.retract_enabled;
            params.compensation_profile.corner_pulse_scale = config.dispensing.corner_pulse_scale;
            params.compensation_profile.curvature_speed_factor = config.dispensing.curvature_speed_factor;
            params.sample_dt = config.dispensing.trajectory_sample_dt;
            params.sample_ds = config.dispensing.trajectory_sample_ds;
        } else {
            SILIGEN_LOG_WARNING("读取系统配置失败: " + config_result.GetError().GetMessage());
        }

        auto valve_coord_result = config_port_->GetValveCoordinationConfig();
        if (valve_coord_result.IsSuccess()) {
            const auto& valve_cfg = valve_coord_result.Value();
            if (valve_cfg.dispensing_interval_ms > 0) {
                params.dispenser_interval_ms = static_cast<uint32>(valve_cfg.dispensing_interval_ms);
            }
            if (valve_cfg.dispensing_duration_ms > 0) {
                params.dispenser_duration_ms = static_cast<uint32>(valve_cfg.dispensing_duration_ms);
            }
            if (valve_cfg.valve_response_ms >= 0) {
                params.valve_response_ms = static_cast<float32>(valve_cfg.valve_response_ms);
            }
            if (valve_cfg.visual_margin_ms >= 0) {
                params.safety_margin_ms = static_cast<float32>(valve_cfg.visual_margin_ms);
            }
            if (valve_cfg.min_interval_ms >= 0) {
                params.min_interval_ms = static_cast<float32>(valve_cfg.min_interval_ms);
            }
        }
    }

    if (overrides.acceleration_mm_s2.has_value() && *overrides.acceleration_mm_s2 > 0.0f) {
        params.acceleration = *overrides.acceleration_mm_s2;
    }
    if (params.acceleration <= 0.0f) {
        params.acceleration = kDefaultAcceleration;
    }

    float32 velocity = default_velocity;
    if (overrides.dry_run) {
        if (overrides.dry_run_speed_mm_s.has_value() && *overrides.dry_run_speed_mm_s > 0.0f) {
            velocity = *overrides.dry_run_speed_mm_s;
        } else if (overrides.dispensing_speed_mm_s.has_value() && *overrides.dispensing_speed_mm_s > 0.0f) {
            velocity = *overrides.dispensing_speed_mm_s;
        }
    } else {
        if (overrides.dispensing_speed_mm_s.has_value() && *overrides.dispensing_speed_mm_s > 0.0f) {
            velocity = *overrides.dispensing_speed_mm_s;
        }
    }

    params.dispensing_velocity = velocity;

    if (overrides.rapid_speed_mm_s.has_value() && *overrides.rapid_speed_mm_s > 0.0f) {
        params.rapid_velocity = *overrides.rapid_speed_mm_s;
    } else {
        params.rapid_velocity = params.dispensing_velocity;
    }

    if (safety_limit_velocity > 0.0f) {
        params.dispensing_velocity = std::min(params.dispensing_velocity, safety_limit_velocity);
        params.rapid_velocity = std::min(params.rapid_velocity, safety_limit_velocity);
    }

    if (params.pulse_per_mm <= 0.0f) {
        params.pulse_per_mm = kDefaultPulsePerMm;
    }
    if (params.dispenser_interval_ms == 0) {
        params.dispenser_interval_ms = kDefaultDispenserIntervalMs;
    }
    if (params.dispenser_duration_ms == 0) {
        params.dispenser_duration_ms = kDefaultDispenserDurationMs;
    }
    if (params.trigger_spatial_interval_mm <= 0.0f) {
        params.trigger_spatial_interval_mm = kDefaultTriggerSpatialIntervalMm;
    }

    if (params.trigger_spatial_interval_mm <= 0.0f && params.dispensing_velocity > 0.0f) {
        params.trigger_spatial_interval_mm =
            params.dispensing_velocity * (static_cast<float32>(params.dispenser_interval_ms) / 1000.0f);
    }

    params.velocity_guard_enabled = overrides.velocity_guard_enabled;
    params.velocity_guard_ratio = overrides.velocity_guard_ratio;
    params.velocity_guard_abs_mm_s = overrides.velocity_guard_abs_mm_s;
    params.velocity_guard_min_expected_mm_s = overrides.velocity_guard_min_expected_mm_s;
    params.velocity_guard_grace_ms = overrides.velocity_guard_grace_ms;
    params.velocity_guard_interval_ms = overrides.velocity_guard_interval_ms;
    params.velocity_guard_max_consecutive = overrides.velocity_guard_max_consecutive;
    params.velocity_guard_stop_on_violation = overrides.velocity_guard_stop_on_violation;

    if (params.dispensing_velocity <= 0.0f) {
        return Result<DispensingRuntimeParams>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "未指定有效点胶速度", "DispensingProcessService"));
    }

    return Result<DispensingRuntimeParams>::Success(params);
}

Result<void> DispensingProcessService::ConfigureCoordinateSystem(const DispensingRuntimeParams& params) noexcept {
    if (!interpolation_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "插补端口未初始化", "DispensingProcessService"));
    }

    Motion::Ports::CoordinateSystemConfig config;
    config.dimension = 2;
    config.axis_map = {LogicalAxisId::X, LogicalAxisId::Y};
    config.max_velocity = params.dispensing_velocity;
    config.max_acceleration = params.acceleration;
    config.look_ahead_enabled = false;

    SILIGEN_LOG_INFO_FMT_HELPER(
        "坐标系配置: max_velocity=%.2fmm/s, acc=%.2fmm/s2",
        config.max_velocity,
        config.max_acceleration);

    return interpolation_port_->ConfigureCoordinateSystem(kCoordinateSystem, config);
}

Result<DispensingExecutionReport> DispensingProcessService::ExecuteProcess(
    const DispensingExecutionPlan& plan,
    const DispensingRuntimeParams& params,
    const DispensingExecutionOptions& options,
    std::atomic<bool>* stop_flag,
    std::atomic<bool>* pause_flag,
    std::atomic<bool>* pause_applied_flag,
    IDispensingExecutionObserver* observer) noexcept {
    auto conn_check = ValidateHardwareConnection();
    if (conn_check.IsError()) {
        return Result<DispensingExecutionReport>::Failure(conn_check.GetError());
    }

    auto coord_result = ConfigureCoordinateSystem(params);
    if (coord_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(coord_result.GetError());
    }

    CoordinateSystemStopGuard coord_guard(interpolation_port_, kCoordinateSystemMask);
    coord_guard.Arm();

    return ExecutePlanInternal(plan, params, options, stop_flag, pause_flag, pause_applied_flag, observer);
}

Result<DispensingExecutionReport> DispensingProcessService::ExecutePlanInternal(
    const DispensingExecutionPlan& plan,
    const DispensingRuntimeParams& params,
    const DispensingExecutionOptions& options,
    std::atomic<bool>* stop_flag,
    std::atomic<bool>* pause_flag,
    std::atomic<bool>* pause_applied_flag,
    IDispensingExecutionObserver* observer) noexcept {
    if (!interpolation_port_) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "插补端口未初始化", "DispensingProcessService"));
    }

    if (options.dispense_enabled && !valve_port_) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "点胶阀端口未初始化", "DispensingProcessService"));
    }

    DispensingController controller;
    ControllerConfig controller_config;
    controller_config.use_hardware_trigger = options.dispense_enabled;
    controller_config.spatial_interval_mm = params.trigger_spatial_interval_mm;
    controller_config.pulse_per_mm = params.pulse_per_mm;
    controller_config.acceleration_mm_s2 = params.acceleration;
    controller_config.trigger_distances_mm = plan.trigger_distances_mm;

    if (plan.trigger_interval_mm > 0.0f) {
        controller_config.spatial_interval_mm = plan.trigger_interval_mm;
    }

    auto trigger_result = plan.interpolation_points.empty()
                              ? controller.Build(plan.motion_trajectory, controller_config)
                              : controller.Build(plan.interpolation_points, controller_config);
    if (trigger_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(trigger_result.GetError());
    }

    auto trigger_output = trigger_result.Value();
    if (options.dispense_enabled && trigger_output.trigger_events.empty()) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                  "位置触发不可用，禁止回退为定时触发",
                  "DispensingProcessService"));
    }
    if (plan.interpolation_segments.empty()) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "插补数据为空", "DispensingProcessService"));
    }

    constexpr uint32 kMinBufferSpace = 20;
    constexpr uint32 kZeroSpaceWarmupBatch = kMinBufferSpace;
    constexpr auto kBufferPollInterval = std::chrono::milliseconds(5);
    constexpr auto kBufferStallTimeout = std::chrono::seconds(10);
    constexpr float32 kMinMovingVelocityMmS = 0.1f;

    auto clear_result = interpolation_port_->ClearInterpolationBuffer(kCoordinateSystem);
    if (clear_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(clear_result.GetError());
    }

    const auto& program = plan.interpolation_segments;
    const auto total_segments = program.size();
    size_t cursor = 0;
    const auto controller_ready_trace = BuildControllerReadyTracePlan(plan);

    if (!plan.interpolation_points.empty()) {
        SILIGEN_LOG_INFO_FMT_HELPER(
            "controller_ready_trace_summary preview_points=%zu controller_segments=%zu traced_segments=%zu "
            "controller_total_length_mm=%.3f preview_total_length_mm=%.3f length_delta_mm=%.3f",
            plan.interpolation_points.size(),
            total_segments,
            controller_ready_trace.rows.size(),
            controller_ready_trace.controller_total_length_mm,
            controller_ready_trace.preview_total_length_mm,
            std::fabs(controller_ready_trace.controller_total_length_mm -
                      controller_ready_trace.preview_total_length_mm));
    }

    auto query_buffer_space = [&]() -> Result<uint32> {
        auto crd_space_result = interpolation_port_->GetInterpolationBufferSpace(kCoordinateSystem);
        if (crd_space_result.IsError()) {
            return Result<uint32>::Failure(crd_space_result.GetError());
        }
        uint32 space = crd_space_result.Value();

        auto lookahead_space_result = interpolation_port_->GetLookAheadBufferSpace(kCoordinateSystem);
        if (lookahead_space_result.IsSuccess() && lookahead_space_result.Value() > 0) {
            space = std::min(space, lookahead_space_result.Value());
        }

        return Result<uint32>::Success(space);
    };

    auto send_batch = [&](size_t batch_size) -> Result<void> {
        for (size_t i = 0; i < batch_size && cursor < total_segments; ++i, ++cursor) {
            if (cursor < controller_ready_trace.rows.size()) {
                LogControllerReadyTraceRow(controller_ready_trace.rows[cursor]);
            } else if (!plan.interpolation_points.empty()) {
                SILIGEN_LOG_WARNING_FMT_HELPER(
                    "controller_ready_dispatch segment_index=%zu missing_trace=1 preview_points=%zu traced_segments=%zu",
                    cursor,
                    plan.interpolation_points.size(),
                    controller_ready_trace.rows.size());
            }
            auto add_result = interpolation_port_->AddInterpolationData(kCoordinateSystem, program[cursor]);
            if (add_result.IsError()) {
                return Result<void>::Failure(add_result.GetError());
            }
        }
        return Result<void>::Success();
    };

    auto resolve_batch_size = [&](uint32 space, size_t remaining_segments) -> size_t {
        if (space == 0U || remaining_segments == 0U) {
            return 0U;
        }

        const auto remaining = static_cast<uint32>(remaining_segments);
        if (remaining <= space) {
            return remaining_segments;
        }

        if (space > kMinBufferSpace) {
            return static_cast<size_t>(std::min<uint32>(remaining, space - kMinBufferSpace));
        }

        return 1U;
    };

    auto initial_space_result = query_buffer_space();
    if (initial_space_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(initial_space_result.GetError());
    }

    const auto initial_space = initial_space_result.Value();
    const auto initial_batch = [&]() -> size_t {
        if (total_segments == 0U) {
            return 0U;
        }

        if (initial_space == 0U) {
            return std::min<std::size_t>(total_segments, static_cast<size_t>(kZeroSpaceWarmupBatch));
        }

        return std::max<std::size_t>(1U, resolve_batch_size(initial_space, total_segments));
    }();

    if (total_segments > 0U && initial_space == 0U) {
        SILIGEN_LOG_INFO_FMT_HELPER(
            "预启动插补空间为0，执行 warmup 预装载: batch=%zu, total_segments=%zu",
            initial_batch,
            total_segments);
    }

    if (initial_batch > 0) {
        auto send_result = send_batch(initial_batch);
        if (send_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(send_result.GetError());
        }
    }

    auto initial_flush = interpolation_port_->FlushInterpolationData(kCoordinateSystem);
    if (initial_flush.IsError()) {
        return Result<DispensingExecutionReport>::Failure(initial_flush.GetError());
    }

    SupplyValveGuard supply_guard(valve_port_);
    if (options.dispense_enabled) {
        auto supply_result = supply_guard.Open();
        if (supply_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(supply_result.GetError());
        }
        auto stabilization_ms = SupplyStabilizationPolicy::Resolve(config_port_);
        if (stabilization_ms.IsError()) {
            return Result<DispensingExecutionReport>::Failure(stabilization_ms.GetError());
        }
        if (stabilization_ms.Value() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(stabilization_ms.Value()));
        }
    }

    DispenserOperationGuard dispenser_guard(valve_port_);
    if (options.dispense_enabled) {
        Ports::PositionTriggeredDispenserParams params_for_trigger;
        params_for_trigger.trigger_events = trigger_output.trigger_events;
        params_for_trigger.pulse_width_ms = params.dispenser_duration_ms;
        params_for_trigger.start_level = ResolveDispenserStartLevel();
        params_for_trigger.coordinate_system = kCoordinateSystem;
        params_for_trigger.position_tolerance_pulse = ResolvePathTriggerTolerancePulse(params.pulse_per_mm);
        DispenseCompensationService compensation_service;
        params_for_trigger = compensation_service.ApplyPositionCompensation(params_for_trigger,
                                                                            params.compensation_profile,
                                                                            false);
        auto start_result = dispenser_guard.Start(params_for_trigger);
        if (start_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(start_result.GetError());
        }
    }

    auto override_result = interpolation_port_->SetCoordinateSystemVelocityOverride(kCoordinateSystem, 100.0f);
    if (override_result.IsError()) {
        SILIGEN_LOG_WARNING("设置坐标系速度倍率失败: " + override_result.GetError().GetMessage());
    }

    auto start_result = interpolation_port_->StartCoordinateSystemMotion(kCoordinateSystemMask);
    if (start_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(start_result.GetError());
    }

    MotionObserverGuard observer_guard(observer);
    observer_guard.Start();
    PublishProgress(observer, 0U, static_cast<uint32>(total_segments));

    auto last_progress = std::chrono::steady_clock::now();
    while (cursor < total_segments) {
        auto pause_result =
            WaitWhilePaused(stop_flag, pause_flag, pause_applied_flag, options.dispense_enabled, observer);
        if (pause_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(pause_result.GetError());
        }

        if (IsStopRequested(stop_flag)) {
            SILIGEN_LOG_INFO("DXF执行循环检测到停止请求，中止执行");
            return Result<DispensingExecutionReport>::Failure(
                Error(ErrorCode::INVALID_STATE, "用户请求停止", "DispensingProcessService"));
        }

        auto space_result = query_buffer_space();
        if (space_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(space_result.GetError());
        }

        auto progress_status_result = interpolation_port_->GetCoordinateSystemStatus(kCoordinateSystem);
        if (progress_status_result.IsSuccess()) {
            const auto& progress_status = progress_status_result.Value();
            auto executed_segments = TryResolveExecutedSegmentsForProgress(
                progress_status,
                static_cast<uint32>(total_segments));
            if (executed_segments.has_value()) {
                PublishProgress(observer, executed_segments.value(), static_cast<uint32>(total_segments));
            }
        }

        const auto space = space_result.Value();
        if (space == 0U) {
            if (IsStopRequested(stop_flag)) {
                SILIGEN_LOG_INFO("缓冲区等待期间检测到停止请求，中止执行");
                return Result<DispensingExecutionReport>::Failure(
                    Error(ErrorCode::INVALID_STATE, "用户请求停止", "DispensingProcessService"));
            }

            auto now = std::chrono::steady_clock::now();
            auto status_result = interpolation_port_->GetCoordinateSystemStatus(kCoordinateSystem);
            if (status_result.IsSuccess()) {
                const auto& status = status_result.Value();
                if (status.is_moving &&
                    (status.current_velocity > kMinMovingVelocityMmS ||
                     status.current_velocity < -kMinMovingVelocityMmS)) {
                    last_progress = now;
                }
            }
            if (now - last_progress > kBufferStallTimeout) {
                uint32 crd_space = space;
                uint32 lookahead_space = 0;
                auto crd_space_result = interpolation_port_->GetInterpolationBufferSpace(kCoordinateSystem);
                if (crd_space_result.IsSuccess()) {
                    crd_space = crd_space_result.Value();
                }
                auto lookahead_space_result = interpolation_port_->GetLookAheadBufferSpace(kCoordinateSystem);
                if (lookahead_space_result.IsSuccess()) {
                    lookahead_space = lookahead_space_result.Value();
                }
                auto stall_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - last_progress).count();
                if (status_result.IsSuccess()) {
                    const auto& status = status_result.Value();
                    SILIGEN_LOG_WARNING_FMT_HELPER(
                        "插补缓冲区无可用空间超时: waited_ms=%lld, space=%u, crd_space=%u, lookahead_space=%u, "
                        "state=%d, moving=%d, remaining=%u, vel=%.3f, raw_status=%d, raw_segment=%d, mc_ret=%d",
                        static_cast<long long>(stall_ms),
                        space,
                        crd_space,
                        lookahead_space,
                        static_cast<int>(status.state),
                        status.is_moving ? 1 : 0,
                        status.remaining_segments,
                        status.current_velocity,
                        status.raw_status_word,
                        status.raw_segment,
                        status.mc_status_ret);
                } else {
                    SILIGEN_LOG_WARNING("插补缓冲区无可用空间超时且获取坐标系状态失败: " +
                                        status_result.GetError().GetMessage());
                }
                return Result<DispensingExecutionReport>::Failure(
                    Error(ErrorCode::MOTION_TIMEOUT, "插补缓冲区长期无可用空间", "DispensingProcessService"));
            }
            std::this_thread::sleep_for(kBufferPollInterval);
            continue;
        }

        const auto batch_size = resolve_batch_size(space, total_segments - cursor);
        if (batch_size == 0) {
            std::this_thread::sleep_for(kBufferPollInterval);
            continue;
        }

        auto send_result = send_batch(batch_size);
        if (send_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(send_result.GetError());
        }

        auto flush_result = interpolation_port_->FlushInterpolationData(kCoordinateSystem);
        if (flush_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(flush_result.GetError());
        }

        last_progress = std::chrono::steady_clock::now();
    }

    const auto final_target_position = ResolveFinalTargetPosition(plan);
    const auto position_tolerance_mm = ResolveCompletionPositionToleranceMm(config_port_);
    const auto timeout_ms = BuildMotionCompletionTimeoutMs(trigger_output.estimated_motion_time_ms);

    SILIGEN_LOG_INFO_FMT_HELPER(
        "等待运动完成: timeout_ms=%d, estimated_motion_time_ms=%.1f, final_target=(%.3f, %.3f), "
        "position_tolerance_mm=%.3f, total_segments=%zu",
        timeout_ms,
        trigger_output.estimated_motion_time_ms,
        final_target_position.x,
        final_target_position.y,
        position_tolerance_mm,
        total_segments);

    auto wait_result = WaitForMotionComplete(
        timeout_ms,
        stop_flag,
        pause_flag,
        pause_applied_flag,
        &final_target_position,
        position_tolerance_mm,
        static_cast<uint32>(total_segments),
        options.dispense_enabled,
        observer);
    if (wait_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(wait_result.GetError());
    }

    PublishProgress(observer, static_cast<uint32>(total_segments), static_cast<uint32>(total_segments));

    DispensingExecutionReport report;
    report.executed_segments = static_cast<uint32>(total_segments);
    report.total_distance = plan.total_length_mm > 0.0f ? plan.total_length_mm : plan.motion_trajectory.total_length;

    return Result<DispensingExecutionReport>::Success(report);
}

Result<void> DispensingProcessService::WaitForMotionComplete(int32 timeout_ms,
                                                             std::atomic<bool>* stop_flag,
                                                             std::atomic<bool>* pause_flag,
                                                             std::atomic<bool>* pause_applied_flag,
                                                             const Point2D* final_target_position,
                                                             float32 position_tolerance_mm,
                                                             uint32 total_segments,
                                                             bool dispense_enabled,
                                                             IDispensingExecutionObserver* observer) noexcept {
    if (!interpolation_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "插补端口未初始化", "DispensingProcessService"));
    }

    auto start = std::chrono::steady_clock::now();
    auto settled_since = std::chrono::steady_clock::time_point{};
    bool settle_tracking = false;
    bool has_initial_position = false;
    Point2D initial_position{};
    if (motion_state_port_) {
        auto initial_position_result = motion_state_port_->GetCurrentPosition();
        if (initial_position_result.IsSuccess()) {
            initial_position = initial_position_result.Value();
            has_initial_position = true;
        }
    }
    bool has_last_position = false;
    Point2D last_position{};
    bool observed_motion = false;
    bool observed_progress_signal = false;
    while (true) {
        auto pause_result = WaitWhilePaused(stop_flag, pause_flag, pause_applied_flag, dispense_enabled, observer);
        if (pause_result.IsError()) {
            return pause_result;
        }

        if (IsStopRequested(stop_flag)) {
            StopExecution(stop_flag, pause_flag);
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "执行已被取消", "DispensingProcessService"));
        }

        auto status_result = interpolation_port_->GetCoordinateSystemStatus(kCoordinateSystem);
        if (status_result.IsError()) {
            return Result<void>::Failure(status_result.GetError());
        }

        const auto& status = status_result.Value();
        auto executed_segments = TryResolveExecutedSegmentsForProgress(status, total_segments);
        if (executed_segments.has_value()) {
            PublishProgress(observer, executed_segments.value(), total_segments);
            if (executed_segments.value() > 0U) {
                observed_progress_signal = true;
            }
        }
        const bool coord_completed = IsCoordinateSystemCompleted(status);

        if (final_target_position && motion_state_port_) {
            auto now = std::chrono::steady_clock::now();
            auto position_result = motion_state_port_->GetCurrentPosition();
            auto x_velocity_result = motion_state_port_->GetAxisVelocity(LogicalAxisId::X);
            auto y_velocity_result = motion_state_port_->GetAxisVelocity(LogicalAxisId::Y);

            if (position_result.IsSuccess() && x_velocity_result.IsSuccess() && y_velocity_result.IsSuccess()) {
                const auto& current_position = position_result.Value();
                if (!has_last_position) {
                    last_position = current_position;
                    has_last_position = true;
                }
                const auto distance_to_target = current_position.DistanceTo(*final_target_position);
                const auto position_delta = current_position.DistanceTo(last_position);
                const auto x_velocity = std::fabs(x_velocity_result.Value());
                const auto y_velocity = std::fabs(y_velocity_result.Value());
                const bool within_target = distance_to_target <= position_tolerance_mm;
                const bool low_velocity =
                    x_velocity <= kCompletionVelocityToleranceMmS &&
                    y_velocity <= kCompletionVelocityToleranceMmS;
                if ((has_initial_position && current_position.DistanceTo(initial_position) > position_tolerance_mm) ||
                    x_velocity > kCompletionVelocityToleranceMmS ||
                    y_velocity > kCompletionVelocityToleranceMmS) {
                    observed_motion = true;
                }

                if (coord_completed && low_velocity && within_target) {
                    if (!settle_tracking) {
                        settled_since = now;
                        settle_tracking = true;
                    } else if (now - settled_since >= kCompletionSettleWindow) {
                        SILIGEN_LOG_INFO_FMT_HELPER(
                            "运动完成采用权威反馈收敛: pos=(%.3f, %.3f), target=(%.3f, %.3f), "
                            "distance=%.4f, delta=%.4f, vx=%.4f, vy=%.4f, observed_motion=%d, "
                            "observed_progress=%d, raw_status=%d, raw_segment=%d",
                            current_position.x,
                            current_position.y,
                            final_target_position->x,
                            final_target_position->y,
                            distance_to_target,
                            position_delta,
                            x_velocity,
                            y_velocity,
                            observed_motion ? 1 : 0,
                            observed_progress_signal ? 1 : 0,
                            status.raw_status_word,
                            status.raw_segment);
                        PublishProgress(observer, total_segments, total_segments);
                        return Result<void>::Success();
                    }
                } else {
                    settle_tracking = false;
                }
                last_position = current_position;
            } else {
                settle_tracking = false;
            }
        } else if (coord_completed) {
            PublishProgress(observer, total_segments, total_segments);
            return Result<void>::Success();
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (timeout_ms > 0 && elapsed > timeout_ms) {
            int raw_status = 0;
            int raw_segment = 0;
            int mc_ret = 0;
            int state = -1;
            int coord_completed_flag = 0;
            int is_moving = 0;
            unsigned remaining_segments = 0;
            float current_velocity = 0.0f;
            float current_x = 0.0f;
            float current_y = 0.0f;
            float x_velocity = 0.0f;
            float y_velocity = 0.0f;
            float distance_to_target = -1.0f;
            bool has_position_snapshot = false;
            std::string x_feedback_source = "unknown";
            std::string y_feedback_source = "unknown";
            float x_profile_position = 0.0f;
            float x_encoder_position = 0.0f;
            float y_profile_position = 0.0f;
            float y_encoder_position = 0.0f;
            float x_profile_velocity = 0.0f;
            float x_encoder_velocity = 0.0f;
            float y_profile_velocity = 0.0f;
            float y_encoder_velocity = 0.0f;
            int x_profile_position_ret = 0;
            int x_encoder_position_ret = 0;
            int y_profile_position_ret = 0;
            int y_encoder_position_ret = 0;
            int x_profile_velocity_ret = 0;
            int x_encoder_velocity_ret = 0;
            int y_profile_velocity_ret = 0;
            int y_encoder_velocity_ret = 0;

            auto timeout_status_result = interpolation_port_->GetCoordinateSystemStatus(kCoordinateSystem);
            if (timeout_status_result.IsSuccess()) {
                const auto& timeout_status = timeout_status_result.Value();
                raw_status = timeout_status.raw_status_word;
                raw_segment = timeout_status.raw_segment;
                mc_ret = timeout_status.mc_status_ret;
                state = static_cast<int>(timeout_status.state);
                coord_completed_flag = IsCoordinateSystemCompleted(timeout_status) ? 1 : 0;
                is_moving = timeout_status.is_moving ? 1 : 0;
                remaining_segments = timeout_status.remaining_segments;
                current_velocity = timeout_status.current_velocity;
            }

            if (motion_state_port_) {
                auto timeout_position_result = motion_state_port_->GetCurrentPosition();
                auto timeout_x_velocity_result = motion_state_port_->GetAxisVelocity(LogicalAxisId::X);
                auto timeout_y_velocity_result = motion_state_port_->GetAxisVelocity(LogicalAxisId::Y);
                if (timeout_position_result.IsSuccess()) {
                    const auto& timeout_position = timeout_position_result.Value();
                    current_x = timeout_position.x;
                    current_y = timeout_position.y;
                    has_position_snapshot = true;
                    if (final_target_position) {
                        distance_to_target = timeout_position.DistanceTo(*final_target_position);
                    }
                }
                if (timeout_x_velocity_result.IsSuccess()) {
                    x_velocity = timeout_x_velocity_result.Value();
                }
                if (timeout_y_velocity_result.IsSuccess()) {
                    y_velocity = timeout_y_velocity_result.Value();
                }
                auto timeout_x_status_result = motion_state_port_->GetAxisStatus(LogicalAxisId::X);
                if (timeout_x_status_result.IsSuccess()) {
                    const auto& x_status = timeout_x_status_result.Value();
                    x_feedback_source = x_status.selected_feedback_source;
                    x_profile_position = x_status.profile_position_mm;
                    x_encoder_position = x_status.encoder_position_mm;
                    x_profile_velocity = x_status.profile_velocity_mm_s;
                    x_encoder_velocity = x_status.encoder_velocity_mm_s;
                    x_profile_position_ret = x_status.profile_position_ret;
                    x_encoder_position_ret = x_status.encoder_position_ret;
                    x_profile_velocity_ret = x_status.profile_velocity_ret;
                    x_encoder_velocity_ret = x_status.encoder_velocity_ret;
                }
                auto timeout_y_status_result = motion_state_port_->GetAxisStatus(LogicalAxisId::Y);
                if (timeout_y_status_result.IsSuccess()) {
                    const auto& y_status = timeout_y_status_result.Value();
                    y_feedback_source = y_status.selected_feedback_source;
                    y_profile_position = y_status.profile_position_mm;
                    y_encoder_position = y_status.encoder_position_mm;
                    y_profile_velocity = y_status.profile_velocity_mm_s;
                    y_encoder_velocity = y_status.encoder_velocity_mm_s;
                    y_profile_position_ret = y_status.profile_position_ret;
                    y_encoder_position_ret = y_status.encoder_position_ret;
                    y_profile_velocity_ret = y_status.profile_velocity_ret;
                    y_encoder_velocity_ret = y_status.encoder_velocity_ret;
                }
            }

            SILIGEN_LOG_ERROR_FMT_HELPER(
                "等待运动完成超时: elapsed_ms=%lld, observed_motion=%d, observed_progress=%d, "
                "coord_completed=%d, state=%d, moving=%d, remaining=%u, coord_vel=%.4f, "
                "raw_status=%d, raw_segment=%d, mc_ret=%d, pos=(%.3f, %.3f), axis_vel=(%.4f, %.4f), "
                "target=(%.3f, %.3f), distance=%.4f, has_pos=%d",
                static_cast<long long>(elapsed),
                observed_motion ? 1 : 0,
                observed_progress_signal ? 1 : 0,
                coord_completed_flag,
                state,
                is_moving,
                remaining_segments,
                current_velocity,
                raw_status,
                raw_segment,
                mc_ret,
                current_x,
                current_y,
                x_velocity,
                y_velocity,
                final_target_position ? final_target_position->x : 0.0f,
                final_target_position ? final_target_position->y : 0.0f,
                distance_to_target,
                has_position_snapshot ? 1 : 0);
            SILIGEN_LOG_ERROR_FMT_HELPER(
                "等待运动完成超时轴反馈: "
                "X[source=%s prf_pos=%.3f enc_pos=%.3f prf_vel=%.4f enc_vel=%.4f ret=(%d,%d,%d,%d)] "
                "Y[source=%s prf_pos=%.3f enc_pos=%.3f prf_vel=%.4f enc_vel=%.4f ret=(%d,%d,%d,%d)]",
                x_feedback_source.c_str(),
                x_profile_position,
                x_encoder_position,
                x_profile_velocity,
                x_encoder_velocity,
                x_profile_position_ret,
                x_encoder_position_ret,
                x_profile_velocity_ret,
                x_encoder_velocity_ret,
                y_feedback_source.c_str(),
                y_profile_position,
                y_encoder_position,
                y_profile_velocity,
                y_encoder_velocity,
                y_profile_position_ret,
                y_encoder_position_ret,
                y_profile_velocity_ret,
                y_encoder_velocity_ret);
            return Result<void>::Failure(
                Error(ErrorCode::MOTION_TIMEOUT, "等待运动完成超时", "DispensingProcessService"));
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(kStatusPollIntervalMs));
    }
}

Result<void> DispensingProcessService::WaitWhilePaused(std::atomic<bool>* stop_flag,
                                                       std::atomic<bool>* pause_flag,
                                                       std::atomic<bool>* pause_applied_flag,
                                                       bool dispense_enabled,
                                                       IDispensingExecutionObserver* observer) noexcept {
    if (!pause_flag || !pause_flag->load()) {
        return Result<void>::Success();
    }

    if (!interpolation_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "插补端口未初始化", "DispensingProcessService"));
    }

    bool pause_entered = false;
    while (pause_flag->load()) {
        if (!pause_entered) {
            auto override_result = interpolation_port_->SetCoordinateSystemVelocityOverride(kCoordinateSystem, 0.0f);
            if (override_result.IsError()) {
                return Result<void>::Failure(override_result.GetError());
            }

            if (dispense_enabled && valve_port_) {
                auto pause_result = valve_port_->PauseDispenser();
                if (pause_result.IsError()) {
                    SILIGEN_LOG_WARNING("暂停点胶阀失败: " + pause_result.GetError().GetMessage());
                }
            }

            if (pause_applied_flag) {
                pause_applied_flag->store(true);
            }
            PublishPauseState(observer, true);
            pause_entered = true;
        }

        if (IsStopRequested(stop_flag)) {
            StopExecution(stop_flag, pause_flag);
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "执行已被取消", "DispensingProcessService"));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kStatusPollIntervalMs));
    }

    if (pause_entered) {
        if (dispense_enabled && valve_port_) {
            auto resume_result = valve_port_->ResumeDispenser();
            if (resume_result.IsError()) {
                SILIGEN_LOG_WARNING("恢复点胶阀失败: " + resume_result.GetError().GetMessage());
            }
        }

        auto override_result = interpolation_port_->SetCoordinateSystemVelocityOverride(kCoordinateSystem, 100.0f);
        if (override_result.IsError()) {
            return Result<void>::Failure(override_result.GetError());
        }

        if (pause_applied_flag) {
            pause_applied_flag->store(false);
        }
        PublishPauseState(observer, false);
    }

    return Result<void>::Success();
}

void DispensingProcessService::StopExecution(std::atomic<bool>* stop_flag, std::atomic<bool>* pause_flag) noexcept {
    if (stop_flag) {
        stop_flag->store(true);
    }
    if (pause_flag) {
        pause_flag->store(false);
    }
    if (interpolation_port_) {
        auto result = interpolation_port_->StopCoordinateSystemMotion(kCoordinateSystemMask);
        if (result.IsError()) {
            SILIGEN_LOG_WARNING("停止坐标系运动失败: " + result.GetError().GetMessage());
        }
    }
    if (valve_port_) {
        auto result = valve_port_->StopDispenser();
        if (result.IsError()) {
            SILIGEN_LOG_WARNING("停止点胶阀失败: " + result.GetError().GetMessage());
        }
    }
}

short DispensingProcessService::ResolveDispenserStartLevel() const noexcept {
    if (!config_port_) {
        return 0;
    }
    auto config_result = config_port_->GetDispenserValveConfig();
    if (config_result.IsError()) {
        SILIGEN_LOG_WARNING("读取点胶阀配置失败: " + config_result.GetError().GetMessage());
        return 0;
    }
    const auto& dispenser_config = config_result.Value();
    const bool active_high = (dispenser_config.pulse_type == 0);
    return static_cast<short>(active_high ? 0 : 1);
}

bool DispensingProcessService::IsStopRequested(const std::atomic<bool>* stop_flag) const noexcept {
    return stop_flag && stop_flag->load();
}

void DispensingProcessService::PublishProgress(IDispensingExecutionObserver* observer,
                                               uint32 executed_segments,
                                               uint32 total_segments) const noexcept {
    if (!observer) {
        return;
    }
    observer->OnProgress(executed_segments, total_segments);
}

void DispensingProcessService::PublishPauseState(IDispensingExecutionObserver* observer, bool paused) const noexcept {
    if (!observer) {
        return;
    }
    observer->OnPauseStateChanged(paused);
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
