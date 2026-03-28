#pragma once

#include "shared/types/Types.h"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

namespace Siligen {

// 会话信息
struct SessionInfo {
    std::string session_id;
    std::string username;
    UserRole role;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_activity;
    int32 timeout_minutes;
};

// 会话管理器 (内存存储)
class SessionService {
   public:
    explicit SessionService(int32 default_timeout_minutes = 30);

    // 创建会话
    std::string CreateSession(const std::string& username, UserRole role, int32 timeout_minutes = 0);

    // 验证会话
    bool ValidateSession(const std::string& session_id);

    // 更新会话活动时间
    void UpdateActivity(const std::string& session_id);

    // 获取会话信息
    std::optional<SessionInfo> GetSession(const std::string& session_id);

    // 销毁会话
    void DestroySession(const std::string& session_id);

    // 清理过期会话
    void CleanupExpiredSessions();

    // 获取活跃会话数量
    size_t GetActiveSessionCount() const {
        return sessions_.size();
    }

   private:
    int32 default_timeout_minutes_;
    std::unordered_map<std::string, SessionInfo> sessions_;

    // 生成唯一会话ID
    std::string GenerateSessionID();

    // 检查会话是否过期
    bool IsExpired(const SessionInfo& session) const;
};

}  // namespace Siligen

