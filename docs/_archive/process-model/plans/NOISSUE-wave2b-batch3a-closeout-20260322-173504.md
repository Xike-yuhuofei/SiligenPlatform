# NOISSUE Wave 2B Batch 3A Closeout

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-173504`
- 对应计划：`docs/process-model/plans/NOISSUE-wave2b-batch3a-root-flip-plan-20260322-173019.md`

## 1. 总体裁决

1. `Wave 2B Batch 3A = Done`
2. `3A root source-root flip = Go`
3. `Batch 3B 进入裁决 = Go`
4. 本批次未修改 `build.ps1` / `test.ps1` 参数面、artifact 根、`integration/reports` 根，也未触碰 `3B` runtime 默认资产路径或 `3C` residual payoff。

## 2. 本批次完成内容

### 3A-1 root provenance gate 定稿

1. `tools/migration/validate_workspace_layout.py`
   - 在既有 selector/wiring 校验上新增 provenance 校验。
   - 若 `control-apps-build/CMakeCache.txt` 存在，则要求：
     - `CMAKE_HOME_DIRECTORY` 必须存在。
     - `CMAKE_HOME_DIRECTORY` 必须等于 selector 解析出的 workspace root。
   - 输出新增：
     - `control_apps_build_root`
     - `control_apps_cache_file`
     - `control_apps_cache_exists`
     - `control_apps_cmake_home_directory`
     - `failed_provenance_count`
2. 本批次未扩大写集到 `CMakeLists.txt`、`tests/CMakeLists.txt` 或 `tools/build/build-validation.ps1`。

### 3A-2 root source-root 证据闭环

1. `build.ps1 -Profile CI -Suite apps` 通过，公开 build 入口继续指向同一 workspace root。
2. `wave2b-batch3a` 全部根级 suite 通过：
   - `apps`
   - `packages`
   - `integration`
   - `protocol-compatibility`
   - `simulation`
3. 新报告目录已落盘到：
   - `integration/reports/verify/wave2b-batch3a/apps`
   - `integration/reports/verify/wave2b-batch3a/packages`
   - `integration/reports/verify/wave2b-batch3a/integration`
   - `integration/reports/verify/wave2b-batch3a/protocol`
   - `integration/reports/verify/wave2b-batch3a/simulation`

## 3. 实际执行命令

### 轻量校验

1. `python -m py_compile tools/migration/validate_workspace_layout.py`
2. `python tools/migration/validate_workspace_layout.py`

### 构建

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`

### 根级 suites

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2b-batch3a\apps -FailOnKnownFailure`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2b-batch3a\packages -FailOnKnownFailure`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite integration -ReportDir integration\reports\verify\wave2b-batch3a\integration -FailOnKnownFailure`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave2b-batch3a\protocol -FailOnKnownFailure`
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite simulation -ReportDir integration\reports\verify\wave2b-batch3a\simulation -FailOnKnownFailure`

## 4. 关键证据

1. `validate_workspace_layout.py` 输出：
   - `selector_present=true`
   - `selector_defaults_to_workspace_root=true`
   - `selector_resolves_to_workspace_root=true`
   - `control_apps_cache_exists=true`
   - `control_apps_cmake_home_directory=D:\Projects\SS-dispense-align`
   - `control_apps_cache_home_directory_present_when_cache_exists=true`
   - `control_apps_cache_home_directory_matches_workspace_root_when_cache_exists=true`
   - `failed_selector_count=0`
   - `failed_provenance_count=0`
   - `failed_wiring_count=0`
2. `workspace-validation.json` 元数据在 `apps/packages/integration/protocol/simulation` 五个 suite 中一致：
   - `source_root_selector_relative="."`
   - `workspace_source_root="D:\Projects\SS-dispense-align"`
   - `control_apps_cmake_home_directory="D:/Projects/SS-dispense-align"`
3. `build.ps1 -Profile CI -Suite apps` 输出：
   - `source-root selector: .`
   - `control-apps source root: D:\Projects\SS-dispense-align`
   - `control-apps cmake home directory: D:\Projects\SS-dispense-align`

## 5. 结果摘要

1. `apps` suite：`passed=18 failed=0 known_failure=0 skipped=0`
2. `packages` suite：`passed=8 failed=0 known_failure=0 skipped=0`
3. `integration` suite：`passed=1 failed=0 known_failure=0 skipped=0`
4. `protocol-compatibility` suite：`passed=1 failed=0 known_failure=0 skipped=0`
5. `simulation` suite：`passed=5 failed=0 known_failure=0 skipped=0`

## 6. 非阻断记录

1. `build.ps1` 期间仍有 protobuf/abseil 链接告警，但未造成构建失败，也不改变本批次 root provenance 结论。
2. 当前工作树仍然很脏；本批次未回退或覆盖任何既有改动，只在 `3A` 最小写集内推进。

## 7. 3B 裁决

1. `3A` 的退出条件已满足：
   - root selector 继续唯一且默认值未漂移。
   - root provenance 已从“报告可观察”收紧为“cache 失配即阻断”。
   - 根级 suites 全绿。
2. `3B = Go`。
3. 后续仍必须保持 `Batch 3` 严格串行：
   - 下一步只能进入 `3B` runtime 默认资产路径 flip。
   - 在 `3B` closeout 前，不得提前推进 `3C`。
