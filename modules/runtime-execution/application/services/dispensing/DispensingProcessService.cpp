#define MODULE_NAME "DispensingProcessService"

#include "DispensingProcessService.h"

#include "SupplyStabilizationPolicy.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/TrajectoryTriggerUtils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace Siligen::RuntimeExecution::Application::Services::Dispensing {

namespace Motion = Siligen::Domain::Motion;
namespace Ports = Siligen::Domain::Dispensing::Ports;
namespace ValueObjects = Siligen::Domain::Dispensing::ValueObjects;

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
constexpr int32 kDefaultCompareAxisMask = 0x03;
constexpr uint32 kDefaultDispenserIntervalMs = 100;
constexpr uint32 kDefaultDispenserDurationMs = 100;
constexpr float32 kDefaultTriggerSpatialIntervalMm = 3.0f;
constexpr uint32 kStatusPollIntervalMs = 50;
constexpr uint32 kMotionCompletionGraceMinMs = 5000;
constexpr uint32 kMotionCompletionGraceMaxMs = 30000;
constexpr float32 kMotionCompletionGraceRatio = 0.15f;
constexpr float32 kCompletionVelocityToleranceMmS = 0.5f;
constexpr float32 kCompletionPositionToleranceFloorMm = 0.2f;
constexpr float32 kDefaultComparePulseWidthUsPerMs = 1000.0f;
constexpr auto kCompletionSettleWindow = std::chrono::milliseconds(250);
constexpr auto kFormalPathClearRetryTimeout = std::chrono::milliseconds(8000);
constexpr auto kStopDrainTimeout = std::chrono::milliseconds(2000);
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

bool IsCoordinateSystemStopDrained(const Motion::Ports::CoordinateSystemStatus& status) {
    if (status.state == Motion::Ports::CoordinateSystemState::IDLE) {
        return true;
    }

    const bool coord_velocity_idle = std::fabs(status.current_velocity) <= kCompletionVelocityToleranceMmS;
    const bool segments_drained = status.remaining_segments == 0U || status.raw_segment <= 0;
    return !status.is_moving && coord_velocity_idle && segments_drained;
}

