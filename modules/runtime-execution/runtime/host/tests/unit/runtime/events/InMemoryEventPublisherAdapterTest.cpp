#include "runtime/events/InMemoryEventPublisherAdapter.h"

#include <gtest/gtest.h>

namespace {

using InMemoryEventPublisherAdapter = Siligen::Infrastructure::Adapters::InMemoryEventPublisherAdapter;
using DomainEvent = Siligen::Domain::System::Ports::DomainEvent;
using EventType = Siligen::Domain::System::Ports::EventType;
using SoftLimitTriggeredEvent = Siligen::Domain::System::Ports::SoftLimitTriggeredEvent;

SoftLimitTriggeredEvent BuildSoftLimitEvent(const std::string& task_id, float position) {
    SoftLimitTriggeredEvent event;
    event.type = EventType::SOFT_LIMIT_TRIGGERED;
    event.task_id = task_id;
    event.position = position;
    event.message = "soft-limit";
    return event;
}

TEST(InMemoryEventPublisherAdapterTest, ReturnsTypedHistoryFromBoundedBuffer) {
    InMemoryEventPublisherAdapter adapter(2);

    ASSERT_TRUE(adapter.Publish(BuildSoftLimitEvent("task-1", 11.0f)).IsSuccess());
    ASSERT_TRUE(adapter.Publish(BuildSoftLimitEvent("task-2", 22.0f)).IsSuccess());
    ASSERT_TRUE(adapter.Publish(BuildSoftLimitEvent("task-3", 33.0f)).IsSuccess());

    auto history_result = adapter.GetEventHistory(EventType::SOFT_LIMIT_TRIGGERED, 10);
    ASSERT_TRUE(history_result.IsSuccess()) << history_result.GetError().GetMessage();
    const auto& history = history_result.Value();
    ASSERT_EQ(history.size(), 2U);

    auto* latest = dynamic_cast<const SoftLimitTriggeredEvent*>(history[0].get());
    auto* previous = dynamic_cast<const SoftLimitTriggeredEvent*>(history[1].get());
    ASSERT_NE(latest, nullptr);
    ASSERT_NE(previous, nullptr);
    EXPECT_EQ(latest->task_id, "task-3");
    EXPECT_FLOAT_EQ(latest->position, 33.0f);
    EXPECT_EQ(previous->task_id, "task-2");
    EXPECT_FLOAT_EQ(previous->position, 22.0f);
}

TEST(InMemoryEventPublisherAdapterTest, ReturnedHistoryRemainsReadableAfterClearAndRepublish) {
    InMemoryEventPublisherAdapter adapter(2);

    ASSERT_TRUE(adapter.Publish(BuildSoftLimitEvent("task-stable", 44.0f)).IsSuccess());

    auto history_result = adapter.GetEventHistory(EventType::SOFT_LIMIT_TRIGGERED, 10);
    ASSERT_TRUE(history_result.IsSuccess()) << history_result.GetError().GetMessage();
    const auto history = history_result.Value();
    ASSERT_EQ(history.size(), 1U);

    ASSERT_TRUE(adapter.ClearEventHistory().IsSuccess());
    ASSERT_TRUE(adapter.Publish(BuildSoftLimitEvent("task-next", 55.0f)).IsSuccess());

    auto* retained = dynamic_cast<const SoftLimitTriggeredEvent*>(history.front().get());
    ASSERT_NE(retained, nullptr);
    EXPECT_EQ(retained->task_id, "task-stable");
    EXPECT_FLOAT_EQ(retained->position, 44.0f);
}

TEST(InMemoryEventPublisherAdapterTest, ClearEventHistoryRemovesStoredEvents) {
    InMemoryEventPublisherAdapter adapter;

    ASSERT_TRUE(adapter.Publish(BuildSoftLimitEvent("task-clear", 12.5f)).IsSuccess());
    ASSERT_TRUE(adapter.ClearEventHistory().IsSuccess());

    auto history_result = adapter.GetEventHistory(EventType::SOFT_LIMIT_TRIGGERED, 10);
    ASSERT_TRUE(history_result.IsSuccess()) << history_result.GetError().GetMessage();
    EXPECT_TRUE(history_result.Value().empty());
}

}  // namespace
