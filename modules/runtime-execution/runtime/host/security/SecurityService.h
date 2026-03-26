#pragma once

#include "security/config/SecurityConfigLoader.h"
#include "shared/types/Types.h"
#include "AuditLogger.h"
#include "InterlockMonitor.h"
#include "NetworkAccessControl.h"
#include "NetworkWhitelist.h"
#include "PermissionService.h"
#include "SafetyLimitsValidator.h"
#include "SessionService.h"
#include "UserService.h"

#include <memory>
#include <string>

namespace Siligen {

// 认证结果
struct AuthenticationResult {
    bool success;
    std::string session_id;
    UserRole role;
    std::string error_message;
};

// 权限验证结果
struct AuthorizationResult {
    bool allowed;
    std::string error_message;
};

// SecurityService: 安全管理器统一入口,协调所有安全子系统
class SecurityService {
   public:
    [[deprecated("Use constructor with SecurityConfig object instead")]]
    SecurityService(const std::string& config_file,
                    const std::string& users_file,
                    const std::string& audit_file);

    SecurityService(const SecurityConfig& config,
                    const std::string& users_file,
                    const std::string& audit_file);

    // 初始化所有安全子系统
    bool Initialize();

    // 用户认证和会话管理
    AuthenticationResult AuthenticateUser(const std::string& username,
                                          const std::string& password,
                                          const std::string& ip_address = "");
    bool ValidateSession(const std::string& session_id);
    void UpdateSessionActivity(const std::string& session_id);
    void Logout(const std::string& session_id);
    std::optional<SessionInfo> GetSessionInfo(const std::string& session_id);

    // 权限验证
    AuthorizationResult CheckPermission(const std::string& session_id, Permission permission);
    bool HasPermission(UserRole role, Permission permission);

    // 网络访问控制
    AccessControlResult CheckNetworkAccess(const std::string& ip, const std::string& username = "");
    void RecordNetworkSuccess(const std::string& ip);
    void RecordNetworkFailure(const std::string& ip);

    // 安全限制验证
    ValidationResult ValidateSpeed(float32 speed_mm_s, const std::string& username = "");
    ValidationResult ValidateAcceleration(float32 accel_mm_s2, const std::string& username = "");
    ValidationResult ValidateJerk(float32 jerk_mm_s3, const std::string& username = "");
    ValidationResult ValidatePosition(float32 x_mm, float32 y_mm, const std::string& username = "");
    ValidationResult ValidateAxisPosition(int32 axis, float32 pos_mm, const std::string& username = "");
    ValidationResult ValidateMotionParams(float32 speed_mm_s,
                                          float32 accel_mm_s2,
                                          float32 jerk_mm_s3,
                                          const std::string& username = "");

    // 用户管理 (需要管理员权限)
    bool RegisterUser(const std::string& session_id,
                      const std::string& username,
                      const std::string& password,
                      UserRole role);
    bool ChangePassword(const std::string& session_id,
                        const std::string& old_password,
                        const std::string& new_password);
    bool AdminResetPassword(const std::string& admin_session_id,
                            const std::string& target_username,
                            const std::string& new_password);
    bool UnlockUser(const std::string& admin_session_id, const std::string& target_username);
    bool DeleteUser(const std::string& admin_session_id, const std::string& target_username);

    // 审计日志
    bool LogOperation(const std::string& session_id,
                      const std::string& operation,
                      const std::string& details,
                      AuditStatus status = AuditStatus::SUCCESS);
    bool LogSafetyEvent(const std::string& username, const std::string& event_type, const std::string& details);

    // T056-T059: 审计日志查询和导出
    std::vector<AuditEntry> QueryAuditLogs(const std::string& session_id, const AuditLogger::QueryFilter& filter);
    bool ExportAuditLogs(const std::string& session_id,
                         const std::string& output_file,
                         const std::vector<AuditEntry>& entries);
    bool CleanOldAuditLogs(const std::string& session_id, int32 retention_days = 90);

    // 配置管理
    const SecurityConfig& GetSecurityConfig() const {
        return config_.GetConfig();
    }
    bool ReloadConfig();

    // 统计信息
    size_t GetActiveSessionCount() const;
    bool SaveUsers();

    // T073: 连锁监控
    bool StartInterlockMonitor();
    void StopInterlockMonitor();
    InterlockState GetInterlockState() const;
    bool IsInterlockTriggered() const;

   private:
    std::string config_file_;
    std::string users_file_;
    std::string audit_file_;

    // 安全子系统
    SecurityConfigLoader config_;
    AuditLogger audit_logger_;
    NetworkWhitelist whitelist_;
    std::unique_ptr<UserService> user_mgr_;
    std::unique_ptr<PermissionService> perm_mgr_;
    std::unique_ptr<SessionService> session_mgr_;
    std::unique_ptr<NetworkAccessControl> network_ctrl_;
    std::unique_ptr<SafetyLimitsValidator> safety_validator_;
    std::unique_ptr<InterlockMonitor> interlock_monitor_;

    // 初始化状态
    bool initialized_;

    // 从会话获取用户名
    std::string GetUsernameFromSession(const std::string& session_id);
    UserRole GetRoleFromSession(const std::string& session_id);
};

}  // namespace Siligen

