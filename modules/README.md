# modules

`modules/` 是 `M0-M11` 的正式业务 owner 根。

## 当前状态

- 本目录已承接各业务 owner 的 canonical 骨架、边界与构建入口。
- live owner 实现必须继续向各模块 canonical 子目录收敛，不再回落到 legacy 根。
- `apps/` 只保留宿主与装配职责，`shared/` 只保留跨模块稳定能力。
