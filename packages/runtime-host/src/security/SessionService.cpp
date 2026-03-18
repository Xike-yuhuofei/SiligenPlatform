#include "SessionService.h"

#include "SecurityLogHelper.h"

#include <iomanip>
#include <random>
#include <sstream>

namespace Siligen {

using Shared::Types::LogLevel;

SessionService::SessionService(int32 default_timeout_minutes) : default_timeout_minutes_(default_timeout_minutes) {}

std::string SessionService::CreateSession(const std::string& username, UserRole role, int32 timeout_minutes) {
    std::string session_id = GenerateSessionID();

    SessionInfo session;
    session.session_id = session_id;
    session.username = username;
    session.role = role;
    session.created_at = std::chrono::system_clock::now();
    session.last_activity = session.created_at;
    session.timeout_minutes = (timeout_minutes > 0) ? timeout_minutes : default_timeout_minutes_;

    sessions_[session_id] = session;

    SecurityLogHelper::Log(LogLevel::INFO, "SessionService", "创建会话: 用户=" + username + ", 会话ID=" + session_id);

    return session_id;
}

bool SessionService::ValidateSession(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        SecurityLogHelper::Log(LogLevel::WARNING, "SessionService", "会话不存在: " + session_id);
        return false;
    }

    if (IsExpired(it->second)) {
        SecurityLogHelper::Log(
            LogLevel::WARNING, "SessionService", "会话已过期: " + session_id + " (用户: " + it->second.username + ")");
        sessions_.erase(it);
        return false;
    }

    return true;
}

void SessionService::UpdateActivity(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second.last_activity = std::chrono::system_clock::now();
    }
}

std::optional<SessionInfo> SessionService::GetSession(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it != sessions_.end() && !IsExpired(it->second)) {
        return it->second;
    }
    return std::nullopt;
}

void SessionService::DestroySession(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        SecurityLogHelper::Log(
            LogLevel::INFO, "SessionService", "销毁会话: " + session_id + " (用户: " + it->second.username + ")");
        sessions_.erase(it);
    }
}

void SessionService::CleanupExpiredSessions() {
    size_t removed = 0;
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (IsExpired(it->second)) {
            SecurityLogHelper::Log(LogLevel::INFO, "SessionService", "清理过期会话: " + it->second.username);
            it = sessions_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        SecurityLogHelper::Log(LogLevel::INFO, "SessionService", "清理了 " + std::to_string(removed) + " 个过期会话");
    }
}

std::string SessionService::GenerateSessionID() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64> dis;

    uint64 id1 = dis(gen);
    uint64 id2 = dis(gen);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << id1 << std::setw(16) << id2;

    return oss.str();
}

bool SessionService::IsExpired(const SessionInfo& session) const {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - session.last_activity).count();

    return elapsed >= session.timeout_minutes;
}

}  // namespace Siligen
