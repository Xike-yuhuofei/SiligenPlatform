#include "UserService.h"

#include "SecurityLogHelper.h"
#include "shared/hashing/HashCalculator.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Siligen {

using Shared::Types::LogLevel;

// 账号锁定时长(15分钟)
constexpr int32 LOCKOUT_DURATION_MINUTES = 15;

// 最大失败尝试次数(3次)
constexpr int32 MAX_FAILED_ATTEMPTS = 3;

UserService::UserService(AuditLogger& audit_logger) : audit_logger_(audit_logger) {}

bool UserService::RegisterUser(const std::string& username,
                               const std::string& password,
                               UserRole role,
                               const std::string& admin_username) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (username.empty()) {
        SecurityLogHelper::Log(LogLevel::ERR, "UserService", "Username cannot be empty");
        return false;
    }

    if (users_.find(username) != users_.end()) {
        SecurityLogHelper::Log(LogLevel::ERR, "UserService", "User already exists: " + username);
        return false;
    }

    std::string error_message;
    if (!ValidatePassword(password, error_message)) {
        SecurityLogHelper::Log(LogLevel::ERR, "UserService", error_message);
        return false;
    }

    UserInfo user;
    user.username = username;
    user.password_hash = HashPassword(password);
    user.role = role;
    user.is_locked = false;
    user.failed_attempts = 0;
    user.created_at = std::chrono::system_clock::now();

    users_[username] = user;

    SecurityLogHelper::Log(LogLevel::INFO,
                "UserService",
                "User registered: " + username + ", role: " + std::to_string(static_cast<int32>(role)));

    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::USER_MANAGEMENT,
                            AuditLevel::INFO,
                            AuditStatus::SUCCESS,
                            admin_username,
                            "RegisterUser",
                            "Created user: " + username + ", role: " + std::to_string(static_cast<int32>(role)),
                            ""});

    return true;
}

bool UserService::AuthenticateUser(const std::string& username,
                                   const std::string& password,
                                   const std::string& ip_address) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        SecurityLogHelper::Log(LogLevel::WARNING, "UserService", "Authentication failed: user not found: " + username);
        audit_logger_.LogAuthentication(username, AuditStatus::FAILURE, "User not found", ip_address);
        return false;
    }

    if (IsLockedOut(username)) {
        auto& user = it->second;
        auto now = std::chrono::system_clock::now();
        auto remaining = std::chrono::duration_cast<std::chrono::minutes>(user.locked_until - now).count();

        SecurityLogHelper::Log(LogLevel::WARNING,
                    "UserService",
                    "Account locked: " + username + ", remaining: " + std::to_string(remaining) + " minutes");
        audit_logger_.LogAuthentication(
            username, AuditStatus::DENIED, "Account locked for " + std::to_string(remaining) + " minutes", ip_address);
        return false;
    }

    auto& user = it->second;
    std::string password_hash = HashPassword(password);

    if (user.password_hash != password_hash) {
        RecordFailedAttempt(username);
        SecurityLogHelper::Log(
            LogLevel::WARNING, "UserService", "Authentication failed: incorrect password for user: " + username);
        audit_logger_.LogAuthentication(username, AuditStatus::FAILURE, "Incorrect password", ip_address);
        return false;
    }

    ResetFailedAttempts(username);
    user.last_login = std::chrono::system_clock::now();

    SecurityLogHelper::Log(LogLevel::INFO, "UserService", "User authenticated: " + username);
    audit_logger_.LogAuthentication(username, AuditStatus::SUCCESS, "Login successful", ip_address);

    return true;
}

