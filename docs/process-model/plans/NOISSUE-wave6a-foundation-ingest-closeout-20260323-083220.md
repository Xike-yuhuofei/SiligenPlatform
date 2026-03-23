# NOISSUE Wave 6A Foundation Ingest Closeout

- 状态：Done
- 日期：2026-03-23
- 分支：feat/dispense/NOISSUE-wave6a-foundation-ingest
- 工作区：`D:\Projects\SS-dispense-align-wave6a`
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260323-083220`

## 1. 总体裁决

1. `Wave 6A workspace layout foundation ingest = Go`
2. `Wave 6A build provenance gate wiring = Go`
3. `Wave 6A minimal validation (packages lane) = Go`
4. `external migration complete declaration = No-Go`（本波不覆盖外部输入补齐）

## 2. 本波目标与范围

1. 把 workspace 路径合同从散落硬编码收敛到单一清单：`cmake/workspace-layout.env`。
2. 在 root/tests CMake 与 Python/PowerShell 验证层统一消费同一 layout 数据。
3. 引入可执行的 provenance 校验，阻断 `control-apps-build` 复用到非当前 workspace 的风险。
4. 不重做 Wave 4B/4C/4D 已完成的 repo-side launcher 修正，不推进外部观察执行。

## 3. 关键落地项

1. 新增 workspace layout 基础设施：
   - `cmake/workspace-layout.env`
   - `cmake/LoadWorkspaceLayout.cmake`
   - `tools/scripts/get-workspace-layout.ps1`
   - `packages/test-kit/src/test_kit/workspace_layout.py`
2. 将 root/tests CMake 切换到 layout loader，并加入 canonical root 一致性约束：
   - `CMakeLists.txt`
   - `tests/CMakeLists.txt`
3. 强化验证链：
   - `tools/migration/validate_workspace_layout.py`（layout + wiring + provenance）
   - `packages/test-kit/src/test_kit/workspace_validation.py`（报告 metadata）
   - `packages/test-kit/src/test_kit/runner.py`（JSON/Markdown 输出 metadata）
   - `tools/build/build-validation.ps1`（layout 解析、provenance 输出、runtime-host unit artifact 检查）

## 4. 执行与验证摘要

1. provenance 问题修复路径：
   - 初次校验失败：`control_apps_cmake_home_directory` 指向旧 worktree `...wave5c-main`
   - 通过 `build.ps1 -Profile CI -Suite apps` 触发 build-root reset 并重配置到 `...wave6a`
2. 验证命令（均 exit code `0`）：
   - `python .\tools\migration\validate_workspace_layout.py`
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -FailOnKnownFailure`
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build\build-validation.ps1 -Profile Local -Suite packages`
   - `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build\build-validation.ps1 -Profile CI -Suite packages`
3. 关键证据：
   - `integration/reports/workspace-validation.md`
   - `integration/reports/workspace-validation.json`

## 5. 残余风险与下一步

1. 本波完成的是 repo 内基础设施和路径合同，不代表外部迁移完成。
2. 下一波应回到外部输入补齐与外部观察重开（field scripts / release package / rollback package）。
