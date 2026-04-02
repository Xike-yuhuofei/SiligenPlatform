# domain

当前阶段 `domain/` 仍是 execution-domain owner 的预留目录，不是 live build root。

在专门迁移启动前：

- job/session/control 的 live owner 仍位于 `application/usecases/**`。
- `runtime/host` 只承接 host core concrete，不重定义 execution-domain owner。
- 不得把来自 `workflow`、`dispense-packaging` 或 app-local shell 的外部 owner 事实伪装成此目录已完成收口。

只有当某项 execution-specific value object、invariant 或纯领域规则被正式迁回 `M9` owner 时，才允许在此目录新增 live 实现。