bool UserService::ChangePassword(const std::string& username,
                                 const std::string& old_password,
                                 const std::string& new_password) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        SecurityLogHelper::Log(LogLevel::ERR, "UserService", "Change password failed: user not found: " + username);
        return false;
    }

    auto& user = it->second;
    std::string old_hash = HashPassword(old_password);

    if (user.password_hash != old_hash) {
        SecurityLogHelper::Log(
            LogLevel::WARNING, "UserService", "Change password failed: incorrect old password for: " + username);
        return false;
    }

    std::string error_message;
    if (!ValidatePassword(new_password, error_message)) {
        SecurityLogHelper::Log(LogLevel::ERR, "UserService", error_message);
        return false;
    }

    user.password_hash = HashPassword(new_password);

    SecurityLogHelper::Log(LogLevel::INFO, "UserService", "Password changed for user: " + username);
    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::USER_MANAGEMENT,
                            AuditLevel::INFO,
                            AuditStatus::SUCCESS,
                            username,
                            "ChangePassword",
                            "Password changed successfully",
                            ""});

    return true;
}

bool UserService::AdminResetPassword(const std::string& admin_username,
                                     const std::string& target_username,
                                     const std::string& new_password) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto admin_it = users_.find(admin_username);
    if (admin_it == users_.end() || admin_it->second.role != UserRole::ADMINISTRATOR) {
        SecurityLogHelper::Log(
            LogLevel::ERR, "UserService", "Reset password denied: not an administrator: " + admin_username);
        return false;
    }

    auto target_it = users_.find(target_username);
    if (target_it == users_.end()) {
        SecurityLogHelper::Log(LogLevel::ERR, "UserService", "Reset password failed: user not found: " + target_username);
        return false;
    }

    std::string error_message;
    if (!ValidatePassword(new_password, error_message)) {
        SecurityLogHelper::Log(LogLevel::ERR, "UserService", error_message);
        return false;
    }

    target_it->second.password_hash = HashPassword(new_password);

    SecurityLogHelper::Log(
        LogLevel::INFO, "UserService", "Password reset by admin: " + admin_username + " for user: " + target_username);
    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::USER_MANAGEMENT,
                            AuditLevel::INFO,
                            AuditStatus::SUCCESS,
                            admin_username,
                            "AdminResetPassword",
                            "Reset password for user: " + target_username,
                            ""});

    return true;
}

bool UserService::UnlockUser(const std::string& admin_username, const std::string& target_username) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto admin_it = users_.find(admin_username);
    if (admin_it == users_.end() || admin_it->second.role != UserRole::ADMINISTRATOR) {
        SecurityLogHelper::Log(
            LogLevel::ERR, "UserService", "Unlock user denied: not an administrator: " + admin_username);
        return false;
    }

    auto target_it = users_.find(target_username);
    if (target_it == users_.end()) {
        SecurityLogHelper::Log(LogLevel::ERR, "UserService", "Unlock user failed: user not found: " + target_username);
        return false;
    }

    auto& user = target_it->second;
    user.is_locked = false;
    user.failed_attempts = 0;

    SecurityLogHelper::Log(
        LogLevel::INFO, "UserService", "User unlocked by admin: " + admin_username + " for user: " + target_username);
    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::USER_MANAGEMENT,
                            AuditLevel::INFO,
                            AuditStatus::SUCCESS,
                            admin_username,
                            "UnlockUser",
                            "Unlocked user: " + target_username,
                            ""});

    return true;
}

bool UserService::DeleteUser(const std::string& admin_username, const std::string& target_username) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto admin_it = users_.find(admin_username);
    if (admin_it == users_.end() || admin_it->second.role != UserRole::ADMINISTRATOR) {
        SecurityLogHelper::Log(
            LogLevel::ERR, "UserService", "Delete user denied: not an administrator: " + admin_username);
        return false;
    }

    auto target_it = users_.find(target_username);
    if (target_it == users_.end()) {
        SecurityLogHelper::Log(LogLevel::ERR, "UserService", "Delete user failed: user not found: " + target_username);
        return false;
    }

    users_.erase(target_it);

    SecurityLogHelper::Log(LogLevel::INFO,
                "UserService",
                "User deleted by admin: " + admin_username + " deleted user: " + target_username);
    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::USER_MANAGEMENT,
                            AuditLevel::INFO,
                            AuditStatus::SUCCESS,
                            admin_username,
                            "DeleteUser",
                            "Deleted user: " + target_username,
                            ""});

    return true;
}

