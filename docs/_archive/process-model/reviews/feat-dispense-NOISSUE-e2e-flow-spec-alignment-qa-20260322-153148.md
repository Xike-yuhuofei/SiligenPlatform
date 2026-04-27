# QA Report

## 1. 环境信息

1. 分支：`feat/dispense/NOISSUE-e2e-flow-spec-alignment`
2. 基线提交：`f6e5668d`
3. 执行时间：`2026-03-22`
4. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-153148`
5. 说明：本轮结论覆盖 `Wave 2B prep` 的 root layout 单一来源、runtime 资产路径汇聚与 residual gate 落地；未清理工作树中既有无关脏变更

## 2. 执行命令与退出码

1. `python tools/migration/validate_shared_freeze.py`
   - exit code：`0`
2. `python tools/migration/validate_workspace_layout.py`
   - exit code：`0`
3. `python tools/migration/validate_wave2b_residuals.py`
   - exit code：`0`
4. `.\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2b-prep\packages -FailOnKnownFailure`
   - exit code：`0`
5. `apps/control-runtime/run.ps1 -DryRun`
   - exit code：`0`
6. `apps/control-tcp-server/run.ps1 -DryRun`
   - exit code：`0`
7. `apps/control-cli/run.ps1 -DryRun`
   - exit code：`0`
8. `.\build.ps1 -Profile CI -Suite apps`
   - exit code：`0`
9. `.\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2b-prep\apps -FailOnKnownFailure`
   - exit code：`0`
10. `.\build.ps1 -Profile CI -Suite integration,protocol-compatibility,simulation`
    - exit code：`0`
11. `.\test.ps1 -Profile CI -Suite integration,protocol-compatibility,simulation -ReportDir integration\reports\verify\wave2b-prep\extended -FailOnKnownFailure`
    - exit code：`0`

## 3. 测试范围

1. `packages` 边界门禁：`shared-freeze`、`workspace-layout-boundary`、`wave2b-residual-ledger`
2. control apps canonical dry-run 与 `apps` 根级 suite
3. `integration`、`protocol-compatibility`、`simulation` 根级 suites
4. simulation suite 内的 scheme C ctest 子集、runtime-core contract tests 与 simulated-line 回归

## 4. 失败项与根因

1. 初次 `packages` 套件失败项：`shared-freeze-boundary`
2. 根因：新增 `packages/test-kit/src/test_kit/workspace_layout.py` 后，`tools/migration/shared_freeze_manifest.json` 与 `packages/test-kit/src/test_kit/workspace_validation.py` 内的 `TEST_KIT_CODE_MANIFEST` 未同步补录

## 5. 修复项与验证证据

1. 已补录 `src/test_kit/workspace_layout.py` 到 `tools/migration/shared_freeze_manifest.json`
2. 已补录 `src/test_kit/workspace_layout.py` 到 `packages/test-kit/src/test_kit/workspace_validation.py` 的 `TEST_KIT_CODE_MANIFEST`
3. 修复后 `validate_shared_freeze.py` 结果为 `illegal_expansion_count=0`
4. 修复后 `packages` suite 结果为 `passed=8 failed=0 known_failure=0 skipped=0`
5. `apps` suite 结果为 `passed=16 failed=0 known_failure=0 skipped=0`
6. `integration + protocol-compatibility + simulation` 结果为 `passed=7 failed=0 known_failure=0 skipped=0`
7. 证据根：
   - `integration/reports/verify/wave2b-prep/packages/`
   - `integration/reports/verify/wave2b-prep/apps/`
   - `integration/reports/verify/wave2b-prep/extended/`

## 6. 未修复项与风险接受结论

1. 当前并未启动 `Wave 2B` 的物理切换，根级 canonical source root 仍未翻转
2. runtime 默认资产路径也未切换到新目录，只是把默认解析收敛到单一入口
3. `Wave 2B` residual dependency 仍存在，只是增加了 ledger gate 防止 live 引用继续扩散
4. `.\build.ps1 -Profile CI -Suite apps` 输出中出现过一次 `abseil_dll.dll` 的 third-party 链接报错，但最终退出码为 `0`，且 control app 产物与后续 `apps` suite 均通过；当前记为非阻断观察项
5. 风险接受结论：接受 `Wave 2B prep` 收口完成；不接受据此直接开始 `Wave 2B` 物理切换

## 7. 发布建议

1. `Wave 2B prep`：`Go`
2. `Wave 2B` 物理切换：`No-Go`
