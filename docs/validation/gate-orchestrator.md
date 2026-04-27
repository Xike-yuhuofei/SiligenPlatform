# Gate Orchestrator

更新时间：`2026-04-26`

本文档是本仓库门禁与自动化测试执行体系的权威文档。后续新增、调整或豁免门禁时，必须先对齐本文档、`scripts/validation/gates/gates.json` 和对应契约测试。

本文档定义统一门禁编排入口 `scripts/validation/invoke-gate.ps1` 的使用方式、gate 类型、证据目录、失败语义和开发者日常遵守规则。现有业务测试脚本继续作为 step 执行，统一入口只负责编排、日志、分类和证据契约。PR passed 不等于 release ready；release Go/No-Go 只能由 release gate 与最新 native、nightly/performance、HIL、package/dry-run 证据共同支撑。

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

`ci.ps1` 仍保留兼容参数，但内部转调 `invoke-gate.ps1 -Gate full-offline`。`invoke-pre-push-gate.ps1` 负责推导待 push commit 集合、生成 pre-push classification evidence、执行 Git hygiene，并把分类选出的 quick steps 传给 `invoke-gate.ps1 -Gate pre-push`。

旧入口的状态：

- `build.ps1`：仍是构建 step 的根入口，可用于定位构建问题。
- `test.ps1`：仍是测试 step 的根入口，可用于目标套件回归。
- `ci.ps1`：兼容入口，等价于调用 `full-offline` gate；新自动化不应再复制其旧编排。
- `run-local-validation-gate.ps1`：仍作为 full-offline/native step 生成本地验证证据。
- GitHub workflow：只负责触发、runner 信任、artifact 上传和 summary，不负责内联维护测试清单。

## 2. 配置文件

- `scripts/validation/gates/gates.json`：gate 与 step 的唯一编排配置。
- `scripts/validation/gates/change-classification.json`：pre-push quick route、native follow-up、HIL follow-up 的唯一分类规则。
- `scripts/validation/classify-change.ps1`：读取分类规则并输出 `classification.json`，字段至少包含 `changed_files`、`categories`、`reasons`、`requires_native_followup`、`requires_hil_followup`、`selected_pre_push_steps`。

每个 step 必须显式声明：

- `id`
- `command`
- `blocking`
- `requiredArtifacts`
- `timeoutSeconds`
- `requiresTool[]`

`blocking=true` 的 step 失败会导致 gate 失败。观察项必须显式写为 `blocking=false`，不再使用隐式 report-only 语义。

## 3. Gate 类型

| Gate | 主要用途 | 默认证据目录 | 说明 |
|---|---|---|---|
| `pre-push` | 本地推送前快速反馈 | `tests/reports/pre-push-gate` | 不默认跑 native full build、HIL、performance。 |
| `pr` | GitHub hosted PR 基线 | `tests/reports/github-actions/strict-pr-gate` | 中等强度 PR baseline，覆盖 HMI/gateway/config/interlock/offline smoke quick checks；不承接 native build。 |
| `full-offline` | 完整离线 CI | `tests/reports/ci` | build/test/local validation/cppcheck/dependency graphs。 |
| `native` | GitHub self-hosted native gate | `tests/reports/github-actions/strict-native-gate` | `full-offline` 的别名，使用 native 证据目录。 |
| `hil` | 受控 HIL | `tests/reports/github-actions/strict-hil-gate` | 先跑离线前置，再跑 controlled HIL。 |
| `nightly` | 夜间性能、长稳与覆盖率 | `tests/reports/nightly` | 包含 full-offline、performance、long-running simulation、baseline drift、performance baseline evidence；coverage 为显式观察项。 |
| `release` | 发布 Go/No-Go 包装 | `tests/reports/releases/orchestrated` | 暂时包装 `release-check.ps1`，发布判定必须同时核对 native/nightly/HIL/package 证据。 |

## 3.1 PR baseline quick checks

