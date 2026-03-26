# dxf-pipeline Strangler Progress

更新时间：`2026-03-22`

## 1. 当前结论

`dxf-pipeline` 已不再承载 `.pb` 导出、preview 生成、simulation input 导出的真实实现；这些真实职责现在以 `packages/engineering-data` 与 `packages/engineering-contracts` 为 canonical。
当前工作区的稳定脚本入口已额外锚定到 `tools/engineering-data-bridge/scripts/*`，用于隔离后续物理迁移对默认路径的影响。

本轮已经完成的切换：

- HMI 运行时预览主链路已切到 gateway `dxf.preview.snapshot` / `dxf.preview.confirm`
- HMI 打开 DXF 的默认候选目录不再优先指向 `dxf-pipeline/tests/fixtures/...`
- `packages/engineering-contracts/tests/test_engineering_contracts.py` 改为只验证 canonical proto、canonical fixture、canonical producer 与 consumer 兼容性
- `engineering-data` 安装入口只保留 canonical CLI 名称，legacy 命令名已退出
- `packages/engineering-data/src/dxf_pipeline/**` 与 `packages/engineering-data/src/proto/dxf_primitives_pb2.py` 已删除
- `dxf-pipeline/.github`、`benchmarks`、`docs`、`examples`、`proto`、`scripts`、`tests`、`tmp` 已从工作区删除，兼容壳也已清零

## 2. HMI 预览链路状态

### 2.1 是否仍依赖 `dxf_pipeline.cli.generate_preview`

否。默认链路已切走。

当前 HMI 生产页预览实际调用：

- 协议：`dxf.preview.snapshot`
- 确认：`dxf.preview.confirm`
- 调用方：`apps/hmi-app/src/hmi_client/ui/main_window.py`

### 2.2 当前 owner 划分

- 运行时 preview gate owner：`packages/transport-gateway`
- preview artifact producer owner：`packages/engineering-data`
- preview schema / fixture owner：`packages/engineering-contracts`

## 3. Canonical API / CLI

### 3.1 `.pb` 导出

- Canonical API：`engineering_data.processing.dxf_to_pb.main(argv)`
- Canonical CLI：
  - `engineering-data-dxf-to-pb`
  - `python tools/engineering-data-bridge/scripts/dxf_to_pb.py`
  - `python packages/engineering-data/scripts/dxf_to_pb.py`

### 3.2 Preview 导出

- Canonical API：`engineering_data.preview.html_preview.generate_preview(PreviewRequest)`
- Canonical CLI：
  - `engineering-data-generate-preview`
  - `python tools/engineering-data-bridge/scripts/generate_preview.py`
  - `python packages/engineering-data/scripts/generate_preview.py`（owner/维护入口，非 workspace 默认入口）
  - 机器可消费输出：追加 `--json`

### 3.3 Simulation Input 导出

- Canonical API：
  - `engineering_data.contracts.simulation_input.load_path_bundle(path)`
  - `engineering_data.contracts.simulation_input.bundle_to_simulation_payload(bundle, ...)`
  - `engineering_data.contracts.simulation_input.write_simulation_payload(path, payload)`
- Canonical CLI：
  - `engineering-data-export-simulation-input`
  - `python tools/engineering-data-bridge/scripts/export_simulation_input.py`
  - `python packages/engineering-data/scripts/export_simulation_input.py`

## 4. legacy 兼容壳退出结果

以下兼容面已退出：

- legacy CLI alias：
  - `dxf-to-pb`
  - `path-to-trajectory`
  - `export-simulation-input`
  - `generate-dxf-preview`
- import / proto shim：
  - `dxf_pipeline.cli.*`
  - `dxf_pipeline.contracts.*`
  - `dxf_pipeline.preview.html_preview`
  - `dxf_pipeline.services.*`
  - `proto.dxf_primitives_pb2`

当前状态说明：

- 工作区与 canonical 包都不再暴露这些 legacy 入口
- 仓内验证已改为负向断言：legacy import / proto / CLI alias 必须不存在
- 顶层目录和兼容壳都已从默认执行链路清零
## 5. 当前结论

1. `dxf-pipeline` 相关外部兼容窗口已结束，仓内只保留 canonical `engineering-data` / `engineering-contracts` / bridge 入口。
2. 下一阶段不再是“继续 strangler”，而是观察仓外环境是否仍缓存旧入口，并按发布/现场链路做人工确认。
3. `legacy-exit` 门禁已升级为兼容壳归零，不允许这些 legacy 名称重新回流。