bool IsCoordinateSystemSettledForFormalPathClear(const Motion::Ports::CoordinateSystemStatus& status) {
    if (status.state == Motion::Ports::CoordinateSystemState::ERROR_STATE) {
        return false;
    }

    const bool coord_velocity_idle = std::fabs(status.current_velocity) <= kCompletionVelocityToleranceMmS;
    const bool segments_drained = status.remaining_segments == 0U || status.raw_segment <= 0;
    return !status.is_moving && coord_velocity_idle && segments_drained;
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

bool IsProfileCompareTerminalCompleted(const Ports::ProfileCompareStatus& status,
                                       uint32 expected_trigger_count) noexcept {
    return status.completed_trigger_count == expected_trigger_count && status.Completed();
}

std::string BuildProfileCompareStatusLogContext(
    uint32 expected_trigger_count,
    const Ports::ProfileCompareStatus& status) {
    std::string context =
        "expected_trigger_count=" + std::to_string(expected_trigger_count) +
        ", completed_trigger_count=" + std::to_string(status.completed_trigger_count) +
        ", remaining_trigger_count=" + std::to_string(status.remaining_trigger_count) +
        ", armed=" + std::to_string(status.armed ? 1 : 0);

    const bool has_hardware_snapshot =
        status.submitted_future_compare_count > 0U ||
        status.hardware_status_word != 0U ||
        status.hardware_remain_data_1 != 0U ||
        status.hardware_remain_data_2 != 0U ||
        status.hardware_remain_space_1 != 0U ||
        status.hardware_remain_space_2 != 0U;
    if (!has_hardware_snapshot) {
        return context;
    }

    context +=
        ", submitted_future_compare_count=" + std::to_string(status.submitted_future_compare_count) +
        ", hw_status=" + std::to_string(status.hardware_status_word) +
        ", remain_data_1=" + std::to_string(status.hardware_remain_data_1) +
        ", remain_data_2=" + std::to_string(status.hardware_remain_data_2) +
        ", remain_space_1=" + std::to_string(status.hardware_remain_space_1) +
        ", remain_space_2=" + std::to_string(status.hardware_remain_space_2);
    return context;
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

class ProfileCompareGuard {
   public:
    explicit ProfileCompareGuard(std::shared_ptr<Ports::IProfileComparePort> port)
        : port_(std::move(port)) {}

    Result<void> Arm(const Ports::ProfileCompareArmRequest& request) {
        if (!port_) {
            return Result<void>::Failure(
                Error(ErrorCode::PORT_NOT_INITIALIZED, "profile compare 端口未初始化"));
        }
        auto result = port_->ArmProfileCompare(request);
        if (result.IsSuccess()) {
            armed_ = true;
        }
        return result;
    }

    Result<Ports::ProfileCompareStatus> GetStatus() const {
        if (!port_) {
            return Result<Ports::ProfileCompareStatus>::Failure(
                Error(ErrorCode::PORT_NOT_INITIALIZED, "profile compare 端口未初始化"));
        }
        return port_->GetProfileCompareStatus();
    }

    Result<void> Disarm() {
        if (!port_ || !armed_) {
            return Result<void>::Success();
        }
        auto result = port_->DisarmProfileCompare();
        if (result.IsSuccess()) {
            armed_ = false;
        }
        return result;
    }

    ~ProfileCompareGuard() {
        if (!port_ || !armed_) {
            return;
        }
        auto result = port_->DisarmProfileCompare();
        if (!result.IsSuccess()) {
            SILIGEN_LOG_WARNING("关闭 profile compare 失败: " + result.GetError().GetMessage());
        }
    }

    ProfileCompareGuard(const ProfileCompareGuard&) = delete;
    ProfileCompareGuard& operator=(const ProfileCompareGuard&) = delete;

   private:
    std::shared_ptr<Ports::IProfileComparePort> port_;
    bool armed_ = false;
};

class DispenserOperationGuard {
   public:
    explicit DispenserOperationGuard(std::shared_ptr<IValvePort> port)
        : port_(std::move(port)), started_(false) {}

    Result<void> StartTimed(const Ports::DispenserValveParams& params) {
        auto result = port_->StartDispenser(params);
        if (result.IsSuccess()) {
            started_ = true;
            return Result<void>::Success();
        }
        return Result<void>::Failure(result.GetError());
    }

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

bool ShouldExecuteStationaryPointShot(const DispensingExecutionPlan& plan) noexcept {
    return plan.RequiresStationaryPointShot();
}

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

int32 ResolveEstimatedMotionTimeMs(const DispensingExecutionPlan& plan) noexcept {
    if (plan.motion_trajectory.total_time > 0.0f) {
        return static_cast<int32>(std::ceil(plan.motion_trajectory.total_time * 1000.0f));
    }

    if (plan.interpolation_points.size() >= 2U) {
        const auto terminal_timestamp = plan.interpolation_points.back().timestamp;
        if (terminal_timestamp > 0.0f) {
            return static_cast<int32>(std::ceil(terminal_timestamp * 1000.0f));
        }
    }

    return 0;
}

float32 ResolveAuthoritativeExecutionTimeMs(const DispensingExecutionPlan& plan,
                                            float32 execution_nominal_time_s) noexcept {
    const auto plan_estimated_time_ms = static_cast<float32>(ResolveEstimatedMotionTimeMs(plan));
    const auto nominal_execution_time_ms =
        execution_nominal_time_s > 0.0f ? execution_nominal_time_s * 1000.0f : 0.0f;
    return std::max(plan_estimated_time_ms, nominal_execution_time_ms);
}

Result<Ports::ProfileCompareArmRequest> BuildProfileCompareArmRequest(
    const Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionSpan& span,
    short start_level) {
    if (span.Empty()) {
        return Result<Ports::ProfileCompareArmRequest>::Failure(
            Error(ErrorCode::INVALID_STATE,
                  "profile_compare execution span 为空",
                  "DispensingProcessService"));
    }

    Ports::ProfileCompareArmRequest request;
    request.expected_trigger_count = span.expected_trigger_count;
    request.start_level = start_level;
    request.compare_source_axis = span.compare_source_axis;
    request.start_boundary_trigger_count = span.start_boundary_trigger_count;
    request.pulse_width_us = span.pulse_width_us;
    request.compare_positions_pulse = span.compare_positions_pulse;

    if (!request.IsValid()) {
        return Result<Ports::ProfileCompareArmRequest>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "profile_compare execution span arm request 无效: " + request.GetValidationError(),
                  "DispensingProcessService"));
    }

    SILIGEN_LOG_INFO_FMT_HELPER(
        "profile_compare_request_summary span_index=%u compare_source_axis=%d business_trigger_count=%u "
        "start_boundary_trigger_count=%u future_compare_count=%zu pulse_width_us=%u",
        span.span_index,
        request.compare_source_axis,
        request.expected_trigger_count,
        request.start_boundary_trigger_count,
        request.compare_positions_pulse.size(),
        request.pulse_width_us);
    return Result<Ports::ProfileCompareArmRequest>::Success(request);
}

struct MaterializedProfileCompareSpan {
    Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionSpan schedule_span;
    std::vector<Siligen::TrajectoryPoint> trajectory_points;
    std::vector<Motion::Ports::InterpolationData> controller_segments;
    Point2D motion_start_position_mm{};
    Point2D final_target_position_mm{};
    float32 estimated_duration_s = 0.0f;
    uint32 global_progress_begin = 0U;
    uint32 global_progress_end = 0U;
};

struct ProfileCompareActualTraceCollector {
    std::vector<ValueObjects::ProfileCompareActualTraceItem> actual_trace;
    std::vector<ValueObjects::ProfileCompareTraceabilityMismatch> mismatches;
    std::string verdict = "passed";
    std::string verdict_reason;
    bool strict_one_to_one_proven = true;
    std::size_t next_expected_index = 0U;

    Result<void> ObserveStatus(
        uint32 cycle_index,
        const MaterializedProfileCompareSpan& span,
        const ValueObjects::ProfileCompareExpectedTrace& expected_trace,
        const Ports::ProfileCompareStatus& status) {
        const auto expected_total = expected_trace.items.size();
        if (expected_total == 0U) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "profile_compare actual trace 缺少 expected trace",
                      "DispensingProcessService"));
        }
        if (status.completed_trigger_count > span.schedule_span.expected_trigger_count) {
            ValueObjects::ProfileCompareTraceabilityMismatch mismatch;
            mismatch.cycle_index = cycle_index;
            mismatch.code = "span_completed_trigger_overflow";
            mismatch.message = "span completed_trigger_count exceeded span expected_trigger_count";
            mismatches.push_back(std::move(mismatch));
            verdict = "failed";
            verdict_reason = "span completed_trigger_count exceeded span expected_trigger_count";
            strict_one_to_one_proven = false;
            return Result<void>::Failure(
                Error(ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                      "profile_compare span completed_trigger_count exceeded span expected_trigger_count",
                      "DispensingProcessService"));
        }

        const auto target_actual_size =
            static_cast<std::size_t>(span.schedule_span.trigger_begin_index) +
            static_cast<std::size_t>(status.completed_trigger_count);

        while (actual_trace.size() < target_actual_size) {
            if (next_expected_index >= expected_total) {
                ValueObjects::ProfileCompareTraceabilityMismatch mismatch;
                mismatch.cycle_index = cycle_index;
                mismatch.code = "unexpected_trigger_completion";
                mismatch.message = "completed_trigger_count exceeded expected trace size";
                mismatches.push_back(std::move(mismatch));
                verdict = "failed";
                verdict_reason = "completed_trigger_count exceeded expected trace size";
                strict_one_to_one_proven = false;
                return Result<void>::Failure(
                    Error(ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                          "profile_compare completed_trigger_count exceeded expected trace size",
                          "DispensingProcessService"));
            }

            const auto& expected_item = expected_trace.items[next_expected_index];
            if (expected_item.span_index != span.schedule_span.span_index) {
                ValueObjects::ProfileCompareTraceabilityMismatch mismatch;
                mismatch.cycle_index = cycle_index;
                mismatch.trigger_sequence_id = expected_item.trigger_sequence_id;
                mismatch.code = "trigger_span_drift";
                mismatch.message = "completed trigger advanced outside current schedule span";
                mismatches.push_back(std::move(mismatch));
                verdict = "failed";
                verdict_reason = "completed trigger advanced outside current schedule span";
                strict_one_to_one_proven = false;
                return Result<void>::Failure(
                    Error(ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                          "profile_compare completed trigger advanced outside current span",
                          "DispensingProcessService"));
            }

            ValueObjects::ProfileCompareActualTraceItem actual_item;
            actual_item.cycle_index = cycle_index;
            actual_item.trigger_sequence_id = expected_item.trigger_sequence_id;
            actual_item.completion_sequence = static_cast<uint32>(actual_trace.size() + 1U);
            actual_item.span_index = span.schedule_span.span_index;
            actual_item.local_completed_trigger_count = static_cast<uint32>(
                actual_trace.size() + 1U - static_cast<std::size_t>(span.schedule_span.trigger_begin_index));
            actual_item.observed_completed_trigger_count = status.completed_trigger_count;
            actual_item.compare_source_axis = expected_item.compare_source_axis;
            actual_item.compare_position_pulse = expected_item.compare_position_pulse;
            actual_item.authority_trigger_ref = expected_item.authority_trigger_ref;
            actual_item.trigger_mode = expected_item.trigger_mode;
            actual_trace.push_back(std::move(actual_item));
            ++next_expected_index;
        }

        return Result<void>::Success();
    }

    void FinalizeTerminal(uint32 cycle_index, const ValueObjects::ProfileCompareExpectedTrace& expected_trace) {
        if (actual_trace.size() == expected_trace.items.size() && mismatches.empty() && verdict == "passed") {
            strict_one_to_one_proven = true;
            verdict_reason.clear();
            return;
        }

        if (actual_trace.size() != expected_trace.items.size()) {
            ValueObjects::ProfileCompareTraceabilityMismatch mismatch;
            mismatch.cycle_index = cycle_index;
            mismatch.code = "terminal_trigger_count_mismatch";
            mismatch.message = "profile_compare terminal actual trace count does not match expected trace count";
            mismatches.push_back(std::move(mismatch));
            if (verdict_reason.empty()) {
                verdict_reason = "profile_compare terminal actual trace count does not match expected trace count";
            }
        } else if (verdict_reason.empty()) {
            verdict_reason = "profile_compare traceability mismatches detected";
        }

        verdict = "failed";
        strict_one_to_one_proven = false;
    }
};

std::optional<Point2D> ResolveInterpolationEndPoint(
    const Motion::Ports::InterpolationData& segment) noexcept;

float32 ResolveControllerReadySegmentLengthMm(const Point2D& start_point,
                                              const Motion::Ports::InterpolationData& segment) noexcept;

float32 CalculatePolylineLength(const std::vector<Siligen::TrajectoryPoint>& points) noexcept {
    if (points.size() < 2U) {
        return 0.0f;
    }

    float32 length_mm = 0.0f;
    for (std::size_t index = 1; index < points.size(); ++index) {
        length_mm += points[index - 1U].position.DistanceTo(points[index].position);
    }
    return length_mm;
}

