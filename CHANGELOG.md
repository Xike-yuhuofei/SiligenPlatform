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

- 建立根仓版本治理基线、release gate、rollback point 定义。
- 新增 `docs/runtime/release-process.md`。
- 新增 `docs/decisions/ADR-003-versioning-and-release.md`。

### Changed

- 明确根仓版本号、tag、changelog、release evidence 以根仓为唯一中心。
- 明确发布流程必须覆盖 apps、contracts、integration 报告。

### Fixed

- 无。

### Known Issues

- 根仓当前 `No commits yet`，还没有可打 tag 的正式提交锚点。
- `apps/control-runtime` 仍无独立可执行文件，当前 dry-run 返回 `BLOCKED`。
- `apps/control-tcp-server` 与 `apps/control-cli` 仍依赖 `control-core/build/bin/**` 兼容产物。
- 现场交付仍依赖手工拷贝 `control-core` 构建产物与 `dxf-pipeline` sibling 目录。
- `hardware smoke` 结果当前不能作为可信 release gate。
- 真实 `HMI -> control-tcp-server` 链路仍被 `IDiagnosticsPort 未注册` 阻塞。

### Rollback Notes

- 首个正式 release 之前，不存在可声称“已验证可回滚”的稳定 tag。
- 后续每个版本都必须同时保存 tag、release evidence、配置/配方快照与 `control-core` 兼容产物快照。
