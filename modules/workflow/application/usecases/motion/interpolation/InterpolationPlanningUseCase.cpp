#define MODULE_NAME "InterpolationPlanningUseCase"

#include "InterpolationPlanningUseCase.h"

#include "domain/motion/CMPCoordinatedInterpolator.h"
#include "shared/types/CMPTypes.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/Error.h"
#include "shared/types/TrajectoryTriggerUtils.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Application::UseCases::Motion::Interpolation {

using Siligen::Shared::Types::uint32;
using Siligen::Shared::Types::DispensingTriggerPoint;

namespace {
constexpr float32 kEpsilon = 1e-6f;

float32 CalculateTotalLength(const std::vector<TrajectoryPoint>& points) {
    if (points.size() < 2) {
        return 0.0f;
    }
    float32 length = 0.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        length += points[i - 1].position.DistanceTo(points[i].position);
    }
    return length;
}

float32 ResolveTotalTime(const std::vector<TrajectoryPoint>& points,
                         float32 fallback_velocity) {
    if (points.empty()) {
        return 0.0f;
    }

    float32 max_timestamp = 0.0f;
    for (const auto& pt : points) {
        max_timestamp = std::max(max_timestamp, pt.timestamp);
    }
    if (max_timestamp > kEpsilon) {
        return max_timestamp;
    }

    float32 length = CalculateTotalLength(points);
    if (fallback_velocity > kEpsilon) {
        return length / fallback_velocity;
    }
    return 0.0f;
}

int32 CountTriggers(const std::vector<TrajectoryPoint>& points) {
    int32 count = 0;
    for (const auto& pt : points) {
        if (pt.enable_position_trigger) {
            ++count;
        }
    }
    return count;
}

std::vector<DispensingTriggerPoint> BuildTriggerPointsByDistance(const std::vector<Point2D>& points,
                                                                  const std::vector<float32>& distances_mm,
                                                                  uint32 pulse_width_us) {
    std::vector<DispensingTriggerPoint> triggers;
    if (points.size() < 2 || distances_mm.empty()) {
        return triggers;
    }

    std::vector<float32> cumulative(points.size(), 0.0f);
    for (size_t i = 1; i < points.size(); ++i) {
        cumulative[i] = cumulative[i - 1] + points[i - 1].DistanceTo(points[i]);
    }

    std::vector<float32> targets = distances_mm;
    std::sort(targets.begin(), targets.end());

    size_t idx = 1;
    uint32 seq = 0;
    for (float32 target : targets) {
        while (idx < cumulative.size() && cumulative[idx] + kEpsilon < target) {
            ++idx;
        }
        if (idx >= cumulative.size()) {
            break;
        }

        float32 seg_len = cumulative[idx] - cumulative[idx - 1];
        float32 ratio = (seg_len > kEpsilon) ? (target - cumulative[idx - 1]) / seg_len : 0.0f;
        Point2D trigger_pos = points[idx - 1] + (points[idx] - points[idx - 1]) * ratio;

        DispensingTriggerPoint trigger;
        trigger.position = trigger_pos;
        trigger.trigger_distance = target;
        trigger.sequence_id = seq++;
        trigger.pulse_width_us = pulse_width_us;
        trigger.is_enabled = true;
        triggers.push_back(trigger);
    }

    return triggers;
}

}  // namespace

bool InterpolationPlanningRequest::Validate() const noexcept {
    if (points.size() < 2) {
        return false;
    }
    if (config.max_velocity <= 0.0f || config.max_acceleration <= 0.0f) {
        return false;
    }
    if ((algorithm == InterpolationAlgorithm::LINEAR ||
         algorithm == InterpolationAlgorithm::CMP_COORDINATED) &&
        config.max_jerk <= 0.0f) {
        return false;
    }
    if (config.time_step <= 0.0f) {
        return false;
    }
    if (config.trigger_spacing_mm < 0.0f) {
        return false;
    }
    if (max_step_size_mm < 0.0f) {
        return false;
    }
    return true;
}

Result<InterpolationPlanningResult> InterpolationPlanningUseCase::Execute(
    const InterpolationPlanningRequest& request) const noexcept {
    if (!request.Validate()) {
        return Result<InterpolationPlanningResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "InterpolationPlanningUseCase"));
    }

    std::vector<TrajectoryPoint> points;
    if (request.algorithm == InterpolationAlgorithm::CMP_COORDINATED &&
        !request.trigger_distances_mm.empty()) {
        Domain::Motion::CMPCoordinatedInterpolator cmp_interpolator;
        CMPConfiguration cmp_config;
        cmp_config.trigger_mode = CMPTriggerMode::POSITION_SYNC;
        cmp_config.cmp_channel = 1;
        cmp_config.pulse_width_us = 2000;
        cmp_config.trigger_position_tolerance = 0.1f;
        cmp_config.time_tolerance_ms = 1.0f;
        cmp_config.enable_compensation = true;
        cmp_config.compensation_factor = 1.0f;
        cmp_config.enable_multi_channel = false;

        auto triggers = BuildTriggerPointsByDistance(request.points,
                                                     request.trigger_distances_mm,
                                                     cmp_config.pulse_width_us);
        points = cmp_interpolator.PositionTriggeredDispensing(request.points, triggers, cmp_config, request.config);
    } else {
        if (request.algorithm == InterpolationAlgorithm::CMP_COORDINATED) {
            return Result<InterpolationPlanningResult>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                      "CMP插补缺少 trigger_distances_mm，不能退化为默认轨迹采样触发",
                      "InterpolationPlanningUseCase"));
        }

        auto interpolator = Domain::Motion::TrajectoryInterpolatorFactory::CreateInterpolator(request.algorithm);
        if (!interpolator) {
            return Result<InterpolationPlanningResult>::Failure(
                Error(ErrorCode::NOT_IMPLEMENTED, "插补算法未实现", "InterpolationPlanningUseCase"));
        }
        points = interpolator->CalculateInterpolation(request.points, request.config);
        if (request.optimize_density && request.max_step_size_mm > kEpsilon) {
            points = interpolator->OptimizeTrajectoryDensity(points, request.max_step_size_mm);
        }
        if (!Siligen::Shared::Types::ApplyTriggerMarkersByDistance(points, request.trigger_distances_mm)) {
            return Result<InterpolationPlanningResult>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                      "trigger_distances_mm 映射到插补轨迹失败",
                      "InterpolationPlanningUseCase"));
        }
    }

    if (points.empty()) {
        return Result<InterpolationPlanningResult>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "插补结果为空", "InterpolationPlanningUseCase"));
    }

    InterpolationPlanningResult result;
    result.points = std::move(points);
    result.total_length_mm = CalculateTotalLength(result.points);
    result.total_time_s = ResolveTotalTime(result.points, request.config.max_velocity);
    result.trigger_count = CountTriggers(result.points);
    return Result<InterpolationPlanningResult>::Success(result);
}

}  // namespace Siligen::Application::UseCases::Motion::Interpolation
