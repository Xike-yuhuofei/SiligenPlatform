# SiligenSuite

仓库根是当前唯一正式入口。目录治理、冻结文档、根级命令和验证证据都从这里发起。

## 当前拓扑

- canonical roots：`apps/`、`modules/`、`shared/`、`docs/`、`samples/`、`tests/`、`scripts/`、`config/`、`data/`、`deploy/`
- 已退出并物理删除：`packages/`、`integration/`、`tools/`、`examples/`
- 本地缓存且默认忽略：`.specify/`、`specs/`
- 本地生成物且默认忽略：根级 `build-*/`、`.claude/`
- control-apps build root 默认只认显式 `SILIGEN_CONTROL_APPS_BUILD_ROOT` 或当前工作区 `build/ca`；`build/`、`build/control-apps/` 和本地发布缓存不再参与默认候选。

当前进程入口：

- `apps/runtime-service/`
- `apps/runtime-gateway/`
- `apps/planner-cli/`
- `apps/hmi-app/`
- `apps/trace-viewer/`

## 根级命令

```powershell
.\scripts\validation\install-python-deps.ps1
.\scripts\validation\invoke-gate.ps1 -Gate pre-push
.\scripts\validation\invoke-gate.ps1 -Gate pr
.\scripts\validation\invoke-gate.ps1 -Gate full-offline
.\scripts\validation\invoke-module-boundary-audit.ps1 -Mode pr
.\build.ps1
.\test.ps1
.\ci.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
```

门禁与自动化测试的权威入口是 `docs/validation/gate-orchestrator.md`。`ci.ps1` 保留为兼容包装，新的门禁编排应进入 `scripts/validation/gates/gates.json`，Module Boundary Audit 模型应进入 `scripts/validation/boundaries/`，不要在 workflow 或临时脚本中复制测试清单。PR baseline、conditional Native/HIL、Nightly 和 Release 的职责边界都以该文档为准；PR passed 不等于 release ready。

## 先看哪里

- 工作区边界：`WORKSPACE.md`
- 冻结基线：`docs/architecture/workspace-baseline.md`
- 9 轴冻结文档：`docs/architecture/dsp-e2e-spec/`
- 门禁/自动化测试权威口径：`docs/validation/gate-orchestrator.md`
- build/test/CI 口径：`docs/architecture/build-and-test.md`
- 路径分类：`docs/architecture/canonical-paths.md`
- 部署与交付：`docs/runtime/deployment.md`

## Git 分支命名

统一格式：`<type>/<scope>/<ticket>-<short-desc>`

- `type`：见项目级 skill `.agents/skills/git-create/SKILL.md` 中的批准白名单
- `scope`：使用稳定的小写模块/域名；推荐列表见项目级 skill `.agents/skills/git-create/SKILL.md`
- `ticket`：优先使用任务系统编号，如 `MC-142`、`BUG-311`、`ARCH-057`、`SPEC-021`
- `short-desc`：英文小写 kebab-case
- 详细规则：项目级 skill `.agents/skills/git-create/SKILL.md`

## Git 收口规则

- 仓库常态只保留 `main` / `origin/main` 作为长期分支。
- 临时开发分支必须满足命名规范，并在合并或废弃后删除本地分支、远端分支和额外 worktree。
- 发布、门禁和交付文档中的根路径默认指向当前 canonical 仓库根 `D:\Projects\SiligenSuite`。