`pr` gate 是所有 PR 必跑的 hosted baseline，目标耗时为 5-15 分钟，GitHub workflow 硬上限 20 分钟。它只通过 `invoke-gate.ps1 -Gate pr` 执行，不允许在 workflow 中复制 step 清单。

`pr` gate 当前包含以下 blocking quick checks：

- `install-python-deps`
- `hmi-formal-gateway-contract`
- `legacy-exit`
- `semgrep`
- `import-linter`
- `module-boundary-bridges`
- `pyright-static`
- `contracts-quick`
- `hmi-runtime-gateway-protocol-compat`
- `recipe-config-schema-quick`
- `state-machine-interlock-contract`
- `offline-simulation-smoke`
- `validation-system-contract`
- `classification-evidence`

`pr` gate 明确不包含 native full build、HIL、performance、release dry-run 或 long-running stability tests。这些由 `native`、`hil`、`nightly` 和 `release` gate 承接。

## 3.2 Pre-push 自动分类与 quick checks

`pre-push` 是风险路由型 quick gate，不跑 native full build、HIL、performance 或 release。默认参数为：

- `Gate=pre-push`
- `Lane=quick-gate`
- `RiskProfile=medium`
- `DesiredDepth=quick`

`invoke-pre-push-gate.ps1` 的变更集合推导顺序：

1. Git pre-push hook stdin：
   - 对普通 ref 更新，使用 hook 提供的 `local_ref local_sha remote_ref remote_sha` 推导待 push range。
   - 对远端分支删除，识别 `local_sha=000...000` 的 `delete-ref` 操作，写入 `pre-push-gate-manifest.json.operations`，不再把“没有 changed files”误判为异常。
2. upstream / remote tracking：没有 hook stdin 时，使用当前分支 `@{upstream}` 到 `HEAD` 的 diff。
3. 显式 `-ChangedScope`：只用于 smoke 或诊断；真实 push 无法推导 range 时必须失败，不得静默放行。

存在 changed-files 的 update-ref 仍然写入 `pre-push-classification.json`，随后只运行 `selected_pre_push_steps`。基础 step 固定为：

- `git-hygiene`
- `tool-readiness`
- `pre-push-classification`
- `changed-file-parse`

delete-ref 不复用 changed-files 分类语义。它必须追加 `remote-branch-delete-safety`，并按以下 fail-closed 规则阻断：

- 默认分支、当前分支、受保护模式命中的分支一律不得删除。
- 仍被任何 worktree checkout 的本地对应分支不得删除。
- 本地对应分支仍有 stash 残留或仍领先权威基线时不得删除。
- GitHub PR 状态为 open、unknown、ambiguous 时不得删除。
- 只有保护判定、本地残留判定、merge 判定、PR 判定全部通过时，删除推送才允许继续。

`tool-readiness` 必须读取同一份 classification evidence 和 `gates.json`，只检查 `selected_pre_push_steps` 实际可能使用的工具与 Python module。它必须生成 `pre-push-tool-readiness.json` / `.md`，不得用占位命令替代 readiness 证据。

风险类别与额外 quick checks：

| 类别 | 触发 quick checks | 说明 |
|---|---|---|
| `docs-only` / `low-risk` | 基础 step | 文档和低风险文本只做 Git hygiene、解析与证据生成。 |
| `hmi-ui` | `pyright-static`, `contracts-quick` | HMI UI 变更跑 Python 静态与 quick contracts。 |
| `hmi-command-chain` | `hmi-formal-gateway-contract`, `hmi-runtime-gateway-protocol-compat`, `contracts-quick` | HMI 命令链路必须验证 gateway contract 和协议兼容。 |
| `runtime-gateway-protocol` | `hmi-runtime-gateway-protocol-compat`, `contracts-quick` | runtime/gateway/shared protocol 变更必须跑协议兼容 quick check。 |
| `recipe-config-schema` | `recipe-config-schema-quick`, `contracts-quick` | 配置、配方、机台参数、schema 变更必须做 schema/config compatibility quick check。 |
| `motion-or-device-control` | `state-machine-interlock-contract`, `offline-simulation-smoke`, `contracts-quick` | 运动、设备、急停、暂停、复位、生产流程变更必须跑互锁/状态机契约和无硬件 dry-run observation smoke。 |
| `validation-or-build-system` | `validation-system-contract`, `contracts-quick` | gate、workflow、build/test 脚本变更必须跑门禁契约。 |
| `native-sensitive` | `native-followup-notice` | pre-push 只记录 native/full-offline follow-up；本地或 CI native gate 仍是权威证据。 |
| `hil-sensitive` | `hil-followup-notice` | pre-push 只记录 HIL follow-up；受控 HIL gate 仍是权威证据。 |

