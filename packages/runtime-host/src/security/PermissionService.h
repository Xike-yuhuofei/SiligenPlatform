#pragma once

#include "shared/errors/ErrorHandler.h"
#include "shared/types/Types.h"

#include <set>

namespace Siligen {

// 权限管理器 - 基于角色的访问控制(RBAC)
class PermissionService {
   public:
    // 移除 Logger 依赖，使用无参构造函数
    PermissionService();

    // 检查角色是否有指定权限
    bool HasPermission(UserRole role, Permission permission) const;

    // 获取角色的所有权限
    std::set<Permission> GetRolePermissions(UserRole role) const;

    // 获取权限名称(用于日志)
    static std::string GetPermissionName(Permission permission);
    static std::string GetRoleName(UserRole role);

   private:
    // 移除 Logger 成员变量
    // 初始化角色权限映射
    void InitializePermissions();

    // 角色权限映射表
    std::set<Permission> operator_permissions_;
    std::set<Permission> supervisor_permissions_;
    std::set<Permission> maintainer_permissions_;
    std::set<Permission> administrator_permissions_;
};

}  // namespace Siligen

