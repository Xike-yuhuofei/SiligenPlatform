// TaskSchedulerAdapter.h - 任务调度器适配器
// Phase 2: DXF Dispensing Execution UseCase 重构 - Infrastructure 层
#pragma once

#include "domain/dispensing/ports/ITaskSchedulerPort.h"
#include "shared/types/Result.h"

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {
namespace System {
namespace TaskScheduler {

using Shared::Types::Result;
using Shared::Types::uint32;
using Shared::Types::uint64;
using Shared::Types::Error;
using Shared::Types::ErrorCode;
using Domain::Dispensing::Ports::ITaskSchedulerPort;
using Domain::Dispensing::Ports::TaskExecutor;
using Domain::Dispensing::Ports::TaskStatus;
using Domain::Dispensing::Ports::TaskStatusInfo;

/**
 * @brief 任务执行上下文
 * Infrastructure 层内部类型,不暴露给Domain层
 */
struct TaskExecutionContext {
    std::string task_id;
    TaskExecutor executor;
    std::atomic<TaskStatus> status;
    std::string error_message;
    std::chrono::steady_clock::time_point submit_time;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    std::atomic<uint32> progress_percent{0};

    TaskExecutionContext()
        : status(TaskStatus::PENDING),
          progress_percent(0) {}
};

/**
 * @brief 任务调度器适配器
 *
 * 实现 ITaskSchedulerPort 接口,提供基于线程池的异步任务调度功能。
 *
 * 技术决策:
 * - 使用 C++17 标准库 (std::thread, std::mutex, std::condition_variable)
 * - 线程池大小可配置,默认4个工作线程
 * - 任务队列使用 FIFO 策略
 * - 线程安全:所有公开方法都使用锁保护
 * - 优雅关闭:析构时等待所有任务完成
 *
 * 使用场景:
 * - DXF点胶执行的后台任务
 * - 硬件测试的异步执行
 * - 任何需要后台处理的Use Case
 *
 * 性能考虑:
 * - 使用 std::atomic 管理任务状态,减少锁竞争
 * - 任务完成后不立即删除,需定期调用CleanupExpiredTasks()
 * - 建议任务数量 < 1000,否则考虑使用专业任务调度库
 *
 * 测试建议:
 * - 单元测试使用小线程池(thread_pool_size=2)
 * - 集成测试验证并发安全性
 * - 性能测试验证吞吐量和延迟
 */
class TaskSchedulerAdapter : public ITaskSchedulerPort {
   public:
    /**
     * @brief 构造函数
     *
     * @param thread_pool_size 线程池大小,默认4个工作线程
     *
     * @note 线程池大小应根据CPU核心数和任务特性调整
     */
    explicit TaskSchedulerAdapter(size_t thread_pool_size = 4);

    /**
     * @brief 析构函数
     *
     * 执行优雅关闭:
     * 1. 设置 shutdown 标志
     * 2. 唤醒所有工作线程
     * 3. 等待所有线程join
     */
    ~TaskSchedulerAdapter();

    // 禁止拷贝和移动
    TaskSchedulerAdapter(const TaskSchedulerAdapter&) = delete;
    TaskSchedulerAdapter& operator=(const TaskSchedulerAdapter&) = delete;
    TaskSchedulerAdapter(TaskSchedulerAdapter&&) = delete;
    TaskSchedulerAdapter& operator=(TaskSchedulerAdapter&&) = delete;

    // ========================================
    // ITaskSchedulerPort 接口实现
    // ========================================

    /**
     * @brief 提交异步任务
     *
     * @param executor 任务执行器
     * @return Result<std::string> 成功时返回任务ID,失败时返回错误
     */
    Result<std::string> SubmitTask(TaskExecutor executor) override;

    /**
     * @brief 查询任务状态
     *
     * @param task_id 任务唯一标识符
     * @return Result<TaskStatusInfo> 成功时返回任务状态,任务不存在时返回错误
     */
    Result<TaskStatusInfo> GetTaskStatus(const std::string& task_id) const override;

    /**
     * @brief 取消任务
     *
     * @param task_id 任务唯一标识符
     * @return Result<void> 成功取消返回Success,任务不存在或无法取消时返回错误
     *
     * @note 只能取消PENDING状态的任务(未开始执行)
     * @note RUNNING状态的任务无法取消(C++17标准库不支持线程中断)
     */
    Result<void> CancelTask(const std::string& task_id) override;

    /**
     * @brief 清理过期任务
     *
     * 删除已完成、失败或取消的任务记录,防止内存泄漏。
     * 建议定期调用(如每100个任务提交后调用一次)。
     */
    void CleanupExpiredTasks() override;

   private:
    // ========================================
    // 线程池管理
    // ========================================

    boost::asio::thread_pool thread_pool_;                                     // Boost.Asio线程池

    // ========================================
    // 任务上下文存储
    // ========================================

    std::unordered_map<std::string, std::shared_ptr<TaskExecutionContext>> tasks_;  // 任务映射表
    mutable std::mutex tasks_mutex_;                                                 // 任务表互斥锁

    // ========================================
    // 内部辅助方法
    // ========================================

    /**
     * @brief 生成唯一任务ID
     *
     * @return std::string 任务ID(格式: "task_<timestamp>_<counter>")
     */
    std::string GenerateTaskID() const;

    /**
     * @brief 执行任务包装器
     *
     * @param context 任务上下文
     *
     * 功能:
     * 1. 设置任务状态为RUNNING
     * 2. 记录开始时间
     * 3. 执行任务executor
     * 4. 捕获异常并更新状态
     * 5. 记录结束时间
     */
    void ExecuteTask(std::shared_ptr<TaskExecutionContext> context);

    /**
     * @brief 任务ID计数器
     *
     * 用于生成唯一任务ID,原子操作保证线程安全。
     */
    mutable std::atomic<uint64> task_id_counter_{0};
};

}  // namespace TaskScheduler
}  // namespace System
}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen
