# dxf-pipeline Strangler Progress

更新时间：`2026-03-18`

## 1. 当前结论

`dxf-pipeline` 已不再承载 `.pb` 导出、preview 生成、simulation input 导出的真实实现；这些真实职责现在以 `packages/engineering-data` 与 `packages/engineering-contracts` 为 canonical。

本轮已经完成的切换：

- `apps/hmi-app` 的 `DxfPipelinePreviewClient` 默认改为执行 `engineering_data.cli.generate_preview`
- HMI 打开 DXF 的默认候选目录不再优先指向 `dxf-pipeline/tests/fixtures/...`
- `packages/engineering-contracts/tests/test_engineering_contracts.py` 改为只验证 canonical proto、canonical fixture、canonical producer 与 consumer 兼容性
- `engineering-data` 安装入口同时暴露 canonical CLI 名称和 legacy CLI 名称，旧命令名不再必须依赖 `dxf-pipeline` 包安装
- `dxf-pipeline/.github`、`benchmarks`、`docs`、`examples`、`proto`、`scripts`、`tests`、`tmp` 已从工作区删除，只保留最小兼容壳

## 2. HMI 预览链路状态

### 2.1 是否仍依赖 `dxf_pipeline.cli.generate_preview`

否。默认链路已切走。

当前 HMI 预览实际调用：

- 模块：`engineering_data.cli.generate_preview`
- 调用方式：`python -m engineering_data.cli.generate_preview --json`
- 调用方：`apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/preview_client.py`

### 2.2 当前 fallback 层次

HMI 预览只在 canonical 路径内做定位，不再默认回退到 sibling `dxf-pipeline/`：

1. `SILIGEN_ENGINEERING_DATA_ROOT`
2. `SILIGEN_WORKSPACE_ROOT/packages/engineering-data`
3. 从当前源码位置向上探测工作区中的 `packages/engineering-data`
4. Python 解释器优先取 `SILIGEN_ENGINEERING_DATA_PYTHON`，否则取工作区 `.venv`，最后回退到当前 `sys.executable`

退出条件：

- 部署与运行手册全部改写为 `engineering-data` 命名
- 不再需要保留 `DxfPipelinePreviewClient` 这个 legacy integration 名称时，可再把 HMI 集成目录从 `integrations/dxf_pipeline` 重命名

## 3. Canonical API / CLI

### 3.1 `.pb` 导出

- Canonical API：`engineering_data.processing.dxf_to_pb.main(argv)`
- Canonical CLI：
  - `engineering-data-dxf-to-pb`
  - `python packages/engineering-data/scripts/dxf_to_pb.py`
- Legacy CLI alias：
  - `dxf-to-pb`

### 3.2 Preview 导出

- Canonical API：`engineering_data.preview.html_preview.generate_preview(PreviewRequest)`
- Canonical CLI：
  - `engineering-data-generate-preview`
  - `python packages/engineering-data/scripts/generate_preview.py`
  - 机器可消费输出：追加 `--json`
- Legacy CLI alias：
  - `generate-dxf-preview`

### 3.3 Simulation Input 导出

- Canonical API：
  - `engineering_data.contracts.simulation_input.load_path_bundle(path)`
  - `engineering_data.contracts.simulation_input.bundle_to_simulation_payload(bundle, ...)`
  - `engineering_data.contracts.simulation_input.write_simulation_payload(path, payload)`
- Canonical CLI：
  - `engineering-data-export-simulation-input`
  - `python packages/engineering-data/scripts/export_simulation_input.py`
- Legacy CLI alias：
  - `export-simulation-input`

## 4. 仍保留的 legacy CLI / import shim

以下兼容面仍保留，但都必须只做 canonical 转发，不能再承载真实实现：

- CLI alias：
  - `dxf-to-pb`
  - `path-to-trajectory`
  - `export-simulation-input`
  - `generate-dxf-preview`
- Import shim：
  - `dxf_pipeline.cli.*`
  - `dxf_pipeline.contracts.*`
  - `dxf_pipeline.preview.html_preview`
  - `dxf_pipeline.services.dxf_preprocessing`
  - `proto.dxf_primitives_pb2`（legacy 包路径下的转发层）

当前状态说明：

- 旧 CLI 名称已由 `packages/engineering-data/pyproject.toml` 直接提供 alias，可逐步脱离 `dxf-pipeline` 包安装
- 旧 Python import 路径仍只存在于 `dxf-pipeline/src`，这仍是删除整个 `dxf-pipeline/` 目录前需要处理的剩余依赖
- 顶层 proto、fixtures、样例、脚本和子项目文档已经删掉，不再构成当前工作区默认链路的一部分

## 5. 删除 `dxf-pipeline/` 的剩余阻塞

### 5.1 仍阻止整目录删除的依赖

1. `dxf_pipeline.*` import shim 仍位于 `dxf-pipeline/src`，外部历史调用如果还 import 这些路径，删除目录会直接中断。
2. `packages/engineering-data/docs/compatibility.md` 仍把 `dxf_pipeline.*` / `dxf-pipeline/src/proto/dxf_primitives_pb2.py` 记录为现存兼容面。
3. `dxf-pipeline/pyproject.toml` 仍声明 legacy console scripts；虽然 canonical 包已提供同名 alias，但若某些环境仍只安装 `dxf-pipeline`，删除前要先迁移安装说明。
4. 历史文档、runbook、排障说明里仍有 `dxf-pipeline` 文本残留，需要在交付文档层完成收口。

### 5.2 已切到 canonical 的链路

- HMI DXF preview 运行链
- `engineering-contracts` 的 proto / preview / simulation contract gate
- `.pb` / preview / simulation input 的 canonical producer 定义
- legacy CLI 命令名的实际实现归属

## 6. 建议删除时机

当前建议状态：`可进入删除准备，但暂不删剩余目录`

建议在以下条件同时满足后删除 `dxf-pipeline/`：

1. 把 `dxf_pipeline.*` import shim 迁到 `packages/engineering-data/src` 下的独立兼容包，或明确确认仓内外已无 import 用户。
2. 部署脚本、运行手册、排障文档全部改为 `engineering-data` 与 `engineering-contracts` 命名。
3. 对外发布一次包含 legacy CLI alias 的 `engineering-data` 安装链，并确认历史 CLI 用户不再要求安装 `dxf-pipeline`。
4. 负向搜索清零以下模式：`dxf-pipeline/src`、`dxf_pipeline.cli.generate_preview`。

在当前工作区里，外围目录已经删掉；等 import shim 迁移完成后，就可以把剩余 `dxf-pipeline/` 彻底移除。
