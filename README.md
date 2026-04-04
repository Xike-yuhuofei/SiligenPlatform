# SiligenSuite

仓库根是当前唯一正式入口。目录治理、冻结文档、根级命令和验证证据都从这里发起。

## 当前拓扑

- canonical roots：`apps/`、`modules/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/`
- 已退出并物理删除：`packages/`、`integration/`、`tools/`、`examples/`

当前进程入口：

- `apps/runtime-service/`
- `apps/runtime-gateway/`
- `apps/planner-cli/`
- `apps/hmi-app/`
- `apps/trace-viewer/`

## 根级命令

```powershell
.\scripts\validation\install-python-deps.ps1
.\build.ps1
.\test.ps1
.\ci.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
```

## 先看哪里

- 工作区边界：`WORKSPACE.md`
- 冻结基线：`docs/architecture/workspace-baseline.md`
- 9 轴冻结文档：`docs/architecture/dsp-e2e-spec/`
- build/test/CI 口径：`docs/architecture/build-and-test.md`
- 路径分类：`docs/architecture/canonical-paths.md`
- 部署与交付：`docs/runtime/deployment.md`

## Git 分支命名

统一格式：`<type>/<scope>/<ticket>-<short-desc>`

- `type`：见 `docs/onboarding/git-branch-naming.md` 中的批准白名单
- `scope`：使用稳定的小写模块/域名；推荐列表见 `docs/onboarding/git-branch-naming.md`
- `ticket`：优先使用任务系统编号，如 `MC-142`、`BUG-311`、`ARCH-057`、`SPEC-021`
- `short-desc`：英文小写 kebab-case
- 详细规则文档：`docs/onboarding/git-branch-naming.md`

## Git 收口规则

- 仓库常态只保留 `main` / `origin/main` 作为长期分支。
- 临时开发分支必须满足命名规范，并在合并或废弃后删除本地分支、远端分支和额外 worktree。
- 发布、门禁和交付文档中的根路径默认指向当前 canonical 仓库根 `D:\Projects\SiligenSuite`。
