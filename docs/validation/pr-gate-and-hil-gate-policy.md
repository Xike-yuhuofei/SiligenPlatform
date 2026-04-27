# PR Gate and HIL Gate Policy

更新时间：`2026-04-25`

本文档是 `SiligenSuite` PR 合并前自动化测试与 HIL 门禁的正式策略文档。

## 1. 目的

本策略定义进入 `main` 前必须满足的自动化门禁基线，目标是保证：

- 所有 PR 合并前至少经过严格离线自动化验证。
- 硬件敏感 PR 合并前必须经过受控 HIL 验证。
- 门禁结果必须作为 GitHub branch protection 的 required checks 阻断合并。
- 本地 hook、PR gate、HIL gate、release gate 的语义边界清晰，不互相替代。

## 2. 门禁分层

| 门禁 | 适用范围 | 执行位置 | 是否阻断合并 | 主要证据 |
|---|---|---|---|---|
| `Strict PR Gate` | 所有目标为 `main` 的 PR | GitHub hosted Windows runner | 是 | `tests/reports/github-actions/strict-pr-gate/` |
| `Strict HIL Gate` | 硬件敏感 PR | self-hosted Windows HIL runner | 是 | HIL case matrix / closed-loop 报告 |
| 本地 `pre-push` gate | 开发者本机推送前 | 本地工作站 | 只阻断本机 push | `tests/reports/pre-push-gate/` |
| release gate | RC / 正式版本发布 | 本地或受控发布环境 | 阻断发布结论 | `tests/reports/releases/<version>/` |

## 3. Strict PR Gate

`Strict PR Gate` 是所有 PR 的最低合并前基线。

### 3.1 触发条件

- `pull_request` 到 `main`
- `merge_group` 到 `main`
- 手动 `workflow_dispatch`

### 3.2 Required Check

GitHub branch protection 必须将以下 check 配置为 required status check：

- `Strict PR Gate`

未通过该 check 的 PR 不得合并到 `main`。

### 3.3 执行命令

```powershell
.\ci.ps1 `
  -Suite all `
  -ReportDir tests\reports\github-actions\strict-pr-gate `
  -Lane full-offline-gate `
  -RiskProfile high `
  -DesiredDepth full-offline
```

### 3.4 覆盖范围

该门禁通过 `ci.ps1` 串联以下阻断项：

1. HMI formal gateway launch contract gate。
2. `legacy-exit` gate。
3. `semgrep`。
4. `import-linter`。
5. `build.ps1 -Profile CI -Suite all`。
6. `test.ps1 -Profile CI -Suite all -FailOnKnownFailure`。
7. workspace validation / runtime owner acceptance gate。
8. local validation gate。
9. 必需报告存在性检查。

`cppcheck` 与 dependency graph export 当前为 report-only，不得作为合并放行依据。

## 4. Strict HIL Gate

`Strict HIL Gate` 是硬件敏感 PR 的额外合并前门禁。该门禁不得由 GitHub hosted runner 伪造通过，必须在能访问真实 HIL 设备的 self-hosted Windows runner 上执行。

### 4.1 Runner 要求

self-hosted runner 必须满足：

- 操作系统为 Windows。
- 注册到 GitHub repository 或 organization。
- 至少具备以下 labels：
  - `self-hosted`
  - `windows`
  - `hil`
- 可访问 HIL 设备、控制卡、驱动、配置、网络和必要 vendor runtime。
- 具备受控的急停、限位、设备复位和异常恢复操作条件。

### 4.2 触发范围

以下 PR 必须经过 `Strict HIL Gate`：

- 修改 runtime 执行链、motion、IO、限位、急停、设备 adapter、控制卡调用或 HIL runner 的 PR。
- 修改 `apps/runtime-service`、`apps/runtime-gateway`、HMI operator 生产链路或真实设备启动链的 PR。
- 修改 HIL 测试脚本、HIL fixtures、设备配置、case matrix 或闭环验证语义的 PR。
- 带有 `hardware-sensitive` 标签的 PR。
- 维护者明确要求 HIL 验证的 PR。

纯文档、纯离线测试、无设备执行影响的变更默认不要求 HIL，但仍必须通过 `Strict PR Gate`。

### 4.3 Required Check

当 PR 属于硬件敏感范围时，GitHub branch protection 必须要求以下 check 通过：

- `Strict HIL Gate`

未通过该 check 的硬件敏感 PR 不得合并到 `main`。

### 4.4 推荐执行命令

```powershell
.\ci.ps1 `
  -Suite all `
  -ReportDir tests\reports\github-actions\strict-hil-gate `
  -Lane limited-hil `
  -RiskProfile hardware-sensitive `
  -DesiredDepth hil `
  -IncludeHilCaseMatrix
```

如 PR 涉及闭环运动、设备状态恢复或 operator 生产链路，应额外启用：

```powershell
-IncludeHilClosedLoop
```

### 4.5 并发与安全

HIL runner 必须限制并发。任何同一机台或同一控制卡资源不得并行执行多个 HIL job。

外部 fork 或不可信代码不得直接访问 HIL runner。需要执行 HIL 的外部贡献必须先经过维护者审查，并由受信任分支或受控触发路径执行。

## 5. 失败语义

以下情况均视为门禁失败，不允许静默放行：

- required check failed。
- required check missing。
- HIL runner offline。
- HIL 设备不可用。
- HIL case matrix 报告缺失。
- HIL closed-loop 报告缺失但本次 PR 需要 closed-loop。
- 测试超时。
- 报告生成失败。
- `known_failure > 0`。
- 自动化脚本以非零退出码结束。

设备故障与代码故障可以在报告中分类，但在 required check 层面均为阻断状态。

## 6. 证据要求

每次门禁运行必须至少能追溯：

- PR 编号。
- commit SHA。
- workflow run id。
- runner 类型。
- 执行时间。
- 执行命令。
- 报告目录。
- 总体结果。
- 失败步骤与失败原因。

GitHub Actions 必须上传对应 `tests/reports/...` 目录作为 artifact。

## 7. 豁免规则

默认不允许用口头确认、聊天记录或本地未上传结果替代 required checks。

如因设备维护、runner 故障或紧急修复必须临时豁免，必须满足：

1. PR 明确记录豁免原因。
2. 维护者明确批准。
3. 后续补跑 HIL 并追加证据。
4. 豁免不得用于正式发布 Go/No-Go。

豁免只能降低单次 PR 合并风险处置要求，不能修改本文档定义的长期门禁基线。

## 8. 与发布门禁的关系

PR 合并门禁通过不等于版本可发布。

正式发布仍必须满足：

- `docs/runtime/release-readiness-standard.md`
- `docs/runtime/release-process.md`
- `docs/validation/release-test-checklist.md`

`Strict PR Gate` 和 `Strict HIL Gate` 是进入 `main` 前的质量基线；release gate 是版本交付前的 Go/No-Go 判定。

## 9. 当前权威入口

- PR 合并策略：本文档。
- 发布判定标准：`docs/runtime/release-readiness-standard.md`。
- 发布流程：`docs/runtime/release-process.md`。
- 测试层级与长期验收：`docs/validation/README.md`。
- DSP E2E 系统规格：`docs/architecture/dsp-e2e-spec/`。
