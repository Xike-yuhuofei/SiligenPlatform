# Release Process

更新时间：`2026-04-01`

## 1. 适用范围

本文档定义 `SiligenSuite` 根仓的正式版本治理与人工发布流程。

发布判定的唯一正式标准见：

- [docs/runtime/release-readiness-standard.md](/D:/Projects/SiligenSuite/docs/runtime/release-readiness-standard.md)

本文档只说明执行流程，不降低发布标准。

执行清单见：

- [docs/validation/release-test-checklist.md](/D:/Projects/SiligenSuite/docs/validation/release-test-checklist.md)

## 2. 发布分类

### 2.1 RC 候选版

- 版本格式：`x.y.z-rc.n`
- 含义：仓内候选版通过最小闭环，可进入受控验证
- 不得外推为正式对外交付

### 2.2 正式发布版

- 版本格式：`x.y.z`
- 含义：满足仓内、仓外与现场门禁
- 必须满足 `release-readiness-standard` 全部正式发布条件

## 3. 仓内必跑门禁

### 3.1 推送门禁

`git push` 与发布不是同一语义：push 只是把提交同步到远程分支，不等于版本可发布。

本仓库支持安装本地 `pre-push` 硬门禁，使每次推送远程前先验证当前提交：

```powershell
Set-Location <repo-root>
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\install-pre-push-gate.ps1
```

安装后，`git push` 会调用：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\invoke-pre-push-gate.ps1
```

默认策略：

1. 工作树必须干净，确保验证对象就是即将推送的提交。
2. 不允许 unresolved conflict。
3. 执行 `ci.ps1 -Suite all -Lane full-offline-gate -RiskProfile high -DesiredDepth full-offline`。
4. 证据写入 `tests\reports\pre-push-gate\<timestamp>-<remote>\`。

说明：本地 `pre-push` 能阻止当前机器发起的 push，但不能替代远程服务端策略。若要阻止绕过本地 hook 的推送，必须在远程仓库启用受保护分支、required checks，或在自托管 Git 服务端启用 pre-receive hook。

### 3.2 PR 合并门禁

合并到 `main` 前的远程硬门禁由 GitHub Actions + branch protection 执行。

PR 入口：

- `.github/workflows/strict-pr-gate.yml`
- check 名称：`Strict PR Gate`

该 check 在 `pull_request` 与 `merge_group` 上执行快速托管基线：

```powershell
.\scripts\validation\assert-hmi-formal-gateway-launch-contract.ps1
.\legacy-exit-check.ps1 -Profile CI -ReportDir tests\reports\github-actions\strict-pr-gate\legacy-exit
.\scripts\validation\invoke-semgrep.ps1 -ReportDir tests\reports\github-actions\strict-pr-gate\semgrep
.\scripts\validation\invoke-import-linter.ps1 -ReportDir tests\reports\github-actions\strict-pr-gate\import-linter
```

仓库的 `main` 分支保护必须将 `Strict PR Gate` 配置为 required status check。未通过该 check 的 PR 不得合并。

native 敏感 PR 还必须通过 `Strict Native Gate`。该 check 由 self-hosted Windows build runner 执行完整 full-offline 构建测试；非 native 敏感 PR 只能由 workflow classifier 明确判定为 `not required` 后通过。

硬件敏感 PR 还必须通过 `Strict HIL Gate`。该 check 由 self-hosted Windows HIL runner 执行真实受控 HIL；非硬件敏感 PR 只能由 workflow classifier 明确判定为 `not required` 后通过。具体策略以 `docs/validation/pr-gate-and-hil-gate-policy.md` 为准。

### 3.3 发布门禁

发布前默认门禁入口（本地）：

```powershell
Set-Location <repo-root>
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
```

门禁脚本会串行执行：

1. 单轨工作区布局校验
2. 根级构建/测试链的预检查
3. freeze/acceptance 相关验证
4. 本地报告固化到 `tests/reports/`

并输出证据到 `tests\reports\local-validation-gate\<timestamp>\`。

在本地门禁通过后，再执行 release 检查：

```powershell
Set-Location <repo-root>
.\release-check.ps1 -Version <version>
```

`release-check.ps1` 当前默认纳入 `hil-case-matrix`。

正式发布版必须显式启用硬件 smoke：

```powershell
Set-Location <repo-root>
.\release-check.ps1 -Version <x.y.z> -IncludeHardwareSmoke
```

如需临时隔离 `hil-case-matrix` 影响，可显式关闭：

```powershell
Set-Location <repo-root>
.\release-check.ps1 -Version <version> -IncludeHilCaseMatrix:$false
```

`release-check.ps1` 会执行：

1. 检查根仓是否已有可打 tag 的提交
2. 检查工作树是否干净，确保发布边界可审计
3. 检查 `CHANGELOG.md` 是否已有目标版本条目
4. 执行 `legacy-exit-check.ps1`，要求 `finding_count=0`
5. 执行根级 `build.ps1 -Profile CI -Suite all`
6. 执行根级 `test.ps1 -Profile CI -Suite all -FailOnKnownFailure`
7. 检查性能基线证据存在
8. 生成 release evidence 到 `tests\reports\releases\<version>\`
9. 默认检查 `ci\hil-case-matrix\case-matrix-summary.json/.md`
10. 执行以下 app dry-run 并固化输出：
   - `apps\hmi-app\run.ps1 -DryRun -DisableGatewayAutostart`
   - `apps\runtime-gateway\run.ps1 -DryRun`
   - `apps\planner-cli\run.ps1 -DryRun`
   - `apps\runtime-service\run.ps1 -DryRun`

说明：

- GitHub Actions required checks 是 PR 合并到 `main` 前的远程硬门禁。
- release gate 是版本发布前的 Go/No-Go 门禁，必须基于发布环境实跑证据给出结论。
- `release-check.ps1` 只能证明仓内发布自动化门禁通过，不替代仓外观察、HIL 与真机门禁。
- `hil-case-matrix` 现在是默认 release 自动化证据的一部分；若显式使用 `-IncludeHilCaseMatrix:$false`，该结果只可用于临时隔离排障，不应作为默认放行口径。

## 4. 正式发布前的附加硬门禁

以下项不因仓内自动化全绿而自动满足：

- 仓外交付物观察：见 [docs/runtime/external-migration-observation.md](/D:/Projects/SiligenSuite/docs/runtime/external-migration-observation.md)
- 现场/HIL 验收：见 [docs/runtime/field-acceptance.md](/D:/Projects/SiligenSuite/docs/runtime/field-acceptance.md)
- 部署与回滚准备：见 [docs/runtime/deployment.md](/D:/Projects/SiligenSuite/docs/runtime/deployment.md)、[docs/runtime/rollback.md](/D:/Projects/SiligenSuite/docs/runtime/rollback.md)
- 发布判定标准：见 [docs/runtime/release-readiness-standard.md](/D:/Projects/SiligenSuite/docs/runtime/release-readiness-standard.md)

## 5. DXF 编辑发布口径

- 内建 DXF 编辑器已删除，不再作为发布项。
- `notify / CLI` 编辑器协作协议已废弃，不再作为 release gate。
- 若交付说明涉及 DXF 编辑，统一引用 `docs/runtime/external-dxf-editing.md`。

## 6. 当前已知边界

1. 仓内 release gate 只能证明 canonical 工作区当前通过，不能直接证明仓外迁移已完成。
2. `hardware smoke` 只能证明最小启动闭环，不能替代 HIL 长稳、真机可运行性或工艺签收。
3. 正式发布结论必须同时满足软件、设备和现场运维三个维度，不接受只凭单一维度放行。