float32 ResolveSpanEstimatedDurationSeconds(const std::vector<Siligen::TrajectoryPoint>& points,
                                            float32 fallback_velocity_mm_s) noexcept {
    if (points.size() >= 2U) {
        const auto duration_s = points.back().timestamp - points.front().timestamp;
        if (duration_s > kTraceEpsilonMm) {
            return duration_s;
        }
    }

    const auto length_mm = CalculatePolylineLength(points);
    if (fallback_velocity_mm_s > kTraceEpsilonMm && length_mm > kTraceEpsilonMm) {
        return length_mm / fallback_velocity_mm_s;
    }
    return 0.0f;
}

float32 ResolveControllerSegmentsNominalDurationSeconds(
    const std::vector<Motion::Ports::InterpolationData>& controller_segments,
    const Point2D& start_point,
    float32 fallback_velocity_mm_s) noexcept {
    if (controller_segments.empty()) {
        return 0.0f;
    }

    float32 duration_s = 0.0f;
    bool has_duration = false;
    auto segment_start = start_point;
    for (const auto& segment : controller_segments) {
        const auto end_point = ResolveInterpolationEndPoint(segment);
        if (!end_point.has_value()) {
            continue;
        }

        const auto segment_length_mm = ResolveControllerReadySegmentLengthMm(segment_start, segment);
        const auto segment_velocity_mm_s =
            segment.velocity > kTraceEpsilonMm ? segment.velocity : fallback_velocity_mm_s;
        if (segment_length_mm > kTraceEpsilonMm && segment_velocity_mm_s > kTraceEpsilonMm) {
            duration_s += segment_length_mm / segment_velocity_mm_s;
            has_duration = true;
        }
        segment_start = end_point.value();
    }

    return has_duration ? duration_s : 0.0f;
}

float32 ResolveProfileCompareSpanEstimatedDurationSeconds(
    const std::vector<Siligen::TrajectoryPoint>& trajectory_points,
    const std::vector<Motion::Ports::InterpolationData>& controller_segments,
    const Point2D& controller_start_point,
    float32 fallback_velocity_mm_s) noexcept {
    const auto trajectory_duration_s =
        ResolveSpanEstimatedDurationSeconds(trajectory_points, fallback_velocity_mm_s);
    if (trajectory_points.empty()) {
        return trajectory_duration_s;
    }

    const auto controller_duration_s = ResolveControllerSegmentsNominalDurationSeconds(
        controller_segments,
        controller_start_point,
        fallback_velocity_mm_s);
    return std::max(trajectory_duration_s, controller_duration_s);
}

std::optional<std::size_t> FindBoundaryIndex(const std::vector<Siligen::TrajectoryPoint>& points,
                                             float32 target_distance_mm,
                                             bool last_match) {
    std::optional<std::size_t> matched_index;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto& point = points[index];
        if (!point.enable_position_trigger) {
            continue;
        }
        if (std::fabs(point.trigger_position_mm - target_distance_mm) > kTraceEpsilonMm) {
            continue;
        }
        matched_index = index;
        if (!last_match) {
            return matched_index;
        }
    }
    return matched_index;
}

std::vector<Siligen::TrajectoryPoint> DropConsecutiveDuplicatePoints(const std::vector<Siligen::TrajectoryPoint>& points) {
    std::vector<Siligen::TrajectoryPoint> result;
    result.reserve(points.size());
    for (const auto& point : points) {
        if (!result.empty() &&
            result.back().position.DistanceTo(point.position) <= kTraceEpsilonMm) {
            result.back() = point;
            continue;
        }
        result.push_back(point);
    }
    return result;
}

std::vector<Motion::Ports::InterpolationData> BuildLinearInterpolationSegments(
    const std::vector<Siligen::TrajectoryPoint>& points,
    const DispensingRuntimeParams& params) {
    std::vector<Motion::Ports::InterpolationData> segments;
    if (points.size() < 2U) {
        return segments;
    }

    const auto normalized_points = DropConsecutiveDuplicatePoints(points);
    if (normalized_points.size() < 2U) {
        return segments;
    }

    segments.reserve(normalized_points.size() - 1U);
    const auto fallback_velocity = params.dispensing_velocity > kTraceEpsilonMm
        ? params.dispensing_velocity
        : 10.0f;
    const auto acceleration = params.acceleration > kTraceEpsilonMm
        ? params.acceleration
        : kDefaultAcceleration;
    for (std::size_t index = 1; index < normalized_points.size(); ++index) {
        Motion::Ports::InterpolationData segment;
        segment.type = Motion::Ports::InterpolationType::LINEAR;
        segment.positions = {
            normalized_points[index].position.x,
            normalized_points[index].position.y,
        };
        segment.velocity = normalized_points[index].velocity > kTraceEpsilonMm
            ? normalized_points[index].velocity
            : fallback_velocity;
        segment.acceleration = acceleration;
        segment.end_velocity = index + 1U == normalized_points.size()
            ? 0.0f
            : (normalized_points[index].velocity > kTraceEpsilonMm
                   ? normalized_points[index].velocity
                   : fallback_velocity);
        segments.push_back(segment);
    }
    return segments;
}

Result<std::vector<Siligen::TrajectoryPoint>> SliceTrajectoryPointsForSpan(
    const DispensingExecutionPlan& plan,
    const Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionSpan& span) {
    if (plan.interpolation_points.size() < 2U) {
        return Result<std::vector<Siligen::TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_STATE,
                  "profile_compare 多 span 执行缺少 interpolation_points",
                  "DispensingProcessService"));
    }

    auto points = Siligen::Shared::Types::ClearTriggerMarkers(plan.interpolation_points);
    Siligen::Shared::Types::ReassignTrajectorySequenceIds(points);
    const auto cumulative_distances = Siligen::Shared::Types::BuildTrajectoryCumulativeDistances(points);
    if (cumulative_distances.empty()) {
        return Result<std::vector<Siligen::TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_STATE,
                  "profile_compare interpolation_points 无法构建累计距离",
                  "DispensingProcessService"));
    }

    const auto total_distance_mm = cumulative_distances.back();
    const auto start_distance_mm = std::clamp(span.start_profile_position_mm, 0.0f, total_distance_mm);
    const auto end_distance_mm = std::clamp(
        std::max(span.start_profile_position_mm, span.end_profile_position_mm),
        start_distance_mm,
        total_distance_mm);

    if (!Siligen::Shared::Types::InsertTriggerMarkerByDistance(
            points,
            start_distance_mm,
            Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm,
            1U)) {
        return Result<std::vector<Siligen::TrajectoryPoint>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                  "profile_compare span 起点无法映射到 interpolation_points",
                  "DispensingProcessService"));
    }
    if (!Siligen::Shared::Types::InsertTriggerMarkerByDistance(
            points,
            end_distance_mm,
            Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm,
            1U)) {
        return Result<std::vector<Siligen::TrajectoryPoint>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                  "profile_compare span 终点无法映射到 interpolation_points",
                  "DispensingProcessService"));
    }

    const auto start_index = FindBoundaryIndex(points, start_distance_mm, false);
    const auto end_index = FindBoundaryIndex(points, end_distance_mm, true);
    if (!start_index.has_value() || !end_index.has_value() || end_index.value() < start_index.value()) {
        return Result<std::vector<Siligen::TrajectoryPoint>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                  "profile_compare span 边界定位失败",
                  "DispensingProcessService"));
    }

    std::vector<Siligen::TrajectoryPoint> sliced_points(
        points.begin() + static_cast<std::ptrdiff_t>(start_index.value()),
        points.begin() + static_cast<std::ptrdiff_t>(end_index.value() + 1U));
    Siligen::Shared::Types::ReassignTrajectorySequenceIds(sliced_points);
    return Result<std::vector<Siligen::TrajectoryPoint>>::Success(std::move(sliced_points));
}

