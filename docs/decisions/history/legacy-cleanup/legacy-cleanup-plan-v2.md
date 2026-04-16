# Legacy Cleanup Plan V2

> 本文档是 `2026-03-19` 形成的历史 cleanup 计划快照，保留第二批收口时的风险表、冻结项与删除依据。
>
> 当前正式状态不要以本文为准。需要查看当前真值时，优先使用：
>
> - [docs/architecture/legacy-cutover-status.md](../../../architecture/legacy-cutover-status.md)
> - [docs/architecture/governance/migration/legacy-deletion-gates.md](../../../architecture/governance/migration/legacy-deletion-gates.md)
> - [docs/decisions/history/legacy-cleanup/legacy-removal-execution.md](./legacy-removal-execution.md)

更新时间：`2026-03-19`

补充说明：

- 本文档中的 `hmi-client` 判定已被 `docs/architecture/governance/migration/legacy-deletion-gates.md` 与 `docs/architecture/history/closeouts/removed-legacy-items.md` 覆盖；以下表格仅保留执行时上下文。
- 本文档中的 `dxf-pipeline` 判定已被 `docs/architecture/history/closeouts/dxf-pipeline-final-cutover.md` 与 `docs/architecture/history/closeouts/removed-legacy-items-final.md` 覆盖；以下表格仅保留执行时上下文。

## 1. 目标与判定口径

本轮基于当前稳定基线和验收结果，只执行“引用已断开、验证已覆盖”的最小安全 legacy 清理。

判定依据：

- `docs/runtime/field-acceptance.md` 只把 `mock`、`simulation`、`protocol compatibility` 作为当前可信通过面。
- `docs/architecture/history/audits/compatibility-shell-audit.md` 明确 `hmi-client`、`dxf-editor` 的 `scripts/*` 仍是兼容壳，`src/` 仍是重复包根，暂时不能删。
- 根级 CI 已统一到 `.github/workflows/workspace-validation.yml`；legacy app 目录下的旧单仓 CI 不再进入当前 build/test/CI 执行链。

本轮继续遵守：

1. 不做粗暴批量删除。
2. 不跨多个高风险模块。
3. 每一项删除都必须可回滚。

## 2. 本轮可删除项

### 2.1 已执行删除

| 路径 | 类型 | 删除依据 | 风险 | 回滚方式 |
|---|---|---|---|---|
| `hmi-client/.github/workflows/ci.yml`、`hmi-client/.github/PULL_REQUEST_TEMPLATE.md` | legacy CI 壳 | 仓内未发现活跃引用；GitHub Actions 当前只识别根级 `.github/workflows/workspace-validation.yml`；旧 `hmi-client-ci` 仍假定 `requirements.txt`、`tests/unit`、`src` 是独立单仓入口，已与现状脱节 | 极低；主要影响历史阅读者对旧目录“似乎还能独立跑 CI”的错觉 | 从删除前工作区备份恢复这两个文件，并同步回退 README、审计文档、移除清单 |
| `dxf-editor/.github/workflows/ci.yml`、`dxf-editor/.github/PULL_REQUEST_TEMPLATE.md` | legacy CI 壳 | 仓内未发现活跃引用；根级 workflow 已覆盖当前 app 验证；旧 `dxf-editor-ci` 同样停留在已移除的 `requirements.txt`、`tests/unit` 假设 | 极低；主要影响历史阅读者对旧目录“似乎还能独立跑 CI”的错觉 | 从删除前工作区备份恢复这两个文件，并同步回退 README、审计文档、移除清单 |

### 2.2 删除前确认

- 根级 `README.md`、`build.ps1`、`test.ps1`、`ci.ps1` 都不调用上述 legacy `.github/` 路径。
- `docs/architecture/build-and-test.md` 当前定义的 CI 入口只有根级 `ci.ps1`。
- 仓内扫描未发现对 `hmi-client/.github/workflows/ci.yml`、`dxf-editor/.github/workflows/ci.yml` 或对应 PR 模板的活跃引用。

## 3. 本轮冻结但不删除项

| 路径 | 当前状态 | 冻结原因 | 风险 | 回滚方式 |
|---|---|---|---|---|
| `hmi-client/scripts/*` | 冻结保留 | 仍需接住历史 `run/test/build` 命令入口 | 现在删除会切断旧命令兼容面 | 若误删，从工作区备份恢复脚本并回退 README/审计文档 |
| `dxf-editor/scripts/*` | 冻结保留 | 仍需接住历史 `run/test/package` 命令入口 | 现在删除会切断旧命令兼容面 | 若误删，从工作区备份恢复脚本并回退 README/审计文档 |
| `hmi-client/src/` | 冻结保留 | 与 `apps/hmi-app/src` 仍暴露同名 `hmi_client` 包根，外部残余 import 风险未清零 | 现在删除可能导致外部直连 import 或人工排障失效 | 若误删，从工作区备份恢复目录，并重跑 apps/compatibility 验证 |
| `dxf-editor/src/` | 冻结保留 | 与 `apps/dxf-editor-app/src` 仍暴露同名 `dxf_editor` 包根，外部残余 import 风险未清零 | 现在删除可能导致外部直连 import 或人工排障失效 | 若误删，从工作区备份恢复目录，并重跑 apps/compatibility 验证 |
| `control-core/src/adapters/tcp/` | 冻结保留 | 仍承担 legacy CMake target alias 与兼容测试断言 | 现在删除会破坏 transport compatibility 与旧 target 名 | 若误删，从工作区备份恢复目录，并重跑 transport/compatibility 测试 |
| `control-core/modules/control-gateway/` | 冻结保留 | 仍承担 legacy `siligen_control_gateway` target 名 | 现在删除会破坏旧 link target 与兼容测试 | 若误删，从工作区备份恢复目录，并重跑 transport/compatibility 测试 |

## 4. 仍不可删除项

| 路径 | 当前角色 | 为什么仍不可删除 | 风险 | 回滚方式 |
|---|---|---|---|---|
| `dxf-pipeline/` | 已完成删除 | 兼容 import/CLI 已迁入 `packages/engineering-data/src`，工作区内目录已不存在 | 当前风险转为工作区外调用者若仍依赖旧安装方式会失效 | 如需回到删除前状态，从 `tmp\legacy-removal-backups\20260319-074024\dxf-pipeline` 恢复并重跑 engineering/HMI 验证 |
| `control-core/build/bin/**/siligen_tcp_server.exe` | 唯一默认 HIL 可执行入口 | `apps/control-tcp-server/run.ps1` 与 `integration/hardware-in-loop/run_hardware_smoke.py` 仍依赖它 | 删除会让 HIL/现场烟测失效 | 若误删，恢复构建产物或从构建机重新生成，并重跑 HIL smoke |
| `control-core/build/bin/**/siligen_cli.exe` | 唯一工作区内 CLI 产物 | `apps/control-cli/run.ps1` 仍转发到它 | 删除会让 CLI wrapper 失效 | 若误删，恢复构建产物或从构建机重新生成，并重跑 CLI dry-run |
| `control-core/apps/control-runtime/`、`control-core/apps/control-tcp-server/` | transitional runtime/tcp 壳 | 真实 runtime/tcp 迁移未闭环，legacy main/alias 仍在使用 | 删除会波及 runtime/tcp 构建与运行链路 | 若误删，从工作区备份恢复目录，并重跑 dry-run/HIL 验证 |
| `control-core/src/domain/`、`control-core/src/application/`、`control-core/modules/process-core/`、`control-core/modules/motion-core/` | process/runtime 真实实现承载面 | `packages/process-runtime-core` 仍直接 include/link 这些实现 | 删除会破坏 runtime/process 构建与测试 | 若误删，从工作区备份恢复目录，并重跑相关单测与 workspace validation |
| `control-core/modules/device-hal/`、`control-core/modules/shared-kernel/` | device/shared-kernel 真实实现面 | `packages/device-adapters`、`packages/device-contracts` 仍直接 include legacy 头文件 | 删除会破坏 device 构建 | 若误删，从工作区备份恢复目录，并重跑相关构建/测试 |
| `control-core/config/machine_config.ini`、`control-core/data/recipes/*` | 配置/数据 fallback | 现场与旧链路仍可能从这里读取 | 删除会影响部署、回滚或现场支持 | 若误删，从备份恢复资产，并在部署环境重新核对 |

## 5. 本轮同步更新

本轮已同步更新：

- `README.md`
- `docs/architecture/build-and-test.md`
- `docs/architecture/history/audits/compatibility-shell-audit.md`
- `hmi-client/README.md`
- `dxf-editor/README.md`
- `docs/architecture/history/closeouts/removed-legacy-items.md`

说明：

- 根级 `build.ps1`、`test.ps1`、`ci.ps1` 的代码路径没有变化，因为它们原本就不依赖 legacy app 本地 `.github/`。
- 本轮 CI 口径变化体现在文档与目录收口，而不是脚本逻辑改写。

## 6. 本轮验证

执行命令：

```powershell
Set-Location D:\Projects\SiligenSuite
powershell -NoProfile -ExecutionPolicy Bypass -File .\hmi-client\scripts\test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\dxf-editor\scripts\test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\legacy-cleanup-v2-apps -FailOnKnownFailure
```

验收点：

- legacy `scripts/*` 兼容入口仍正常转发
- canonical `apps` suite 仍可通过
- 本轮不把 HIL、real hardware、`control-runtime` 独立 exe 缺口伪装成“已解决”
