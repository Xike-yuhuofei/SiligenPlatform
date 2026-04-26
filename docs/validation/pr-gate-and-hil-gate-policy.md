# PR Gate and HIL Gate Policy

更新时间：`2026-04-26`

本文档是 `SiligenSuite` PR 合并前自动化测试、native 构建测试与 HIL 门禁的正式策略文档。统一编排入口为 `scripts/validation/invoke-gate.ps1`，具体 gate/step 配置见 `scripts/validation/gates/gates.json`。

## 1. 目的

本策略定义进入 `main` 前必须满足的自动化门禁基线，目标是保证：

- 所有 PR 合并前至少经过快速、严格、可复现的托管自动化基线。
- native 敏感 PR 合并前必须经过完整离线构建与测试。
- 硬件敏感 PR 合并前必须经过受控 HIL 验证。
- 门禁结果必须作为 GitHub branch protection 的 required checks 阻断合并。
- 本地 hook、PR gate、HIL gate、release gate 的语义边界清晰，不互相替代。

## 2. 门禁分层

| 门禁 | 适用范围 | 执行位置 | 是否阻断合并 | 主要证据 |
|---|---|---|---|---|
| `Strict PR Gate` | 所有目标为 `main` 的 PR | GitHub hosted Windows runner | 是 | `tests/reports/github-actions/strict-pr-gate/` |
| `Strict Native Gate` | 所有目标为 `main` 的 PR 都产生 check；native 敏感 PR 真实执行 full-offline 构建测试 | classifier 使用 GitHub hosted runner；native 构建测试使用 self-hosted Windows build runner | 是 | `tests/reports/github-actions/strict-native-gate/` |
| `Strict HIL Gate` | 所有目标为 `main` 的 PR 都产生 check；硬件敏感 PR 真实执行 HIL | classifier 使用 GitHub hosted runner；HIL 使用 self-hosted Windows runner | 是 | `tests/reports/github-actions/strict-hil-gate/` |
| 本地 `pre-push` gate | 开发者本机推送前 | 本地工作站 | 只阻断本机 push | `tests/reports/pre-push-gate/` |
| release gate | RC / 正式版本发布 | 本地或受控发布环境 | 阻断发布结论 | `tests/reports/releases/<version>/` |

## 3. Strict PR Gate

`Strict PR Gate` 是所有 PR 的最低合并前基线。该门禁必须在 GitHub hosted runner 上从远程提交重新 checkout 后执行，不复用开发者本地编译产物。

### 3.1 Required Check

GitHub branch protection 必须将以下 check 配置为 required status check：

- `Strict PR Gate`

未通过该 check 的 PR 不得合并到 `main`。

### 3.2 执行命令

```powershell
.\scripts\validation\invoke-gate.ps1 `
  -Gate pr `
  -ReportRoot tests\reports\github-actions\strict-pr-gate
```

### 3.3 覆盖范围

该门禁覆盖以下快速且阻断的基线项：

1. HMI formal gateway launch contract gate。
2. `legacy-exit` gate。
3. `semgrep`。
4. `import-linter`。
5. `module-boundary-bridges`。
6. `pyright-static`。

该门禁不得承接耗时 native 编译，也不得用本地 artifact 替代远程执行结果。native 编译和完整 full-offline 测试由 `Strict Native Gate` 承接。

## 4. Strict Native Gate

`Strict Native Gate` 是 native 敏感 PR 的额外合并前门禁。该门禁用于承接耗时的完整离线构建与测试，必须在专用 self-hosted Windows build runner 上执行，不得由本地编译产物、开发者口头说明或 GitHub hosted runner 缺省通过替代。

### 4.1 Required Check

GitHub branch protection 必须将以下 check 配置为 required status check：

- `Strict Native Gate`

该 check 必须对每个 PR 产生稳定结果。非 native 敏感 PR 只能由 workflow classifier 判定为 `not required` 后通过；native 敏感 PR 必须完成真实 full-offline 构建测试并产生证据。

### 4.2 Runner 要求

self-hosted build runner 必须满足：

- 操作系统为 Windows。
- 注册到 GitHub repository 或 organization。
- 至少具备以下 labels：
  - `self-hosted`
  - `Windows`
  - `X64`
  - `build`
- 具备本仓 native 构建、测试和验证所需的固定工具链。
- runner workspace 必须从 GitHub 远程提交 checkout 后构建，不得复用开发者本地构建目录作为通过依据。
- active runner 根目录固定为短路径 `D:\r`。
- active checkout workspace 固定为 `D:\r\_work\SiligenPlatform\SiligenPlatform`，workspace 根路径长度不得超过 45 个字符。
- Windows Service 必须运行在可稳定访问 GitHub、可执行 Git/CMake/MSBuild/Python/PowerShell 的账号下。
- 旧路径 `D:\GitHubRunners\siligen-build` 只允许作为短期回退留存，不得作为正式 active runner path。

### 4.3 Native 敏感判定

以下任一条件成立时，PR 必须真实执行完整 native gate：

- PR 带有 `native-sensitive` label。
- `workflow_dispatch` 显式输入 `force_native=true`。
- workflow 无法可靠取得变更文件列表。
- 改动命中 `scripts/validation/gates/change-classification.json` 中 `native.patterns` 定义的路径或语义范围。

纯文档、纯 HMI Python 单元层变更、无 native 构建影响的变更默认不要求 native gate，但仍必须通过 `Strict PR Gate`。

### 4.4 Native 执行入口

Native 执行入口固定为当前正式 full-offline CI gate：

```powershell
.\scripts\validation\invoke-gate.ps1 `
  -Gate native `
  -ReportRoot tests\reports\github-actions\strict-native-gate
```

禁止使用：

- 本地开发机 build artifact。
- 手工上传 artifact 作为 required check 通过依据。
- 缺失 runner 时自动降级为 `not required`。
- 缺失报告时自动放行。
- checkout 失败时自动跳过 native gate。

### 4.5 并发

Native build runner 可以复用缓存和工具链，但 required check 的验证对象必须是 GitHub Actions checkout 出的目标提交。runner 并发策略必须避免同一 workspace 或同一构建目录被多个 job 同时写入。

## 5. Strict HIL Gate

`Strict HIL Gate` 是硬件敏感 PR 的额外合并前门禁。该门禁不得由 GitHub hosted runner 伪造通过，必须在能访问真实 HIL 设备的 self-hosted Windows runner 上执行。

### 5.1 Required Check

GitHub branch protection 必须将以下 check 配置为 required status check：

- `Strict HIL Gate`

该 check 必须对每个 PR 产生稳定结果。非硬件敏感 PR 只能由 workflow classifier 判定为 `not required` 后通过；硬件敏感 PR 必须完成真实 HIL 执行并产生证据。

### 5.2 Runner 要求

self-hosted runner 必须满足：

- 操作系统为 Windows。
- 注册到 GitHub repository 或 organization。
- 至少具备以下 labels：
  - `self-hosted`
  - `windows`
  - `hil`
- 可访问 HIL 设备、控制卡、驱动、配置、网络和必要 vendor runtime。
- 具备受控的急停、限位、设备复位和异常恢复操作条件。

### 5.3 硬件敏感判定

以下任一条件成立时，PR 必须真实执行 HIL：

- PR 带有 `hardware-sensitive` label。
- `workflow_dispatch` 显式输入 `force_hil=true`。
- 改动命中 `scripts/validation/gates/change-classification.json` 中 `hil.patterns` 定义的路径或语义范围。

纯文档、纯离线测试、无设备执行影响的变更默认不要求 HIL，但仍必须通过 `Strict PR Gate`。

### 5.4 HIL 执行入口

HIL 执行入口固定为统一 `hil` gate。该 gate 先执行离线前置验证，再执行当前正式受控 HIL quick gate：

```powershell
.\scripts\validation\invoke-gate.ps1 `
  -Gate hil `
  -ReportRoot tests\reports\github-actions\strict-hil-gate
```

禁止使用：

- `-AllowSkipOnMissingHardware`
- `-Executor`
- `-PublishLatestOnPass:$true`
- `-ReuseExistingGateway:$false`

### 5.5 并发与安全

HIL runner 必须限制并发。任何同一机台或同一控制卡资源不得并行执行多个 HIL job。

外部 fork 或不可信代码不得直接访问 HIL runner。需要执行 HIL 的外部贡献必须先经过维护者审查，并由受信任分支或受控触发路径执行。

## 6. 失败语义

以下情况均视为门禁失败，不允许静默放行：

- required check failed。
- required check missing。
- Native runner offline。
- Native build/test 报告缺失。
- HIL runner offline。
- HIL 设备不可用。
- HIL case matrix 报告缺失。
- `hil-controlled-gate-summary.json` 缺失。
- `hil-controlled-release-summary.md` 缺失。
- 测试超时。
- 报告生成失败。
- `known_failure > 0`。
- 自动化脚本以非零退出码结束。

设备故障与代码故障可以在报告中分类，但在 required check 层面均为阻断状态。

## 7. 证据要求

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
- `gate-manifest.json`、`gate-summary.json`、`gate-summary.md`。
- `logs/<step-id>.log`。

GitHub Actions 必须上传对应 `tests/reports/...` 目录作为 artifact。

## 8. 豁免规则

默认不允许用口头确认、聊天记录或本地未上传结果替代 required checks。

如因设备维护、runner 故障或紧急修复必须临时豁免，必须满足：

1. PR 明确记录豁免原因。
2. 维护者明确批准。
3. 后续补跑对应 native/HIL gate 并追加证据。
4. 豁免不得用于正式发布 Go/No-Go。

豁免只能降低单次 PR 合并风险处置要求，不能修改本文档定义的长期门禁基线。

## 9. 与发布门禁的关系

PR 合并门禁通过不等于版本可发布。

正式发布仍必须满足：

- `docs/runtime/release-readiness-standard.md`
- `docs/runtime/release-process.md`
- `docs/validation/release-test-checklist.md`

`Strict PR Gate`、`Strict Native Gate` 和 `Strict HIL Gate` 是进入 `main` 前的质量基线；release gate 是版本交付前的 Go/No-Go 判定。

## 10. 当前权威入口

- PR 合并策略：本文档。
- 统一门禁编排：`docs/validation/gate-orchestrator.md`。
- Gate 配置：`scripts/validation/gates/gates.json`。
- 变更分类规则：`scripts/validation/gates/change-classification.json`。
- 发布判定标准：`docs/runtime/release-readiness-standard.md`。
- 发布流程：`docs/runtime/release-process.md`。
- 测试层级与长期验收：`docs/validation/README.md`。
- DSP E2E 系统规格：`docs/architecture/dsp-e2e-spec/`。
