# shared/contracts

`shared/contracts/` 是跨模块稳定契约的 canonical 子根。

## 正式职责

- 存放多个模块共同依赖且需长期稳定维护的契约（schema、protobuf、interface、payload 约定）。
- 承接 `application`、`engineering`、`device` 等跨 owner 契约的公共面，支持下游模块解耦演进。
- 作为 `application`、`engineering`、`device` 契约的唯一工作区公共承载面。

## 不应承载

- 绑定单一业务 owner 的内部 DTO、命令语义或临时字段。
- 实现层代码、设备适配细节、运行时对象状态。
- 未登记版本策略的破坏性改动。

## 迁移与退出规则

- 迁移来源根只保留 bridge；新增或变更跨模块稳定契约必须在 `shared/contracts/` 落位。
- 所有 bridge 必须显式声明目标路径与退出条件，禁止隐式 include/path fallback。
- 旧根退出以 `wave-mapping.md` 与 `validation-gates.md` 的根级门禁为准，不以口头约定为准。
