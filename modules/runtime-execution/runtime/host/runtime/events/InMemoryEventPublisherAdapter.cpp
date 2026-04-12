// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "InMemoryEventPublisher"

#include "InMemoryEventPublisherAdapter.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

namespace {

template <typename TEvent>
std::shared_ptr<DomainEvent> CloneTypedEvent(const DomainEvent& event) {
    if (const auto* typed_event = dynamic_cast<const TEvent*>(&event)) {
        return std::make_shared<TEvent>(*typed_event);
    }
    return std::make_shared<DomainEvent>(event);
}

}  // namespace

InMemoryEventPublisherAdapter::InMemoryEventPublisherAdapter(size_t max_history_size)
    : max_history_size_(max_history_size) {}

Result<void> InMemoryEventPublisherAdapter::Publish(const DomainEvent& event) {
    try {
        // 1. 分发到本地订阅者
        DispatchToLocalSubscribers(event);

        // 2. 记录到历史
        RecordEvent(event);

        return Result<void>::Success();
    } catch (const std::exception& e) {
        return Result<void>::Failure(
            Error(ErrorCode::UNKNOWN_ERROR, "Failed to publish event: " + std::string(e.what()), "InMemoryEventPublisherAdapter"));
    }
}

Result<void> InMemoryEventPublisherAdapter::PublishAsync(const DomainEvent& event) {
    // MVP 简化实现：同步发布
    // TODO: 后续可使用线程池异步发布
    return Publish(event);
}

Result<int32_t> InMemoryEventPublisherAdapter::Subscribe(EventType type, EventHandler handler) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);

    int32_t subscription_id = next_subscription_id_++;
    auto wrapped_handler = [handler](const DomainEvent& event) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            SILIGEN_LOG_ERROR(std::string("Subscriber callback exception: ") + e.what());
        } catch (...) {
            SILIGEN_LOG_ERROR("Unknown subscriber callback exception");
        }
    };

    auto& signal = signals_[type];
    connections_[subscription_id] = signal.connect(wrapped_handler);
    subscription_types_[subscription_id] = type;
    subscriptions_by_type_[type].push_back(subscription_id);

    return Result<int32_t>::Success(subscription_id);
}

Result<void> InMemoryEventPublisherAdapter::Unsubscribe(int32_t subscription_id) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);

    auto conn_it = connections_.find(subscription_id);
    if (conn_it == connections_.end()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Subscription not found", "InMemoryEventPublisherAdapter"));
    }

    conn_it->second.disconnect();
    connections_.erase(conn_it);

    auto type_it = subscription_types_.find(subscription_id);
    if (type_it != subscription_types_.end()) {
        auto& ids = subscriptions_by_type_[type_it->second];
        ids.erase(std::remove(ids.begin(), ids.end(), subscription_id), ids.end());
        subscription_types_.erase(type_it);
    }

    return Result<void>::Success();
}

Result<void> InMemoryEventPublisherAdapter::UnsubscribeAll(EventType type) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    auto it = subscriptions_by_type_.find(type);
    if (it != subscriptions_by_type_.end()) {
        for (int32_t id : it->second) {
            auto conn_it = connections_.find(id);
            if (conn_it != connections_.end()) {
                conn_it->second.disconnect();
                connections_.erase(conn_it);
            }
            subscription_types_.erase(id);
        }
        it->second.clear();
    }
    return Result<void>::Success();
}

Result<std::vector<DomainEvent*>> InMemoryEventPublisherAdapter::GetEventHistory(
    EventType type, int32_t max_count) const {
    std::vector<DomainEvent*> history;
    if (max_count <= 0) {
        return Result<std::vector<DomainEvent*>>::Success(std::move(history));
    }

    std::lock_guard<std::mutex> lock(history_mutex_);
    history.reserve(std::min<std::size_t>(static_cast<std::size_t>(max_count), event_history_.size()));
    for (auto it = event_history_.rbegin(); it != event_history_.rend() &&
                                           history.size() < static_cast<std::size_t>(max_count);
         ++it) {
        if (!it->event || it->event->type != type) {
            continue;
        }
        history.push_back(it->event.get());
    }
    return Result<std::vector<DomainEvent*>>::Success(std::move(history));
}

