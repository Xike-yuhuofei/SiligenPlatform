# Release Readiness Standard

更新时间：`2026-04-01`

## 1. 目的

本文档是 `SiligenSuite` 上位机版本发布的唯一正式判定标准。

- 任何版本若未满足本文全部适用门禁，不得宣称“具备发布条件”。
- 任何评审、QA、预发布或现场说明，均不得绕过本文降低口径。
- 本文同时适用于软件工程门禁与机械/自动化工程门禁。

## 2. 发布分类

### 2.1 RC 候选版

`RC` 只表示仓内候选版已经满足最小发布闭环，可进入受控验证或有限范围试运行，不等同正式对外交付。

`RC` 至少必须满足：

- `G1-G6`
- 所有自动化门禁 `failed=0`
- 所有自动化门禁 `known_failure=0`
- `legacy-exit finding_count=0`

### 2.2 正式发布版

正式发布版表示可用于对外交付、现场部署、验收或版本冻结。

正式发布版必须满足：

- `G1-G8`
- 任何阻断项均为 `0`
- 不存在 `BLOCKED` 应用入口
- 已完成软件、仓外与现场验证闭环

## 3. 发布门禁

| 编号 | 门禁 | 硬规则 | 最低证据 |
|---|---|---|---|
| `G1` | 范围与边界冻结 | 版本范围、适用机型、协议边界、配置边界、非目标项必须明确；不得以历史目录或兼容壳作为正式入口 | `CHANGELOG.md`、版本范围说明、部署/回滚文档 |
| `G2` | 源码与制品可追溯 | 必须从唯一提交生成唯一制品；工作树必须可审计；版本、提交、依赖、报告路径必须可回溯 | Git `HEAD`、release manifest、版本条目 |
| `G3` | 构建与仓内验证通过 | `build/test/ci` 必须通过；`known_failure=0`；`legacy-exit finding_count=0`；canonical app dry-run 不得为 `BLOCKED` | `build.ps1`、`test.ps1`、`ci.ps1`、`release-check.ps1` 证据 |
| `G4` | 契约、e2e 与性能基线冻结 | `apps/contracts/e2e/performance` 必须实跑通过；性能基线必须冻结并可复查 | `tests/reports/baseline-*`、`tests/reports/performance/latest.*` |
| `G5` | 部署与回滚可执行 | 安装、升级、回滚、配置迁移、故障导出路径必须明确；不得依赖已删除旧根 | `docs/runtime/deployment.md`、`docs/runtime/rollback.md`、交付清单 |
| `G6` | 发布包表达一致 | 仓库、文档、脚本、交付说明必须统一使用单轨根与 canonical 入口；不得混用历史语义 | 发布流程文档、系统验收报告、交付材料 |
| `G7` | 仓外交付物观察通过 | 已部署环境、发布包、现场脚本、回滚包不得残留 legacy 入口、删除目录别名或错误配置根 | `docs/runtime/external-migration-observation.md` 对应观察证据 |
| `G8` | 设备级与现场级验证通过 | `hardware smoke` 只能证明最小启动闭环；正式发布必须补齐 HIL/真机链路、异常恢复和必要长稳 | HIL 报告、现场记录、异常注入记录 |

## 4. 不可降低的红线

以下任一条件成立，结论直接为 `No-Go`：

- 工作树不干净，无法明确发布提交边界
- `CHANGELOG.md` 未包含目标版本正式条目
- `build/test/ci` 任一失败
- 任一自动化报告存在 `known_failure > 0`
- `legacy-exit finding_count > 0`
- 任一 canonical 应用 dry-run 为 `BLOCKED`
- 使用已删除旧根、兼容壳、历史别名或非 canonical 入口作为正式交付口径
- 仅凭 mock、simulation 或 hardware smoke 宣称“已完成正式现场验收”
- 设备安全、互锁、急停、限位、断网、超时、配置错误等异常场景无验证证据
- 回滚方案不明确，或无法说明版本、配置、数据如何恢复

## 5. 结果语义

### 5.1 Go

只有当当前发布分类要求的全部门禁都通过，才可给出 `Go`。

### 5.2 Conditional Go

本标准不接受用 `Conditional Go` 替代正式发布结论。

- 对 `RC`：允许标记“可继续进入下一阶段验证”，但不得对外表述为正式可交付。
- 对正式发布：不存在“带阻断上线”的合规结论。

### 5.3 No-Go

只要存在红线项、证据缺失、门禁未跑或现场验证未闭环，即为 `No-Go`。

## 6. 最低执行要求

### 6.1 RC 候选版最低执行链

```powershell
Set-Location <repo-root>
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
.\release-check.ps1 -Version <x.y.z-rc.n>
```

### 6.2 正式发布版最低执行链

```powershell
Set-Location <repo-root>
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
.\release-check.ps1 -Version <x.y.z> -IncludeHardwareSmoke
```

上述 `release-check.ps1` 当前默认纳入 `hil-case-matrix`，要求 `hil-case-matrix/case-matrix-summary.json/.md` 作为同批自动化证据。

如需临时隔离矩阵影响，可显式传 `-IncludeHilCaseMatrix:$false`；该结果仅可用于排障，不构成本节定义的默认执行链，也不替代 `G8` 所要求的 HIL / 真机 / 现场验证闭环。

在此基础上，还必须补齐：

- 仓外交付物观察
- HIL / 真机 / 长稳证据

## 7. 执行解释

- `release-check.ps1` 负责执行仓内可自动化校验项，是本文的脚本化子集，不等于全部门禁。
- `docs/runtime/field-acceptance.md` 定义现场验证层的最低口径。
- `docs/runtime/deployment.md` 与 `docs/runtime/rollback.md` 定义交付与回退约束。
- `docs/runtime/external-migration-observation.md` 定义仓外观察范围。

## 8. 强制性声明

从 `2026-03-25` 起，本文作为上位机发布判定标准硬性执行。

- 未满足本文要求，不得出具“具备发布条件”“可正式发版”“可对外交付”的结论。
- 任一评审结论若与本文冲突，以本文为准。
