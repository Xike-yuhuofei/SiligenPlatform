#include "NetworkAccessControl.h"

#include "SecurityLogHelper.h"
#include "shared/errors/ErrorDescriptions.h"

namespace Siligen {

using Shared::Types::LogLevel;

NetworkAccessControl::NetworkAccessControl(AuditLogger& audit_logger, NetworkWhitelist& whitelist)
    : audit_logger_(audit_logger), whitelist_(whitelist) {}

AccessControlResult NetworkAccessControl::CheckAccess(const std::string& ip, const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    CleanupExpiredLockouts();

    AccessControlResult result;

    // T034: 检查临时黑名单(3次失败锁定5分钟)
    auto it = attempts_.find(ip);
    if (it != attempts_.end()) {
        auto now = std::chrono::system_clock::now();
        if (now < it->second.lockout_until) {
            result.allowed = false;
            result.error_code = SystemErrorCode::CONNECTION_BLOCKED;
            result.error_message = GetSystemErrorDescription(SystemErrorCode::CONNECTION_BLOCKED);

            // T035: 网络访问日志 [SECURITY]
            SecurityLogHelper::Log(LogLevel::ERR,
                                   "NetworkAccessControl",
                                   "[SECURITY] Connection blocked (temporary lockout) [IP: " + ip + "]");

            // T036: 连接拒绝的审计日志
            audit_logger_.LogAudit({std::chrono::system_clock::now(),
                                    AuditCategory::NETWORK_ACCESS,
                                    AuditLevel::SECURITY,
                                    AuditStatus::DENIED,
                                    username,
                                    "CONNECTION_BLOCKED",
                                    "IP temporarily locked due to too many failed attempts",
                                    ip});

            return result;
        }
    }

    // T033: 检查白名单
    if (!whitelist_.IsAllowed(ip)) {
        result.allowed = false;
        result.error_code = SystemErrorCode::IP_NOT_WHITELISTED;
        result.error_message = GetSystemErrorDescription(SystemErrorCode::IP_NOT_WHITELISTED);

        // T035: 网络访问日志 [SECURITY]
        SecurityLogHelper::Log(LogLevel::WARNING,
                               "NetworkAccessControl",
                               "[SECURITY] Connection denied (not in whitelist) [IP: " + ip + "]");

        // T036: 连接拒绝的审计日志
        audit_logger_.LogAudit({std::chrono::system_clock::now(),
                                AuditCategory::NETWORK_ACCESS,
                                AuditLevel::SECURITY,
                                AuditStatus::DENIED,
                                username,
                                "IP_NOT_WHITELISTED",
                                "Connection attempt from non-whitelisted IP",
                                ip});

        return result;
    }

    // 通过验证
    result.allowed = true;
    result.error_code = SystemErrorCode::SUCCESS;

    // T035: 访问允许日志
    SecurityLogHelper::Log(LogLevel::INFO, "NetworkAccessControl", "[SECURITY] Connection allowed [IP: " + ip + "]");

    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::NETWORK_ACCESS,
                            AuditLevel::INFO,
                            AuditStatus::SUCCESS,
                            username,
                            "CONNECTION_ALLOWED",
                            "IP in whitelist, access granted",
                            ip});

    return result;
}

void NetworkAccessControl::RecordSuccess(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attempts_.find(ip);
    if (it != attempts_.end()) {
        if (it->second.failure_count > 0) {
            SecurityLogHelper::Log(
                LogLevel::INFO, "NetworkAccessControl", "Connection success, reset failure count for IP: " + ip);
        }
        attempts_.erase(it);
    }
}

void NetworkAccessControl::RecordFailure(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& attempt = attempts_[ip];
    attempt.failure_count++;
    attempt.last_attempt = std::chrono::system_clock::now();

    SecurityLogHelper::Log(LogLevel::WARNING,
                           "NetworkAccessControl",
                           "[SECURITY] Connection failure #" + std::to_string(attempt.failure_count) + " [IP: " + ip +
                               "]");

    // T034: 3次失败触发5分钟锁定
    if (attempt.failure_count >= max_failed_attempts_) {
        attempt.lockout_until = attempt.last_attempt + std::chrono::minutes(lockout_duration_minutes_);

        SecurityLogHelper::Log(LogLevel::ERR,
                               "NetworkAccessControl",
                               "[SECURITY] IP locked for " + std::to_string(lockout_duration_minutes_) + " minutes due to " +
                                   std::to_string(max_failed_attempts_) + " failed attempts [IP: " + ip + "]");

        audit_logger_.LogAudit({std::chrono::system_clock::now(),
                                AuditCategory::NETWORK_ACCESS,
                                AuditLevel::SECURITY,
                                AuditStatus::FAILURE,
                                "",
                                "TEMPORARY_LOCKOUT",
                                "IP locked due to too many failed connection attempts (" +
                                    std::to_string(max_failed_attempts_) + " failures)",
                                ip});
    }
}

int32 NetworkAccessControl::GetFailureCount(const std::string& ip) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = attempts_.find(ip);
    return (it != attempts_.end()) ? it->second.failure_count : 0;
}

bool NetworkAccessControl::IsLockedOut(const std::string& ip) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = attempts_.find(ip);
    if (it == attempts_.end()) return false;

    auto now = std::chrono::system_clock::now();
    return now < it->second.lockout_until;
}

void NetworkAccessControl::ClearAttemptHistory(const std::string& ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    attempts_.erase(ip);
    SecurityLogHelper::Log(LogLevel::INFO, "NetworkAccessControl", "Cleared attempt history for IP: " + ip);
}

void NetworkAccessControl::CleanupExpiredLockouts() {
    auto now = std::chrono::system_clock::now();
    for (auto it = attempts_.begin(); it != attempts_.end();) {
        if (now >= it->second.lockout_until && it->second.failure_count >= max_failed_attempts_) {
            SecurityLogHelper::Log(LogLevel::INFO, "NetworkAccessControl", "Lockout expired for IP: " + it->first);
            it = attempts_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace Siligen

