# NOISSUE Wave 4A Scope - engineering-data External Compatibility Window Exit

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-204444`
- 上游收口：`docs/process-model/plans/NOISSUE-wave3c-closeout-20260322-202853.md`

## 1. 阶段目标

1. 退出 `engineering-data` 当前仍保留的外部 legacy CLI alias、`dxf_pipeline.*` import shim 与 `proto.dxf_primitives_pb2` 兼容层。
2. 把 `packages/process-runtime-core` 外部 DXF launcher 协议切到 canonical 子命令，避免兼容壳退出后打断 `SILIGEN_DXF_PROJECT_ROOT` 模式。
3. 把 `legacy-exit` 从 `dxf-pipeline` 冻结防回流升级为兼容壳归零门禁，并同步 current-fact 文档。

## 2. in-scope

1. `packages/process-runtime-core` 中 DXF `.pb` 预处理的外部 launcher 合同。
2. `packages/engineering-data` 的 `pyproject.toml`、`src/dxf_pipeline/**`、`src/proto/dxf_primitives_pb2.py`、兼容测试与 README/compatibility 文档。
3. `tools/scripts/legacy_exit_checks.py` 与 `docs/architecture/` 中描述 `dxf-pipeline` 兼容窗口的 current-fact 文档。
4. `docs/process-model/plans/`、`docs/process-model/reviews/` 中本阶段新增工件与证据。

## 3. out-of-scope

1. `tools/engineering-data-bridge/scripts/*` 与 `packages/engineering-data/scripts/*` 的 owner/bridge 分层重设计。
2. `control-core` 其余残余治理。
3. 仓外 sibling repo、现场脚本和已部署环境的真实迁移执行；本阶段只给出兼容窗口结束结论与人工确认要求。
4. 新的 DXF 预处理能力或 API 扩展。

## 4. 完成标准

1. `engineering-data` 只暴露 canonical `engineering-data-*` CLI 名称，不再安装 legacy alias。
2. `packages/engineering-data/src/dxf_pipeline/**` 与 `src/proto/dxf_primitives_pb2.py` 从仓内删除，legacy import/proto 必须失败。
3. `DxfPbPreparationService` 在 `SILIGEN_DXF_PROJECT_ROOT` 模式下向外部 launcher 传递 canonical 子命令 `engineering-data-dxf-to-pb`。
4. `legacy-exit` 报告对 `dxf-pipeline` 兼容壳归零通过，current-fact 文档统一表述为“兼容窗口已结束”。

## 5. 阶段退出门禁

1. `python packages/engineering-data/tests/test_engineering_data_compatibility.py`
2. `python packages/engineering-data/tests/test_dxf_pipeline_legacy_exit.py`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite packages`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave4a-closeout\packages -FailOnKnownFailure`
5. `python integration/scenarios/run_engineering_regression.py`
6. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave4a-closeout\protocol -FailOnKnownFailure`
7. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4a-closeout\legacy-exit`
