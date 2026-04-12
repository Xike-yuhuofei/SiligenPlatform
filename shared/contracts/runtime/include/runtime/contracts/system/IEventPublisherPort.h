#pragma once

#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Domain::Supervision::Ports {

using Shared::Types::Result;
using Shared::Types::LogicalAxisId;
using Shared::Types::float32;
using Shared::Types::int64;
using Shared::Types::int32;
using Shared::Types::uint32;
using Shared::Types::Point2D;

enum class EventType {
    POSITION_CHANGED,
    MOTION_COMPLETED,
    MOTION_STARTED,
    EMERGENCY_STOP,
    AXIS_ERROR,
    HOMING_STARTED,
    HOMING_COMPLETED,
    TRIGGER_ACTIVATED,
    SYSTEM_STATE_CHANGED,
    CONFIGURATION_CHANGED,
    DXF_EXECUTION_STARTED,
    DXF_EXECUTION_PROGRESS,
    DXF_EXECUTION_COMPLETED,
    DXF_EXECUTION_FAILED,
    DXF_EXECUTION_CANCELLED,
    DXF_PARSING_STARTED,
    DXF_PARSING_COMPLETED,
    DXF_PARSING_FAILED,
    WORKFLOW_STAGE_CHANGED,
    WORKFLOW_STAGE_FAILED,
    WORKFLOW_EXPORT_FAILED,
    SOFT_LIMIT_TRIGGERED,
    SOFT_LIMIT_VIOLATION,
    HARDWARE_CONNECTED,
    HARDWARE_DISCONNECTED,
    HARDWARE_CONNECTION_FAILED
};

struct DomainEvent {
    EventType type;
    int64 timestamp = 0;
    std::string source;
    std::string message;

    virtual ~DomainEvent() = default;
};

struct PositionChangedEvent : public DomainEvent {
    LogicalAxisId axis = LogicalAxisId::X;
    float32 old_position = 0.0f;
    float32 new_position = 0.0f;
    Point2D position_2d{0, 0};
};

struct MotionCompletedEvent : public DomainEvent {
    LogicalAxisId axis = LogicalAxisId::X;
    bool success = false;
    float32 final_position = 0.0f;
    int32 error_code = 0;
};

struct EmergencyStopEvent : public DomainEvent {
    std::string reason;
    bool is_hardware_triggered = false;
};

struct AxisErrorEvent : public DomainEvent {
    LogicalAxisId axis = LogicalAxisId::X;
    int32 error_code = 0;
    std::string error_description;
};

struct DXFExecutionStartedEvent : public DomainEvent {
    std::string task_id;
    std::string dxf_filepath;
    uint32 total_segments = 0;
};

struct DXFExecutionProgressEvent : public DomainEvent {
    std::string task_id;
    uint32 executed_segments = 0;
    uint32 total_segments = 0;
    uint32 progress_percent = 0;
    float32 elapsed_seconds = 0.0f;
    float32 estimated_remaining_seconds = 0.0f;
};

struct DXFExecutionCompletedEvent : public DomainEvent {
    std::string task_id;
    uint32 total_segments = 0;
    float32 total_seconds = 0.0f;
    bool success = true;
};

struct DXFExecutionFailedEvent : public DomainEvent {
    std::string task_id;
    std::string error_message;
    uint32 failed_at_segment = 0;
    int32 error_code = 0;
};

struct DXFExecutionCancelledEvent : public DomainEvent {
    std::string task_id;
    uint32 cancelled_at_segment = 0;
    std::string cancel_reason;
};

struct SoftLimitTriggeredEvent : public DomainEvent {
    std::string task_id;
    LogicalAxisId axis = LogicalAxisId::X;
    float32 position = 0.0f;
    bool is_positive_limit = true;
};

struct SoftLimitViolationEvent : public DomainEvent {
    std::string task_id;
    uint32 segment_index = 0;
    LogicalAxisId violating_axis = LogicalAxisId::X;
    float32 actual_position = 0.0f;
    float32 limit_value = 0.0f;
    bool is_positive_violation = true;
};

struct HardwareConnectionEvent : public DomainEvent {
    std::string hardware_id;
    std::string connection_info;
    bool success = false;
    int32 error_code = 0;
};

struct SystemStateChangedEvent : public DomainEvent {
    std::string previous_state;
    std::string new_state;
    std::string trigger_event;
};

using EventHandler = std::function<void(const DomainEvent&)>;

class IEventPublisherPort {
   public:
    virtual ~IEventPublisherPort() = default;

    virtual Result<void> Publish(const DomainEvent& event) = 0;
    virtual Result<void> PublishAsync(const DomainEvent& event) = 0;
    virtual Result<int32> Subscribe(EventType type, EventHandler handler) = 0;
    virtual Result<void> Unsubscribe(int32 subscription_id) = 0;
    virtual Result<void> UnsubscribeAll(EventType type) = 0;
    virtual Result<std::vector<std::shared_ptr<const DomainEvent>>> GetEventHistory(
        EventType type,
        int32 max_count = 100) const = 0;
    virtual Result<void> ClearEventHistory() = 0;
};

}  // namespace Siligen::Domain::Supervision::Ports

namespace Siligen::Domain::System::Ports {

using EventType = Siligen::Domain::Supervision::Ports::EventType;
using DomainEvent = Siligen::Domain::Supervision::Ports::DomainEvent;
using PositionChangedEvent = Siligen::Domain::Supervision::Ports::PositionChangedEvent;
using MotionCompletedEvent = Siligen::Domain::Supervision::Ports::MotionCompletedEvent;
using EmergencyStopEvent = Siligen::Domain::Supervision::Ports::EmergencyStopEvent;
using AxisErrorEvent = Siligen::Domain::Supervision::Ports::AxisErrorEvent;
using DXFExecutionStartedEvent = Siligen::Domain::Supervision::Ports::DXFExecutionStartedEvent;
using DXFExecutionProgressEvent = Siligen::Domain::Supervision::Ports::DXFExecutionProgressEvent;
using DXFExecutionCompletedEvent = Siligen::Domain::Supervision::Ports::DXFExecutionCompletedEvent;
using DXFExecutionFailedEvent = Siligen::Domain::Supervision::Ports::DXFExecutionFailedEvent;
using DXFExecutionCancelledEvent = Siligen::Domain::Supervision::Ports::DXFExecutionCancelledEvent;
using SoftLimitTriggeredEvent = Siligen::Domain::Supervision::Ports::SoftLimitTriggeredEvent;
using SoftLimitViolationEvent = Siligen::Domain::Supervision::Ports::SoftLimitViolationEvent;
using HardwareConnectionEvent = Siligen::Domain::Supervision::Ports::HardwareConnectionEvent;
using SystemStateChangedEvent = Siligen::Domain::Supervision::Ports::SystemStateChangedEvent;
using EventHandler = Siligen::Domain::Supervision::Ports::EventHandler;
using IEventPublisherPort = Siligen::Domain::Supervision::Ports::IEventPublisherPort;

}  // namespace Siligen::Domain::System::Ports
