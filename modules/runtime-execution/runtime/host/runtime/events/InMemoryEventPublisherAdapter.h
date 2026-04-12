#pragma once

#include "runtime/contracts/system/IEventPublisherPort.h"

#include <boost/signals2/connection.hpp>
#include <boost/signals2/signal.hpp>

#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <deque>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

// 显式 using 声明
using Siligen::Domain::System::Ports::IEventPublisherPort;
using Siligen::Domain::System::Ports::DomainEvent;
using Siligen::Domain::System::Ports::EventType;
using Siligen::Domain::System::Ports::EventHandler;

/**
 * @brief 内存事件发布适配器 (MVP 实现)
 *
 * 职责：
 * 1. 实现 IEventPublisherPort 端口接口
 * 2. 提供本地订阅机制 (应用层内部回调)
 * 3. 简单的事件历史记录 (用于调试)
 * 4. 不依赖外部库
 *
 * 适用场景：
 * - 应用内事件分发
 * - TCP/HMI 运行链路
 * - 单元测试
 */
class InMemoryEventPublisherAdapter : public IEventPublisherPort {
public:
    explicit InMemoryEventPublisherAdapter(size_t max_history_size = 100);
    ~InMemoryEventPublisherAdapter() = default;

    // 实现 IEventPublisherPort 接口
    Shared::Types::Result<void> Publish(const DomainEvent& event) override;
    Shared::Types::Result<void> PublishAsync(const DomainEvent& event) override;

    Shared::Types::Result<int32_t> Subscribe(EventType type, EventHandler handler) override;
    Shared::Types::Result<void> Unsubscribe(int32_t subscription_id) override;
    Shared::Types::Result<void> UnsubscribeAll(EventType type) override;

    Shared::Types::Result<std::vector<DomainEvent*>> GetEventHistory(
        EventType type, int32_t max_count = 100) const override;
    Shared::Types::Result<void> ClearEventHistory() override;

private:
    // 事件订阅管理
    using EventSignal = boost::signals2::signal<void(const DomainEvent&)>;
    std::unordered_map<EventType, EventSignal> signals_;
    std::unordered_map<int32_t, boost::signals2::connection> connections_;
    std::unordered_map<int32_t, EventType> subscription_types_;
    std::unordered_map<EventType, std::vector<int32_t>> subscriptions_by_type_;
    int32_t next_subscription_id_ = 1;
    mutable std::mutex subscribers_mutex_;

    // 事件历史记录 (简单队列实现)
    struct EventRecord {
        std::shared_ptr<DomainEvent> event;
    };
    std::deque<EventRecord> event_history_;
    size_t max_history_size_;
    mutable std::mutex history_mutex_;

    /**
     * @brief 本地分发事件（触发订阅者回调）
     */
    void DispatchToLocalSubscribers(const DomainEvent& event);

    /**
     * @brief 记录事件到历史记录
     */
    void RecordEvent(const DomainEvent& event);

    /**
     * @brief 为历史记录复制事件对象，保留常见派生事件字段
     */
    static std::shared_ptr<DomainEvent> CloneEvent(const DomainEvent& event);
};

}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen


