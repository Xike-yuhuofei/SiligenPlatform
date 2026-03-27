# hmi-application

`M11 hmi-application`

## 模块职责

- 提供用户命令入口、审批入口、状态展示和追溯查询视图。

## Owner 对象

- `UserCommand`
- `ApprovalDecision`
- `UIViewModel`

## 输入契约

- 用户操作命令
- 审批请求
- 查询请求

## 输出契约

- 对 `M0/M9/M10` 的命令转发
- 只读状态展示
- 审批结论与查询结果视图

## 禁止越权边界

- 不得直接写 owner 对象。
- 不得绕过 `M0/M9` 控设备。
- 不得直接改写规划事实。

## 测试入口

- `S09` 命令转发、审批、阻断和状态展示回归