Result<std::vector<MaterializedProfileCompareSpan>> MaterializeProfileCompareSpans(
    const DispensingExecutionPlan& plan,
    const DispensingRuntimeParams& params,
    const Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionSchedule& schedule) {
    std::vector<MaterializedProfileCompareSpan> spans;
    spans.reserve(schedule.spans.size());

    const bool can_use_full_plan_directly =
        schedule.spans.size() == 1U &&
        std::fabs(schedule.spans.front().start_profile_position_mm) <= kTraceEpsilonMm &&
        std::fabs(schedule.spans.front().end_profile_position_mm - schedule.path_total_length_mm) <=
            Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm &&
        !plan.interpolation_segments.empty();

    for (const auto& schedule_span : schedule.spans) {
        MaterializedProfileCompareSpan materialized;
        materialized.schedule_span = schedule_span;
        materialized.final_target_position_mm = schedule_span.end_position_mm;

        if (can_use_full_plan_directly) {
            materialized.trajectory_points = plan.interpolation_points;
            materialized.controller_segments = plan.interpolation_segments;
        } else {
            auto sliced_points_result = SliceTrajectoryPointsForSpan(plan, schedule_span);
            if (sliced_points_result.IsError()) {
                return Result<std::vector<MaterializedProfileCompareSpan>>::Failure(
                    sliced_points_result.GetError());
            }
            materialized.trajectory_points = sliced_points_result.Value();
            materialized.controller_segments = BuildLinearInterpolationSegments(
                materialized.trajectory_points,
                params);
        }

        spans.push_back(std::move(materialized));
    }

    for (std::size_t index = 0; index < spans.size(); ++index) {
        auto& materialized = spans[index];
        if (index > 0U) {
            // Later spans resume from the previous span's actual terminal point. If the schedule
            // window starts after that point, the controller still executes the implicit lead-in.
            materialized.motion_start_position_mm = spans[index - 1U].final_target_position_mm;
        } else if (!materialized.trajectory_points.empty()) {
            materialized.motion_start_position_mm = materialized.trajectory_points.front().position;
        } else {
            materialized.motion_start_position_mm = materialized.schedule_span.start_position_mm;
        }

        materialized.estimated_duration_s = ResolveProfileCompareSpanEstimatedDurationSeconds(
            materialized.trajectory_points,
            materialized.controller_segments,
            materialized.motion_start_position_mm,
            params.dispensing_velocity);
    }

    return Result<std::vector<MaterializedProfileCompareSpan>>::Success(std::move(spans));
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

std::optional<Point2D> ResolveProgramStartPosition(const DispensingExecutionPlan& plan) noexcept {
    if (!plan.motion_trajectory.points.empty()) {
        return Point2D(plan.motion_trajectory.points.front().position);
    }
    if (!plan.interpolation_points.empty()) {
        return plan.interpolation_points.front().position;
    }
    return std::nullopt;
}

int32 EstimateLinearMotionTimeMs(float32 distance_mm, float32 velocity_mm_s) noexcept {
    if (distance_mm <= kTraceEpsilonMm || velocity_mm_s <= kTraceEpsilonMm) {
        return 0;
    }
    return static_cast<int32>(std::ceil((distance_mm / velocity_mm_s) * 1000.0f));
}

Motion::Ports::InterpolationData BuildPrePositionSegment(const Point2D& target_position,
                                                         const DispensingRuntimeParams& params) noexcept {
    Motion::Ports::InterpolationData segment;
    segment.type = Motion::Ports::InterpolationType::LINEAR;
    segment.positions = {target_position.x, target_position.y};
    segment.velocity = params.dispensing_velocity;
    segment.acceleration = params.acceleration;
    segment.end_velocity = 0.0f;
    return segment;
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
      profile_compare_port_(std::dynamic_pointer_cast<IProfileComparePort>(valve_port_)),
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
    config.use_current_planned_position_as_origin = false;

    SILIGEN_LOG_INFO_FMT_HELPER(
        "坐标系配置: max_velocity=%.2fmm/s, acc=%.2fmm/s2",
        config.max_velocity,
        config.max_acceleration);

    return interpolation_port_->ConfigureCoordinateSystem(kCoordinateSystem, config);
}

Result<DispensingExecutionReport> DispensingProcessService::ExecuteProcess(
    const Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated& execution_package,
    const DispensingRuntimeParams& params,
    const DispensingExecutionOptions& options,
    std::atomic<bool>* stop_flag,
    std::atomic<bool>* pause_flag,
    std::atomic<bool>* pause_applied_flag,
    IDispensingExecutionObserver* observer) noexcept {
    const auto& plan = execution_package.execution_plan;
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

    return ExecutePlanInternal(
        plan,
        ResolveExecutionNominalTimeS(execution_package),
        params,
        options,
        stop_flag,
        pause_flag,
        pause_applied_flag,
        observer);
}

Result<void> DispensingProcessService::PrePositionToPlanStart(const DispensingExecutionPlan& plan,
                                                              const DispensingRuntimeParams& params,
                                                              std::atomic<bool>* stop_flag,
                                                              std::atomic<bool>* pause_flag,
                                                              std::atomic<bool>* pause_applied_flag) noexcept {
    if (!interpolation_port_ || !motion_state_port_) {
        return Result<void>::Success();
    }

    const auto program_start = ResolveProgramStartPosition(plan);
    if (!program_start.has_value()) {
        return Result<void>::Success();
    }

    auto current_position_result = motion_state_port_->GetCurrentPosition();
    if (current_position_result.IsError()) {
        SILIGEN_LOG_WARNING("读取当前位置失败，跳过计划首点预定位: " +
                            current_position_result.GetError().GetMessage());
        return Result<void>::Success();
    }

    const auto& current_position = current_position_result.Value();
    const auto position_tolerance_mm = ResolveCompletionPositionToleranceMm(config_port_);
    const auto distance_to_start = current_position.DistanceTo(program_start.value());
    if (distance_to_start <= position_tolerance_mm) {
        SILIGEN_LOG_INFO_FMT_HELPER(
            "计划首点预定位跳过: current=(%.3f, %.3f), start=(%.3f, %.3f), distance=%.4f, tolerance=%.4f",
            current_position.x,
            current_position.y,
            program_start->x,
            program_start->y,
            distance_to_start,
            position_tolerance_mm);
        return Result<void>::Success();
    }

    if (IsStopRequested(stop_flag)) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "用户请求停止", "DispensingProcessService"));
    }

    const auto pre_position_segment = BuildPrePositionSegment(program_start.value(), params);
    const auto estimated_motion_time_ms =
        EstimateLinearMotionTimeMs(distance_to_start, pre_position_segment.velocity);
    const auto timeout_ms = BuildMotionCompletionTimeoutMs(static_cast<float32>(estimated_motion_time_ms));

    SILIGEN_LOG_INFO_FMT_HELPER(
        "计划首点预定位: current=(%.3f, %.3f), start=(%.3f, %.3f), distance=%.4f, velocity=%.3f, timeout_ms=%d",
        current_position.x,
        current_position.y,
        program_start->x,
        program_start->y,
        distance_to_start,
        pre_position_segment.velocity,
        timeout_ms);

    auto clear_result = interpolation_port_->ClearInterpolationBuffer(kCoordinateSystem);
    if (clear_result.IsError()) {
        return clear_result;
    }

    auto add_result = interpolation_port_->AddInterpolationData(kCoordinateSystem, pre_position_segment);
    if (add_result.IsError()) {
        return add_result;
    }

    auto flush_result = interpolation_port_->FlushInterpolationData(kCoordinateSystem);
    if (flush_result.IsError()) {
        return flush_result;
    }

    auto override_result = interpolation_port_->SetCoordinateSystemVelocityOverride(kCoordinateSystem, 100.0f);
    if (override_result.IsError()) {
        SILIGEN_LOG_WARNING("计划首点预定位设置速度倍率失败: " + override_result.GetError().GetMessage());
    }

    auto start_result = interpolation_port_->StartCoordinateSystemMotion(kCoordinateSystemMask);
    if (start_result.IsError()) {
        return start_result;
    }

    auto wait_result = WaitForMotionComplete(timeout_ms,
                                             stop_flag,
                                             pause_flag,
                                             pause_applied_flag,
                                             &program_start.value(),
                                             position_tolerance_mm,
                                             0U,
                                             0U,
                                             false,
                                             nullptr);
    if (wait_result.IsError()) {
        return wait_result;
    }

    SILIGEN_LOG_INFO_FMT_HELPER(
        "计划首点预定位完成: start=(%.3f, %.3f), distance=%.4f",
        program_start->x,
        program_start->y,
        distance_to_start);
    return Result<void>::Success();
}

