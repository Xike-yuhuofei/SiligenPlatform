#pragma once

#include "domain/dispensing/ports/IValvePort.h"
#include "domain/geometry/GeometryTypes.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"
#include <memory>
#include <vector>

namespace Siligen::Application::UseCases::Dispensing::Valve::Coordination {

using Siligen::Shared::Types::Result;
using Siligen::Domain::Dispensing::Ports::IValvePort;
using Siligen::Domain::Geometry::GeometrySegment;

/**
 * @brief 阀门时序配置
 */
struct ValveTimingConfig {
    uint32 interval_ms;      ///< 点胶间隔(毫秒)
    uint32 duration_ms;      ///< 点胶持续时间(毫秒)
    float32 velocity;        ///< 运动速度(mm/s)

    bool Validate() const {
        return (interval_ms >= 10 && interval_ms <= 60000) &&
               (duration_ms >= 10 && duration_ms <= 5000) &&
               (velocity > 0.0f && velocity <= 1000.0f);
    }
};

/**
 * @brief 触发点信息
 */
struct TriggerPoint {
    float32 position_mm;     ///< 沿路径的位置(mm)
    float32 time_s;          ///< 时间(秒)
    uint32 segment_index;    ///< 所属段索引
};

/**
 * @brief 时序计算结果
 */
struct ValveTimingResult {
    float32 total_length_mm;                ///< 总路径长度(mm)
    float32 total_time_s;                   ///< 总时间(秒)
    uint32 trigger_count;                   ///< 触发次数
    std::vector<TriggerPoint> trigger_points; ///< 触发点序列
};

/**
 * @brief 阀门协调用例
 *
 * 职责: 编排DXF路径与阀门时序协调逻辑
 * 架构合规: Application层用例,通过端口访问Domain层
 */
class ValveCoordinationUseCase {
public:
    /**
     * @brief 构造函数
     * @param valve_port 阀门控制端口
     */
    explicit ValveCoordinationUseCase(std::shared_ptr<IValvePort> valve_port);

    ~ValveCoordinationUseCase() = default;

    // 禁止拷贝和移动
    ValveCoordinationUseCase(const ValveCoordinationUseCase&) = delete;
    ValveCoordinationUseCase& operator=(const ValveCoordinationUseCase&) = delete;

    /**
     * @brief 计算阀门触发时序参数
     * @param path 几何路径
     * @param config 时序配置
     * @return Result<ValveTimingResult> 时序结果
     */
    Result<ValveTimingResult> CalculateTimingParameters(
        const std::vector<GeometrySegment>& path,
        const ValveTimingConfig& config);

private:
    std::shared_ptr<IValvePort> valve_port_;
    float32 current_velocity_;  ///< 临时存储当前计算速度

    /**
     * @brief 计算路径总长度
     */
    float32 CalculateTotalLength(const std::vector<GeometrySegment>& path);

    /**
     * @brief 生成触发点序列
     */
    std::vector<TriggerPoint> GenerateTriggerPoints(
        const std::vector<GeometrySegment>& path,
        float32 interval_mm);
};

}  // namespace Siligen::Application::UseCases::Dispensing::Valve::Coordination




