# System Acceptance Report

更新时间：`2026-03-18`

## 1. 总结

- 本次验收基于真实执行结果，不采纳“未执行但返回 0”的假阳性。
- 协议兼容、工程场景回归、simulation 回归、DXF 编辑器头less DXF 加载均通过。
- `HMI -> control-tcp-server` 真实链路未通过验收：真实 `siligen_tcp_server.exe` 在端口就绪前即退出，报错 `IDiagnosticsPort 未注册`，导致真实 `status / recipe / dxf / alarms` 低风险查询链路均未能完成。
- HMI 客户端协议栈对 mock server 的全命令冒烟通过，但该结果只能记为 `mock-only`，不能替代真实 TCP server 验收。
- `integration/hardware-in-loop/run_hardware_smoke.py` 当前缺少模块入口调用，直接执行时会立即返回 `0` 且不运行 `main()`；因此根级本地 `test.ps1` 中的 `hardware-smoke: passed` 结论无效，必须人工纠偏。

## 2. 执行命令

```powershell
.\tools\scripts\install-python-deps.ps1
.\build.ps1 -Profile Local -Suite all
.\test.ps1 -Profile Local -Suite all -ReportDir integration\reports\system-acceptance -IncludeHardwareSmoke
.\ci.ps1 -Suite all -ReportDir integration\reports\system-acceptance-ci
python integration\hardware-in-loop\run_hardware_smoke.py
```

补充的真实验收命令：

- PowerShell inline Python：启动 `control-core\build\bin\Debug\siligen_tcp_server.exe`，再用 `apps\hmi-app` 的 `TcpClient` / `CommandProtocol` 做真实 `HMI -> TCP` 低风险探测。
- PowerShell inline Python：启动 `apps\hmi-app\src\hmi_client\tools\mock_server.py`，调用 `mock_smoke_test.main()` 做 HMI mock-only 全命令冒烟。
- PowerShell inline Python：调用 `apps\dxf-editor-app\src\dxf_editor\io\dxf_loader.load_dxf()` 头less 加载 `rect_diag.dxf`。

## 3. 协议兼容验证结果

| 验证项 | 命令 | 结果 | 说明 |
|---|---|---|---|
| application-contracts compatibility | `python packages\application-contracts\tests\test_protocol_compatibility.py` | 通过 | 应用层命令/查询 fixture 与契约检查通过。 |
| engineering-contracts compatibility | `python packages\engineering-contracts\tests\test_engineering_contracts.py` | 通过 | proto/schema/fixture 检查通过，仅有 `ezdxf`/`pyparsing` deprecation warning。 |
| engineering-data compatibility | `python packages\engineering-data\tests\test_engineering_data_compatibility.py` | 通过 | 导出 `rect_diag.pb` 成功。 |
| transport-gateway compatibility | `python packages\transport-gateway\tests\test_transport_gateway_compatibility.py` | 通过 | `TcpCommandDispatcher` 注册方法与 `application-contracts` 对齐。 |
| protocol-compatibility suite | `python integration\protocol-compatibility\run_protocol_compatibility.py` | 通过 | 跨 `application-contracts / engineering-contracts / engineering-data / transport-gateway` 套件整体通过。 |

结论：

- 协议定义层与 dispatcher 注册层兼容性通过。
- 本节结论只覆盖契约与适配层，不等同于真实 `control-tcp-server` 运行态可用性。

## 4. 场景回归结果

| 验证项 | 命令 | 结果 | 说明 |
|---|---|---|---|
| engineering regression | `python integration\scenarios\run_engineering_regression.py` | 通过 | `rect_diag.dxf -> rect_diag.pb -> simulation-input.json` 与 canonical fixture 一致。 |
| engineering-data export path | 包含于 `packages\engineering-data\tests\test_engineering_data_compatibility.py` | 通过 | 证明 simulation input/export 关键路径可执行。 |
| dxf-editor-app headless DXF load | PowerShell inline Python + `dxf_editor.io.dxf_loader.load_dxf()` | 通过 | 成功加载 `rect_diag.dxf`，结果为 `5` 个实体、`2` 个图层、版本 `AC1009`。 |

结论：

- DXF 加载与工程数据导出主路径通过。
- 本次未执行 DXF 编辑器 GUI 交互式保存/编辑回归；仅验证头less 加载能力。

## 5. Simulation 回归结果

| 验证项 | 命令 | 结果 | 说明 |
|---|---|---|---|
| simulation-engine build | `.\build.ps1 -Profile Local -Suite all` | 通过 | 重新配置并构建 `simulation_engine_smoke_test`、`simulation_engine_json_io_test`、`simulate_dxf_path`。 |
| simulation-engine smoke | `packages\simulation-engine\build\Debug\simulation_engine_smoke_test.exe` | 通过 | 根级 `test.ps1` 已执行。 |
| simulation-engine json io | `packages\simulation-engine\build\Debug\simulation_engine_json_io_test.exe` | 通过 | 根级 `test.ps1` 已执行。 |
| simulated-line regression | `python integration\simulated-line\run_simulated_line.py` | 通过 | `rect_diag` 仿真结果与 `integration\regression-baselines\rect_diag.simulation-baseline.json` 对齐。 |

