# Premerge Review Report

## 1. 评审上下文

1. 分支：`feat/dispense/NOISSUE-wave6a-foundation-ingest`
2. 评审日期：`2026-03-23`
3. 评审范围：Wave 6A workspace layout / provenance 基础设施改动

## 2. 变更面核对

1. CMake 路径合同收敛：
   - `CMakeLists.txt`
   - `tests/CMakeLists.txt`
   - `cmake/LoadWorkspaceLayout.cmake`
   - `cmake/workspace-layout.env`
2. 校验与报告链路：
   - `tools/scripts/get-workspace-layout.ps1`
   - `tools/migration/validate_workspace_layout.py`
   - `tools/build/build-validation.ps1`
   - `packages/test-kit/src/test_kit/workspace_layout.py`
   - `packages/test-kit/src/test_kit/workspace_validation.py`
   - `packages/test-kit/src/test_kit/runner.py`

## 3. 评审结论

1. 阻断性问题：无。
2. 关键设计判断：
   - 采用单一 `workspace-layout.env` 作为合同源，方向正确。
   - root/tests CMake 双侧都加入 canonical source root 约束，可防止非预期 source root 复用。
   - provenance 检查同时覆盖 cache 存在性与 `CMAKE_HOME_DIRECTORY` 一致性，契合迁移阶段风险。
3. 已修正项：
   - `build-validation.ps1` 在 build-root reset 后，末尾 provenance 输出改为从当前 cache 重新读取，避免打印旧值。

## 4. 剩余注意事项

1. `control-apps-build` 是共享 build root，必须继续串行构建，避免跨 worktree 互踩。
2. 本波仅处理基础设施，不替代后续外部输入采集与外部观察执行。

## 5. 裁决

1. `Wave 6A premerge readiness = Go`
