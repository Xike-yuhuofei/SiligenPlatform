# 安全模块约束

## 继承全局约束
C++17 | Windows API | 禁止提交密钥

## 安全原则
- 审计日志: 所有安全相关操作记录
- 权限控制: 操作前验证权限
- 网络白名单: 限制连接来源
- 输入验证: 所有外部输入必验证

## 安全检查点
- 用户认证与会话管理
- 操作权限验证
- 安全限位检查
- 审计日志记录
- 网络访问控制

## 按需加载
- 安全配置: @config/security_config.ini
- 审计日志: @src/infrastructure/security/AuditLogger.h
- 权限管理: @src/infrastructure/security/PermissionManager.h

## 约束检查
- 禁止硬编码密钥/密码
- 禁止绕过安全检查
- 所有操作记录审计日志
- 失败安全(Fail-Safe)原则
- 最小权限原则
