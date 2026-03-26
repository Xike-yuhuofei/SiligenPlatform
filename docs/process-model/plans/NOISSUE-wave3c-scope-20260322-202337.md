# NOISSUE Wave 3C Scope - Workspace Entry Tightening for control-cli + Engineering-Data Bridge

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-202337`
- 上游收口：`docs/process-model/plans/NOISSUE-wave3b-closeout-20260322-200303.md`

## 1. 阶段目标

1. 收口 `apps/control-cli` 剩余的 workspace path/resolver 旁路，使默认配置、默认 DXF fixture、默认 preview 脚本与默认输出目录全部以 workspace-rooted canonical 解析执行。
2. 把工作区内部 `engineering-data` 默认脚本入口统一到 `tools/engineering-data-bridge/scripts/*`，使 bridge 成为稳定 workspace 入口，owner 脚本只保留为包内维护入口。
3. 同步 current-fact 文档、规划工件与验证证据，使 `Wave 3C` 成为 `Wave 3` 内部入口规范化的收口批次。

## 2. in-scope

1. `apps/control-cli` 中与配置路径、preview 默认脚本、cwd 无关化相关的实现与文档。
2. `integration/scenarios`、`integration/performance`、`packages/engineering-data` 中已经切向 bridge 的工作区默认脚本入口与配套说明。
3. `docs/architecture/`、`docs/process-model/plans/`、`docs/process-model/reviews/` 中与以上边界直接相关的 current-fact 文档与证据。

## 3. out-of-scope

1. 删除或收紧 `packages/engineering-data/src/dxf_pipeline/**` 的仓外兼容窗口。
2. 修改 `packages/engineering-data/tests/test_dxf_pipeline_legacy_shims.py` 所覆盖的 legacy import/CLI shim 策略。
3. 扩展到 `control-cli` 之外的其他 app/path 治理。
4. 新增功能、重构 preview 算法实现 owner，或改变 `engineering-data` 的 canonical API/CLI 归属。

## 4. 完成标准

1. `apps/control-cli` 默认 preview 脚本入口改为 `tools/engineering-data-bridge/scripts/generate_preview.py`，且 `SILIGEN_DXF_PREVIEW_SCRIPT` override 语义保持不变。
2. `apps/control-cli` 默认配置路径继续使用 `config/machine/machine_config.ini`，显式旧 alias 保持 hard-fail，不引入 compat warning。
3. 工作区内默认脚本入口统一以 bridge 为准：`dxf_to_pb.py`、`path_to_trajectory.py`、`export_simulation_input.py`、`generate_preview.py`。
4. current-fact 文档明确：bridge 是 workspace 稳定入口，`packages/engineering-data/scripts/*` 是 owner/维护入口，仓外兼容窗口本阶段不变。

## 5. 阶段退出门禁

1. `python packages/engineering-data/tests/test_engineering_data_compatibility.py`
2. `python packages/engineering-data/tests/test_dxf_pipeline_legacy_shims.py`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File apps/control-cli/run.ps1 -DryRun`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave3c-closeout\apps -FailOnKnownFailure`
6. `python integration/scenarios/run_engineering_regression.py`
7. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave3c-closeout\protocol -FailOnKnownFailure`
