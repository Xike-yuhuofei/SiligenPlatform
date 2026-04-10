# Legacy Cleanup Plan

> 本文档是 `2026-03-18` 形成的历史 cleanup 计划快照，只保留当时的删除范围、风险判断与执行前假设。
>
> 当前正式状态不要以本文为准。需要查看当前真值时，优先使用：
>
> - [docs/architecture/legacy-cutover-status.md](../../../architecture/legacy-cutover-status.md)
> - [docs/architecture/legacy-deletion-gates.md](../../../architecture/legacy-deletion-gates.md)
> - [docs/decisions/history/legacy-cleanup/legacy-removal-execution.md](./legacy-removal-execution.md)

更新时间：`2026-03-18`

补充说明：

- 本文档记录的是第一批 legacy 收缩动作；`hmi-client` 的最终目录删除已由 `docs/architecture/legacy-deletion-gates.md` 与 `docs/architecture/history/closeouts/removed-legacy-items.md` 覆盖。

## 1. 目标

基于当前稳定基线与验收结果，先执行第一批“明确安全”的 legacy 清理，目标不是一次性删空旧目录，而是先移除已经没有活跃执行链路引用、却会继续诱导旧用法的安装面、测试面和打包面。

本轮采用的原则：

1. 只删仓内已确认没有活跃依赖的内容。
2. 保留仍承担过渡职责的 wrapper、compatibility shell 和 legacy 承载体。
3. 每删一批都同步更新 README、审计文档和验证结果。

## 2. 本轮可删除项

### 2.1 已执行删除

| 路径 | 类别 | 删除原因 | 风险 | 回滚方式 |
|---|---|---|---|---|
| `hmi-client/tests/` | legacy 重复测试树 | `scripts/test.ps1` 已转发到 `apps/hmi-app/scripts/test.ps1`，`apps/packages/integration/tools` 中未发现活跃引用 | 少数人工习惯可能仍会手动进入 legacy 目录找旧测试 | 从上一份工作区备份恢复目录即可 |
| `hmi-client/pyproject.toml` | legacy 安装面 | 会继续暴露 `hmi_client` 包并诱导 `pip install -e hmi-client` | 个别外部个人脚本若仍从旧目录做 editable install 会受影响 | 从工作区备份恢复文件 |
| `hmi-client/requirements.txt` | legacy 安装面 | 根级安装入口已切到 `apps/hmi-app/requirements.txt` | 同上 | 从工作区备份恢复文件 |
| `hmi-client/packaging/` | 失效打包目录 | 目录内无有效文件，且当前发布链不再使用该目录 | 几乎无 | 从工作区备份恢复目录 |
| `dxf-editor/tests/` | legacy 重复测试树 | `scripts/test.ps1` 已转发到 `apps/dxf-editor-app/scripts/test.ps1`，仓内未发现活跃引用 | 少数人工习惯可能仍会手动进入 legacy 目录找旧测试 | 从工作区备份恢复目录 |
| `dxf-editor/pyproject.toml` | legacy 安装面 | 会继续暴露 `dxf_editor` 包并诱导 `pip install -e dxf-editor` | 个别外部个人脚本若仍从旧目录做 editable install 会受影响 | 从工作区备份恢复文件 |
| `dxf-editor/requirements.txt` | legacy 安装面 | 根级安装入口已切到 `apps/dxf-editor-app/requirements.txt` | 同上 | 从工作区备份恢复文件 |
| `dxf-editor/packaging/` | 失效打包目录 | 目录内无有效文件，且当前发布链不再使用该目录 | 几乎无 | 从工作区备份恢复目录 |

### 2.2 删除前的确认结果

本轮执行前，已确认以下事实：

- `apps/`、`packages/`、`integration/`、`tools/` 当前没有活跃引用指向上述 legacy `tests/`、`pyproject.toml`、`requirements.txt`、`packaging/`。
- legacy HMI 与 DXF 的旧命令兼容壳仍由 `scripts/*.ps1` 提供，删除上述内容不会破坏旧命令入口本身。
- 当前验收链路和根级验证都依赖 canonical app，而不是 legacy 安装面。

## 3. 本轮不可删除项

| 路径 | 当前结论 | 不能删的原因 |
|---|---|---|
| `hmi-client/scripts/` | 保留 | 仍承担历史命令兼容入口。 |
| `dxf-editor/scripts/` | 保留 | 仍承担历史命令兼容入口。 |
| `hmi-client/src/` | 暂不删除 | 仍暴露 `hmi_client` 包名，删除前需至少一个兼容周期确认无外部直连 import。 |
| `dxf-editor/src/` | 暂不删除 | 仍暴露 `dxf_editor` 包名，删除前需至少一个兼容周期确认无外部直连 import。 |
| `dxf-pipeline/` | 不可删除 | canonical HMI 预览当前仍直接依赖该兼容层。 |
| `control-core/apps/control-tcp-server/` | 不可删除 | 真实 TCP server 薄入口仍在这里。 |
| `control-core/apps/control-runtime/` | 不可删除 | 仍保留兼容 target 名称，且 root runtime 入口尚未闭环。 |
| `control-core/src/adapters/tcp/` | 不可删除 | 兼容测试仍要求 legacy target 存在。 |
| `control-core/modules/control-gateway/` | 不可删除 | 兼容测试和 legacy CMake target 仍依赖。 |
| `control-core/src/domain/`、`control-core/src/application/` | 不可删除 | `packages/process-runtime-core` 仍直接依赖这些实现。 |
| `control-core/modules/shared-kernel/` | 不可删除 | `device-*` packages 仍 include 该 legacy 头文件。 |
| `control-core/config/machine_config.ini` | 暂不删除 | 仍是配置 fallback。 |
| `control-core/data/recipes/` | 暂不删除 | 仍是数据 fallback。 |

## 4. 风险判断

### 4.1 已接受风险

本轮接受的风险仅限于：

- 少数仍手工使用 legacy `pyproject.toml`、`requirements.txt`、`tests/` 的个人习惯会被打断。

接受依据：

- 当前仓内自动化已经不再从这些路径起步。
- legacy `scripts/` 仍保留，因此旧命令兼容入口没有被一起切断。

### 4.2 本轮明确不接受的风险

以下风险本轮不接受，因此对应路径不删：

1. 影响 HMI 预览、部署或现场流程的 `dxf-pipeline` 依赖。
2. 影响 TCP server / CLI / runtime wrapper 的 `control-core` 运行链路。
3. 影响 `process-runtime-core`、`device-adapters`、`device-contracts` 的 legacy include / link 依赖。
4. 直接删除 legacy `src/`，导致外部残余 import 无法兜底。

## 5. 回滚方式

本轮回滚非常直接：

1. 从发布前工作区备份恢复被删文件或目录。
2. 重新执行本轮验证命令。
3. 若恢复的是 HMI 或 DXF legacy 安装面，再同步检查 README 和审计文档是否需要回退。

建议保留的回滚备份最小集合：

- `hmi-client/tests/`
- `hmi-client/pyproject.toml`
- `hmi-client/requirements.txt`
- `hmi-client/packaging/`
- `dxf-editor/tests/`
- `dxf-editor/pyproject.toml`
- `dxf-editor/requirements.txt`
- `dxf-editor/packaging/`

## 6. 本轮同步更新

本轮已同步更新：

- `hmi-client/README.md`
- `dxf-editor/README.md`
- `docs/architecture/history/audits/compatibility-shell-audit.md`

说明：

- build/test 脚本本轮无需改代码，因为 legacy `scripts/*.ps1` 早已转发到 canonical app。
- 本轮重点是清理旧安装面与测试面，而不是改动 canonical 构建逻辑。

## 7. 验证要求

本轮删除后需要验证两类能力：

1. legacy 命令兼容壳仍可用
2. canonical app 的根级 `apps` 验证仍可通过

执行命令见本文件下一节。

## 8. 本轮验证

本轮计划执行：

```powershell
Set-Location D:\Projects\SiligenSuite
powershell -NoProfile -ExecutionPolicy Bypass -File .\hmi-client\scripts\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\dxf-editor\scripts\package.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\hmi-client\scripts\test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\dxf-editor\scripts\test.ps1
.\test.ps1 -Profile Local -Suite apps -ReportDir integration\reports\verify\legacy-cleanup-apps
```

实际结果：

- `hmi-client\scripts\build.ps1`：通过
- `dxf-editor\scripts\package.ps1`：通过
- `hmi-client\scripts\test.ps1`：通过，`3` 项测试通过
- `dxf-editor\scripts\test.ps1`：通过，`2` 项测试通过
- `.\test.ps1 -Profile Local -Suite apps -ReportDir integration\reports\verify\legacy-cleanup-apps`：通过，`passed=4`、`failed=0`、`known_failure=1`

本轮唯一 `known_failure`：

- `control-runtime-dryrun`

说明：

- 这是当前基线内已知缺口，不是由本轮清理引入的新回归。
- 报告路径：`integration\reports\verify\legacy-cleanup-apps\workspace-validation.md`

## 9. 下一批技术债

仍需后续清理的重点技术债：

1. `hmi-client/src/` 与 `dxf-editor/src/` 仍是重复 Python 包根。
2. `dxf-pipeline` 仍被 canonical HMI 直接调用，尚不能下线。
3. `control-core/build/bin/**` 仍是 TCP server / CLI 的真实产物来源。
4. `apps/control-runtime` 仍无独立 exe。
5. `control-core/src/*`、`modules/*` 仍承担多个 package 的真实实现。
