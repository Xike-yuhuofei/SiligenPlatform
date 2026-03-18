#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <functional>
#include <string>

namespace Siligen::Domain::Dispensing::Ports {

using Shared::Types::Result;
using Shared::Types::uint32;

/**
 * @brief 任务执行器类型
 * 封装需要异步执行的业务逻辑
 */
using TaskExecutor = std::function<void()>;

/**
 * @brief 任务状态
 */
enum class TaskStatus {
    PENDING,    // 等待执行
    RUNNING,    // 执行中
    COMPLETED,  // 完成
    FAILED,     // 失败
    CANCELLED   // 已取消
};

/**
 * @brief 任务状态查询响应
 */
struct TaskStatusInfo {
    std::string task_id;
    TaskStatus status;
    uint32 progress_percent;
    std::string error_message;

    TaskStatusInfo()
        : task_id(""),
          status(TaskStatus::PENDING),
          progress_percent(0),
          error_message("") {}

    TaskStatusInfo(const std::string& id, TaskStatus s, uint32 progress, const std::string& error = "")
        : task_id(id),
          status(s),
          progress_percent(progress),
          error_message(error) {}
};

/**
 * @brief 任务调度器端口接口
 *
 * 抽象异步任务管理功能,使Application层可以提交后台任务而无需直接依赖线程管理。
 * Infrastructure层的TaskSchedulerAdapter负责实现具体的线程池和任务调度逻辑。
 *
 * 设计原则:
 * - 纯虚接口,无任何实现
 * - 使用Result<T>进行错误处理
 * - 通过TaskID字符串标识任务
 * - 支持任务提交、查询、取消、清理操作
 */
class ITaskSchedulerPort {
   public:
    virtual ~ITaskSchedulerPort() = default;

    /**
     * @brief 提交异步任务
     *
     * @param executor 任务执行器,封装需要异步执行的业务逻辑
     * @return Result<std::string> 成功时返回唯一的任务ID,失败时返回错误信息
     *
     * @note 任务ID用于后续查询状态和取消操作
     */
    virtual Result<std::string> SubmitTask(TaskExecutor executor) = 0;

    /**
     * @brief 查询任务状态
     *
     * @param task_id 任务唯一标识符
     * @return Result<TaskStatusInfo> 成功时返回任务状态信息,任务不存在时返回错误
     */
    virtual Result<TaskStatusInfo> GetTaskStatus(const std::string& task_id) const = 0;

    /**
     * @brief 取消任务
     *
     * @param task_id 任务唯一标识符
     * @return Result<void> 成功取消返回Success,任务不存在或无法取消时返回错误
     *
     * @note 只能取消PENDING或RUNNING状态的任务
     */
    virtual Result<void> CancelTask(const std::string& task_id) = 0;

    /**
     * @brief 清理过期任务
     *
     * 清理已完成、失败或取消的任务记录,防止内存泄漏。
     * Infrastructure层可根据时间戳或任务数量实现清理策略。
     */
    virtual void CleanupExpiredTasks() = 0;
};

}  // namespace Siligen::Domain::Dispensing::Ports

