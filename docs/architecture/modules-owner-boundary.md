# Modules Owner Boundary

更新时间：`2026-04-23`

本文是 `modules/` owner 边界的当前正式收束页。  
它只保留仍然有效的稳定结论；历史评审过程、证据强度审查和初版判断已下沉到 [history/module-boundary/](./history/module-boundary/)。

## 1. 当前正式结论

- `modules/` 仍然是业务 owner 根，`apps/` 只负责宿主/装配，`shared/` 只负责稳定公共层
- 当前主要问题不是根级规则缺失，而是若干模块的 owner 落地与实现收口未完成
- 当前优先级最高的边界问题集中在 `hmi-application`、`dispense-packaging`、`runtime-execution`
- `workflow` 默认 build slice 已收口为 `M0` canonical/orchestration owner；当前主要问题是正式文档口径与 root 失真分支未同步，而不是 live compat aggregation 仍在默认主线上承载 owner 事实
- `motion-planning` 对 `process-path` 仍存在明确领域模型穿透；问题核心是 consumer 没停在 `process-path` 的 contract/application surface

## 2. 当前优先级

### `P0`

- `hmi-application`：模块 owner 面未实体化，宿主侧仍保留 owner 级启动/门禁语义
- `dispense-packaging`：已吸入部分 planning 职责，并与 `workflow` 形成事实双落点
- `runtime-execution`：当前作用域过宽，执行 owner、host glue、infrastructure glue 尚未收口

### `P1`

- `workflow`：正式文档与治理 Oracle 仍有历史漂移；root 已退役 optional `security` 分支需从默认 owner 叙述中剥离
- `process-path` 与 `motion-planning`：存在明确领域模型穿透

### `P2`

- `topology-feature`：存在目录语义错位
- `process-planning`：当前更像 configuration owner，完整 `ProcessPlan` owner 事实不足
- `trace-diagnostics`：README 声称的 owner 范围大于当前实现落地

## 3. 当前使用规则

1. 需要当前 owner 边界判断时，默认引用本文
2. 若需要查看具体审查证据、初版判断和修订轨迹，再进入 [history/module-boundary/](./history/module-boundary/)
3. 若后续实现发生实质收口，应直接更新本文，而不是继续堆叠新的 review 稿作为正式入口
