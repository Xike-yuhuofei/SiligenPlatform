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
    // MVP 简化实现：返回空列表
    // 实际实现需要存储完整的 DomainEvent 对象
    // 当前只存储简化的 EventRecord
    return Result<std::vector<DomainEvent*>>::Success(std::vector<DomainEvent*>{});
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
    event_history_.push_back({
        event.type,
        event.message,
        event.timestamp
    });

    // 限制历史记录大小
    while (event_history_.size() > max_history_size_) {
        event_history_.pop_front();
    }
}

}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen
