# Control Core Provenance Audit

更新时间：`2026-03-19`

## 1. 审计目标

本次只审计以下 4 类对象：

1. `control-core/config/*`
2. `control-core/data/recipes/*`
3. `control-core/build/bin/*`
4. 工作区外部署 / HIL / 脚本引用

目标是确认哪些 residual consumer 已归零，哪些仍阻塞 `control-core/` 物理删除。

## 2. 结论总表

| 审计对象 | 当前实体状态 | residual consumer 结果 | 结论 |
|---|---|---|---|
| `control-core/config/*` | 仍存在 `control-core/config/machine_config.ini` | 审计范围内未发现 live consumer | 已归零，可随 `control-core/` 一起删 |
| `control-core/data/recipes/*` | 仍存在 9 个历史 JSON 资产 | 审计范围内未发现 live consumer | 已归零，可随 `control-core/` 一起删 |
| `control-core/build/bin/*` | 仍存在 legacy CLI / TCP server / unit test 产物 | 审计范围内未发现 live consumer | provenance 已归零，可随 `control-core/` 一起删 |
| 工作区外部署 / HIL / 脚本引用 | 审计了 `D:\Projects\*` sibling 项目与 `D:\Deploy` | 未发现 live residual consumer；只发现 1 条 lineage 文档说明 | 工作区外未见阻塞项 |

## 3. 证据

### 3.1 `control-core/config/*`

现状：

- 目录内仅发现 `control-core/config/machine_config.ini`
- 活跃运行链路已经切到根级 `config/machine/machine_config.ini`

正向 canonical 证据：

- `packages/test-kit/src/test_kit/workspace_validation.py` 使用 `config/machine/machine_config.ini`
- `integration/performance/collect_baselines.py` 使用 `config/machine/machine_config.ini`
- `docs/runtime/deployment.md` 明确要求“不要再同步 `control-core\config` 作为默认副本”

负向审计结果：

- 在工作区活跃代码 / 脚本范围内，未发现任何 `control-core/config/machine_config.ini` 的 live runtime/build/HIL consumer
- 工作区外仅命中 `D:\Projects\Backend_CPP\docs\implementation-notes\old-repo-disposition-checklist-2026-03-16.md:21`
  - 该命中只是迁移映射说明：`config/ -> D:\Projects\SiligenSuite\control-core\config`
  - 不是运行态、构建态或部署态消费者

结论：

- `control-core/config/*` residual consumer 已归零

### 3.2 `control-core/data/recipes/*`

现状：

- 目录内保留 9 个历史 JSON 资产：
  - `import_template_bundle.json`
  - `recipe_export.json`
  - `templates.json`
  - `audit/*`
  - `recipes/*`
  - `versions/*`

正向 canonical 证据：

- `packages/runtime-host` 已切到根级 `data/recipes/`
- `docs/runtime/deployment.md` 要求默认发布根级 `data\recipes\`
- `docs/runtime/rollback.md` 要求默认回滚根级 `data\recipes\`

负向审计结果：

- 在工作区活跃代码 / 脚本范围内，未发现任何 `control-core/data/recipes/*` 的 live runtime/build/HIL consumer
- 工作区外未发现 `control-core/data/recipes` 命中

结论：

- `control-core/data/recipes/*` residual consumer 已归零

### 3.3 `control-core/build/bin/*`

现状：

- 目录下仍保留 `siligen_cli.exe`、`siligen_tcp_server.exe`、`siligen_unit_tests.exe`、`siligen_pr1_tests.exe` 等历史产物

已完成迁移的链路：

- `apps/control-runtime/run.ps1` 只认 canonical `siligen_control_runtime.exe`
- `apps/control-tcp-server/run.ps1` 只认 canonical `siligen_tcp_server.exe`
- `integration/hardware-in-loop/run_hardware_smoke.py` 默认只认 `<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_tcp_server.exe`
- `apps/hmi-app/config/gateway-launch.sample.json` 的部署样例已经指向 `D:\Deploy\Siligen\siligen_tcp_server.exe`

非阻塞但需保留说明的文档命中：

- 当前运行 / 回滚文档已经同步为 canonical 口径，不再把 `control-core\build\bin\**\siligen_cli.exe` 记录为默认或显式 CLI fallback。

结论：

- `control-core/build/bin/*` residual consumer 已归零
- 该目录不再构成 `control-core/` 物理删除的独立 provenance 阻塞项

### 3.4 工作区外部署 / HIL / 脚本引用

审计范围：

- `D:\Projects\Backend_CPP`
- `D:\Projects\Document`
- `D:\Projects\DXF`
- `D:\Projects\ERP`
- `D:\Deploy`

搜索关键词：

- `siligen_control_gateway_tcp_adapter`
- `siligen_tcp_adapter`
- `siligen_control_gateway`
- `control-core\config`
- `control-core\data\recipes`
- `control-core\build\bin`
- `siligen_cli.exe`
- `siligen_tcp_server.exe`
- `run_hardware_smoke.py`

结果：

- 未发现任何工作区外 alias residual consumer
- 未发现任何工作区外 `control-core/build/bin`、`control-core/data/recipes` 的 live deployment / HIL / script consumer
- 唯一命中是 `Backend_CPP` 中的 lineage 文档说明，不构成运行态依赖
- `D:\Deploy` 目录下未命中上述 legacy 路径；当前可见部署样例也已指向 canonical `siligen_tcp_server.exe`

## 4. 附带发现

以下命中不属于本次强制审计的 3 个目录，但会影响未来 `control-core/` 物理删除：

- 当前未发现新的 HMI runtime/provenance consumer。
- `2026-03-19` 已进一步完成 `control-core/modules/shared-kernel`、`control-core/src/domain`、`control-core/src/application`、`control-core/modules/process-core`、`control-core/modules/motion-core` 的 fresh provenance 审计与物理删除。
- 因此，后续阻塞已收敛为 `control-core/src/shared`、`control-core/src/infrastructure`、`control-core/modules/device-hal`、`control-core/third_party` 与顶层 CMake/source-root owner。

## 5. 进入物理删除前还剩什么

1. 迁出 `packages/process-runtime-core` 对 `control-core/src/shared`、`control-core/src/infrastructure/adapters/planning/dxf` 和 `control-core/third_party` 的剩余依赖
2. 迁出 `packages/device-adapters`、`packages/device-contracts` 对 `control-core/modules/device-hal` 与 `control-core/third_party` 的剩余依赖
3. 继续迁出 `control-core` 顶层 CMake/source-root owner 职责
