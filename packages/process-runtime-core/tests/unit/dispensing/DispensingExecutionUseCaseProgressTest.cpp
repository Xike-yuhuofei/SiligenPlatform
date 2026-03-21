#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>

#define private public
#include "application/usecases/dispensing/DispensingExecutionUseCase.h"
#undef private

namespace {

using Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using Siligen::Application::UseCases::Dispensing::TaskExecutionContext;
using Siligen::Application::UseCases::Dispensing::TaskState;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlanner;

template <typename T>
std::shared_ptr<T> MakeDummyShared() {
    return std::shared_ptr<T>(reinterpret_cast<T*>(0x1), [](T*) {});
}

DispensingExecutionUseCase CreateExecutionUseCase() {
    return DispensingExecutionUseCase(
        MakeDummyShared<DispensingPlanner>(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        MakeDummyShared<Siligen::Application::Services::DXF::DxfPbPreparationService>());
}

}  // namespace

TEST(DispensingExecutionUseCaseProgressTest, RunningTaskUsesEstimatedProgressFallback) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = "task-progress";
    context->state.store(TaskState::RUNNING);
    context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    context->total_segments.store(9);
    context->executed_segments.store(0);
    context->estimated_execution_ms.store(40000);
    use_case.tasks_[context->task_id] = context;

    auto running_result = use_case.GetTaskStatus(context->task_id);
    ASSERT_TRUE(running_result.IsSuccess());
    const auto running_status = running_result.Value();
    EXPECT_GT(running_status.progress_percent, 0U);
    EXPECT_LT(running_status.progress_percent, 100U);
    EXPECT_GT(running_status.executed_segments, 0U);

    context->state.store(TaskState::PAUSED);
    auto paused_result = use_case.GetTaskStatus(context->task_id);
    ASSERT_TRUE(paused_result.IsSuccess());
    const auto paused_status = paused_result.Value();
    EXPECT_GE(paused_status.progress_percent, running_status.progress_percent);
    EXPECT_GE(paused_status.executed_segments, running_status.executed_segments);
}

TEST(DispensingExecutionUseCaseProgressTest, RunningTaskNeverReportsFullyCompletedProgress) {
    auto use_case = CreateExecutionUseCase();

    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = "task-running-clamp";
    context->state.store(TaskState::RUNNING);
    context->start_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    context->total_segments.store(9);
    context->executed_segments.store(9);
    context->reported_executed_segments.store(9);
    context->reported_progress_percent.store(100);
    context->estimated_execution_ms.store(40000);
    use_case.tasks_[context->task_id] = context;

    auto status_result = use_case.GetTaskStatus(context->task_id);
    ASSERT_TRUE(status_result.IsSuccess());
    const auto status = status_result.Value();
    EXPECT_EQ(status.state, "running");
    EXPECT_LT(status.progress_percent, 100U);
    EXPECT_LT(status.executed_segments, status.total_segments);
}
