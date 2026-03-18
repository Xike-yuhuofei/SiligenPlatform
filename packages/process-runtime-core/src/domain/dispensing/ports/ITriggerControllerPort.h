#pragma once

#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Dispensing::Ports {

using Shared::Types::LogicalAxisId;
using Shared::Types::Result;

/**
 * @brief CMP触发模式
 */
enum class TriggerMode {
    SINGLE_POINT,  // 单点触发
    CONTINUOUS,    // 连续触发
    RANGE          // 范围触发
};

/**
 * @brief CMP信号类型
 */
enum class SignalType {
    PULSE,  // 脉冲信号
    LEVEL   // 电平信号
};

/**
 * @brief 触发配置
 */
struct TriggerConfig {
    int32 channel = 1;
    TriggerMode mode = TriggerMode::SINGLE_POINT;
    SignalType signal_type = SignalType::PULSE;
    int32 pulse_width_us = 2000;
    int32 delay_time_us = 0;
    bool abs_position_flag = false;
};

/**
 * @brief 触发状态
 */
struct TriggerStatus {
    bool is_enabled = false;
    int32 trigger_count = 0;
    float32 last_trigger_position = 0.0f;
};

/**
 * @brief 触发控制端口接口
 * 定义CMP位置触发控制的核心操作
 */
class ITriggerControllerPort {
   public:
    virtual ~ITriggerControllerPort() = default;

    // 触发配置
    virtual Result<void> ConfigureTrigger(const TriggerConfig& config) = 0;
    virtual Result<TriggerConfig> GetTriggerConfig() const = 0;

    // 单点触发
    virtual Result<void> SetSingleTrigger(LogicalAxisId axis, float32 position, int32 pulse_width_us) = 0;

    // 连续触发
    virtual Result<void> SetContinuousTrigger(
        LogicalAxisId axis, float32 start_pos, float32 end_pos, float32 interval, int32 pulse_width_us) = 0;

    // 范围触发
    virtual Result<void> SetRangeTrigger(LogicalAxisId axis, float32 start_pos, float32 end_pos, int32 pulse_width_us) = 0;

    // 序列触发（显式触发点列表）
    virtual Result<void> SetSequenceTrigger(LogicalAxisId axis,
                                            const std::vector<float32>& positions,
                                            int32 pulse_width_us) = 0;

    // 触发控制
    virtual Result<void> EnableTrigger(LogicalAxisId axis) = 0;
    virtual Result<void> DisableTrigger(LogicalAxisId axis) = 0;
    virtual Result<void> ClearTrigger(LogicalAxisId axis) = 0;

    // 状态查询
    virtual Result<TriggerStatus> GetTriggerStatus(LogicalAxisId axis) const = 0;
    virtual Result<bool> IsTriggerEnabled(LogicalAxisId axis) const = 0;
    virtual Result<int32> GetTriggerCount(LogicalAxisId axis) const = 0;
};

}  // namespace Siligen::Domain::Dispensing::Ports

