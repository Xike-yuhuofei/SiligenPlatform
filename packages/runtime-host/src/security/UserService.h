#pragma once

#include "shared/types/Types.h"
#include "AuditLogger.h"

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Siligen {

// 用户信息
struct UserInfo {
    std::string username;
    std::string password_hash;
    UserRole role;
    bool is_locked;
    std::chrono::system_clock::time_point locked_until;
    int32 failed_attempts;
    std::chrono::system_clock::time_point last_login;
    std::chrono::system_clock::time_point created_at;
};

// 用户管理器 - 用户注册、认证、密码修改、账号锁定
class UserService {
   public:
    explicit UserService(AuditLogger& audit_logger);

    // 用户注册
    bool RegisterUser(const std::string& username,
                      const std::string& password,
                      UserRole role,
                      const std::string& admin_username);

    // 用户认证
    bool AuthenticateUser(const std::string& username, const std::string& password, const std::string& ip_address = "");

    // 修改密码
    bool ChangePassword(const std::string& username, const std::string& old_password, const std::string& new_password);

    // 管理员强制修改密码
    bool AdminResetPassword(const std::string& admin_username,
                            const std::string& target_username,
                            const std::string& new_password);

    // 解锁用户
    bool UnlockUser(const std::string& admin_username, const std::string& target_username);

    // 删除用户
    bool DeleteUser(const std::string& admin_username, const std::string& target_username);

    // 查询用户信息
    bool GetUserInfo(const std::string& username, UserInfo& user_info) const;

    // 获取用户角色
    UserRole GetUserRole(const std::string& username) const;

    // 检查用户是否存在
    bool UserExists(const std::string& username) const;

    // 从配置文件加载用户
    bool LoadFromFile(const std::string& filename);

    // 保存用户到配置文件
    bool SaveToFile(const std::string& filename) const;

   private:
    AuditLogger& audit_logger_;
    std::unordered_map<std::string, UserInfo> users_;
    mutable std::mutex mutex_;

    // 密码验证
    bool ValidatePassword(const std::string& password, std::string& error_message) const;

    // 记录失败尝试
    void RecordFailedAttempt(const std::string& username);

    // 重置失败计数
    void ResetFailedAttempts(const std::string& username);

    // 检查是否锁定
    bool IsLockedOut(const std::string& username) const;

    // 计算密码哈希
    std::string HashPassword(const std::string& password) const;
};

}  // namespace Siligen

