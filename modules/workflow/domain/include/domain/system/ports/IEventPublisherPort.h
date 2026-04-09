#pragma once

#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <functional>
#include <string>

namespace Siligen::Domain::System::Ports {

using Shared::Types::Result;
using Shared::Types::LogicalAxisId;
using Shared::Types::float32;
using Shared::Types::int32;
using Shared::Types::uint32;
using Shared::Types::Point2D;

/**
 * @brief 事件类型
 */
enum class EventType {
    POSITION_CHANGED,      // 位置变化
    MOTION_COMPLETED,      // 运动完成
    MOTION_STARTED,        // 运动开始
    EMERGENCY_STOP,        // 紧急停止
    AXIS_ERROR,            // 轴错误
    HOMING_STARTED,        // 回零开始
    HOMING_COMPLETED,      // 回零完成
    TRIGGER_ACTIVATED,     // 触发激活
    SYSTEM_STATE_CHANGED,  // 系统状态变化
    CONFIGURATION_CHANGED, // 配置变化

    // DXF执行相关事件
    DXF_EXECUTION_STARTED,   // DXF执行开始
    DXF_EXECUTION_PROGRESS,  // DXF执行进度
    DXF_EXECUTION_COMPLETED, // DXF执行完成
    DXF_EXECUTION_FAILED,    // DXF执行失败
    DXF_EXECUTION_CANCELLED, // DXF执行取消

    // DXF解析相关事件
    DXF_PARSING_STARTED,   // DXF解析开始
    DXF_PARSING_COMPLETED, // DXF解析完成
    DXF_PARSING_FAILED,    // DXF解析失败
    WORKFLOW_STAGE_CHANGED, // workflow 阶段状态变化
    WORKFLOW_STAGE_FAILED,  // workflow 阶段失败
    WORKFLOW_EXPORT_FAILED, // workflow 导出失败
    SOFT_LIMIT_TRIGGERED,  // 软限位触发（硬件触发）
    SOFT_LIMIT_VIOLATION,  // 软限位违规（轨迹验证失败）

    // 硬件连接相关事件
    HARDWARE_CONNECTED,         // 硬件连接成功
    HARDWARE_DISCONNECTED,      // 硬件断开连接
    HARDWARE_CONNECTION_FAILED  // 硬件连接失败
};

/**
 * @brief 领域事件基类
 */
struct DomainEvent {
    EventType type;
    int64 timestamp = 0;
    std::string source;   // 事件源
    std::string message;  // 事件消息

    virtual ~DomainEvent() = default;
};

/**
 * @brief 位置变化事件
 */
struct PositionChangedEvent : public DomainEvent {
    LogicalAxisId axis = LogicalAxisId::X;
    float32 old_position = 0.0f;
    float32 new_position = 0.0f;
    Point2D position_2d{0, 0};
};

/**
 * @brief 运动完成事件
 */
struct MotionCompletedEvent : public DomainEvent {
    LogicalAxisId axis = LogicalAxisId::X;
    bool success = false;
    float32 final_position = 0.0f;
    int32 error_code = 0;
};

/**
 * @brief 紧急停止事件
 */
struct EmergencyStopEvent : public DomainEvent {
    std::string reason;
    bool is_hardware_triggered = false;
};

/**
 * @brief 轴错误事件
 */
struct AxisErrorEvent : public DomainEvent {
    LogicalAxisId axis = LogicalAxisId::X;
    int32 error_code = 0;
    std::string error_description;
};

/**
 * @brief DXF执行开始事件
 */
struct DXFExecutionStartedEvent : public DomainEvent {
    std::string task_id;
    std::string dxf_filepath;
    uint32 total_segments = 0;
};

/**
 * @brief DXF执行进度事件
 */
struct DXFExecutionProgressEvent : public DomainEvent {
    std::string task_id;
    uint32 executed_segments = 0;
    uint32 total_segments = 0;
    uint32 progress_percent = 0;
    float32 elapsed_seconds = 0.0f;
    float32 estimated_remaining_seconds = 0.0f;
};

/**
 * @brief DXF执行完成事件
 */
struct DXFExecutionCompletedEvent : public DomainEvent {
    std::string task_id;
    uint32 total_segments = 0;
    float32 total_seconds = 0.0f;
    bool success = true;
};

/**
 * @brief DXF执行失败事件
 */
struct DXFExecutionFailedEvent : public DomainEvent {
    std::string task_id;
    std::string error_message;
    uint32 failed_at_segment = 0;
    int32 error_code = 0;
};

/**
 * @brief DXF执行取消事件
 */
struct DXFExecutionCancelledEvent : public DomainEvent {
    std::string task_id;
    uint32 cancelled_at_segment = 0;
    std::string cancel_reason;
};

/**
 * @brief 软限位触发事件（硬件触发）
 *
 * 当轴位置超过配置的软限位时硬件触发此事件
 */
struct SoftLimitTriggeredEvent : public DomainEvent {
    std::string task_id;         // 当前任务ID
    LogicalAxisId axis = LogicalAxisId::X;  // 触发轴号
    float32 position = 0.0f;     // 当前位置
    bool is_positive_limit = true;  // 是否正向限位触发
};

/**
 * @brief 软限位违规事件（轨迹验证失败）
 *
 * 当轨迹验证发现轨迹点超出软限位范围时发布此事件
 */
struct SoftLimitViolationEvent : public DomainEvent {
    std::string task_id;         // 当前任务ID
    uint32 segment_index = 0;    // 违规段索引
    LogicalAxisId violating_axis = LogicalAxisId::X;  // 违规轴号
    float32 actual_position = 0.0f;  // 实际位置
    float32 limit_value = 0.0f;      // 限位值
    bool is_positive_violation = true;  // 是否正向违规
};

/**
 * @brief 硬件连接事件
 *
 * 当硬件连接成功、失败或断开时发布此事件
 */
struct HardwareConnectionEvent : public DomainEvent {
    std::string hardware_id;      // 硬件标识 (如 "MultiCard")
    std::string connection_info;  // 连接信息 (IP地址等)
    bool success = false;         // 是否成功
    int32 error_code = 0;         // 错误码 (失败时)
};

/**
 * @brief 系统状态变化事件
 *
 * 当设备/系统运行状态发生变化时发布此事件（由 DispenserModel 驱动）
 */
struct SystemStateChangedEvent : public DomainEvent {
    std::string previous_state;   // 前一状态
    std::string new_state;        // 新状态
    std::string trigger_event;    // 触发事件
};

/**
 * @brief 事件处理器类型
 */
using EventHandler = std::function<void(const DomainEvent&)>;

/**
 * @brief 事件发布端口接口
 * 定义事件发布和订阅机制
 */
class IEventPublisherPort {
   public:
    virtual ~IEventPublisherPort() = default;

    // 事件发布
    virtual Result<void> Publish(const DomainEvent& event) = 0;
    virtual Result<void> PublishAsync(const DomainEvent& event) = 0;

    // 事件订阅
    virtual Result<int32> Subscribe(EventType type, EventHandler handler) = 0;
    virtual Result<void> Unsubscribe(int32 subscription_id) = 0;
    virtual Result<void> UnsubscribeAll(EventType type) = 0;

    // 事件历史
    virtual Result<std::vector<DomainEvent*>> GetEventHistory(EventType type, int32 max_count = 100) const = 0;
    virtual Result<void> ClearEventHistory() = 0;
};

}  // namespace Siligen::Domain::System::Ports

