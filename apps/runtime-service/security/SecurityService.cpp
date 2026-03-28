#include "SecurityService.h"

#include "SecurityLogHelper.h"
#include "shared/errors/ErrorDescriptions.h"

namespace Siligen {

using Shared::Types::LogLevel;

SecurityService::SecurityService(const std::string& config_file,
                                 const std::string& users_file,
                                 const std::string& audit_file)
    : config_file_(config_file),
      users_file_(users_file),
      audit_file_(audit_file),
      config_(),
      audit_logger_(audit_file),
      whitelist_(),
      initialized_(false) {}

SecurityService::SecurityService(const SecurityConfig& config,
                                 const std::string& users_file,
                                 const std::string& audit_file)
    : config_file_(""),
      users_file_(users_file),
      audit_file_(audit_file),
      config_(),
      audit_logger_(audit_file),
      whitelist_(),
      initialized_(false) {
    const_cast<SecurityConfig&>(config_.GetConfig()) = config;
}

bool SecurityService::Initialize() {
    SecurityLogHelper::Log(LogLevel::INFO, "SecurityService", "初始化安全管理器...");

    // 1. 加载安全配置
    if (!config_file_.empty()) {
        if (!config_.Load(config_file_)) {
            SecurityLogHelper::Log(LogLevel::ERR, "SecurityService", "加载安全配置失败");
            return false;
        }
    }

    const auto& cfg = config_.GetConfig();

    // 2. 初始化网络白名单
    if (cfg.enable_ip_whitelist) {
        for (const auto& ip : cfg.ip_whitelist) {
            whitelist_.AddIP(ip);
        }
        SecurityLogHelper::Log(
            LogLevel::INFO, "SecurityService", "IP白名单已加载: " + std::to_string(cfg.ip_whitelist.size()) + " 个IP");
    }

    // 3. 初始化子系统
    user_mgr_ = std::make_unique<UserService>(audit_logger_);
    perm_mgr_ = std::make_unique<PermissionService>();
    session_mgr_ = std::make_unique<SessionService>(cfg.session_timeout_minutes);
    network_ctrl_ = std::make_unique<NetworkAccessControl>(audit_logger_, whitelist_);
    safety_validator_ = std::make_unique<SafetyLimitsValidator>(audit_logger_, cfg);

    // T073: 初始化连锁监控器
    interlock_monitor_ = std::make_unique<InterlockMonitor>(audit_logger_);
    InterlockConfig interlock_cfg;
    interlock_cfg.enabled = cfg.interlock_enabled;
    interlock_cfg.emergency_stop_input = cfg.emergency_stop_input;
    interlock_cfg.emergency_stop_active_low = true;
    interlock_cfg.safety_door_input = cfg.safety_door_input;
    interlock_cfg.pressure_sensor_input = cfg.pressure_sensor_input;
    interlock_cfg.temperature_sensor_input = cfg.temperature_sensor_input;
    interlock_cfg.voltage_sensor_input = cfg.voltage_sensor_input;
    interlock_cfg.poll_interval_ms = cfg.interlock_poll_interval_ms;
    interlock_cfg.pressure_critical_low = cfg.pressure_critical_low;
    interlock_cfg.pressure_warning_low = cfg.pressure_warning_low;
    interlock_cfg.pressure_warning_high = cfg.pressure_warning_high;
    interlock_cfg.pressure_critical_high = cfg.pressure_critical_high;
    interlock_cfg.temperature_critical_low = cfg.temperature_critical_low;
    interlock_cfg.temperature_normal_low = cfg.temperature_normal_low;
    interlock_cfg.temperature_normal_high = cfg.temperature_normal_high;
    interlock_cfg.temperature_critical_high = cfg.temperature_critical_high;
    interlock_cfg.voltage_min = cfg.voltage_min;
    interlock_cfg.voltage_max = cfg.voltage_max;
    interlock_cfg.self_test_interval_hours = cfg.interlock_self_test_interval_hours;

    if (!interlock_monitor_->Initialize(interlock_cfg)) {
        SecurityLogHelper::Log(LogLevel::ERR, "SecurityService", "初始化连锁监控器失败");
        return false;
    }

    // 4. 加载用户数据
    if (!user_mgr_->LoadFromFile(users_file_)) {
        SecurityLogHelper::Log(LogLevel::WARNING, "SecurityService", "加载用户文件失败,将创建默认管理员账户");
        // 创建默认管理员 (admin/admin123)
        if (!user_mgr_->RegisterUser("admin", "admin123", UserRole::ADMINISTRATOR, "系统")) {
            SecurityLogHelper::Log(LogLevel::ERR, "SecurityService", "创建默认管理员失败");
            return false;
        }
        user_mgr_->SaveToFile(users_file_);
    }

    initialized_ = true;
    SecurityLogHelper::Log(LogLevel::INFO, "SecurityService", "安全管理器初始化完成");
    return true;
}

AuthenticationResult SecurityService::AuthenticateUser(const std::string& username,
                                                       const std::string& password,
                                                       const std::string& ip_address) {
    AuthenticationResult result;
    result.success = false;

    if (!initialized_) {
        result.error_message = "安全管理器未初始化";
        return result;
    }

    // 1. 检查网络访问
    auto net_result = network_ctrl_->CheckAccess(ip_address, username);
    if (!net_result.allowed) {
        result.error_message = net_result.error_message;
        network_ctrl_->RecordFailure(ip_address);
        return result;
    }

    // 2. 用户认证
    if (!user_mgr_->AuthenticateUser(username, password, ip_address)) {
        result.error_message = "用户名或密码错误";
        network_ctrl_->RecordFailure(ip_address);
        return result;
    }

    // 3. 创建会话
    UserRole role = user_mgr_->GetUserRole(username);
    result.session_id = session_mgr_->CreateSession(username, role);
    result.role = role;
    result.success = true;

    network_ctrl_->RecordSuccess(ip_address);

    SecurityLogHelper::Log(LogLevel::INFO,
                "SecurityService",
                "[SECURITY] 用户登录成功 [用户: " + username + ", 角色: " + PermissionService::GetRoleName(role) +
                    ", 会话: " + result.session_id + "]");

    return result;
}

bool SecurityService::ValidateSession(const std::string& session_id) {
    if (!initialized_) return false;

    bool valid = session_mgr_->ValidateSession(session_id);
    if (valid) {
        session_mgr_->CleanupExpiredSessions();
    }
    return valid;
}

void SecurityService::UpdateSessionActivity(const std::string& session_id) {
    if (!initialized_) return;
    session_mgr_->UpdateActivity(session_id);
}

void SecurityService::Logout(const std::string& session_id) {
    if (!initialized_) return;

    auto session = session_mgr_->GetSession(session_id);
    if (session.has_value()) {
        SecurityLogHelper::Log(LogLevel::INFO,
                    "SecurityService",
                    "[SECURITY] 用户登出 [用户: " + session->username + ", 会话: " + session_id + "]");

        audit_logger_.LogAudit({std::chrono::system_clock::now(),
                                AuditCategory::USER_MANAGEMENT,
                                AuditLevel::INFO,
                                AuditStatus::SUCCESS,
                                session->username,
                                "Logout",
                                "User logged out",
                                ""});
    }

    session_mgr_->DestroySession(session_id);
}

std::optional<SessionInfo> SecurityService::GetSessionInfo(const std::string& session_id) {
    if (!initialized_) return std::nullopt;
    return session_mgr_->GetSession(session_id);
}

AuthorizationResult SecurityService::CheckPermission(const std::string& session_id, Permission permission) {
    AuthorizationResult result;
    result.allowed = false;

    if (!initialized_) {
        result.error_message = "安全管理器未初始化";
        return result;
    }

    // 1. 验证会话
    if (!session_mgr_->ValidateSession(session_id)) {
        result.error_message = "会话无效或已过期";
        return result;
    }

    // 2. 获取用户角色
    auto session = session_mgr_->GetSession(session_id);
    if (!session.has_value()) {
        result.error_message = "无法获取会话信息";
        return result;
    }

    // 3. 检查权限
    if (!perm_mgr_->HasPermission(session->role, permission)) {
        result.error_message = "权限不足: 需要 " + PermissionService::GetPermissionName(permission);

        SecurityLogHelper::Log(LogLevel::WARNING,
                    "SecurityService",
                    "[SECURITY] 权限拒绝 [用户: " + session->username +
                        ", 角色: " + PermissionService::GetRoleName(session->role) +
                        ", 操作: " + PermissionService::GetPermissionName(permission) + "]");

        audit_logger_.LogAuthorization(session->username,
                                       AuditStatus::DENIED,
                                       PermissionService::GetPermissionName(permission),
                                       "Permission denied");
        return result;
    }

    // 4. 更新会话活动时间
    session_mgr_->UpdateActivity(session_id);
    result.allowed = true;
    return result;
}

bool SecurityService::HasPermission(UserRole role, Permission permission) {
    if (!initialized_) return false;
    return perm_mgr_->HasPermission(role, permission);
}

AccessControlResult SecurityService::CheckNetworkAccess(const std::string& ip, const std::string& username) {
    if (!initialized_) {
        AccessControlResult result;
        result.allowed = false;
        result.error_code = static_cast<SystemErrorCode>(-6);
        result.error_message = "安全管理器未初始化";
        return result;
    }
    return network_ctrl_->CheckAccess(ip, username);
}

void SecurityService::RecordNetworkSuccess(const std::string& ip) {
    if (!initialized_) return;
    network_ctrl_->RecordSuccess(ip);
}

void SecurityService::RecordNetworkFailure(const std::string& ip) {
    if (!initialized_) return;
    network_ctrl_->RecordFailure(ip);
}

ValidationResult SecurityService::ValidateSpeed(float32 speed_mm_s, const std::string& username) {
    if (!initialized_) {
        return ValidationResult{false, "安全管理器未初始化", static_cast<SystemErrorCode>(-6)};
    }
    return safety_validator_->ValidateSpeed(speed_mm_s, username);
}

ValidationResult SecurityService::ValidateAcceleration(float32 accel_mm_s2, const std::string& username) {
    if (!initialized_) {
        return ValidationResult{false, "安全管理器未初始化", static_cast<SystemErrorCode>(-6)};
    }
    return safety_validator_->ValidateAcceleration(accel_mm_s2, username);
}

ValidationResult SecurityService::ValidateJerk(float32 jerk_mm_s3, const std::string& username) {
    if (!initialized_) {
        return ValidationResult{false, "安全管理器未初始化", static_cast<SystemErrorCode>(-6)};
    }
    return safety_validator_->ValidateJerk(jerk_mm_s3, username);
}

ValidationResult SecurityService::ValidatePosition(float32 x_mm, float32 y_mm, const std::string& username) {
    if (!initialized_) {
        return ValidationResult{false, "安全管理器未初始化", static_cast<SystemErrorCode>(-6)};
    }
    return safety_validator_->ValidatePosition(x_mm, y_mm, username);
}

ValidationResult SecurityService::ValidateAxisPosition(int32 axis, float32 pos_mm, const std::string& username) {
    if (!initialized_) {
        return ValidationResult{false, "安全管理器未初始化", static_cast<SystemErrorCode>(-6)};
    }
    return safety_validator_->ValidateAxisPosition(axis, pos_mm, username);
}

ValidationResult SecurityService::ValidateMotionParams(float32 speed_mm_s,
                                                       float32 accel_mm_s2,
                                                       float32 jerk_mm_s3,
                                                       const std::string& username) {
    if (!initialized_) {
        return ValidationResult{false, "安全管理器未初始化", static_cast<SystemErrorCode>(-6)};
    }
    return safety_validator_->ValidateMotionParams(speed_mm_s, accel_mm_s2, jerk_mm_s3, username);
}

bool SecurityService::RegisterUser(const std::string& session_id,
                                   const std::string& username,
                                   const std::string& password,
                                   UserRole role) {
    if (!initialized_) return false;

    // 验证管理员权限
    auto auth = CheckPermission(session_id, Permission::USER_MANAGEMENT);
    if (!auth.allowed) {
        SecurityLogHelper::Log(LogLevel::WARNING, "SecurityService", "注册用户失败: " + auth.error_message);
        return false;
    }

    std::string admin_username = GetUsernameFromSession(session_id);
    return user_mgr_->RegisterUser(username, password, role, admin_username);
}

bool SecurityService::ChangePassword(const std::string& session_id,
                                     const std::string& old_password,
                                     const std::string& new_password) {
    if (!initialized_) return false;

    if (!ValidateSession(session_id)) {
        SecurityLogHelper::Log(LogLevel::WARNING, "SecurityService", "修改密码失败: 会话无效");
        return false;
    }

    std::string username = GetUsernameFromSession(session_id);
    return user_mgr_->ChangePassword(username, old_password, new_password);
}

bool SecurityService::AdminResetPassword(const std::string& admin_session_id,
                                         const std::string& target_username,
                                         const std::string& new_password) {
    if (!initialized_) return false;

    // 验证管理员权限
    auto auth = CheckPermission(admin_session_id, Permission::USER_MANAGEMENT);
    if (!auth.allowed) {
        SecurityLogHelper::Log(LogLevel::WARNING, "SecurityService", "重置密码失败: " + auth.error_message);
        return false;
    }

    std::string admin_username = GetUsernameFromSession(admin_session_id);
    return user_mgr_->AdminResetPassword(admin_username, target_username, new_password);
}

bool SecurityService::UnlockUser(const std::string& admin_session_id, const std::string& target_username) {
    if (!initialized_) return false;

    // 验证管理员权限
    auto auth = CheckPermission(admin_session_id, Permission::USER_MANAGEMENT);
    if (!auth.allowed) {
        SecurityLogHelper::Log(LogLevel::WARNING, "SecurityService", "解锁用户失败: " + auth.error_message);
        return false;
    }

    std::string admin_username = GetUsernameFromSession(admin_session_id);
    return user_mgr_->UnlockUser(admin_username, target_username);
}

bool SecurityService::DeleteUser(const std::string& admin_session_id, const std::string& target_username) {
    if (!initialized_) return false;

    // 验证管理员权限
    auto auth = CheckPermission(admin_session_id, Permission::USER_MANAGEMENT);
    if (!auth.allowed) {
        SecurityLogHelper::Log(LogLevel::WARNING, "SecurityService", "删除用户失败: " + auth.error_message);
        return false;
    }

    std::string admin_username = GetUsernameFromSession(admin_session_id);
    return user_mgr_->DeleteUser(admin_username, target_username);
}

bool SecurityService::LogOperation(const std::string& session_id,
                                   const std::string& operation,
                                   const std::string& details,
                                   AuditStatus status) {
    if (!initialized_) return false;

    auto session = session_mgr_->GetSession(session_id);
    if (!session.has_value()) {
        return false;
    }

    return audit_logger_.LogAudit({std::chrono::system_clock::now(),
                                   AuditCategory::MOTION_CONTROL,
                                   AuditLevel::INFO,
                                   status,
                                   session->username,
                                   operation,
                                   details,
                                   ""});
}

bool SecurityService::LogSafetyEvent(const std::string& username,
                                     const std::string& event_type,
                                     const std::string& details) {
    if (!initialized_) return false;
    return audit_logger_.LogSafetyEvent(username, event_type, details);
}

bool SecurityService::ReloadConfig() {
    if (!config_.Load(config_file_)) {
        SecurityLogHelper::Log(LogLevel::ERR, "SecurityService", "重新加载配置失败");
        return false;
    }

    const auto& cfg = config_.GetConfig();
    safety_validator_->UpdateConfig(cfg);

    SecurityLogHelper::Log(LogLevel::INFO, "SecurityService", "安全配置已重新加载");
    return true;
}

size_t SecurityService::GetActiveSessionCount() const {
    if (!initialized_) return 0;
    return session_mgr_->GetActiveSessionCount();
}

bool SecurityService::SaveUsers() {
    if (!initialized_) return false;
    return user_mgr_->SaveToFile(users_file_);
}

std::string SecurityService::GetUsernameFromSession(const std::string& session_id) {
    auto session = session_mgr_->GetSession(session_id);
    return session.has_value() ? session->username : "";
}

UserRole SecurityService::GetRoleFromSession(const std::string& session_id) {
    auto session = session_mgr_->GetSession(session_id);
    return session.has_value() ? session->role : UserRole::OPERATOR;
}

// T059: 查询审计日志 (带审计记录)
std::vector<AuditEntry> SecurityService::QueryAuditLogs(const std::string& session_id,
                                                        const AuditLogger::QueryFilter& filter) {
    if (!initialized_) return {};

    // 权限检查：VIEW_AUDIT_LOG 权限(维护员及以上)
    auto auth = CheckPermission(session_id, Permission::VIEW_AUDIT_LOG);
    if (!auth.allowed) {
        SecurityLogHelper::Log(LogLevel::WARNING, "SecurityService", "查询审计日志失败: 权限不足");
        return {};
    }

    std::string username = GetUsernameFromSession(session_id);

    // 构建时间范围字符串(简化版)
    auto start_t = std::chrono::system_clock::to_time_t(filter.start_time);
    auto end_t = std::chrono::system_clock::to_time_t(filter.end_time);
    char start_buf[32], end_buf[32];
    std::strftime(start_buf, sizeof(start_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&start_t));
    std::strftime(end_buf, sizeof(end_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&end_t));

    // 记录审计日志查询操作
    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::USER_MANAGEMENT,
                            AuditLevel::INFO,
                            AuditStatus::SUCCESS,
                            username,
                            "查询审计日志",
                            "时间范围: " + std::string(start_buf) + " 至 " + std::string(end_buf),
                            ""});

    // 执行查询
    return audit_logger_.QueryLogs(filter);
}