bool UserService::GetUserInfo(const std::string& username, UserInfo& user_info) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }

    user_info = it->second;
    return true;
}

UserRole UserService::GetUserRole(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = users_.find(username);
    if (it == users_.end()) {
        return UserRole::OPERATOR;
    }

    return it->second.role;
}

bool UserService::UserExists(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return users_.find(username) != users_.end();
}

bool UserService::LoadFromFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(filename);
    if (!file.is_open()) {
        SecurityLogHelper::Log(LogLevel::ERR, "UserService", "Failed to open user file: " + filename);
        return false;
    }

    users_.clear();
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string username, password_hash;
        int32 role_int;

        if (!(iss >> username >> password_hash >> role_int)) {
            continue;
        }

        UserInfo user;
        user.username = username;
        user.password_hash = password_hash;
        user.role = static_cast<UserRole>(role_int);
        user.is_locked = false;
        user.failed_attempts = 0;
        user.created_at = std::chrono::system_clock::now();

        users_[username] = user;
    }

    SecurityLogHelper::Log(LogLevel::INFO, "UserService", "Loaded " + std::to_string(users_.size()) + " users from: " + filename);

    return true;
}

bool UserService::SaveToFile(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream file(filename);
    if (!file.is_open()) {
        SecurityLogHelper::Log(LogLevel::ERR, "UserService", "Failed to open user file for writing: " + filename);
        return false;
    }

    file << "# Username PasswordHash Role\n";

    for (const auto& pair : users_) {
        const auto& user = pair.second;
        file << user.username << " " << user.password_hash << " " << static_cast<int32>(user.role) << "\n";
    }

    SecurityLogHelper::Log(LogLevel::INFO, "UserService", "Saved " + std::to_string(users_.size()) + " users to: " + filename);

    return true;
}

bool UserService::ValidatePassword(const std::string& password, std::string& error_message) const {
    if (password.length() < 8) {
        error_message = "Password must be at least 8 characters";
        return false;
    }

    bool has_letter = false;
    bool has_digit = false;

    // 使用 unsigned char 避免 UTF-8 字符导致的负值传入字符分类函数
    for (unsigned char c : password) {
        if (std::isalpha(c)) has_letter = true;
        if (std::isdigit(c)) has_digit = true;
    }

    if (!has_letter || !has_digit) {
        error_message = "Password must contain letters and digits";
        return false;
    }

    return true;
}

void UserService::RecordFailedAttempt(const std::string& username) {
    auto it = users_.find(username);
    if (it == users_.end()) {
        return;
    }

    auto& user = it->second;
    user.failed_attempts++;

    if (user.failed_attempts >= MAX_FAILED_ATTEMPTS) {
        user.is_locked = true;
        user.locked_until = std::chrono::system_clock::now() + std::chrono::minutes(LOCKOUT_DURATION_MINUTES);

        SecurityLogHelper::Log(LogLevel::WARNING, "UserService", "Account locked due to failed attempts: " + username);
    }
}

void UserService::ResetFailedAttempts(const std::string& username) {
    auto it = users_.find(username);
    if (it == users_.end()) {
        return;
    }

    it->second.failed_attempts = 0;
    it->second.is_locked = false;
}

bool UserService::IsLockedOut(const std::string& username) const {
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }

    const auto& user = it->second;
    if (!user.is_locked) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    if (now >= user.locked_until) {
        return false;
    }

    return true;
}

std::string UserService::HashPassword(const std::string& password) const {
    uint32 hash = HashCalculator::CalculateCRC32(password);
    return std::to_string(hash);
}

}  // namespace Siligen


