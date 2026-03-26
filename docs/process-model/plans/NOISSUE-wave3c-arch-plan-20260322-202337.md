# NOISSUE Wave 3C Architecture Plan - control-cli Workspace Path Tightening + Engineering-Data Bridge Normalization

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-202337`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave3c-scope-20260322-202337.md`

## 1. 目标结构

```text
apps/control-cli
  -> CommandLineParser default config = canonical machine config
  -> ResolveCliConfigPath(...)
       -> ResolveConfigFilePathWithBridge(...)
  -> dxf-preview
       -> ResolvePreviewScriptPath()
            -> tools/engineering-data-bridge/scripts/generate_preview.py
       -> ResolvePreviewOutputDir()
            -> integration/reports/dxf-preview

workspace callers
  -> tools/engineering-data-bridge/scripts/*
       -> packages/engineering-data/scripts/*
            -> engineering_data.cli / canonical API
```

## 2. 组件职责

### 2.1 `apps/control-cli`

1. 承接配置路径、DXF fallback 与 preview 默认入口的 workspace-rooted 解析。
2. 继续保留 `SILIGEN_DXF_PREVIEW_SCRIPT`、`SILIGEN_ENGINEERING_DATA_PYTHON`、`SILIGEN_DXF_PREVIEW_PYTHON` override；未设置时只走 bridge 默认入口。
3. CLI 帮助、README 与 cutover 文档必须与当前默认行为一致，不允许继续把 owner 脚本描述成 workspace 默认入口。

### 2.2 `tools/engineering-data-bridge`

1. 作为工作区稳定脚本锚点，承接 `dxf_to_pb.py`、`path_to_trajectory.py`、`export_simulation_input.py`、`generate_preview.py`。
2. 只负责稳定路径和转调，不承接 canonical API/CLI 的真实实现 owner。

### 2.3 `packages/engineering-data`

1. 继续拥有 `engineering_data.*` canonical API/CLI 与 `packages/engineering-data/scripts/*` owner 入口。
2. 兼容窗口继续保留 legacy import/CLI shim，但仅做 canonical 转发。

## 3. 失败模式与补救

1. 失败模式：`control-cli` 默认 preview 仍绑定 owner 脚本
   - 补救：把默认脚本常量切到 bridge，并用 `control-cli-dxf-preview` / `control-cli-dxf-preview-subdir` 回归验证。
2. 失败模式：文档仍把 owner 脚本表述为 workspace 默认入口
   - 补救：只更新 current-fact 文档，不回写历史计划证据。
3. 失败模式：兼容窗口被误收紧
   - 补救：保留 `test_dxf_pipeline_legacy_shims.py`，并在文档中显式注明本阶段不改仓外兼容策略。

## 4. 回滚边界

1. `control-cli` 入口收口回滚单元：
   - `apps/control-cli/CommandHandlersInternal.h`
   - `apps/control-cli/README.md`
   - `docs/architecture/control-cli-cutover.md`
2. workspace bridge 口径回滚单元：
   - `integration/scenarios/run_engineering_regression.py`
   - `integration/performance/collect_baselines.py`
   - `packages/engineering-data/README.md`
   - `packages/engineering-data/docs/compatibility.md`
