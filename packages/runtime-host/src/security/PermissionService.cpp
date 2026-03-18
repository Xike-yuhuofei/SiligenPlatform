#include "PermissionService.h"

#include "SecurityLogHelper.h"
#include "shared/errors/ErrorHandler.h"

namespace Siligen {

using Shared::Types::LogLevel;

PermissionService::PermissionService() {
    InitializePermissions();
}

void PermissionService::InitializePermissions() {
    // 操作员权限 (基础操作)
    operator_permissions_ = {
        Permission::VIEW_STATUS, Permission::JOG_MOTION, Permission::HOMING, Permission::DISPENSING};

    // 主管权限 (操作员 + 配置管理)
    supervisor_permissions_ = operator_permissions_;
    supervisor_permissions_.insert(
        {Permission::CONFIGURE_PARAMETERS, Permission::MANAGE_TASKS, Permission::LOAD_FILES});

    // 维护员权限 (主管 + 诊断维护)
    maintainer_permissions_ = supervisor_permissions_;
    maintainer_permissions_.insert(
        {Permission::SYSTEM_DIAGNOSTICS, Permission::CALIBRATION, Permission::VIEW_AUDIT_LOG});

    // 管理员权限 (所有权限)
    administrator_permissions_ = maintainer_permissions_;
    administrator_permissions_.insert(
        {Permission::USER_MANAGEMENT, Permission::SYSTEM_CONFIG, Permission::SECURITY_CONFIG, Permission::EXPORT_DATA});
}

bool PermissionService::HasPermission(UserRole role, Permission permission) const {
    const std::set<Permission>* perms = nullptr;

    switch (role) {
        case UserRole::OPERATOR:
            perms = &operator_permissions_;
            break;
        case UserRole::SUPERVISOR:
            perms = &supervisor_permissions_;
            break;
        case UserRole::MAINTAINER:
            perms = &maintainer_permissions_;
            break;
        case UserRole::ADMINISTRATOR:
        case UserRole::SUPER_ADMIN:
            perms = &administrator_permissions_;
            break;
        default:
            return false;
    }

    bool has_permission = perms->find(permission) != perms->end();

    if (!has_permission) {
        SecurityLogHelper::Log(LogLevel::WARNING,
                               "PermissionService",
                               GetRoleName(role) + " 缺少权限: " + GetPermissionName(permission));
    }

    return has_permission;
}

std::set<Permission> PermissionService::GetRolePermissions(UserRole role) const {
    switch (role) {
        case UserRole::OPERATOR:
            return operator_permissions_;
        case UserRole::SUPERVISOR:
            return supervisor_permissions_;
        case UserRole::MAINTAINER:
            return maintainer_permissions_;
        case UserRole::ADMINISTRATOR:
        case UserRole::SUPER_ADMIN:
            return administrator_permissions_;
        default:
            return {};
    }
}

std::string PermissionService::GetPermissionName(Permission permission) {
    switch (permission) {
        case Permission::VIEW_STATUS:
            return "查看状态";
        case Permission::JOG_MOTION:
            return "点动操作";
        case Permission::HOMING:
            return "回零操作";
        case Permission::DISPENSING:
            return "点胶操作";
        case Permission::CONFIGURE_PARAMETERS:
            return "参数配置";
        case Permission::MANAGE_TASKS:
            return "任务管理";
        case Permission::LOAD_FILES:
            return "文件加载";
        case Permission::SYSTEM_DIAGNOSTICS:
            return "系统诊断";
        case Permission::CALIBRATION:
            return "系统校准";
        case Permission::VIEW_AUDIT_LOG:
            return "查看审计日志";
        case Permission::USER_MANAGEMENT:
            return "用户管理";
        case Permission::SYSTEM_CONFIG:
            return "系统配置";
        case Permission::SECURITY_CONFIG:
            return "安全配置";
        case Permission::EXPORT_DATA:
            return "数据导出";
        default:
            return "未知权限";
    }
}

std::string PermissionService::GetRoleName(UserRole role) {
    switch (role) {
        case UserRole::OPERATOR:
            return "操作员";
        case UserRole::SUPERVISOR:
            return "主管";
        case UserRole::MAINTAINER:
            return "维护员";
        case UserRole::ADMINISTRATOR:
        case UserRole::SUPER_ADMIN:
            return "管理员";
        default:
            return "未知角色";
    }
}

}  // namespace Siligen

