# Changelog

`SiligenSuite` 的版本日志以根仓为中心维护，不按子项目分别发版。

格式约定：

- 版本标题：`## [x.y.z] - YYYY-MM-DD` 或 `## [x.y.z-rc.n] - YYYY-MM-DD`
- 每个版本至少包含：`Added`、`Changed`、`Fixed`、`Known Issues`、`Rollback Notes`
- apps、contracts、integration 的变化必须写进同一个根仓版本条目

当前状态：

- 尚未形成任何正式根仓 release tag
- 当前仍处于重构后首个发布基线准备阶段

## [Unreleased]

### Added

- 无。

### Changed

- 无。

### Fixed

- 无。

### Known Issues

- 无。

### Rollback Notes

- 无。

## [0.1.0-rc.1] - 2026-03-24

### Added

- 建立基于当前 canonical workspace 的首个根仓候选发布基线，用于冻结 `workspace template refactor` 与架构重构后的仓内验证口径。
- 新增面向根仓的本地验证与 release evidence 入口，统一收敛 `build.ps1`、`test.ps1`、`ci.ps1`、`scripts/validation/run-local-validation-gate.ps1` 与 `release-check.ps1`。
- 补充 `shared/`、`tools/`、`scripts/`、`cmake/` 等重构根目录对应的门禁与说明资产，为后续 owner root 切换提供仓内基线。

### Changed

- 根仓 release 口径从旧的 `Unreleased` 占位描述，更新为 `0.1.0-rc.1` 候选版条目，并要求后续以根仓版本为唯一对外版本。
- `control-runtime`、`control-tcp-server`、`control-cli` 的默认执行与验证口径继续对齐 canonical control-apps build root，不再把 legacy fallback 视作正常 release 路径。
- 发布流程明确以仓内验证为主：先执行本地 validation gate，再执行 `release-check.ps1` 生成 `integration/reports/releases/<version>/` 证据。

### Fixed

- 移除 `No commits yet` 的过时阻塞叙述；根仓当前已有可用于候选版审查的提交锚点。
- 移除 `control-runtime dry-run BLOCKED`、`CLI/TCP 依赖 legacy 产物` 等与当前 canonical cutover 文档不一致的旧版本日志表述。
- 将发布风险重新定位为“当前候选版仍需范围冻结与证据收口”，而不是继续沿用已失效的 pre-cutover blocker。

### Known Issues

- 本次 `RC` 审查基线是 `2026-03-24` 的当前工作树快照，而不是已经白名单收口的最终发布提交；即使 gate 通过，也不能直接等同于 merge / tag 就绪。
- 当前候选版同时覆盖产品改动、验证脚本改动和流程资产改动，后续如要对外发布，仍需拆分发布必要项与治理项。
- 仓外交付包、回滚包、现场脚本观察与长期硬件验证，不属于本轮仓内 `RC` 必过 gate，需在后续正式发布前单独补齐。
- 兼容壳入口 `apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli` 仍在仓内承担 live 入口职责，而 `apps/runtime-service`、`apps/runtime-gateway`、`apps/planner-cli` 的 owner 化切换仍处于推进中。

### Rollback Notes

- `0.1.0-rc.1` 的最小回滚点应由四件套共同定义：提交 hash、release evidence、`config/` 与 `data/recipes/` 快照，以及 canonical control-apps 构建产物快照。
- 本轮若仅完成仓内 `RC evidence`，则只能声称“候选版可回放”，不能声称“已形成现场验证完备的稳定回滚点”。
