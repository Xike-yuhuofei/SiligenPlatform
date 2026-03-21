# QA Regression Report

## 1. 环境信息（分支、commit、执行时间）
- 分支：$(@{Branch=feat/hmi/NOISSUE-dispense-trajectory-preview-v2; BranchSafe=feat-hmi-NOISSUE-dispense-trajectory-preview-v2; Ticket=NOISSUE; BranchCompliant=True; Type=feat; Scope=hmi; ShortDesc=dispense-trajectory-preview-v2; Date=2026-03-22; Timestamp=20260322-014949}.Branch)
- Commit：$commit
- 执行时间：$now
- 测试计划：docs/process-model/plans/NOISSUE-test-plan-20260321-221824.md

## 2. 执行命令与退出码
1. ./test.ps1 -Profile CI -Suite apps -FailOnKnownFailure -> xit 0（首次，报告显示 passed=13 failed=3）
2. powershell -NoProfile -ExecutionPolicy Bypass -File apps/hmi-app/scripts/online-smoke.ps1 -> xit 0
3. powershell -NoProfile -ExecutionPolicy Bypass -File apps/hmi-app/scripts/verify-online-ready-timeout.ps1 -> xit 0
4. powershell -NoProfile -ExecutionPolicy Bypass -File apps/hmi-app/scripts/verify-online-recovery-loop.ps1 -> xit 0
5. ./test.ps1 -Profile CI -Suite apps -FailOnKnownFailure -> xit 0（复测，passed=16 failed=0）

## 3. 测试范围
- 根级 apps 套件门禁（control-runtime / control-tcp-server / control-cli / hmi-app dryrun+unit+smoke）
- HMI 在线 smoke 失败链路专项复测：
  - online-smoke
  - verify-online-ready-timeout
  - verify-online-recovery-loop

## 4. 失败项与根因
- 失败项（首次门禁）
  1. hmi-app-online-smoke
  2. hmi-app-online-ready-timeout
  3. hmi-app-online-recovery-loop
- 统一根因
  - MainWindow.__init__ 在属性未初始化时调用 _update_preview_refresh_button_state()，读取 self._connected 触发 AttributeError。
  - 复现栈：pps/hmi-app/src/hmi_client/ui/main_window.py（初始化流程 -> _update_preview_refresh_button_state）

## 5. 修复项与验证证据（包含复测命令）
- 修复文件：pps/hmi-app/src/hmi_client/ui/main_window.py
- 修复内容：
  1. 在 __init__ 增加 _connected/_hw_connected 默认初始化。
  2. 在 _refresh_launch_status_ui 按会话快照同步 _connected/_hw_connected，覆盖 offline/snapshot-none 分支。
  3. 在 _update_preview_refresh_button_state 使用 getattr(self, "_connected", False) 防御初始化顺序问题。
- 复测证据：
  - 三个在线 smoke 命令均 xit 0。
  - 根级门禁二次执行 passed=16 failed=0。

## 6. 未修复项与风险接受结论
- 未修复项：无（本轮阻断缺陷已闭环）。
- 风险接受结论：
  - 可接受。当前风险主要为后续改动再次破坏初始化顺序；建议保留在线 smoke 作为 PR 必跑门禁。

## 7. 发布建议（Go / No-Go）
- 结论：Go
- 前提：合并前继续保持 pps 套件门禁全绿。