// T059: 导出审计日志 (带审计记录)
bool SecurityService::ExportAuditLogs(const std::string& session_id,
                                      const std::string& output_file,
                                      const std::vector<AuditEntry>& entries) {
    if (!initialized_) return false;

    // 权限检查：EXPORT_DATA 权限(管理员)
    auto auth = CheckPermission(session_id, Permission::EXPORT_DATA);
    if (!auth.allowed) {
        SecurityLogHelper::Log(LogLevel::WARNING, "SecurityService", "导出审计日志失败: 权限不足");
        return false;
    }

    std::string username = GetUsernameFromSession(session_id);

    // 记录审计日志导出操作
    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::USER_MANAGEMENT,
                            AuditLevel::INFO,
                            AuditStatus::SUCCESS,
                            username,
                            "导出审计日志",
                            "文件: " + output_file + ", 记录数: " + std::to_string(entries.size()),
                            ""});

    // 执行导出
    return audit_logger_.ExportToJSON(output_file, entries);
}

// T059: 清理旧日志 (带审计记录)
bool SecurityService::CleanOldAuditLogs(const std::string& session_id, int32 retention_days) {
    if (!initialized_) return false;

    // 权限检查：需要 ADMINISTRATOR 权限
    auto auth = CheckPermission(session_id, Permission::USER_MANAGEMENT);
    if (!auth.allowed) {
        SecurityLogHelper::Log(LogLevel::WARNING, "SecurityService", "清理旧日志失败: 权限不足");
        return false;
    }

    std::string username = GetUsernameFromSession(session_id);

    // 记录审计日志清理操作
    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::SYSTEM_CONFIG,
                            AuditLevel::INFO,
                            AuditStatus::SUCCESS,
                            username,
                            "清理旧审计日志",
                            "保留天数: " + std::to_string(retention_days),
                            ""});

    // 执行清理
    return audit_logger_.CleanOldLogs(retention_days);
}

// T073: 连锁监控器接口实现
bool SecurityService::StartInterlockMonitor() {
    if (!initialized_ || !interlock_monitor_) {
        SecurityLogHelper::Log(LogLevel::ERR, "SecurityService", "连锁监控器未初始化");
        return false;
    }

    interlock_monitor_->Start();
    SecurityLogHelper::Log(LogLevel::INFO, "SecurityService", "连锁监控器已启动");
    return true;
}

void SecurityService::StopInterlockMonitor() {
    if (interlock_monitor_) {
        interlock_monitor_->Stop();
        SecurityLogHelper::Log(LogLevel::INFO, "SecurityService", "连锁监控器已停止");
    }
}

InterlockState SecurityService::GetInterlockState() const {
    if (interlock_monitor_) {
        return interlock_monitor_->GetState();
    }
    return InterlockState();
}

bool SecurityService::IsInterlockTriggered() const {
    if (interlock_monitor_) {
        return interlock_monitor_->IsTriggered();
    }
    return false;
}

}  // namespace Siligen

