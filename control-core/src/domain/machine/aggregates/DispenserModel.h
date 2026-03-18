// DispenserModel.h
// 版本: 1.0.0
// 描述: 点胶机领域模型 - 聚合根(Aggregate Root)
//
// 六边形架构 - 领域层模型
// 依赖方向: 依赖shared类型层,不依赖infrastructure层
// Task: T045 - Phase 4 建立清晰的模块边界

#pragma once

#include "shared/types/AxisStatus.h"
#include "shared/types/CMPTypes.h"
#include "shared/types/ConfigTypes.h"
#include "shared/types/DomainEvent.h"
#include "shared/types/Error.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"
#include "shared/util/ModuleDependencyCheck.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Machine {
namespace Aggregates {
namespace Legacy {

// Using declarations for shared types
using ::Siligen::DispenserState;

/// @brief 点胶机状态转字符串
inline const char* DispenserStateToString(DispenserState state) {
    switch (state) {
        case DispenserState::UNINITIALIZED:
            return "UNINITIALIZED";
        case DispenserState::INITIALIZING:
            return "INITIALIZING";
        case DispenserState::READY:
            return "READY";
        case DispenserState::DISPENSING:
            return "DISPENSING";
        case DispenserState::PAUSED:
            return "PAUSED";
        case DispenserState::ERROR_STATE:
            return "ERROR_STATE";
        case DispenserState::EMERGENCY_STOP:
            return "EMERGENCY_STOP";
        default:
            return "UNKNOWN";
    }
}

/// @brief 点胶任务信息
/// @details 封装单个点胶任务的参数和状态
struct DispensingTask {
    std::string task_id;                                  // 任务ID (Task ID)
    std::vector<Siligen::Shared::Types::Point2D> path;    // 点胶路径 (Dispensing path)
    Siligen::Shared::Types::CMPConfiguration cmp_config;  // CMP配置 (CMP configuration)
    float movement_speed;                                 // 移动速度(mm/s) (Movement speed)
    std::chrono::system_clock::time_point created_at;     // 创建时间 (Created at)
    std::chrono::system_clock::time_point started_at;     // 开始时间 (Started at)
    std::chrono::system_clock::time_point completed_at;   // 完成时间 (Completed at)
    bool is_completed;                                    // 是否完成 (Is completed)
    int32_t current_point_index;                          // 当前路径点索引 (Current point index)

    DispensingTask()
        : task_id(""),
          path(),
          cmp_config(),
          movement_speed(0.0f),
          created_at(std::chrono::system_clock::now()),
          started_at(),
          completed_at(),
          is_completed(false),
          current_point_index(0) {}

    // 计算任务进度百分比 (Calculate task progress percentage)
    float GetProgress() const {
        if (path.empty()) return 0.0f;
        return (static_cast<float>(current_point_index) / path.size()) * 100.0f;
    }

    // 获取剩余路径点数量 (Get remaining path points count)
    int32_t GetRemainingPoints() const {
        if (path.empty()) return 0;
        return static_cast<int32_t>(path.size()) - current_point_index;
    }

    // 验证任务是否有效 (Validate if task is valid)
    bool Validate() const {
        if (path.empty()) return false;
        if (movement_speed <= 0.0f) return false;
        if (!cmp_config.Validate()) return false;
        return true;
    }
};

/// @brief 点胶机领域模型(聚合根)
/// @details 封装点胶机的完整业务状态和行为规则
///
/// 领域不变式(Domain Invariants):
/// - 未初始化状态不能执行点胶操作
/// - 错误状态必须清除后才能继续操作
/// - 紧急停止后必须重新初始化
/// - 任务队列最多容纳100个任务
///
/// 状态转换规则:
/// - UNINITIALIZED → INITIALIZING → READY
/// - READY → DISPENSING → READY
/// - DISPENSING → PAUSED → DISPENSING
/// - ANY → ERROR_STATE → READY (after error clear)
/// - ANY → EMERGENCY_STOP → UNINITIALIZED (after reset)
///
/// 使用示例:
/// @code
/// DispenserModel model;
/// model.SetState(DispenserState::INITIALIZING);
/// // ... 初始化操作 ...
/// model.SetState(DispenserState::READY);
/// DispensingTask task;
/// task.path = {Point2D(0, 0), Point2D(100, 100)};
/// model.AddTask(task);
/// model.StartTask(task.task_id);
/// @endcode
class DispenserModel : public Siligen::Shared::Util::DomainModuleTag {
   public:
    /// @brief 默认构造函数
    DispenserModel();

    ~DispenserModel() = default;

    // 禁止拷贝和移动 (领域模型唯一实例)
    DispenserModel(const DispenserModel&) = delete;
    DispenserModel& operator=(const DispenserModel&) = delete;
    DispenserModel(DispenserModel&&) = delete;
    DispenserModel& operator=(DispenserModel&&) = delete;

    // ============================================================
    // 状态管理
    // ============================================================

    /// @brief 设置点胶机状态
    /// @param state 新状态
    /// @return Result<void> 设置结果
    /// @note 会验证状态转换是否合法
    Siligen::Shared::Types::Result<void> SetState(DispenserState state);

    /// @brief 获取当前状态
    /// @return 当前点胶机状态
    DispenserState GetState() const;

    /// @brief 检查是否可以执行点胶操作
    /// @return Result<bool> 是否可以点胶
    /// @note 仅READY和PAUSED状态可以点胶
    Siligen::Shared::Types::Result<bool> CanDispense() const;

    /// @brief 检查是否处于错误状态
    /// @return 是否有错误
    bool HasError() const;

    /// @brief 清除错误状态
    /// @return Result<void> 清除结果
    /// @note 清除后状态转换为READY
    Siligen::Shared::Types::Result<void> ClearError();

