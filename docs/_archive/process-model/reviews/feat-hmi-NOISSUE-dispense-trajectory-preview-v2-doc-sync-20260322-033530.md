# Doc Sync Report

- branch: `feat/hmi/NOISSUE-dispense-trajectory-preview-v2`
- generated_at: `2026-03-22 03:35:30 +08:00`
- scope: 文档与本次代码改动对齐（engineering-contracts 测试口径、CI apps 构建参数、run.ps1 跨代码页约束）

## 变更文件

1. `packages/engineering-contracts/README.md`
2. `docs/architecture/build-and-test.md`
3. `docs/troubleshooting/canonical-runbook.md`

## 关键差异摘要

1. `engineering-contracts` 验证口径由“固定比对 Backend_CPP”更新为：
   - 必须比对工作区公开 canonical：`data/schemas/engineering/dxf/v1/dxf_primitives.proto`
   - 仅在存在外部 `Backend_CPP/proto/dxf_primitives.proto` 时做附加一致性校验
2. `build-and-test` 新增 CI 构建事实：
   - `tools/build/build-validation.ps1` 在 control-apps CI 构建中启用 `SILIGEN_USE_PCH=ON`、`SILIGEN_PARALLEL_COMPILE=ON`
   - `cmake --build` 显式使用 `--parallel`
3. `post-refactor-runbook` 增加故障条目：
   - `run.ps1` wrapper 文本若包含非 ASCII，在非 UTF-8 代码页 PowerShell 可能触发解析失败

## 待确认项

1. 无阻断待确认项。
