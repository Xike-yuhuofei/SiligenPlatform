# dxf-pipeline Final Cutover

更新时间：`2026-03-19`

## 1. 范围与结果

本轮目标是把 `dxf-pipeline/` 剩余的真实职责全部迁出，使工作区不再需要 `D:\Projects\SiligenSuite\dxf-pipeline` 参与任何默认运行链路。

执行结果：

- `dxf_pipeline.*` import shim 已迁入 `packages/engineering-data/src/dxf_pipeline`
- `proto.dxf_primitives_pb2` 兼容层已迁入 `packages/engineering-data/src/proto`
- legacy console scripts 继续保留原名称，但 canonical owner 已统一为 `packages/engineering-data`
- `dxf-pipeline/` 已整体移出工作区并备份到 `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-074024\dxf-pipeline`

## 2. 删除前确认结果

- `apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/preview_client.py` 已直接调用 `engineering_data.cli.generate_preview`
- 顶层 `hmi-client/src/hmi_client/integrations/dxf_pipeline/preview_client.py` 已从 `dxf_pipeline.cli.generate_preview` 切换到 `engineering_data.cli.generate_preview`
- `packages/engineering-data/pyproject.toml` 已由 canonical 包直接暴露以下 legacy CLI 名称：
  - `dxf-to-pb`
  - `path-to-trajectory`
  - `export-simulation-input`
  - `generate-dxf-preview`
- 前置验证已确认以下命令在只提供 `packages/engineering-data/src` 的情况下仍可工作：
  - `python -m dxf_pipeline.cli.generate_preview --help`
  - `python -m dxf_pipeline.cli.dxf_to_pb --help`
- 工作区内未再发现真实代码调用 `dxf-pipeline/src` 作为 source root

## 3. 实际删除内容

- 整个 `dxf-pipeline/` 目录
- 目录移除方式：移动到回滚备份目录，而不是直接不可恢复删除
- 回滚路径：`D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-074024\dxf-pipeline`

## 4. 保留内容及原因

工作区内不再保留 `dxf-pipeline/` 目录。

保留的是 canonical 包中的兼容壳：

- `packages/engineering-data/src/dxf_pipeline/**`
  - 原因：继续承接历史 `import dxf_pipeline...` 用户，但真实实现全部转发到 `engineering_data.*`
- `packages/engineering-data/src/proto/dxf_primitives_pb2.py`
  - 原因：继续承接历史 `proto.dxf_primitives_pb2` import
- `packages/engineering-data/pyproject.toml` 中的 legacy script alias
  - 原因：继续承接历史 CLI 名称，但安装 owner 已改为 canonical 包

不再支持的旧做法：

- 把 `dxf-pipeline/src` 加到 `PYTHONPATH`
- 依赖 `dxf-pipeline` 包本身提供 legacy CLI 安装面

## 5. `dxf_pipeline.*` import shim 迁移结果

已迁移到 `packages/engineering-data/src` 的兼容面：

- `dxf_pipeline.cli.*`
- `dxf_pipeline.contracts.*`
- `dxf_pipeline.preview.html_preview`
- `dxf_pipeline.services.*`
- `proto.dxf_primitives_pb2`

迁移原则：

- 兼容包仍保留历史模块名
- 真正实现只从 `engineering_data.*` 导入
- 不再从 `dxf-pipeline/src` 注入 `sys.path`

## 6. Legacy Console Scripts Canonical 转发路径

当前 canonical owner：`packages/engineering-data/pyproject.toml`

脚本映射关系：

- `dxf-to-pb` -> `engineering_data.cli.dxf_to_pb:main`
- `path-to-trajectory` -> `engineering_data.cli.path_to_trajectory:main`
- `export-simulation-input` -> `engineering_data.cli.export_simulation_input:main`
- `generate-dxf-preview` -> `engineering_data.cli.generate_preview:main`

工作区脚本入口：

- `packages/engineering-data/scripts/dxf_to_pb.py`
- `packages/engineering-data/scripts/path_to_trajectory.py`
- `packages/engineering-data/scripts/export_simulation_input.py`
- `packages/engineering-data/scripts/generate_preview.py`

## 7. 当前工作区调用方切换结果

### 7.1 `apps/hmi-app`

- 结果：已切到 canonical
- 当前调用：`python -m engineering_data.cli.generate_preview --json`
- 状态：不再依赖 sibling `dxf-pipeline/`

### 7.2 顶层 `hmi-client`

- 结果：已切到 canonical
- 当前调用：`python -m engineering_data.cli.generate_preview --json`
- 状态：不再依赖 sibling `dxf-pipeline/`

### 7.3 契约 / 集成链路

- `packages/engineering-contracts/tests/test_engineering_contracts.py`
  - 结果：保持 canonical，仅消费 `engineering_data` 与 HMI consumer
- `integration/scenarios/run_engineering_regression.py`
  - 结果：只依赖 `packages/engineering-data/scripts/*`
- `integration/protocol-compatibility/run_protocol_compatibility.py`
  - 结果：通过 canonical `engineering-data` / `engineering-contracts` 校验

## 8. 删除后回归验证结果

删除后已执行并通过：

- `python -m unittest packages.engineering-data.tests.test_engineering_data_compatibility`
  - `Ran 3 tests ... OK`
- `python -m unittest packages.engineering-data.tests.test_dxf_pipeline_legacy_shims`
  - `Ran 3 tests ... OK`
- `python -m unittest apps.hmi-app.tests.unit.test_preview_client`
  - `Ran 1 test ... OK`
- `python -m unittest packages.engineering-contracts.tests.test_engineering_contracts`
  - `Ran 10 tests ... OK`
- `python integration\scenarios\run_engineering_regression.py`
  - `engineering regression passed`
- `python integration\protocol-compatibility\run_protocol_compatibility.py`
  - `protocol compatibility suite passed`

额外验证：

- `Test-Path D:\Projects\SiligenSuite\dxf-pipeline` -> `False`
- `python -m dxf_pipeline.cli.generate_preview --help` 在 `PYTHONPATH=packages/engineering-data/src` 下可执行
- `python -m dxf_pipeline.cli.dxf_to_pb --help` 在 `PYTHONPATH=packages/engineering-data/src` 下可执行

## 9. 工作区外调用者风险与兼容窗口

风险点：

- 工作区外如果仍把 `dxf-pipeline/src` 直接塞进 `PYTHONPATH`，本次 cutover 后会失效
- 工作区外如果仍只安装 `dxf-pipeline` 包，也会失去 legacy CLI 安装面

兼容窗口说明：

- 继续保留 legacy import 名称和 legacy CLI 名称
- 兼容 owner 改为 `engineering-data`
- 外部调用者应在下一次环境刷新前完成迁移：
  - 安装 `engineering-data`
  - 或在源码模式下改为使用 `packages/engineering-data/src`

## 10. 最终结论

- 剩余运行阻塞项：无
- 已清理的 legacy 入口：
  - `dxf-pipeline/` 整目录
  - `dxf-pipeline/src` source root
  - `dxf-pipeline/pyproject.toml` legacy console script 安装面
  - 顶层 `hmi-client` 对 `dxf_pipeline.cli.generate_preview` 的直接调用
- 删除 `dxf-pipeline/` 的确认条件：已满足

