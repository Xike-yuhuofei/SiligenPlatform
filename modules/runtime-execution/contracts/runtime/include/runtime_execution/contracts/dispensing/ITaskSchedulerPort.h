#pragma once

// Compatibility surface: prefer the canonical dispense-packaging port when visible,
// but keep a local mirror for consumers that only receive runtime-execution contracts.
#if __has_include("domain/dispensing/ports/ITaskSchedulerPort.h")
#include "domain/dispensing/ports/ITaskSchedulerPort.h"
#else
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <functional>
#include <string>

namespace Siligen::Domain::Dispensing::Ports {

using Shared::Types::Result;
using Shared::Types::uint32;

using TaskExecutor = std::function<void()>;

enum class TaskStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED
};

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

class ITaskSchedulerPort {
   public:
    virtual ~ITaskSchedulerPort() = default;

    virtual Result<std::string> SubmitTask(TaskExecutor executor) = 0;
    virtual Result<TaskStatusInfo> GetTaskStatus(const std::string& task_id) const = 0;
    virtual Result<void> CancelTask(const std::string& task_id) = 0;
    virtual void CleanupExpiredTasks() = 0;
};

}  // namespace Siligen::Domain::Dispensing::Ports
#endif