结论：

- simulation-engine 构建、JSON IO、baseline 回归均通过。
- 当前未发现新的 simulation 数值回归。

## 6. HMI -> TCP 基本链路结果

### 6.1 真实链路

| 验证项 | 命令 | 结果 | 说明 |
|---|---|---|---|
| real server startup probe | PowerShell inline Python + `control-core\build\bin\Debug\siligen_tcp_server.exe` | 阻塞 | server 在端口 `127.0.0.1:9527` 就绪前退出，`stderr` 为 `[TCP Server] 错误: IDiagnosticsPort 未注册`。 |
| HMI read-only probe | PowerShell inline Python + `apps\hmi-app` `TcpClient` / `CommandProtocol` | 未验证 | 因真实 server 启动失败，真实 `ping / status / recipe.list / recipe.templates / recipe.schema.default / recipe.import(dry_run) / dxf.load / dxf.info / dxf.progress / alarms.list` 未能实际发出。 |

结论：

- 真实 `HMI -> TCP` 基本链路当前不能验收通过。
- 问题首先是运行态启动阻塞，不是已证实的应用契约不兼容。

### 6.2 mock-only 补充验证

| 验证项 | 命令 | 结果 | 说明 |
|---|---|---|---|
| HMI mock smoke | PowerShell inline Python + `mock_server.py` + `mock_smoke_test.main()` | 通过（mock-only） | `ping / connect / status / home / jog / move / dispenser / supply / dxf.load / dxf.info / dxf.execute / dxf.progress / alarms` 全链路通过。 |

结论：

- `apps/hmi-app` 客户端协议栈本身可工作。
- 该结果不能替代真实 `control-tcp-server` 验收，只能说明客户端与协议封装没有明显断裂。

### 6.3 高风险动作链路

- 真实 `connect / home / jog / move / dxf.execute` 未验证。
- 原因：真实 TCP server 启动已被阻塞，且在未确认真实机台隔离/安全条件前，不执行可能引发物理动作的命令。

## 7. Hardware-In-Loop / Smoke 结果

| 验证项 | 命令 | 结果 | 说明 |
|---|---|---|---|
| root local hardware-smoke case | `.\test.ps1 -Profile Local -Suite all -IncludeHardwareSmoke` | 无效 | 根级报告将其记为 `passed`，但该结论不可信。 |
| direct hardware-smoke script | `python integration\hardware-in-loop\run_hardware_smoke.py` | 无效 | 脚本直接返回 `0` 且无输出；实际未调用 `main()`。 |
| manual low-risk smoke | PowerShell inline Python + 启动真实 `siligen_tcp_server.exe` + 等待端口 | 阻塞 | 真实 server 报错 `IDiagnosticsPort 未注册`，未形成可连接端口。 |
| real machine / HIL actuation | 未执行 | 未执行 | 当前未确认真实硬件环境，也未进行实际机台动作冒烟。 |

结论：

- `hardware-in-loop` 自动化入口当前不可作为真实验收依据。
- 在修复脚本入口问题之前，任何来自该脚本的“通过”都必须视为无效。

## 8. 已知失败 vs 新回归

### 8.1 基线失败项

| 项目 | 分类 | 依据 | 影响 |
|---|---|---|---|
| `apps\control-runtime\run.ps1 -DryRun` 报 `BLOCKED` | 已知失败 | 根级本地 `test.ps1` 将其归类为 `known_failure`；`docs\architecture\build-and-test.md` 已说明尚无独立可执行文件。 | 不影响本次协议/仿真回归，但说明 canonical app 入口仍未实装。 |
| `siligen_tcp_server.exe` 启动时报 `IDiagnosticsPort 未注册` | 已知失败 | `integration\hardware-in-loop\README.md` 明确写明命中当前已知模式时应归类为 `known_failure`。 | 阻塞真实 `HMI -> TCP` 与硬件冒烟。 |

### 8.2 新回归 / 新发现问题

| 项目 | 分类 | 依据 | 影响 |
|---|---|---|---|
| `integration\hardware-in-loop\run_hardware_smoke.py` 缺少模块入口调用 | 新回归 | 文件末尾没有 `if __name__ == "__main__": raise SystemExit(main())`，直接运行脚本只会返回 `0`。 | 造成根级本地验收把未执行的 hardware smoke 记成通过，掩盖真实阻塞。 |

## 9. 本次验收判定

- 协议兼容：通过
- 场景回归：通过
- simulation 回归：通过
- DXF 加载关键路径：通过
- HMI -> TCP 真实基本链路：未通过 / 阻塞
- hardware-in-loop / smoke：未通过验收前置条件，自动化入口无效，真实 server 启动阻塞

建议后续动作：

1. 先修复 `integration\hardware-in-loop\run_hardware_smoke.py` 的模块入口问题，恢复 smoke 结果可信度。
2. 再定位并解决 `siligen_tcp_server.exe` 的 `IDiagnosticsPort 未注册`，之后重跑真实 `HMI -> TCP` 低风险链路。
3. 在确认安全隔离后，再补真实 `connect / home / jog / move / dxf.execute` 的机台侧冒烟。
