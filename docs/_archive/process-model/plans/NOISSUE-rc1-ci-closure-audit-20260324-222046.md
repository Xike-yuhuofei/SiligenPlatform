# RC1 CI Closure Audit

## 1. 目标

冻结 `0.1.0-rc.1` 后续“第二提交”的真实依赖闭包，明确为什么本阶段不执行第二个实际提交。

## 2. 结论

1. 第二提交不能按“仅提交 `ci.ps1` 修复”规划。
2. 当前 `ci.ps1` 的 staged 版本已经依赖 `HEAD` 中不存在的 canonical runner 和 validator 资产。
3. 若继续在本阶段执行第二个实际提交，边界会扩张为 `workspace-template-refactor` 的大批次收口，超出本阶段目标。
4. 因此本阶段只做 RC 文档基线提交；第二提交降级为闭包审计和范围冻结。

## 3. 直接依赖闭包

### 3.1 根级入口

1. `build.ps1`
2. `test.ps1`
3. `ci.ps1`

### 3.2 canonical runners / wrappers

1. `scripts/build/build-validation.ps1`
2. `scripts/validation/invoke-workspace-tests.ps1`
3. `scripts/migration/legacy-exit-checks.py`
4. `tools/build/build-validation.ps1`
5. `tools/scripts/invoke-workspace-tests.ps1`
6. `tools/scripts/legacy_exit_checks.py`
7. `tools/scripts/run-local-validation-gate.ps1`

### 3.3 validators

1. `tools/migration/validate_workspace_layout.py`
2. `scripts/migration/validate_dsp_e2e_spec_docset.py`

### 3.4 layout / freeze wiring

1. `cmake/LoadWorkspaceLayout.cmake`
2. `cmake/workspace-layout.env`
3. `docs/architecture/canonical-paths.md`
4. `docs/architecture/directory-responsibilities.md`

## 4. HEAD 缺失的关键项

以下路径在当前 `HEAD` 中不存在，但已被 staged 的根级入口或 validator 直接依赖：

1. `scripts/build/build-validation.ps1`
2. `scripts/validation/invoke-workspace-tests.ps1`
3. `scripts/migration/legacy-exit-checks.py`
4. `tools/migration/validate_workspace_layout.py`
5. `scripts/migration/validate_dsp_e2e_spec_docset.py`
6. `cmake/LoadWorkspaceLayout.cmake`
7. `cmake/workspace-layout.env`
8. `shared/CMakeLists.txt`
9. `shared/contracts/CMakeLists.txt`
10. `shared/kernel/CMakeLists.txt`
11. `shared/testing/CMakeLists.txt`
12. `shared/README.md`
13. `shared/contracts/README.md`
14. `shared/kernel/README.md`
15. `shared/testing/README.md`
16. `modules/CMakeLists.txt`
17. `modules/workflow/README.md`
18. `apps/runtime-service/README.md`
19. `apps/runtime-gateway/README.md`

## 5. 为什么不能做第二个实际提交

1. `build.ps1` staged 版本已改为优先转发 `scripts/build/build-validation.ps1`；该文件在 `HEAD` 中不存在。
2. `test.ps1` staged 版本已改为优先转发 `scripts/validation/invoke-workspace-tests.ps1`；该文件在 `HEAD` 中不存在。
3. `ci.ps1` staged 版本已改为优先调用 `scripts/migration/legacy-exit-checks.py`，并串接 `tools/scripts/run-local-validation-gate.ps1`。
4. `tools/scripts/run-local-validation-gate.ps1` 的当前工作树版本又要求 `validate_workspace_layout.py` 与 `validate_dsp_e2e_spec_docset.py` 同时存在并产出固定报告。
5. `validate_workspace_layout.py` 会校验 `cmake/`、`shared/`、`modules/`、`apps/runtime-service/`、`apps/runtime-gateway/` 等结构；这些结构在当前 `HEAD` 中仍大量缺失。
6. 因此第二提交已经不是“CI 入口修复”，而是“CI 入口 + canonical runner + layout/freeze validator + root skeleton”闭包。

## 6. 下一阶段建议边界

1. 下一阶段如果继续处理第二提交，名称应定义为“CI 依赖闭包收口”或“workspace-template-refactor staged batch 收口”，不能继续沿用“`ci.ps1` 单文件修复”口径。
2. 下一阶段实施前，应先对以下集合做一次提交级白名单冻结：
   - 根级入口：`build.ps1`、`test.ps1`、`ci.ps1`
   - canonical runner：`scripts/build/`、`scripts/validation/`、`scripts/migration/`
   - wrapper：`tools/build/`、`tools/scripts/`
   - validator：`tools/migration/validate_workspace_layout.py`、`scripts/migration/validate_dsp_e2e_spec_docset.py`
   - layout/root skeleton：`cmake/`、`shared/`、必要的 `modules/` / `apps/runtime-service/` / `apps/runtime-gateway/`

## 7. 本阶段执行口径

1. 本阶段不提交第二个实际提交。
2. 本阶段只允许完成 RC 文档基线提交。
3. 第二提交的唯一正式输入，以本审计文档和 `NOISSUE-rc1-whitelist-prep-20260324-213217.md` 为准。
