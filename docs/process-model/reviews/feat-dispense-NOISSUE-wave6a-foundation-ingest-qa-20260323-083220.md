# QA Report

## 1. 环境信息

1. 分支：`feat/dispense/NOISSUE-wave6a-foundation-ingest`
2. 工作区：`D:\Projects\SS-dispense-align-wave6a`
3. 执行日期：`2026-03-23`
4. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260323-083220`

## 2. 执行命令与退出码

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite packages`
   - exit code：`0`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`
   - exit code：`0`
3. `python .\tools\migration\validate_workspace_layout.py`
   - exit code：`0`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -FailOnKnownFailure`
   - exit code：`0`
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build\build-validation.ps1 -Profile Local -Suite packages`
   - exit code：`0`
6. `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build\build-validation.ps1 -Profile CI -Suite packages`
   - exit code：`0`

## 3. 结果摘要

1. workspace layout/provenance 校验：
   - `missing_key_count=0`
   - `missing_path_count=0`
   - `failed_provenance_count=0`
   - `failed_wiring_count=0`
2. packages 最小验证：
   - `passed=5 failed=0 known_failure=0 skipped=0`
   - 报告：`integration/reports/workspace-validation.md`
3. metadata 证据：
   - `workspace_root=D:\Projects\SS-dispense-align-wave6a`
   - `control_apps_cmake_home_directory=D:/Projects/SS-dispense-align-wave6a`
4. `build-validation` 本波新增产物检查通过：
   - `siligen_runtime_host_unit_tests.exe` 可解析并命中 canonical 输出目录。

## 4. 非阻断项

1. 构建日志存在既有第三方/工具链 warning（gtest/protobuf/abseil），本波未引入新增失败。

## 5. 结论

1. `Wave 6A QA verdict = Go`
