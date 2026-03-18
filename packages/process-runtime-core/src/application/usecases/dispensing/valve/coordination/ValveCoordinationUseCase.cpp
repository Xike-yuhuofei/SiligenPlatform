#include "ValveCoordinationUseCase.h"
#include "shared/types/Error.h"
#include <cmath>

namespace Siligen::Application::UseCases::Dispensing::Valve::Coordination {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

ValveCoordinationUseCase::ValveCoordinationUseCase(
    std::shared_ptr<IValvePort> valve_port)
    : valve_port_(std::move(valve_port)), current_velocity_(0.0f)
{
}

Result<ValveTimingResult> ValveCoordinationUseCase::CalculateTimingParameters(
    const std::vector<GeometrySegment>& path,
    const ValveTimingConfig& config)
{
    // 验证配置
    if (!config.Validate()) {
        return Result<ValveTimingResult>::Failure(
            Error{ErrorCode::INVALID_PARAMETER, "阀门时序配置无效"}
        );
    }

    // 验证路径非空
    if (path.empty()) {
        return Result<ValveTimingResult>::Failure(
            Error{ErrorCode::INVALID_PARAMETER, "路径为空"}
        );
    }

    // 验证路径段有效性
    for (const auto& segment : path) {
        if (!segment.IsValid()) {
            return Result<ValveTimingResult>::Failure(
                Error{ErrorCode::INVALID_PARAMETER, "路径包含无效几何段"}
            );
        }
    }

    // 计算总长度
    const float32 total_length = CalculateTotalLength(path);
    if (total_length <= 0.0f) {
        return Result<ValveTimingResult>::Failure(
            Error{ErrorCode::INVALID_PARAMETER, "路径总长度为零"}
        );
    }

    // 计算总时间
    const float32 total_time = total_length / config.velocity;

    // 计算触发间隔(距离)
    const float32 interval_s = config.interval_ms / 1000.0f;
    const float32 interval_mm = config.velocity * interval_s;

    // 保存速度到临时成员变量
    current_velocity_ = config.velocity;

    // 生成触发点
    std::vector<TriggerPoint> trigger_points = GenerateTriggerPoints(path, interval_mm);

    // 构建结果
    ValveTimingResult result;
    result.total_length_mm = total_length;
    result.total_time_s = total_time;
    result.trigger_count = static_cast<uint32>(trigger_points.size());
    result.trigger_points = std::move(trigger_points);

    return Result<ValveTimingResult>::Success(std::move(result));
}

float32 ValveCoordinationUseCase::CalculateTotalLength(
    const std::vector<GeometrySegment>& path)
{
    float32 total = 0.0f;
    for (const auto& segment : path) {
        total += std::abs(segment.Length()); // 取绝对值处理顺时针弧
    }
    return total;
}

std::vector<TriggerPoint> ValveCoordinationUseCase::GenerateTriggerPoints(
    const std::vector<GeometrySegment>& path,
    float32 interval_mm)
{
    std::vector<TriggerPoint> points;

    float32 accumulated_length = 0.0f;
    float32 next_trigger = interval_mm;

    for (uint32 i = 0; i < path.size(); ++i) {
        const float32 segment_length = std::abs(path[i].Length()); // 取绝对值处理顺时针弧
        const float32 segment_start = accumulated_length;
        const float32 segment_end = accumulated_length + segment_length;

        // 在当前段内生成所有触发点
        while (next_trigger <= segment_end) { // 包括路径末端的触发点
            TriggerPoint point;
            point.position_mm = next_trigger;
            point.time_s = next_trigger / current_velocity_; // 使用临时成员变量
            point.segment_index = i;
            points.push_back(point);

            next_trigger += interval_mm;
        }

        accumulated_length = segment_end;
    }

    return points;
}

}  // namespace Siligen::Application::UseCases::Dispensing::Valve::Coordination