`native-sensitive` 和 `hil-sensitive` 在 pre-push 中不直接升级执行 full native/HIL。只有分类证据和 follow-up notice 不足以替代 required CI/HIL 结果。

## 3.3 条件增强门禁

`Strict Native Gate` 和 `Strict HIL Gate` 必须对所有 PR 产生稳定 GitHub check。非敏感 PR 由 classifier 输出 `not required` 后通过；敏感 PR 必须真实执行对应 gate。classifier 规则只允许维护在 `scripts/validation/gates/change-classification.json`，workflow 只负责调用 `classify-change.ps1`、执行 runner 信任判定、调用 `invoke-gate.ps1` 和上传证据。

Native required 条件包括：C++ runtime/gateway/planner、CMake/build/packaging、shared contracts/kernel/runtime、integration/e2e/protocol compatibility、validation scripts、`native-sensitive` label、`force_native`，以及 classifier 不可靠时默认 native required。

HIL required 条件包括：motion/homing/jog/move/dispensing sequence、IO/device adapter/vendor runtime、安全互锁/急停/限位、machine/hardware config、operator production workflow、`hardware-sensitive` label 和 `force_hil`。

不可信 fork 不得访问 self-hosted native/HIL runner。runner 离线、设备不可用、artifact 缺失、classification 失败或 required runner 不可信时，required check 必须 fail-closed。

## 3.4 Nightly / Release 闭环

`nightly` gate 承接 full-offline、performance suite、long-running simulation、baseline drift check、coverage report 和 performance baseline evidence。`python-coverage` 与 `cpp-coverage` 当前显式 `blocking=false`，只作为观察项；性能、长稳、baseline drift 和 performance baseline evidence 仍是 blocking。

`release` gate 暂时包装 `release-check.ps1`，用于版本、clean worktree、CHANGELOG、package/dry-run/release manifest 等发布自动化检查。发布 Go/No-Go 还必须核对最新 native/full-offline、nightly/performance 和 HIL 证据；PR passed 不能外推为 release ready。

## 3.5 日常选择矩阵

| 场景 | 必跑 gate | 可选/补充 | 说明 |
|---|---|---|---|
| 本地普通开发准备 push | `pre-push` | 目标 `test.ps1` 套件 | 默认快速反馈，按变更分类触发 quick checks。 |
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

pre-push wrapper 还会在同一目录生成：

- `pre-push-gate-manifest.json`：待 push range、changed files、dirty worktree 策略、分类、selected steps、native/HIL follow-up。
- `pre-push-classification.json`：分类证据。
- `tool-readiness/pre-push-tool-readiness.json`：selected-step 工具和 Python module readiness。

各 step 的业务报告目录固定为 `ReportRoot/<step-id>/`，例如：

- `tests/reports/github-actions/strict-pr-gate/semgrep/`
- `tests/reports/github-actions/strict-native-gate/cppcheck/`
- `tests/reports/github-actions/strict-hil-gate/controlled-hil/`

## 5. 失败语义

以下情况会使 gate 失败：

- blocking step 退出码非零。
- blocking step 超时。
- blocking step 的 required artifact 缺失。
- required tool 缺失。
- gate 级 required artifact 缺失。
- `SkipStep` 不带 `SkipJustification`。
- pre-push 无法可靠推导待 push commit range，且调用者没有显式提供 smoke 用 `ChangedScope`。
- Git hygiene 发现冲突、大文件、secret、生成物、路径违规、或 changed JSON/YAML/PowerShell/Python parse 失败。

非阻断 step 失败会记录在 summary 中，但不会改变 gate 总体通过状态。当前只有 nightly coverage 类观察项使用 `blocking=false`。

失败后的处理要求：

- 修复后重新运行同一个 gate，不能只补跑失败 step 后宣称 gate 通过。
- 工具缺失属于门禁失败；先运行 `scripts/validation/install-python-deps.ps1` 或安装对应系统工具。
- pre-push 不在运行中联网下载工具；`pyright` 必须预先安装在 PATH 上，不再使用 `npx --yes` fallback。
- Native runner、HIL runner 或硬件不可用时，required check 不能自动降级为通过。
- 设备故障可以在报告中分类为环境问题，但 required gate 仍保持阻断状态。

## 5.1 Dirty worktree 策略

pre-push 不再以“整个 worktree 必须完全干净”作为唯一模式。默认验证的是待 push commit 集合。

- 如果 worktree 干净，manifest 记录 `dirty_worktree=false`。
- 如果存在未提交改动，但这些文件与待 push changed files 不重叠，pre-push 允许继续，并在 `pre-push-gate-manifest.json` 和 `gate-manifest.json` 中记录 `dirty_worktree=true`、`dirty_files`、`dirty_policy=allowed-disjoint-dirty-worktree`。
- 如果未提交改动与待 push files 重叠，pre-push fail-closed，因为无法证明验证对象只来自待 push commit。
- pre-push 不允许使用 `git reset --hard`、`git checkout --` 或其它破坏性命令隔离工作树；需要隔离时使用独立 worktree 或由开发者显式处理。

这条策略禁止默默忽略未提交改动：允许继续时必须有 manifest 字段，无法隔离时必须失败。

## 5.2 工具安装建议

pre-push 只检查当前 selected steps 可能使用的工具：

- 基础 step：`git`、`python`、`powershell`。
- HMI UI 静态检查：`pyright`，需预先安装，例如 `npm install -g pyright`。
- Python contract quick checks：通过 `python -m pytest` 执行；缺依赖时运行 `.\scripts\validation\install-python-deps.ps1`。
- YAML changed-file parse：需要 `PyYAML` 或 `ruamel.yaml` 已安装；pre-push 不会临时联网安装。
- 配置/schema compatibility：需要 `jsonschema`；缺依赖时运行 `.\scripts\validation\install-python-deps.ps1`。

`pydeps`、`cppcheck`、dependency graph 等 native/full-offline 工具不阻断普通 pre-push，除非某个 selected step 明确声明需要它们。

## 5.3 Git hygiene quick checks

`git-hygiene` 统一产出 `pre-push-hygiene.json` 与 `pre-push-hygiene.md`，覆盖：

- unresolved conflict
- 待 push commit range 确认
- 大文件阈值检查
- 常见 secret / token / password 模式扫描
- 禁止提交明显生成物或报告目录
- 路径/文件名规范检查
- changed JSON/YAML/PowerShell/Python 基本 parse 检查

## 5.4 配置/schema 与离线仿真 quick checks

`recipe-config-schema-quick` 的正式边界：

- `data/schemas/**/*.schema.json` 必须用 `jsonschema.validators.validator_for(...).check_schema(...)` 自校验。
- 当前真实机台配置 `config/machine/machine_config.ini` 必须用 `configparser.ConfigParser(strict=True)` 解析。
- 其它 `config/`、recipe、machine、parameter JSON/YAML 若没有显式 schema/registry，必须 fail-closed；不得退化为 parse-only 通过。

`offline-simulation-smoke` 的正式入口是：

```powershell
python -m pytest tests\integration\scenarios\first-layer\test_real_dxf_machine_dryrun_observation_contract.py -q
```

该 smoke 必须保持无硬件、无真实运动、无 vendor runtime 启动。changed-file vendor/hardware marker 扫描只是附加 guard，不能替代 dry-run observation contract。

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
