# NOISSUE Wave 3C Closeout

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-202853`
- 上游收口：`docs/process-model/plans/NOISSUE-wave3b-closeout-20260322-200303.md`
- 关联计划：
  - `docs/process-model/plans/NOISSUE-wave3c-scope-20260322-202337.md`
  - `docs/process-model/plans/NOISSUE-wave3c-arch-plan-20260322-202337.md`
  - `docs/process-model/plans/NOISSUE-wave3c-test-plan-20260322-202337.md`
- 关联验证目录：`integration/reports/verify/wave3c-closeout/`

## 1. 总体裁决

1. `Wave 3C = Done`
2. `Wave 3C closeout = Done`
3. `next-wave planning = Go`
4. `next-wave implementation = No-Go`

## 2. 本阶段实际完成内容

### 2.1 `control-cli` 工作区入口最终收口

1. `apps/control-cli/CommandHandlersInternal.h`
   - 默认 preview 脚本入口从 owner 脚本切到 `tools/engineering-data-bridge/scripts/generate_preview.py`
   - 默认配置路径继续统一到 `ResolveConfigFilePathWithBridge(...)`
   - 默认 DXF fixture 与 preview 输出目录继续走 workspace-rooted 解析
2. `apps/control-cli/README.md`
   - 明确 `dxf-preview` 默认脚本入口与脚本/解释器 override 优先级
3. `docs/architecture/control-cli-cutover.md`
   - 补录 `Wave 3C` 入口收口结论：默认 config 解析 hard-fail 策略不变，bridge 成为 preview 默认入口

### 2.2 工作区 `engineering-data` bridge 默认入口口径固化

1. `integration/scenarios/run_engineering_regression.py`
   - 默认调用 `tools/engineering-data-bridge/scripts/dxf_to_pb.py`
   - 默认调用 `tools/engineering-data-bridge/scripts/export_simulation_input.py`
   - 默认调用 `tools/engineering-data-bridge/scripts/generate_preview.py`
2. `integration/performance/collect_baselines.py`
   - `.pb` / simulation input 默认脚本入口改到 bridge
3. `packages/engineering-data/README.md`
4. `packages/engineering-data/docs/compatibility.md`
5. `docs/architecture/dxf-pipeline-strangler-progress.md`
   - 统一 current-fact：bridge 是 workspace 稳定入口，`packages/engineering-data/scripts/*` 继续保留为 owner/维护入口

### 2.3 规划与证据工件补齐

1. 新增 `Wave 3C` 的 scope / arch / test plan 工件
2. 新增 `integration/reports/verify/wave3c-closeout/apps/`
3. 新增 `integration/reports/verify/wave3c-closeout/protocol/`
4. 工程回归 `integration/scenarios/run_engineering_regression.py` 已单独通过

## 3. 实际执行命令

### 3.1 静态与兼容验证

1. `python packages/engineering-data/tests/test_engineering_data_compatibility.py`
2. `python packages/engineering-data/tests/test_dxf_pipeline_legacy_shims.py`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File apps/control-cli/run.ps1 -DryRun`

### 3.2 构建与回归

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave3c-closeout\apps -FailOnKnownFailure`
3. `python integration/scenarios/run_engineering_regression.py`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave3c-closeout\protocol -FailOnKnownFailure`

## 4. 关键证据

1. `integration/reports/verify/wave3c-closeout/apps/workspace-validation.md`
   - `passed=23 failed=0 known_failure=0 skipped=0`
   - `control-cli-bootstrap-check-subdir` 继续通过
   - `control-cli-bootstrap-check-compat-alias-rejected` 继续通过
   - `control-cli-dxf-preview` 与 `control-cli-dxf-preview-subdir` 继续通过
2. `integration/reports/verify/wave3c-closeout/protocol/workspace-validation.md`
   - `passed=1 failed=0 known_failure=0 skipped=0`
   - `protocol compatibility suite passed`
3. `python integration/scenarios/run_engineering_regression.py`
   - 输出 `engineering regression passed`
4. scoped 搜索：
   - `rg -n "packages/engineering-data/scripts/generate_preview\.py" apps integration packages tools --glob "!docs/process-model/**"`
   - live code 仅剩 `packages/engineering-data/README.md` 中的 owner 入口说明
5. scoped 搜索：
   - `rg -n "current_path\(" apps/control-cli`
   - 结果：`0` 命中

## 5. 阶段结论

1. `Wave 3C` 已完成 `control-cli` 工作区内默认路径与 preview 默认脚本入口的最终收口。
2. 工作区内部 `engineering-data` 脚本默认入口已统一到 bridge，owner 脚本继续保留但不再作为 workspace 默认入口描述。
3. 仓外兼容窗口未被误收紧；`test_dxf_pipeline_legacy_shims.py` 继续通过，说明本阶段没有破坏外部兼容策略。

## 6. 非阻断记录

1. `build.ps1 -Profile CI -Suite apps` 期间 protobuf/abseil 的既有链接 warning 仍存在；本批次未改变其结论。
2. 当前工作树仍明显不干净，但 `Wave 3C` 的实现、验证与 closeout 证据已闭环。
3. 下一阶段若继续推进，应单独锁定“仓外兼容窗口退出/legacy shim 生命周期”范围，不能直接把本次 closeout 当作兼容窗口结束证据。