    // ============================================================
    // 任务管理
    // ============================================================

    /// @brief 添加点胶任务到队列
    /// @param task 点胶任务
    /// @return Result<void> 添加结果
    /// @note 会验证任务有效性,检查队列容量限制
    Siligen::Shared::Types::Result<void> AddTask(const DispensingTask& task);

    /// @brief 开始执行指定任务
    /// @param task_id 任务ID
    /// @return Result<void> 开始结果
    /// @note 状态会转换为DISPENSING
    Siligen::Shared::Types::Result<void> StartTask(const std::string& task_id);

    /// @brief 暂停当前任务
    /// @return Result<void> 暂停结果
    /// @note 状态会转换为PAUSED
    Siligen::Shared::Types::Result<void> PauseCurrentTask();

    /// @brief 恢复当前任务
    /// @return Result<void> 恢复结果
    /// @note 状态会转换为DISPENSING
    Siligen::Shared::Types::Result<void> ResumeCurrentTask();

    /// @brief 取消指定任务
    /// @param task_id 任务ID
    /// @return Result<void> 取消结果
    Siligen::Shared::Types::Result<void> CancelTask(const std::string& task_id);

    /// @brief 完成当前任务
    /// @return Result<void> 完成结果
    /// @note 状态会转换为READY
    Siligen::Shared::Types::Result<void> CompleteCurrentTask();

    /// @brief 获取当前任务
    /// @return Result<DispensingTask> 当前任务
    Siligen::Shared::Types::Result<DispensingTask> GetCurrentTask() const;

    /// @brief 获取所有待执行任务
    /// @return Result<vector<DispensingTask>> 任务列表
    Siligen::Shared::Types::Result<std::vector<DispensingTask>> GetPendingTasks() const;

    /// @brief 获取任务队列大小
    /// @return Result<int32_t> 队列大小
    Siligen::Shared::Types::Result<int32_t> GetTaskQueueSize() const;

    /// @brief 清空所有任务
    /// @return Result<void> 清空结果
    /// @note 会停止当前任务,清空队列
    Siligen::Shared::Types::Result<void> ClearAllTasks();

    // ============================================================
    // 配置管理
    // ============================================================

    /// @brief 设置运动配置
    /// @param config 运动配置
    /// @return Result<void> 设置结果
    Siligen::Shared::Types::Result<void> SetMotionConfig(const Siligen::Shared::Types::MotionConfig& config);

    /// @brief 获取运动配置
    /// @return 当前运动配置
    const Siligen::Shared::Types::MotionConfig& GetMotionConfig() const;

    /// @brief 设置连接配置
    /// @param config 连接配置
    /// @return Result<void> 设置结果
    Siligen::Shared::Types::Result<void> SetConnectionConfig(const Siligen::Shared::Types::ConnectionConfig& config);

    /// @brief 获取连接配置
    /// @return 当前连接配置
    const Siligen::Shared::Types::ConnectionConfig& GetConnectionConfig() const;

    // ============================================================
    // 统计信息
    // ============================================================

    /// @brief 获取已完成任务总数
    /// @return 已完成任务数
    int32_t GetCompletedTaskCount() const;

    /// @brief 获取总运行时间(秒)
    /// @return 总运行时间
    double GetTotalRunningTime() const;

    /// @brief 获取点胶总路径长度(mm)
    /// @return 总路径长度
    double GetTotalDispensingLength() const;

    /// @brief 重置统计信息
    /// @return Result<void> 重置结果
    Siligen::Shared::Types::Result<void> ResetStatistics();

    // ============================================================
    // 领域规则验证
    // ============================================================

    /// @brief 验证状态转换是否合法
    /// @param from_state 源状态
    /// @param to_state 目标状态
    /// @return Result<void> 验证结果
    static Siligen::Shared::Types::Result<void> ValidateStateTransition(DispenserState from_state,
                                                                        DispenserState to_state);

    /// @brief 验证任务是否可以开始
    /// @param task 待验证的任务
    /// @return Result<void> 验证结果
    Siligen::Shared::Types::Result<void> ValidateTaskStart(const DispensingTask& task) const;

    // ============================================================
    // 调试和诊断
    // ============================================================

    /// @brief 获取模型的JSON表示
    /// @return JSON格式的状态字符串
    std::string ToJson() const;

    /// @brief 获取模型的字符串表示
    /// @return 状态字符串
    std::string ToString() const;

   private:
    // 点胶机当前状态
    DispenserState current_state_;

    // 当前正在执行的任务
    DispensingTask current_task_;

    // 待执行任务队列
    std::vector<DispensingTask> pending_tasks_;

    // 已完成任务队列(保留最近100个)
    std::vector<DispensingTask> completed_tasks_;

    // 运动配置
    Siligen::Shared::Types::MotionConfig motion_config_;

    // 连接配置
    Siligen::Shared::Types::ConnectionConfig connection_config_;

    // 统计信息
    int32_t total_completed_tasks_;                           // 总完成任务数
    double total_running_time_;                               // 总运行时间(秒)
    double total_dispensing_length_;                          // 总点胶路径长度(mm)
    std::chrono::system_clock::time_point model_created_at_;  // 模型创建时间

    // 任务队列容量限制
    static constexpr int32_t MAX_PENDING_TASKS = 100;
    static constexpr int32_t MAX_COMPLETED_TASKS_HISTORY = 100;

    // 辅助方法
    bool IsStateTransitionValid(DispenserState from_state, DispenserState to_state) const;
    void UpdateStatistics(const DispensingTask& completed_task);
    double CalculatePathLength(const std::vector<Siligen::Shared::Types::Point2D>& path) const;
};

}  // namespace Legacy
}  // namespace Aggregates
}  // namespace Machine
}  // namespace Domain
}  // namespace Siligen