Result<DispensingExecutionReport> DispensingProcessService::ExecutePlanInternal(
    const DispensingExecutionPlan& plan,
    float32 execution_nominal_time_s,
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

    if (ShouldExecuteStationaryPointShot(plan)) {
        auto pre_position_result = PrePositionToPlanStart(plan, params, stop_flag, pause_flag, pause_applied_flag);
        if (pre_position_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(pre_position_result.GetError());
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

        if (options.dispense_enabled) {
            DispenserOperationGuard dispenser_guard(valve_port_);
            Ports::DispenserValveParams timed_params;
            timed_params.count = 1U;
            timed_params.intervalMs = std::max<uint32>(10U, params.dispenser_interval_ms);
            timed_params.durationMs = std::max<uint32>(10U, params.dispenser_duration_ms);
            auto start_result = dispenser_guard.StartTimed(timed_params);
            if (start_result.IsError()) {
                return Result<DispensingExecutionReport>::Failure(start_result.GetError());
            }

            const auto timeout_ms =
                static_cast<int32>(timed_params.intervalMs + timed_params.durationMs + kMotionCompletionGraceMinMs);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
            while (std::chrono::steady_clock::now() < deadline) {
                if (IsStopRequested(stop_flag)) {
                    return Result<DispensingExecutionReport>::Failure(
                        Error(ErrorCode::INVALID_STATE, "用户请求停止", "DispensingProcessService"));
                }

                auto pause_result =
                    WaitWhilePaused(stop_flag, pause_flag, pause_applied_flag, options.dispense_enabled, observer);
                if (pause_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(pause_result.GetError());
                }

                auto status_result = valve_port_->GetDispenserStatus();
                if (status_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(status_result.GetError());
                }
                const auto& status = status_result.Value();
                if (status.IsCompleted()) {
                    break;
                }
                if (status.HasError()) {
                    return Result<DispensingExecutionReport>::Failure(
                        Error(ErrorCode::HARDWARE_ERROR, status.errorMessage, "DispensingProcessService"));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(kStatusPollIntervalMs));
            }

            auto final_status_result = valve_port_->GetDispenserStatus();
            if (final_status_result.IsError()) {
                return Result<DispensingExecutionReport>::Failure(final_status_result.GetError());
            }
            const auto& final_status = final_status_result.Value();
            if (!final_status.IsCompleted()) {
                return Result<DispensingExecutionReport>::Failure(
                    Error(ErrorCode::TIMEOUT, "单点静止点胶未在超时内完成", "DispensingProcessService"));
            }
        }

        PublishProgress(observer, 1U, 1U);
        DispensingExecutionReport report;
        report.executed_segments = 1U;
        report.total_distance = 0.0f;
        return Result<DispensingExecutionReport>::Success(report);
    }

    const bool require_profile_compare =
        options.dispense_enabled &&
        plan.production_trigger_mode == ValueObjects::ProductionTriggerMode::PROFILE_COMPARE;
    if (plan.production_trigger_mode == ValueObjects::ProductionTriggerMode::PROFILE_COMPARE &&
        !options.use_hardware_trigger) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "PROFILE_COMPARE 执行要求 use_hardware_trigger=true",
                  "DispensingProcessService"));
    }
    if (options.dispense_enabled && !require_profile_compare) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                  "DXF生产路径要求 owner 下发 PROFILE_COMPARE trigger program",
                  "DispensingProcessService"));
    }
    if (!require_profile_compare && plan.interpolation_segments.empty()) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "插补数据为空", "DispensingProcessService"));
    }

    std::shared_ptr<const Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionSchedule>
        profile_compare_schedule;
    std::vector<MaterializedProfileCompareSpan> materialized_compare_spans;
    ProfileCompareActualTraceCollector actual_trace_collector;
    if (require_profile_compare) {
        if (!profile_compare_port_) {
            return Result<DispensingExecutionReport>::Failure(
                Error(ErrorCode::PORT_NOT_INITIALIZED,
                      "点胶阀端口未实现 profile compare 能力",
                      "DispensingProcessService"));
        }
        profile_compare_schedule = options.profile_compare_schedule;
        if (!profile_compare_schedule) {
            return Result<DispensingExecutionReport>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "PROFILE_COMPARE 执行缺少正式 execution schedule",
                      "DispensingProcessService"));
        }
        if (!options.expected_trace || options.expected_trace->Empty()) {
            return Result<DispensingExecutionReport>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "PROFILE_COMPARE 执行缺少 expected trace",
                      "DispensingProcessService"));
        }
        auto schedule_validation =
            Siligen::RuntimeExecution::Contracts::Dispensing::ValidateProfileCompareExecutionSchedule(
                plan,
                *profile_compare_schedule);
        if (schedule_validation.IsError()) {
            return Result<DispensingExecutionReport>::Failure(schedule_validation.GetError());
        }
        if (options.expected_trace->items.size() != profile_compare_schedule->expected_trigger_count) {
            return Result<DispensingExecutionReport>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "PROFILE_COMPARE expected trace 数量与 execution schedule 不一致",
                      "DispensingProcessService"));
        }
        auto materialized_spans_result =
            MaterializeProfileCompareSpans(plan, params, *profile_compare_schedule);
        if (materialized_spans_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(materialized_spans_result.GetError());
        }
        materialized_compare_spans = materialized_spans_result.Value();
    }

    auto pre_position_result = PrePositionToPlanStart(plan, params, stop_flag, pause_flag, pause_applied_flag);
    if (pre_position_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(pre_position_result.GetError());
    }

    if (require_profile_compare) {
        const uint32 total_segments = !plan.interpolation_segments.empty()
            ? static_cast<uint32>(plan.interpolation_segments.size())
            : (plan.interpolation_points.size() > 1U
                   ? static_cast<uint32>(plan.interpolation_points.size() - 1U)
                   : 0U);
        const auto path_total_length_mm = profile_compare_schedule->path_total_length_mm > kTraceEpsilonMm
            ? profile_compare_schedule->path_total_length_mm
            : (plan.total_length_mm > kTraceEpsilonMm ? plan.total_length_mm : plan.motion_trajectory.total_length);

        uint32 previous_progress_end = 0U;
        for (std::size_t index = 0; index < materialized_compare_spans.size(); ++index) {
            auto& span = materialized_compare_spans[index];
            span.global_progress_begin = previous_progress_end;
            if (index + 1U == materialized_compare_spans.size()) {
                span.global_progress_end = total_segments;
            } else if (total_segments == 0U || path_total_length_mm <= kTraceEpsilonMm) {
                span.global_progress_end = previous_progress_end;
            } else {
                auto mapped = static_cast<uint32>(std::llround(
                    (span.schedule_span.end_profile_position_mm / path_total_length_mm) *
                    static_cast<float32>(total_segments)));
                mapped = std::clamp(mapped, previous_progress_end, total_segments);
                if (!span.controller_segments.empty() &&
                    mapped == previous_progress_end &&
                    mapped < total_segments) {
                    ++mapped;
                }
                span.global_progress_end = mapped;
            }
            previous_progress_end = span.global_progress_end;
        }

        constexpr uint32 kMinBufferSpace = 20;
        constexpr uint32 kZeroSpaceWarmupBatch = kMinBufferSpace;
        constexpr auto kBufferPollInterval = std::chrono::milliseconds(5);
        constexpr auto kBufferStallTimeout = std::chrono::seconds(10);
        constexpr float32 kMinMovingVelocityMmS = 0.1f;
        const auto position_tolerance_mm = ResolveCompletionPositionToleranceMm(config_port_);
        const auto start_level = ResolveDispenserStartLevel();

        auto query_buffer_space = [&]() -> Result<uint32> {
            auto crd_space_result = interpolation_port_->GetInterpolationBufferSpace(kCoordinateSystem);
            if (crd_space_result.IsError()) {
                return Result<uint32>::Failure(crd_space_result.GetError());
            }
            uint32 space = crd_space_result.Value();

            auto lookahead_space_result = interpolation_port_->GetLookAheadBufferSpace(kCoordinateSystem);
            if (lookahead_space_result.IsSuccess() && lookahead_space_result.Value() > 0U) {
                space = std::min(space, lookahead_space_result.Value());
            }
            return Result<uint32>::Success(space);
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

        auto map_local_progress = [&](const MaterializedProfileCompareSpan& span,
                                      uint32 local_executed_segments) -> uint32 {
            if (span.controller_segments.empty()) {
                return span.global_progress_end;
            }
            const auto local_total_segments = static_cast<uint32>(span.controller_segments.size());
            if (local_total_segments == 0U ||
                span.global_progress_end <= span.global_progress_begin) {
                return span.global_progress_end;
            }
            const auto local_clamped = std::min(local_executed_segments, local_total_segments);
            const auto global_delta = span.global_progress_end - span.global_progress_begin;
            return span.global_progress_begin +
                   static_cast<uint32>(
                       (static_cast<std::uint64_t>(local_clamped) * static_cast<std::uint64_t>(global_delta)) /
                       static_cast<std::uint64_t>(local_total_segments));
        };

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

        auto override_result = interpolation_port_->SetCoordinateSystemVelocityOverride(kCoordinateSystem, 100.0f);
        if (override_result.IsError()) {
            SILIGEN_LOG_WARNING("设置坐标系速度倍率失败: " + override_result.GetError().GetMessage());
        }

        MotionObserverGuard observer_guard(observer);
        observer_guard.Start();
        PublishProgress(observer, 0U, total_segments);

        ProfileCompareGuard profile_compare_guard(profile_compare_port_);
        for (const auto& span : materialized_compare_spans) {
            auto arm_request_result = BuildProfileCompareArmRequest(span.schedule_span, start_level);
            if (arm_request_result.IsError()) {
                return Result<DispensingExecutionReport>::Failure(arm_request_result.GetError());
            }

            auto clear_result = ClearInterpolationBufferForFormalPath(stop_flag, pause_flag, pause_applied_flag);
            if (clear_result.IsError()) {
                return Result<DispensingExecutionReport>::Failure(clear_result.GetError());
            }

            auto arm_result = profile_compare_guard.Arm(arm_request_result.Value());
            if (arm_result.IsError()) {
                return Result<DispensingExecutionReport>::Failure(arm_result.GetError());
            }

            if (span.controller_segments.empty()) {
                auto compare_status_result = profile_compare_guard.GetStatus();
                if (compare_status_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(compare_status_result.GetError());
                }
                const auto& compare_status = compare_status_result.Value();
                if (compare_status.HasError()) {
                    return Result<DispensingExecutionReport>::Failure(
                        Error(ErrorCode::HARDWARE_ERROR,
                              compare_status.error_message,
                              "DispensingProcessService"));
                }
                auto observe_status_result = actual_trace_collector.ObserveStatus(
                    1U,
                    span,
                    *options.expected_trace,
                    compare_status);
                if (observe_status_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(observe_status_result.GetError());
                }
                if (compare_status.completed_trigger_count != span.schedule_span.expected_trigger_count) {
                    return Result<DispensingExecutionReport>::Failure(
                        Error(ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                              "profile_compare immediate-only span 未完成全部触发点",
                              "DispensingProcessService"));
                }
                auto observe_result = actual_trace_collector.ObserveStatus(
                    1U,
                    span,
                    *options.expected_trace,
                    compare_status);
                if (observe_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(observe_result.GetError());
                }

                auto disarm_result = profile_compare_guard.Disarm();
                if (disarm_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(disarm_result.GetError());
                }
                PublishProgress(observer, span.global_progress_end, total_segments);
                continue;
            }

            const auto& program = span.controller_segments;
            const auto local_total_segments = program.size();
            size_t cursor = 0U;

            auto send_batch = [&](size_t batch_size) -> Result<void> {
                for (size_t index = 0; index < batch_size && cursor < local_total_segments; ++index, ++cursor) {
                    auto add_result = interpolation_port_->AddInterpolationData(kCoordinateSystem, program[cursor]);
                    if (add_result.IsError()) {
                        return Result<void>::Failure(add_result.GetError());
                    }
                }
                return Result<void>::Success();
            };

            auto initial_space_result = query_buffer_space();
            if (initial_space_result.IsError()) {
                return Result<DispensingExecutionReport>::Failure(initial_space_result.GetError());
            }
            const auto initial_space = initial_space_result.Value();
            const auto initial_batch = [&]() -> size_t {
                if (local_total_segments == 0U) {
                    return 0U;
                }
                if (initial_space == 0U) {
                    return std::min<std::size_t>(
                        local_total_segments,
                        static_cast<size_t>(kZeroSpaceWarmupBatch));
                }
                return std::max<std::size_t>(1U, resolve_batch_size(initial_space, local_total_segments));
            }();

            if (initial_batch > 0U) {
                auto send_result = send_batch(initial_batch);
                if (send_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(send_result.GetError());
                }
            }

            auto flush_result = interpolation_port_->FlushInterpolationData(kCoordinateSystem);
            if (flush_result.IsError()) {
                return Result<DispensingExecutionReport>::Failure(flush_result.GetError());
            }

            auto start_result = interpolation_port_->StartCoordinateSystemMotion(kCoordinateSystemMask);
            if (start_result.IsError()) {
                return Result<DispensingExecutionReport>::Failure(start_result.GetError());
            }

            auto last_progress = std::chrono::steady_clock::now();
            while (cursor < local_total_segments) {
                auto pause_result =
                    WaitWhilePaused(stop_flag, pause_flag, pause_applied_flag, false, observer);
                if (pause_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(pause_result.GetError());
                }

                if (IsStopRequested(stop_flag)) {
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
                    auto local_executed = TryResolveExecutedSegmentsForProgress(
                        progress_status,
                        static_cast<uint32>(local_total_segments));
                    if (local_executed.has_value()) {
                        PublishProgress(
                            observer,
                            map_local_progress(span, local_executed.value()),
                            total_segments);
                    }
                }

                auto compare_status_result = profile_compare_guard.GetStatus();
                if (compare_status_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(compare_status_result.GetError());
                }
                const auto& compare_status = compare_status_result.Value();
                if (compare_status.HasError()) {
                    return Result<DispensingExecutionReport>::Failure(
                        Error(ErrorCode::HARDWARE_ERROR,
                              compare_status.error_message,
                              "DispensingProcessService"));
                }
                auto observe_result = actual_trace_collector.ObserveStatus(
                    1U,
                    span,
                    *options.expected_trace,
                    compare_status);
                if (observe_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(observe_result.GetError());
                }

                const auto space = space_result.Value();
                if (space == 0U) {
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
                        return Result<DispensingExecutionReport>::Failure(
                            Error(ErrorCode::MOTION_TIMEOUT,
                                  "profile_compare span 插补缓冲区长期无可用空间",
                                  "DispensingProcessService"));
                    }
                    std::this_thread::sleep_for(kBufferPollInterval);
                    continue;
                }

                const auto batch_size = resolve_batch_size(space, local_total_segments - cursor);
                if (batch_size == 0U) {
                    std::this_thread::sleep_for(kBufferPollInterval);
                    continue;
                }

                auto send_result = send_batch(batch_size);
                if (send_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(send_result.GetError());
                }

                auto span_flush_result = interpolation_port_->FlushInterpolationData(kCoordinateSystem);
                if (span_flush_result.IsError()) {
                    return Result<DispensingExecutionReport>::Failure(span_flush_result.GetError());
                }
                last_progress = std::chrono::steady_clock::now();
            }

            const auto estimated_motion_time_ms = static_cast<float32>(
                std::max(0.0f, span.estimated_duration_s * 1000.0f));
            const auto timeout_ms = BuildMotionCompletionTimeoutMs(estimated_motion_time_ms);
            auto wait_result = WaitForMotionComplete(
                timeout_ms,
                stop_flag,
                pause_flag,
                pause_applied_flag,
                &span.final_target_position_mm,
                position_tolerance_mm,
                static_cast<uint32>(local_total_segments),
                span.schedule_span.expected_trigger_count,
                false,
                nullptr,
                [&](const Ports::ProfileCompareStatus& compare_status) -> Result<void> {
                    return actual_trace_collector.ObserveStatus(
                        1U,
                        span,
                        *options.expected_trace,
                        compare_status);
                });
            if (wait_result.IsError()) {
                return Result<DispensingExecutionReport>::Failure(wait_result.GetError());
            }

            auto disarm_result = profile_compare_guard.Disarm();
            if (disarm_result.IsError()) {
                return Result<DispensingExecutionReport>::Failure(disarm_result.GetError());
            }
            PublishProgress(observer, span.global_progress_end, total_segments);
        }

        DispensingExecutionReport report;
        report.executed_segments = total_segments;
        report.total_distance = plan.total_length_mm > 0.0f ? plan.total_length_mm : plan.motion_trajectory.total_length;
        actual_trace_collector.FinalizeTerminal(1U, *options.expected_trace);
        report.actual_trace = std::move(actual_trace_collector.actual_trace);
        report.traceability_mismatches = std::move(actual_trace_collector.mismatches);
        report.traceability_verdict = actual_trace_collector.verdict;
        report.traceability_verdict_reason = actual_trace_collector.verdict_reason;
        report.strict_one_to_one_proven = actual_trace_collector.strict_one_to_one_proven;
        return Result<DispensingExecutionReport>::Success(report);
    }

    constexpr uint32 kMinBufferSpace = 20;
    constexpr uint32 kZeroSpaceWarmupBatch = kMinBufferSpace;
    constexpr auto kBufferPollInterval = std::chrono::milliseconds(5);
    constexpr auto kBufferStallTimeout = std::chrono::seconds(10);
    constexpr float32 kMinMovingVelocityMmS = 0.1f;

    auto clear_result = ClearInterpolationBufferForFormalPath(stop_flag, pause_flag, pause_applied_flag);
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

    const bool pause_dispenser_on_pause = options.dispense_enabled;

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
            WaitWhilePaused(stop_flag, pause_flag, pause_applied_flag, pause_dispenser_on_pause, observer);
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
    const auto plan_estimated_motion_time_ms = static_cast<float32>(ResolveEstimatedMotionTimeMs(plan));
    const auto nominal_execution_time_ms =
        execution_nominal_time_s > 0.0f ? execution_nominal_time_s * 1000.0f : 0.0f;
    const auto effective_estimated_motion_time_ms =
        ResolveAuthoritativeExecutionTimeMs(plan, execution_nominal_time_s);
    const auto timeout_ms = BuildMotionCompletionTimeoutMs(effective_estimated_motion_time_ms);

    SILIGEN_LOG_INFO_FMT_HELPER(
        "等待运动完成: timeout_ms=%d, plan_estimated_motion_time_ms=%.1f, "
        "execution_nominal_time_ms=%.1f, effective_estimated_motion_time_ms=%.1f, "
        "final_target=(%.3f, %.3f), "
        "position_tolerance_mm=%.3f, total_segments=%zu",
        timeout_ms,
        plan_estimated_motion_time_ms,
        nominal_execution_time_ms,
        effective_estimated_motion_time_ms,
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
        0U,
        pause_dispenser_on_pause,
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
                                                             uint32 profile_compare_expected_trigger_count,
                                                             bool dispense_enabled,
                                                             IDispensingExecutionObserver* observer,
                                                             const std::function<Result<void>(const Ports::ProfileCompareStatus&)>&
                                                                 profile_compare_status_observer) noexcept {
    if (!interpolation_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "插补端口未初始化", "DispensingProcessService"));
    }
    if (profile_compare_expected_trigger_count > 0U && !profile_compare_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED,
                  "profile compare 端口未初始化",
                  "DispensingProcessService"));
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
    bool profile_compare_terminal_wait_logged = false;
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

        std::optional<Ports::ProfileCompareStatus> compare_status_snapshot;
        if (profile_compare_expected_trigger_count > 0U && profile_compare_port_) {
            auto compare_status_result = profile_compare_port_->GetProfileCompareStatus();
            if (compare_status_result.IsError()) {
                return Result<void>::Failure(compare_status_result.GetError());
            }

            const auto& compare_status = compare_status_result.Value();
            if (compare_status.HasError()) {
                return Result<void>::Failure(
                    Error(ErrorCode::HARDWARE_ERROR,
                          compare_status.error_message,
                          "DispensingProcessService"));
            }
            compare_status_snapshot = compare_status;
            if (profile_compare_status_observer) {
                auto observer_result = profile_compare_status_observer(compare_status);
                if (observer_result.IsError()) {
                    return observer_result;
                }
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
                        if (compare_status_snapshot.has_value() &&
                            !IsProfileCompareTerminalCompleted(
                                compare_status_snapshot.value(),
                                profile_compare_expected_trigger_count)) {
                            if (!profile_compare_terminal_wait_logged) {
                                SILIGEN_LOG_WARNING(
                                    "运动已收敛但 profile_compare 仍未完成: " +
                                    BuildProfileCompareStatusLogContext(
                                        profile_compare_expected_trigger_count,
                                        compare_status_snapshot.value()));
                                profile_compare_terminal_wait_logged = true;
                            }
                        } else {
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
                    }
                } else {
                    settle_tracking = false;
                    profile_compare_terminal_wait_logged = false;
                }
                last_position = current_position;
            } else {
                settle_tracking = false;
                profile_compare_terminal_wait_logged = false;
            }
        } else if (coord_completed) {
            if (compare_status_snapshot.has_value() &&
                !IsProfileCompareTerminalCompleted(
                    compare_status_snapshot.value(),
                    profile_compare_expected_trigger_count)) {
                if (!profile_compare_terminal_wait_logged) {
                    SILIGEN_LOG_WARNING(
                        "坐标系已完成但 profile_compare 仍未完成: " +
                        BuildProfileCompareStatusLogContext(
                            profile_compare_expected_trigger_count,
                            compare_status_snapshot.value()));
                    profile_compare_terminal_wait_logged = true;
                }
            } else {
                PublishProgress(observer, total_segments, total_segments);
                return Result<void>::Success();
            }
        } else {
            profile_compare_terminal_wait_logged = false;
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
            std::optional<Ports::ProfileCompareStatus> timeout_compare_status;

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

            if (profile_compare_expected_trigger_count > 0U && profile_compare_port_) {
                auto compare_status_result = profile_compare_port_->GetProfileCompareStatus();
                if (compare_status_result.IsError()) {
                    return Result<void>::Failure(compare_status_result.GetError());
                }
                const auto& compare_status = compare_status_result.Value();
                if (compare_status.HasError()) {
                    return Result<void>::Failure(
                        Error(ErrorCode::HARDWARE_ERROR,
                              compare_status.error_message,
                              "DispensingProcessService"));
                }
                timeout_compare_status = compare_status;
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

            if (timeout_compare_status.has_value()) {
                SILIGEN_LOG_ERROR(
                    "等待运动完成时 profile_compare 状态: " +
                    BuildProfileCompareStatusLogContext(
                        profile_compare_expected_trigger_count,
                        timeout_compare_status.value()));

                const bool motion_terminal_ready =
                    coord_completed_flag == 1 &&
                    (!final_target_position ||
                     (has_position_snapshot &&
                      distance_to_target >= 0.0f &&
                      distance_to_target <= position_tolerance_mm &&
                      std::fabs(x_velocity) <= kCompletionVelocityToleranceMmS &&
                      std::fabs(y_velocity) <= kCompletionVelocityToleranceMmS));
                if (motion_terminal_ready &&
                    !IsProfileCompareTerminalCompleted(
                        timeout_compare_status.value(),
                        profile_compare_expected_trigger_count)) {
                    return Result<void>::Failure(
                        Error(ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                              "profile_compare 未完成全部触发点",
                              "DispensingProcessService"));
                }
            }
            return Result<void>::Failure(
                Error(ErrorCode::MOTION_TIMEOUT, "等待运动完成超时", "DispensingProcessService"));
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(kStatusPollIntervalMs));
    }
}

Result<void> DispensingProcessService::ClearInterpolationBufferForFormalPath(std::atomic<bool>* stop_flag,
                                                                             std::atomic<bool>* pause_flag,
                                                                             std::atomic<bool>* pause_applied_flag) noexcept {
    if (!interpolation_port_) {
        return Result<void>::Success();
    }

    const auto clear_start = std::chrono::steady_clock::now();
    auto settled_since = std::chrono::steady_clock::time_point{};
    bool settle_tracking = false;
    Motion::Ports::CoordinateSystemStatus last_status{};
    bool has_status = false;
    bool has_clear_error = false;
    std::string last_clear_error_message;
    while (std::chrono::steady_clock::now() - clear_start < kFormalPathClearRetryTimeout) {
        auto pause_result = WaitWhilePaused(stop_flag, pause_flag, pause_applied_flag, false, nullptr);
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

        const auto now = std::chrono::steady_clock::now();
        last_status = status_result.Value();
        has_status = true;
        if (IsCoordinateSystemSettledForFormalPathClear(last_status)) {
            if (!settle_tracking) {
                settled_since = now;
                settle_tracking = true;
            }
            if (now - settled_since >= kCompletionSettleWindow) {
                auto clear_result = interpolation_port_->ClearInterpolationBuffer(kCoordinateSystem);
                if (clear_result.IsSuccess()) {
                    return Result<void>::Success();
                }
                has_clear_error = true;
                last_clear_error_message = clear_result.GetError().GetMessage();
            }
        } else {
            settle_tracking = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kStatusPollIntervalMs));
    }

    if (has_status) {
        if (has_clear_error) {
            SILIGEN_LOG_WARNING_FMT_HELPER(
                "预定位完成后清缓冲未在超时内成功: state=%d, moving=%d, remaining=%u, coord_vel=%.4f, raw_status=%d, "
                "raw_segment=%d, last_clear_error=%s",
                static_cast<int>(last_status.state),
                last_status.is_moving ? 1 : 0,
                last_status.remaining_segments,
                last_status.current_velocity,
                last_status.raw_status_word,
                last_status.raw_segment,
                last_clear_error_message.c_str());
        } else {
            SILIGEN_LOG_WARNING_FMT_HELPER(
                "预定位完成后坐标系未在超时内进入清缓冲重试窗口: state=%d, moving=%d, remaining=%u, coord_vel=%.4f, "
                "raw_status=%d, raw_segment=%d",
                static_cast<int>(last_status.state),
                last_status.is_moving ? 1 : 0,
                last_status.remaining_segments,
                last_status.current_velocity,
                last_status.raw_status_word,
                last_status.raw_segment);
        }
    } else {
        SILIGEN_LOG_WARNING("预定位完成后清缓冲未在超时内成功: 未读取到状态");
    }

    return Result<void>::Failure(
        Error(ErrorCode::MOTION_TIMEOUT, "预定位完成后清缓冲未在超时内成功", "DispensingProcessService"));
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
    bool stop_command_sent = false;
    if (interpolation_port_) {
        auto result = interpolation_port_->StopCoordinateSystemMotion(kCoordinateSystemMask);
        if (result.IsError()) {
            SILIGEN_LOG_WARNING("停止坐标系运动失败: " + result.GetError().GetMessage());
        } else {
            stop_command_sent = true;
        }
    }
    if (valve_port_) {
        auto result = valve_port_->StopDispenser();
        if (result.IsError()) {
            SILIGEN_LOG_WARNING("停止点胶阀失败: " + result.GetError().GetMessage());
        }
    }
    if (profile_compare_port_) {
        auto result = profile_compare_port_->DisarmProfileCompare();
        if (result.IsError()) {
            SILIGEN_LOG_WARNING("关闭 profile compare 失败: " + result.GetError().GetMessage());
        }
    }
    if (!stop_command_sent || !interpolation_port_) {
        return;
    }

    const auto drain_start = std::chrono::steady_clock::now();
    auto settled_since = std::chrono::steady_clock::time_point{};
    bool settle_tracking = false;
    Motion::Ports::CoordinateSystemStatus last_status{};
    bool has_status = false;
    while (std::chrono::steady_clock::now() - drain_start < kStopDrainTimeout) {
        const auto now = std::chrono::steady_clock::now();
        auto status_result = interpolation_port_->GetCoordinateSystemStatus(kCoordinateSystem);
        if (status_result.IsError()) {
            SILIGEN_LOG_WARNING("停止后读取坐标系状态失败: " + status_result.GetError().GetMessage());
            return;
        }

        last_status = status_result.Value();
        has_status = true;
        if (IsCoordinateSystemStopDrained(last_status)) {
            if (!settle_tracking) {
                settled_since = now;
                settle_tracking = true;
            } else if (now - settled_since >= kCompletionSettleWindow) {
                return;
            }
        } else {
            settle_tracking = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kStatusPollIntervalMs));
    }

    if (has_status) {
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "停止后坐标系未在超时内收敛: state=%d, moving=%d, remaining=%u, coord_vel=%.4f, raw_status=%d, raw_segment=%d",
            static_cast<int>(last_status.state),
            last_status.is_moving ? 1 : 0,
            last_status.remaining_segments,
            last_status.current_velocity,
            last_status.raw_status_word,
            last_status.raw_segment);
    } else {
        SILIGEN_LOG_WARNING("停止后坐标系未在超时内收敛: 未读取到状态");
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

}  // namespace Siligen::RuntimeExecution::Application::Services::Dispensing
