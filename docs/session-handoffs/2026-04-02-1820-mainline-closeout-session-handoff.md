# Session Handoff

## 1. 任务目标

- 整理当前 `main` 上的在途改动。
- 将任务相关改动收敛到独立分支并形成可审查提交。
- 推送远端并创建指向 `main` 的 draft PR。

## 2. 任务范围

- 允许修改范围：
  - 当前工作区内已存在的任务相关代码与文档改动。
  - `docs/session-handoffs/` 下的本次交接文档。
- 禁止修改范围：
  - 不顺手修复验证脚本的历史路径问题。
  - 不提交本地临时备份文件 `AGENTS.md.bak`。
  - 不扩展为新的功能开发或跨模块重构。

## 3. 已完成项

- 从 `main` 切出分支 `refactor/runtime/NOISSUE-mainline-closeout`。
- 核对当前改动边界，确认任务相关文件包含：
  - `AGENTS.md`
  - `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.cpp`
  - `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.h`
  - `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpMotionFacade.h`
  - `apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`
  - `apps/runtime-service/container/ApplicationContainer.System.cpp`
- `apps/runtime-service/runtime/dispensing/WorkflowDispensingProcessPortAdapter.h`
- `apps/runtime-service/runtime/system/DispenserModelMachineExecutionStateBackend.h`
- `apps/runtime-service/run.ps1`
- `apps/planner-cli/run.ps1`
- `modules/runtime-execution/runtime/host/CMakeLists.txt`
- `scripts/validation/fast-forward-main.ps1`
- 本交接文档
- 确认 `AGENTS.md.bak` 为未纳入提交的本地备份文件。
- 完成最小构建验证，并补做基于实际 build root 的 dry-run 验证。

## 4. 未完成项

- 暂无实现类未完成项。
- 尚未执行提交、推送、创建 draft PR 之前，此文档需要随最终提交一并纳入版本库。

## 5. 修改文件列表

- `AGENTS.md`
- `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.cpp`
- `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.h`
- `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpMotionFacade.h`
- `apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`
- `apps/runtime-service/container/ApplicationContainer.System.cpp`
- `apps/runtime-service/runtime/dispensing/WorkflowDispensingProcessPortAdapter.h`
- `apps/runtime-service/runtime/system/DispenserModelMachineExecutionStateBackend.h`
- `apps/runtime-service/run.ps1`
- `apps/planner-cli/run.ps1`
- `modules/runtime-execution/runtime/host/CMakeLists.txt`
- `scripts/validation/fast-forward-main.ps1`
- `docs/session-handoffs/2026-04-02-1820-mainline-closeout-session-handoff.md`

## 6. 关键实现决策

- 分支命名采用 `refactor/runtime/NOISSUE-mainline-closeout`：
  - 仓库文档与脚本对 ticket 规则存在冲突，但仓库已有 `NOISSUE` 实践，且 `resolve-workflow-context.ps1` 明确兼容 `NOISSUE`。
- 不把 `AGENTS.md.bak` 纳入提交：
  - 该文件是本地备份，不属于可审查交付物。
- 不在本次收尾中修复测试脚本路径问题：
  - 初始 closeout 时确认了 `build.ps1` 的 canonical build root 为 `C:\Users\Xike\AppData\Local\SS\cab-528aede677f1`。
  - 后续已补充最小修复：统一 `apps/runtime-service/run.ps1` 与 `apps/planner-cli/run.ps1` 的默认搜索根，使其兼容 canonical build root 与旧路径。
  - `apps/planner-cli/run.ps1` 额外修正了工作区根解析，避免 hash 与搜索路径偏到 `apps/` 子目录。

## 7. 执行过的验证命令及结果

- `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Profile Local -Suite apps -SkipHeavyTargets`
  - 结果：通过。
- `powershell -NoProfile -ExecutionPolicy Bypass -File test.ps1 -Profile Local -Suite apps -ReportDir tests\reports\local-apps-closeout`
  - 结果：失败。
  - 失败原因：`runtime-service-dry-run` 和 `planner-cli-dry-run` 默认查找的 build root 与 `build.ps1` 实际输出目录不一致。
- `$env:SILIGEN_CONTROL_APPS_BUILD_ROOT='C:\Users\Xike\AppData\Local\SS\cab-528aede677f1'; powershell -NoProfile -ExecutionPolicy Bypass -File apps\runtime-service\run.ps1 -DryRun`
  - 结果：通过。
- `$env:SILIGEN_CONTROL_APPS_BUILD_ROOT='C:\Users\Xike\AppData\Local\SS\cab-528aede677f1'; powershell -NoProfile -ExecutionPolicy Bypass -File apps\planner-cli\run.ps1 -DryRun`
  - 结果：通过。
- `$env:SILIGEN_CONTROL_APPS_BUILD_ROOT='C:\Users\Xike\AppData\Local\SS\cab-528aede677f1'; powershell -NoProfile -ExecutionPolicy Bypass -File apps\runtime-gateway\run.ps1 -DryRun`
  - 结果：通过。
- `powershell -NoProfile -ExecutionPolicy Bypass -File apps\runtime-service\run.ps1 -DryRun`
  - 结果：通过。
- `powershell -NoProfile -ExecutionPolicy Bypass -File apps\planner-cli\run.ps1 -DryRun`
  - 结果：通过。
- `powershell -NoProfile -ExecutionPolicy Bypass -File test.ps1 -Profile Local -Suite apps -ReportDir tests\reports\local-apps-closeout-2`
  - 结果：通过。

## 8. 当前风险

- 当前补丁已让 `test.ps1 -Suite apps` 在默认路径假设下恢复全绿。
- 本次 PR 会带上 `AGENTS.md` 的整版重写，评审时需要单独关注仓库级规则替换是否符合团队预期。

## 9. 阻断点

- 无阻断当前提交、推送、创建 draft PR 的权限问题。
- 无阻断提交流程的非绿项；`apps` 聚合门禁已恢复通过。

## 10. 下一步最小动作

- 仅 stage 任务相关文件，排除 `AGENTS.md.bak`。
- 提交当前改动。
- 推送 `refactor/runtime/NOISSUE-mainline-closeout`。
- 更新现有 draft PR 的验证说明，标明 `apps` 聚合门禁已通过。
