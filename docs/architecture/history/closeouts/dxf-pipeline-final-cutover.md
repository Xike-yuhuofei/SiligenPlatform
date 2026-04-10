# DXF Tooling Final Cutover

更新时间：`2026-03-22`

## 1. 范围与结果

本轮目标是把 `dxf-pipeline/` 剩余的真实职责全部迁出，使工作区不再需要 `D:\Projects\SiligenSuite\dxf-pipeline` 参与任何默认运行链路。

执行结果：

- `dxf-pipeline/` 已整体移出工作区并备份到 `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-074024\dxf-pipeline`
- `packages/engineering-data/src/dxf_pipeline/**` 已删除
- `packages/engineering-data/src/proto/dxf_primitives_pb2.py` 已删除
- legacy console script alias 已从 `packages/engineering-data/pyproject.toml` 删除，只保留 canonical `engineering-data-*`

## 2. 删除前确认结果

- `apps/hmi-app` 生产页预览链路已切到 gateway `dxf.preview.snapshot` / `dxf.preview.confirm`
- 顶层 `hmi-client` 历史 preview bridge 已从 `dxf_pipeline.cli.generate_preview` 切换到 `engineering_data.cli.generate_preview`
- `packages/process-runtime-core` 的外部 DXF launcher 协议已切到 canonical 子命令 `engineering-data-dxf-to-pb`
- 工作区内未再发现真实代码调用 `dxf-pipeline/src` 作为 source root

## 3. 实际删除内容

- 整个 `dxf-pipeline/` 目录
- 目录移除方式：移动到回滚备份目录，而不是直接不可恢复删除
- 回滚路径：`D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-074024\dxf-pipeline`

## 4. 保留内容及原因

工作区内不再保留 `dxf-pipeline/` 目录。

当前不再保留任何 `dxf-pipeline` 兼容壳：

- 不再保留 `packages/engineering-data/src/dxf_pipeline/**`
- 不再保留 `packages/engineering-data/src/proto/dxf_primitives_pb2.py`
- 不再保留 `packages/engineering-data/pyproject.toml` 中的 legacy script alias

不再支持的旧做法：

- 把 `dxf-pipeline/src` 加到 `PYTHONPATH`
- 依赖 `dxf-pipeline` 包本身提供 legacy CLI 安装面
- 继续使用 `dxf_pipeline.*`、`proto.dxf_primitives_pb2` 或 legacy CLI 名称

## 5. 兼容壳退出结果

以下 legacy 入口已全部退出：

- `dxf_pipeline.cli.*`
- `dxf_pipeline.contracts.*`
- `dxf_pipeline.preview.html_preview`
- `dxf_pipeline.services.*`
- `proto.dxf_primitives_pb2`

当前原则：

- 只保留 `engineering_data.*` canonical API / CLI
- 不再从任何兼容层转发 legacy 模块名
- `legacy-exit` 门禁把这些 legacy 名称视为归零项

## 6. engineering-data 正式执行入口

当前 canonical owner：`packages/engineering-data/pyproject.toml`

当前仅保留 canonical CLI：

- `engineering-data-dxf-to-pb` -> `engineering_data.cli.dxf_to_pb:main`
- `engineering-data-path-to-trajectory` -> `engineering_data.cli.path_to_trajectory:main`
- `engineering-data-export-simulation-input` -> `engineering_data.cli.export_simulation_input:main`
- `engineering-data-generate-preview` -> `engineering_data.cli.generate_preview:main`

工作区正式脚本入口：

- `tools/engineering-data-bridge/scripts/dxf_to_pb.py`
- `tools/engineering-data-bridge/scripts/path_to_trajectory.py`
- `tools/engineering-data-bridge/scripts/export_simulation_input.py`
- `tools/engineering-data-bridge/scripts/generate_preview.py`

owner-local 维护入口仍保留在：

- `packages/engineering-data/scripts/dxf_to_pb.py`
- `packages/engineering-data/scripts/path_to_trajectory.py`
- `packages/engineering-data/scripts/export_simulation_input.py`
- `packages/engineering-data/scripts/generate_preview.py`

## 7. 当前工作区调用方切换结果

### 7.1 `apps/hmi-app`

- 结果：运行时主链路已切到 canonical gateway preview gate
- 当前调用：`dxf.preview.snapshot` / `dxf.preview.confirm`
- 状态：不再依赖 sibling `dxf-pipeline/`

### 7.2 顶层 `hmi-client`

- 结果：已切到 canonical
- 当前调用：`python -m engineering_data.cli.generate_preview --json`
- 状态：不再依赖 sibling `dxf-pipeline/`

额外约束：

- `tools/engineering-data-bridge/scripts/*` 现在直接导入 `engineering_data.cli.*`
- `packages/engineering-data/scripts/*` 不再是工作区默认转调面
- `process-runtime-core` 内的历史 `dxf_pipeline_path` 命名已统一替换为 `engineering_data_entry_root`

### 7.3 契约 / 集成链路

- `packages/engineering-contracts/tests/test_engineering_contracts.py`
  - 结果：保持 canonical，仅消费 `engineering_data` 与 HMI consumer
- `integration/scenarios/run_engineering_regression.py`
  - 结果：默认只依赖 `tools/engineering-data-bridge/scripts/*`
- `integration/protocol-compatibility/run_protocol_compatibility.py`
  - 结果：通过 canonical `engineering-data` / `engineering-contracts` 校验

## 8. 删除后回归验证结果

删除后已执行并通过：

- `python -m unittest packages.engineering-data.tests.test_engineering_data_compatibility`
  - `Ran 3 tests ... OK`
- `python packages/engineering-data/tests/test_dxf_pipeline_legacy_exit.py`
  - `Ran 2 tests ... OK`
- `python -m unittest packages.engineering-contracts.tests.test_engineering_contracts`
  - `Ran 10 tests ... OK`
- `python integration\scenarios\run_engineering_regression.py`
  - `engineering regression passed`
- `python integration\protocol-compatibility\run_protocol_compatibility.py`
  - `protocol compatibility suite passed`

额外验证：

- `Test-Path D:\Projects\SiligenSuite\dxf-pipeline` -> `False`
- `import dxf_pipeline...` 在 `PYTHONPATH=packages/engineering-data/src` 下必须失败
- `import proto.dxf_primitives_pb2` 在 `PYTHONPATH=packages/engineering-data/src` 下必须失败

## 9. 工作区外调用者风险与兼容窗口

风险点：

- 工作区外如果仍把 `dxf-pipeline/src` 直接塞进 `PYTHONPATH`，本次 cutover 后会失效
- 工作区外如果仍只安装 `dxf-pipeline` 包，也会失去 legacy CLI 安装面

兼容窗口说明：

- 外部兼容窗口已结束
- 外部调用者必须迁移到 canonical `engineering-data-*` CLI 与 `engineering_data.*` / `engineering_data.proto.dxf_primitives_pb2`
- 若现场脚本仍缓存旧入口，只能在升级前先完成人工迁移，不能再依赖仓内兼容壳

## 10. 最终结论

- 剩余运行阻塞项：无
- 已清理的 legacy 入口：
  - `dxf-pipeline/` 整目录
  - `dxf-pipeline/src` source root
  - `packages/engineering-data/src/dxf_pipeline/**`
  - `packages/engineering-data/src/proto/dxf_primitives_pb2.py`
  - `packages/engineering-data/pyproject.toml` 中的 legacy console script alias
  - 顶层 `hmi-client` 对 `dxf_pipeline.cli.generate_preview` 的直接调用
- 删除 `dxf-pipeline` 兼容窗口的确认条件：已满足
