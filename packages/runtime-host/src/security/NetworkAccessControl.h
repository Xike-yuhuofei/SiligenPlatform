#pragma once

#include "shared/errors/ErrorDescriptions.h"
#include "shared/types/Types.h"
#include "AuditLogger.h"
#include "NetworkWhitelist.h"

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Siligen {

// 连接尝试记录
struct ConnectionAttempt {
    int32 failure_count = 0;
    std::chrono::system_clock::time_point last_attempt;
    std::chrono::system_clock::time_point lockout_until;
};

// 网络访问控制结果
struct AccessControlResult {
    bool allowed = false;
    SystemErrorCode error_code = SystemErrorCode::SUCCESS;
    std::string error_message;
};

// 网络访问控制器
class NetworkAccessControl {
   public:
    NetworkAccessControl(AuditLogger& audit_logger, NetworkWhitelist& whitelist);

    // 检查IP是否允许连接(T033-T036: 白名单+失败计数+临时黑名单+审计日志)
    AccessControlResult CheckAccess(const std::string& ip, const std::string& username = "");

    // 记录连接成功(重置失败计数)
    void RecordSuccess(const std::string& ip);

    // 记录连接失败(增加失败计数,可能触发临时锁定)
    void RecordFailure(const std::string& ip);

    // 配置参数
    void SetMaxFailedAttempts(int32 max_attempts) {
        max_failed_attempts_ = max_attempts;
    }
    void SetLockoutDuration(int32 minutes) {
        lockout_duration_minutes_ = minutes;
    }

    // 获取当前状态
    int32 GetFailureCount(const std::string& ip) const;
    bool IsLockedOut(const std::string& ip) const;
    void ClearAttemptHistory(const std::string& ip);

   private:
    AuditLogger& audit_logger_;
    NetworkWhitelist& whitelist_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ConnectionAttempt> attempts_;

    int32 max_failed_attempts_ = 3;       // 最大失败次数
    int32 lockout_duration_minutes_ = 5;  // 锁定时长(分钟)

    void CleanupExpiredLockouts();
};

}  // namespace Siligen