Result<void> InMemoryEventPublisherAdapter::ClearEventHistory() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    event_history_.clear();
    return Result<void>::Success();
}

void InMemoryEventPublisherAdapter::DispatchToLocalSubscribers(const DomainEvent& event) {
    EventSignal* signal = nullptr;
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        auto it = signals_.find(event.type);
        if (it != signals_.end()) {
            signal = &it->second;
        }
    }

    if (signal) {
        try {
            (*signal)(event);
        } catch (const std::exception& e) {
            SILIGEN_LOG_ERROR(std::string("Event dispatch exception: ") + e.what());
        } catch (...) {
            SILIGEN_LOG_ERROR("Unknown event dispatch exception");
        }
    }
}

void InMemoryEventPublisherAdapter::RecordEvent(const DomainEvent& event) {
    std::lock_guard<std::mutex> lock(history_mutex_);

    // 添加到历史记录
    event_history_.push_back({CloneEvent(event)});

    // 限制历史记录大小
    while (event_history_.size() > max_history_size_) {
        event_history_.pop_front();
    }
}

std::shared_ptr<DomainEvent> InMemoryEventPublisherAdapter::CloneEvent(const DomainEvent& event) {
    switch (event.type) {
        case EventType::POSITION_CHANGED:
            return CloneTypedEvent<Siligen::Domain::System::Ports::PositionChangedEvent>(event);
        case EventType::MOTION_COMPLETED:
            return CloneTypedEvent<Siligen::Domain::System::Ports::MotionCompletedEvent>(event);
        case EventType::EMERGENCY_STOP:
            return CloneTypedEvent<Siligen::Domain::System::Ports::EmergencyStopEvent>(event);
        case EventType::AXIS_ERROR:
            return CloneTypedEvent<Siligen::Domain::System::Ports::AxisErrorEvent>(event);
        case EventType::DXF_EXECUTION_STARTED:
            return CloneTypedEvent<Siligen::Domain::System::Ports::DXFExecutionStartedEvent>(event);
        case EventType::DXF_EXECUTION_PROGRESS:
            return CloneTypedEvent<Siligen::Domain::System::Ports::DXFExecutionProgressEvent>(event);
        case EventType::DXF_EXECUTION_COMPLETED:
            return CloneTypedEvent<Siligen::Domain::System::Ports::DXFExecutionCompletedEvent>(event);
        case EventType::DXF_EXECUTION_FAILED:
            return CloneTypedEvent<Siligen::Domain::System::Ports::DXFExecutionFailedEvent>(event);
        case EventType::DXF_EXECUTION_CANCELLED:
            return CloneTypedEvent<Siligen::Domain::System::Ports::DXFExecutionCancelledEvent>(event);
        case EventType::SOFT_LIMIT_TRIGGERED:
            return CloneTypedEvent<Siligen::Domain::System::Ports::SoftLimitTriggeredEvent>(event);
        case EventType::SOFT_LIMIT_VIOLATION:
            return CloneTypedEvent<Siligen::Domain::System::Ports::SoftLimitViolationEvent>(event);
        case EventType::HARDWARE_CONNECTED:
        case EventType::HARDWARE_DISCONNECTED:
        case EventType::HARDWARE_CONNECTION_FAILED:
            return CloneTypedEvent<Siligen::Domain::System::Ports::HardwareConnectionEvent>(event);
        case EventType::SYSTEM_STATE_CHANGED:
            return CloneTypedEvent<Siligen::Domain::System::Ports::SystemStateChangedEvent>(event);
        case EventType::MOTION_STARTED:
        case EventType::HOMING_STARTED:
        case EventType::HOMING_COMPLETED:
        case EventType::TRIGGER_ACTIVATED:
        case EventType::CONFIGURATION_CHANGED:
        case EventType::DXF_PARSING_STARTED:
        case EventType::DXF_PARSING_COMPLETED:
        case EventType::DXF_PARSING_FAILED:
        case EventType::WORKFLOW_STAGE_CHANGED:
        case EventType::WORKFLOW_STAGE_FAILED:
        case EventType::WORKFLOW_EXPORT_FAILED:
        default:
            return std::make_shared<DomainEvent>(event);
    }
}

}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen
