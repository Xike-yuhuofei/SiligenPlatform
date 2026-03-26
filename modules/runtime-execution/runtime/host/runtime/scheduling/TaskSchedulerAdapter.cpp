#include "TaskSchedulerAdapter.h"

#include "shared/types/Error.h"

#include <sstream>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {
namespace System {
namespace TaskScheduler {

// ========================================
// Constructor and Destructor
// ========================================

TaskSchedulerAdapter::TaskSchedulerAdapter(size_t thread_pool_size)
    : thread_pool_(thread_pool_size) {}

TaskSchedulerAdapter::~TaskSchedulerAdapter() {
    thread_pool_.join();
}

// ========================================
// ITaskSchedulerPort 接口实现
// ========================================

Result<std::string> TaskSchedulerAdapter::SubmitTask(TaskExecutor executor) {
    if (!executor) {
        return Result<std::string>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "任务执行器为空",
            "TaskSchedulerAdapter::SubmitTask"));
    }

    // 生成任务ID
    auto task_id = GenerateTaskID();

    // 创建任务上下文
    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = task_id;
    context->executor = std::move(executor);
    context->status.store(TaskStatus::PENDING);
    context->submit_time = std::chrono::steady_clock::now();

    // 存储任务上下文
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        tasks_[task_id] = context;
    }

    // 提交到线程池执行
    boost::asio::post(thread_pool_, [this, context]() {
        ExecuteTask(context);
    });

    return Result<std::string>::Success(task_id);
}

Result<TaskStatusInfo> TaskSchedulerAdapter::GetTaskStatus(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return Result<TaskStatusInfo>::Failure(Error(
            ErrorCode::NOT_FOUND,
            "任务不存在: " + task_id,
            "TaskSchedulerAdapter::GetTaskStatus"));
    }

    auto& context = it->second;
    TaskStatusInfo info(
        context->task_id,
        context->status.load(),
        context->progress_percent.load(),
        context->error_message);

    return Result<TaskStatusInfo>::Success(info);
}

Result<void> TaskSchedulerAdapter::CancelTask(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return Result<void>::Failure(Error(
            ErrorCode::NOT_FOUND,
            "任务不存在: " + task_id,
            "TaskSchedulerAdapter::CancelTask"));
    }

    auto& context = it->second;
    auto current_status = context->status.load();

    // 只能取消PENDING状态的任务
    if (current_status != TaskStatus::PENDING) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_STATE,
            "只能取消PENDING状态的任务,当前状态: " + std::to_string(static_cast<int>(current_status)),
            "TaskSchedulerAdapter::CancelTask"));
    }

    // 设置为CANCELLED状态
    context->status.store(TaskStatus::CANCELLED);
    context->end_time = std::chrono::steady_clock::now();

    return Result<void>::Success();
}

void TaskSchedulerAdapter::CleanupExpiredTasks() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    // 收集需要删除的任务ID
    std::vector<std::string> tasks_to_remove;
    for (const auto& pair : tasks_) {
        auto status = pair.second->status.load();
        if (status == TaskStatus::COMPLETED ||
            status == TaskStatus::FAILED ||
            status == TaskStatus::CANCELLED) {
            tasks_to_remove.push_back(pair.first);
        }
    }

    // 删除任务
    for (const auto& task_id : tasks_to_remove) {
        tasks_.erase(task_id);
    }
}

// ========================================
// Private Implementation
// ========================================

std::string TaskSchedulerAdapter::GenerateTaskID() const {
    // 生成唯一ID: "task_<timestamp>_<counter>"
    auto counter = task_id_counter_.fetch_add(1);
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();

    std::ostringstream oss;
    oss << "task_" << timestamp << "_" << counter;
    return oss.str();
}

void TaskSchedulerAdapter::ExecuteTask(std::shared_ptr<TaskExecutionContext> context) {
    if (context->status.load() == TaskStatus::CANCELLED) {
        return;
    }
    // 设置为RUNNING状态
    context->status.store(TaskStatus::RUNNING);
    context->start_time = std::chrono::steady_clock::now();

    try {
        // 执行任务
        context->executor();

        // 设置为COMPLETED状态
        context->status.store(TaskStatus::COMPLETED);
        context->progress_percent.store(100);
    } catch (const std::exception& e) {
        // 捕获异常,设置为FAILED状态
        context->status.store(TaskStatus::FAILED);
        context->error_message = std::string("任务执行异常: ") + e.what();
    } catch (...) {
        // 捕获未知异常
        context->status.store(TaskStatus::FAILED);
        context->error_message = "任务执行发生未知异常";
    }

    // 记录结束时间
    context->end_time = std::chrono::steady_clock::now();
}

}  // namespace TaskScheduler
}  // namespace System
}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen
