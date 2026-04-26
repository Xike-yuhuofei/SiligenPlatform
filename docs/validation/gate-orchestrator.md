# Gate Orchestrator

更新时间：`2026-04-26`

本文档是本仓库门禁与自动化测试执行体系的权威文档。后续新增、调整或豁免门禁时，必须先对齐本文档、`scripts/validation/gates/gates.json` 和对应契约测试。

本文档定义统一门禁编排入口 `scripts/validation/invoke-gate.ps1` 的使用方式、gate 类型、证据目录、失败语义和开发者日常遵守规则。现有业务测试脚本继续作为 step 执行，统一入口只负责编排、日志、分类和证据契约。

## 0. 权威关系

门禁体系以以下顺序判定真值：

1. `scripts/validation/gates/gates.json`：机器执行真值，定义 gate、step、blocking、required artifacts、timeout 和 tool readiness。
2. 本文档：开发者执行真值，定义何时跑哪个 gate、证据如何保存、失败如何处理。
3. `docs/validation/pr-gate-and-hil-gate-policy.md`：GitHub PR / Native / HIL required check 策略。
4. `docs/validation/release-test-checklist.md` 与 runtime 发布文档：发布 Go/No-Go 策略。

如旧文档、旧脚本说明或个人习惯与本文档冲突，以本文档和 `gates.json` 为准。不得在 GitHub workflow、pre-push hook 或临时脚本中重新维护一份独立的测试清单。

## 0.1 开发者必须遵守的规则

- 日常推送前默认运行 `pre-push` gate；不要把 full-offline 作为默认本地 push 阻断项。
- PR 合并前至少依赖 GitHub `Strict PR Gate`；native 敏感变更还必须通过 `Strict Native Gate`；硬件敏感变更还必须通过 `Strict HIL Gate`。
- 改动 `apps/runtime-*`、`modules/runtime-execution`、`modules/motion-planning`、`shared/contracts`、`scripts/validation`、`build.ps1`、`test.ps1`、`ci.ps1` 或 CMake 相关内容时，按分类规则触发或手动补跑 `native` / `full-offline`。
- 改动 HIL、设备适配、运动控制、机台配置或 HMI operator/production/runtime/gateway/TCP 链路时，按分类规则触发或手动补跑 `hil`。
- 新增门禁项必须进入 `gates.json`，并补契约测试；不得只改 workflow 或只改本地 hook。
- `blocking=false` 必须是显式设计，且只能用于观察项；不得以“report-only”口径隐式放行架构、契约、native、HIL 或发布必要证据。
- `full-offline` / `native` 必须在 build/test 前完成 blocking `tool-readiness`；self-hosted runner 缺工具时要前置失败，不允许跑完长链路后再暴露环境缺口。
- 使用 `-SkipStep` 必须提供 `-SkipJustification`，并在 PR / release 记录中说明原因和补跑计划。
- 报告目录、`gate-summary.json`、`gate-summary.md` 和 `logs/<step-id>.log` 是门禁证据的一部分，不得删除后声称 gate 已通过。

## 1. 统一入口

```powershell
.\scripts\validation\invoke-gate.ps1 `
  -Gate pre-push|pr|native|hil|release|full-offline|nightly `
  -ReportRoot tests\reports\<target>
```

公共参数：

- `-Gate`：选择门禁类型。
- `-ReportRoot`：指定本次运行证据根目录；不传时使用 `scripts/validation/gates/gates.json` 中的默认目录。
- `-ChangedScope[]`：传递变更范围给底层 build/test/local validation step。
- `-SkipStep[]` 与 `-SkipJustification`：显式跳过 orchestrator step；跳过必须带理由。
- `-ForceNative` / `-ForceHil`：记录强制执行语义，供 workflow 或手动运行留下证据。

`ci.ps1` 仍保留兼容参数，但内部转调 `invoke-gate.ps1 -Gate full-offline`。`invoke-pre-push-gate.ps1` 仍负责 Git 冲突和 clean worktree 检查，之后默认调用 `invoke-gate.ps1 -Gate pre-push`。

旧入口的状态：

- `build.ps1`：仍是构建 step 的根入口，可用于定位构建问题。
- `test.ps1`：仍是测试 step 的根入口，可用于目标套件回归。
- `ci.ps1`：兼容入口，等价于调用 `full-offline` gate；新自动化不应再复制其旧编排。
- `run-local-validation-gate.ps1`：仍作为 full-offline/native step 生成本地验证证据。
- GitHub workflow：只负责触发、runner 信任、artifact 上传和 summary，不负责内联维护测试清单。

## 2. 配置文件

- `scripts/validation/gates/gates.json`：gate 与 step 的唯一编排配置。
- `scripts/validation/gates/change-classification.json`：native/HIL 变更分类规则。
- `scripts/validation/classify-change.ps1`：读取分类规则并输出 `classification.json`。

每个 step 必须显式声明：

- `id`
- `command`
- `blocking`
- `requiredArtifacts`
- `timeoutSeconds`
- `requiresTool[]`（需要时）

`blocking=true` 的 step 失败会导致 gate 失败。观察项必须显式写为 `blocking=false`，不再使用隐式 report-only 语义。

## 3. Gate 类型

| Gate | 主要用途 | 默认证据目录 | 说明 |
|---|---|---|---|
| `pre-push` | 本地推送前快速反馈 | `tests/reports/pre-push-gate` | 不默认跑 native full build、HIL、performance。 |
| `pr` | GitHub hosted PR 基线 | `tests/reports/github-actions/strict-pr-gate` | 架构、契约、静态边界检查；不承接 native build。 |
| `full-offline` | 完整离线 CI | `tests/reports/ci` | install deps、tool readiness、build/test、cppcheck、dependency graphs、local validation。 |
| `native` | GitHub self-hosted native gate | `tests/reports/github-actions/strict-native-gate` | `full-offline` 的别名，使用 native 证据目录，并以前置 tool readiness 作为 runner baseline 阻断。 |
| `hil` | 受控 HIL | `tests/reports/github-actions/strict-hil-gate` | 先跑离线前置，再跑 controlled HIL。 |
| `nightly` | 夜间性能与覆盖率 | `tests/reports/nightly` | 包含 full-offline、performance、coverage 观察项。 |
| `release` | 发布 Go/No-Go 包装 | `tests/reports/releases/orchestrated` | 暂时包装 `release-check.ps1`，保留发布专用逻辑。 |

## 3.1 日常选择矩阵

| 场景 | 必跑 gate | 可选/补充 | 说明 |
|---|---|---|---|
| 本地普通开发准备 push | `pre-push` | 目标 `test.ps1` 套件 | 默认快速反馈，覆盖架构、契约和静态边界。 |
| PR 基线 | `pr` | 无 | GitHub hosted runner 执行，所有 PR 必须通过。 |
| C++、CMake、runtime、shared contracts、验证脚本变更 | `native` 或 `full-offline` | `nightly` | native 敏感变更必须有完整离线构建测试证据。 |
| 设备、HIL、运动控制、机台配置、operator runtime 链路变更 | `hil` | `native` | HIL 只能在受信任 self-hosted runner 或受控窗口执行。 |
| 性能、稳定性、覆盖率基线变更 | `nightly` | `native` | coverage step 当前为观察项，性能结果仍按 nightly gate 判断。 |
| 发布候选 | `release` | `native`、`nightly`、`hil` | 发布 Go/No-Go 不得只依赖 PR gate。 |

开发者可以用 `test.ps1` 或 `pytest` 做更小的定位回归，但这些命令不能替代 required gate 证据。

## 4. 输出证据

每次 gate 运行都会在 `ReportRoot` 下生成：

- `gate-manifest.json`：运行输入、解析后的 gate、step 命令、工具与证据规则。
- `gate-summary.json`：机器可读结果。
- `gate-summary.md`：人工可读结果。
- `logs/<step-id>.log`：每个 step 的 stdout/stderr、退出码和超时信息。

各 step 的业务报告目录固定为 `ReportRoot/<step-id>/`，例如：

- `tests/reports/github-actions/strict-pr-gate/semgrep/`
- `tests/reports/github-actions/strict-native-gate/cppcheck/`
- `tests/reports/github-actions/strict-hil-gate/controlled-hil/`

`local-validation-gate` 的总结文件由 step 级 `requiredArtifacts` 独占声明。gate 级 `requiredArtifacts` 不重复声明同一证据，避免一份缺失被重复计为两个失败原因。

## 5. 失败语义

以下情况会使 gate 失败：

- blocking step 退出码非零。
- blocking step 超时。
- blocking step 的 required artifact 缺失。
- required tool 缺失；对于 `full-offline` / `native`，该失败应在 `tool-readiness` step 前置暴露。
- gate 级 required artifact 缺失。该检查只在前置 blocking steps 全部通过后执行，避免把派生性缺证据噪声叠加到首个真实失败之上。
- `SkipStep` 不带 `SkipJustification`。

非阻断 step 失败会记录在 summary 中，但不会改变 gate 总体通过状态。当前只有 nightly coverage 类观察项使用 `blocking=false`。

失败后的处理要求：

- 修复后重新运行同一个 gate，不能只补跑失败 step 后宣称 gate 通过。
- 工具缺失属于门禁失败；先运行 `scripts/validation/install-python-deps.ps1` 或安装对应系统工具。
- Native runner、HIL runner 或硬件不可用时，required check 不能自动降级为通过。
- 设备故障可以在报告中分类为环境问题，但 required gate 仍保持阻断状态。

## 6. 常用命令

本地快速 pre-push gate：

```powershell
.\scripts\validation\invoke-pre-push-gate.ps1
```

手动执行完整离线 gate：

```powershell
.\scripts\validation\invoke-pre-push-gate.ps1 -Gate full-offline
```

PR gate smoke：

```powershell
.\scripts\validation\invoke-gate.ps1 `
  -Gate pr `
  -ReportRoot tests\reports\tmp\gate-pr-smoke
```

Native gate：

```powershell
.\scripts\validation\invoke-gate.ps1 `
  -Gate native `
  -ReportRoot tests\reports\github-actions\strict-native-gate
```

HIL gate：

```powershell
.\scripts\validation\invoke-gate.ps1 `
  -Gate hil `
  -ReportRoot tests\reports\github-actions\strict-hil-gate
```

分类 smoke：

```powershell
.\scripts\validation\classify-change.ps1 `
  -Mode all `
  -OutputPath tests\reports\tmp\classification-smoke\classification.json `
  -ChangedFile scripts\validation\invoke-gate.ps1
```

契约验证：

```powershell
python -m pytest `
  tests\contracts\test_gate_orchestrator_contract.py `
  tests\contracts\test_strict_runner_gate_contract.py `
  tests\contracts\test_layered_validation_lane_policy_contract.py `
  -q
```

## 7. 修改门禁体系的流程

修改门禁体系时必须同步完成：

1. 更新 `scripts/validation/gates/gates.json` 或 `change-classification.json`。
2. 更新 orchestrator 或 wrapper 脚本。
3. 更新本文档和相关策略文档。
4. 更新 `tests/contracts/test_gate_orchestrator_contract.py` 或相关契约测试。
5. 运行 PowerShell parse、JSON parse、契约测试和至少一个相关 gate smoke。

禁止只改 GitHub workflow、只改 pre-push hook、只改文档或只改脚本中的任意一种单点变更。
