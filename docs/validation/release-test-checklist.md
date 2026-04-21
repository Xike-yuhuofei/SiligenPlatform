# 正式发版前测试执行清单

更新时间：`2026-04-02`

## 1. 目的

本文用于把正式发版前需要执行的测试和验证动作按阶段收敛成可勾选清单。

- 本文补充执行顺序，不降低 [docs/runtime/release-readiness-standard.md](/D:/Projects/SiligenSuite/docs/runtime/release-readiness-standard.md) 的任何硬门禁。
- 本文包含上机前模拟/弱依赖验证、仓内自动化门禁、仓外交付物观察、HIL/真机验证和最终放行判定。
- 只有当对应测试实际通过且证据已归档后，才能勾选复选框。

## 2. 执行信息

- 目标正式版本：`待填写`
- 目标 RC 版本：`待填写`
- 执行日期：`待填写`
- 执行人：`待填写`
- 机台/环境：`待填写`
- 统一证据根：`tests\reports\verify\release-validation-<yyyyMMdd-HHmmss>`

建议先在 PowerShell 会话中初始化：

```powershell
Set-Location D:\Projects\SiligenSuite

$Version = "0.1.0"
$RcVersion = "0.1.0-rc.1"
$CanonicalRecipeId = "recipe-7d1b00f4-6a99"
$CanonicalVersionId = "version-fea9ce29-f963"
$EvidenceRootRelative = Join-Path "tests\reports\verify" ("release-validation-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
$EvidenceRoot = Join-Path (Get-Location) $EvidenceRootRelative
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
```

## 3. 使用规则

1. 每个复选框只对应一个明确测试或验证动作。
2. 测试失败、`known_failure`、`skipped`、证据缺失时，不得勾选。
3. 如遇阻断，保持该项未勾选，并在文末“阻断记录”补充问题和证据路径。
4. 正式发布结论只在全部适用项完成后给出，不接受以部分通过替代最终放行。

## 4. Phase 1 上机前模拟与弱依赖验证

- [x] `P1-01` HMI 离线 UI smoke  
  命令：
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File .\apps\hmi-app\scripts\offline-smoke.ps1
  ```
  通过标准：脚本退出码为 `0`；GUI 壳层、offline 模式和禁用保护符合预期。

- [x] `P1-02` HMI mock 互锁单元回归  
  命令：
  ```powershell
  python .\apps\hmi-app\tests\unit\test_mock_server_interlocks.py
  ```
  通过标准：安全门打开时 `dxf.job.start` 被拒绝；急停时 `dxf.job.resume` 被拒绝；状态页能反映 `door/estop`。

- [x] `P1-03` 协议与契约兼容性测试  
  命令：
  ```powershell
  python .\tests\integration\protocol-compatibility\run_protocol_compatibility.py
  ```
  通过标准：`application-contracts`、`engineering-contracts`、`engineering-data`、`transport-gateway` 全部通过。

- [x] `P1-04` 工程数据回归  
  命令：
  ```powershell
  python .\tests\integration\scenarios\run_engineering_regression.py
  ```
  通过标准：`DXF -> .pb -> simulation-input.json -> preview` 全链路通过，输出与 canonical fixture 一致。

- [x] `P1-05` 仿真联动回归  
  命令：
  ```powershell
  python .\tests\e2e\simulated-line\run_simulated_line.py --report-dir (Join-Path $EvidenceRootRelative "phase1-simulated-line")
  ```
  通过标准：compatibility engine 与 scheme C minimal closed-loop 基线全部通过，重复执行结果保持确定性。  
  证据：`$EvidenceRoot\phase1-simulated-line\simulated-line-summary.json/.md`

- [x] `P1-06` 第一层 TCP 预条件矩阵  
  命令：
  ```powershell
  python .\tests\integration\scenarios\first-layer\run_tcp_precondition_matrix.py `
    --report-dir (Join-Path $EvidenceRootRelative "phase1-tcp-precondition") `
    --recipe-id $CanonicalRecipeId `
    --version-id $CanonicalVersionId
  ```
  通过标准：未回零不得 `home.go`，未加载 DXF 不得执行，缺少激活配方时必须按预期阻断。  
  说明：脚本当前会在报告目录下自动生成隔离空配方工作区并启动 gateway，排除真实工作区激活配方对 `S2` 的污染；本次证据中的 `gateway_cwd` 为 `phase1-tcp-precondition\isolated-empty-recipe-workspace`。  
  证据：`$EvidenceRoot\phase1-tcp-precondition\tcp-precondition-matrix.json/.md`

- [x] `P1-07` 第一层 TCP 急停链探针  
  命令：
  ```powershell
  python .\tests\integration\scenarios\first-layer\run_tcp_estop_chain.py --report-dir (Join-Path $EvidenceRootRelative "phase1-tcp-estop")
  ```
  通过标准：`connect -> estop -> status(estop 可见) -> estop.reset -> status(estop 清除) -> disconnect(disconnected=true)` 全链通过。  
  说明：脚本当前默认自动选择空闲端口并把 `--port` 显式传给 gateway，避免本机已有 `9527` listener 污染测试会话；本次证据在端口 `57415` 上通过。该项仅验证无机台第一层协议与状态收敛，不可替代 HIL/真机异常恢复放行。  
  证据：`$EvidenceRoot\phase1-tcp-estop\tcp-estop-chain.json/.md`

- [x] `P1-08` 性能与稳定性基线采集  
  命令：
  ```powershell
  python .\tests\performance\collect_baselines.py `
    --report-dir (Join-Path $EvidenceRootRelative "phase1-performance") `
    --recipe-id $CanonicalRecipeId `
    --version-id $CanonicalVersionId
  ```
  通过标准：基线采集完成，输出 `latest.json/.md`；无异常退化或明显抖动。  
  说明：`2026-03-25T14:06:58.767473+00:00` 已按当前脚本口径重采；DXF 预处理入口已对齐 `scripts\engineering-data\*.py`，`dxf.job.resume_without_pause` 记录为预期拒绝 `protocol.invalid_state`，不计入稳定性失败。  
  证据：`$EvidenceRoot\phase1-performance\latest.json/.md`

## 5. Phase 2 仓内自动化门禁

- [x] `P2-01` 根级构建门禁  
  命令：
  ```powershell
  .\build.ps1 -Profile CI -Suite all
  ```
  通过标准：退出码为 `0`，无构建失败。

- [x] `P2-02` 根级测试门禁  
  命令：
  ```powershell
  .\test.ps1 -Profile CI -Suite all -ReportDir (Join-Path $EvidenceRootRelative "phase2-workspace-validation") -FailOnKnownFailure
  ```
  通过标准：`failed=0` 且 `known_failure=0`。  
  证据：`$EvidenceRoot\phase2-workspace-validation\workspace-validation.json/.md`

- [x] `P2-03` 根级 CI 门禁  
  命令：
  ```powershell
  .\ci.ps1 -Suite all -ReportDir (Join-Path $EvidenceRootRelative "phase2-ci")
  ```
  通过标准：`legacy-exit`、`workspace-validation`、`dsp-e2e-spec-docset`、`local-validation-gate` 证据完整。  
  证据：`$EvidenceRoot\phase2-ci\`

- [x] `P2-04` 本地 validation gate  
  命令：
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1 -ReportRoot (Join-Path $EvidenceRootRelative "phase2-local-validation-gate")
  ```
  通过标准：所有 step 为 `passed`，必要工件完整落盘。  
  证据：`$EvidenceRoot\phase2-local-validation-gate\<timestamp>\local-validation-gate-summary.json/.md`

## 6. Phase 3 RC 候选版验证

当前阶段说明（`2026-03-26`）：

- 历史 `worktree triage` 材料仅用于解释曾经的收口过程，不再作为当前主线的有效入口证据。
- 当前仓库常态要求：`main...origin/main` 干净、无额外 worktree、无残留临时分支。
- 因此 `P3-01` 与 `P3-02` 必须基于当前 `$EvidenceRoot` 重新生成证据，不接受复用旧批次路径。
- 若当前主线再次出现脏工作树或临时 worktree，应先完成清理，再继续 RC 验证。

- [ ] `P3-01` 执行 RC release-check  
  命令：
  ```powershell
  .\release-check.ps1 -Version $RcVersion -ReportDir (Join-Path $EvidenceRootRelative "phase3-rc-release-check")
  ```
  通过标准：工作树干净；`build/test/release-check` 全部通过；dry-run 无 `BLOCKED`；`hil-case-matrix/case-matrix-summary.json/.md` 存在且通过；RC evidence 生成成功。
  证据：`$EvidenceRoot\phase3-rc-release-check\`
  默认已纳入 HIL case matrix；仅在临时隔离排障时显式使用：`-IncludeHilCaseMatrix:$false`

- [ ] `P3-02` 复核 RC 证据完整性  
  复核范围：
  ```text
  release-manifest.txt
  ci/
  legacy-exit/
  dryrun-hmi-app.txt
  dryrun-runtime-gateway.txt
  dryrun-planner-cli.txt
  dryrun-runtime-service.txt
  ```
  通过标准：版本、提交、报告路径、app dry-run 输出可追溯，无 `known_failure`、无 `BLOCKED`。

## 7. Phase 4 仓外交付物观察

- [ ] `P4-01` 登记现场脚本输入  
  命令：
  ```powershell
  .\scripts\validation\register-external-observation-intake.ps1 -Scope field-scripts -SourcePath <field-script-root> -ReportDir (Join-Path $EvidenceRootRelative "phase4-external-observation\intake")
  ```
  通过标准：已生成 intake `.json/.md`，现场脚本来源可追溯。

- [ ] `P4-02` 登记 release package 输入  
  命令：
  ```powershell
  .\scripts\validation\register-external-observation-intake.ps1 -Scope release-package -SourcePath <unpacked-release-root> -ReportDir (Join-Path $EvidenceRootRelative "phase4-external-observation\intake")
  ```
  通过标准：已生成 intake `.json/.md`，发布包根目录可扫描。

- [ ] `P4-03` 登记 rollback package 输入  
  命令：
  ```powershell
  .\scripts\validation\register-external-observation-intake.ps1 -Scope rollback-package -SourcePath <rollback-root> -ReportDir (Join-Path $EvidenceRootRelative "phase4-external-observation\intake")
  ```
  通过标准：已生成 intake `.json/.md`，回滚包来源可追溯。

- [ ] `P4-04` 执行仓外交付物观察  
  命令：
  ```powershell
  .\scripts\validation\run-external-migration-observation.ps1 -ReportRoot (Join-Path $EvidenceRootRelative "phase4-external-observation")
  ```
  通过标准：外部 launcher、现场脚本、release package、rollback package 结论均为 `Go`。  
  证据：`$EvidenceRoot\phase4-external-observation\observation\`

- [ ] `P4-05` 复核仓外交付物无 legacy 入口  
  复核范围：
  ```text
  不得出现 dxf_pipeline compat shell
  不得出现 legacy DXF CLI alias
  不得出现 config\machine_config.ini root alias
  ```
  通过标准：未发现 legacy 入口、compat shell 或错误配置根。

## 8. Phase 5 设备级、HIL 与现场验证

- [ ] `P5-01` 完成真实硬件 bring-up/smoke 前检查  
  参考文档：  
  [docs/runtime/hardware-bring-up-smoke-sop.md](/D:/Projects/SiligenSuite/docs/runtime/hardware-bring-up-smoke-sop.md)
  通过标准：硬停止条件全部排除，配置、vendor 资产、dry-run 和现场安全前提明确。

- [ ] `P5-02` 执行 hardware smoke  
  命令：
  ```powershell
  python .\tests\e2e\hardware-in-loop\run_hardware_smoke.py
  ```
  通过标准：最小启动闭环通过。  
  说明：该项只能证明最小启动闭环，不可单独作为正式发布放行依据。

- [ ] `P5-03` 执行 HIL 快速门禁（60s，唯一 HIL 门禁）
  命令：
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\e2e\hardware-in-loop\run_hil_controlled_test.ps1 `
    -Profile Local `
    -UseTimestampedReportDir `
    -HilDurationSeconds 60 `
    -HilPauseResumeCycles 3 `
    -PublishLatestOnPass:$false
  ```
 通过标准：`hil-controlled-gate-summary.json` 的 `overall_status=passed`；`offline-prereq/workspace-validation`、`hardware-smoke`、`hil-closed-loop` 全部通过；若保留默认 `hil-case-matrix`，则其 `overall_status=passed`；evidence bundle/manifest/index 兼容；若使用 override，必须显式记录 `-OperatorOverrideReason`。
  说明：脚本默认强制 attach 现有 gateway；不得通过 `-ReuseExistingGateway:$false` 另起第二条 gateway 轨道。
  若 `P5-03` 阻塞：保留 `hil-controlled-gate-summary.*` 与 `hil-controlled-release-summary.md` 作为 blocked evidence，记录阻断原因后停止；不得执行已禁用的 formal gate。
  默认已纳入多轮矩阵；仅在临时隔离排障时显式使用：`-IncludeHilCaseMatrix:$false`

- [ ] `P5-04` 已禁用：不再执行 HIL 正式门禁（1800s）
  说明：formal gate 已废止；`run_hil_controlled_test.ps1` 只允许 `HilDurationSeconds=60`，且不再允许 `PublishLatestOnPass=true` 或 `-Executor`。

- [ ] `P5-05` 完成真机多批次回归记录  
  参考模板：  
  [docs/validation/dxf-real-dispense-multi-cycle-template-v1.md](/D:/Projects/SiligenSuite/docs/validation/dxf-real-dispense-multi-cycle-template-v1.md)
  通过标准：`artifact / plan / job` 主链在真机上可重复执行，运行中和结束后的 IO/状态收敛一致。

- [ ] `P5-06` 完成异常恢复专项验证  
  覆盖范围：
  ```text
  报警
  急停
  互锁
  超时
  断网或连接退化
  暂停/恢复
  ```
  通过标准：每类异常至少有一条专项证据，且恢复路径明确；不得以“建议后补”代替通过结论。

- [ ] `P5-07` 完成现场长稳或多轮复测  
  参考文档：  
  [docs/runtime/long-run-stability.md](/D:/Projects/SiligenSuite/docs/runtime/long-run-stability.md)
  通过标准：跨时段或多轮验证无新增阻断，结果可复现。

- [ ] `P5-08` 完成部署与回滚受控演练  
  参考文档：  
  [docs/runtime/deployment.md](/D:/Projects/SiligenSuite/docs/runtime/deployment.md)
  [docs/runtime/rollback.md](/D:/Projects/SiligenSuite/docs/runtime/rollback.md)
  通过标准：安装、升级、回滚、配置恢复和产物恢复路径明确且可执行；回滚后验证通过。

## 9. Phase 6 正式版放行判定

- [ ] `P6-01` 重新执行最终本地 validation gate  
  命令：
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1 -ReportRoot (Join-Path $EvidenceRootRelative "phase6-local-validation-gate")
  ```
  通过标准：最终候选版本对应的根级门禁仍为 `passed`。

- [ ] `P6-02` 执行正式版 release-check  
  命令：
  ```powershell
  .\release-check.ps1 -Version $Version -IncludeHardwareSmoke -ReportDir (Join-Path $EvidenceRootRelative "phase6-formal-release-check")
  ```
  通过标准：正式版 release evidence 生成成功；不使用 `blocked` 豁免；性能基线、app dry-run、`hil-case-matrix/case-matrix-summary.json/.md` 全部满足要求。
  证据：`$EvidenceRoot\phase6-formal-release-check\`
  默认已纳入 HIL case matrix；仅在临时隔离排障时显式使用：`-IncludeHilCaseMatrix:$false`

- [ ] `P6-03` 按 `G1-G8` 完成最终放行复核  
  参考文档：  
  [docs/runtime/release-readiness-standard.md](/D:/Projects/SiligenSuite/docs/runtime/release-readiness-standard.md)
  通过标准：范围、可追溯、仓内验证、契约/e2e/性能、部署/回滚、仓外观察、设备/HIL/现场验证全部闭环。

- [ ] `P6-04` 形成最终 `Go / No-Go` 结论  
  记录项：
  ```text
  release_decision = Go | No-Go
  version = <x.y.z>
  evidence_root = <path>
  blocked_items = <none or item ids>
  approver = <name>
  date = <yyyy-mm-dd>
  ```
  通过标准：仅当全部适用测试已通过且无红线项时，才允许填写 `Go`。

## 10. 阻断记录

- `item_id`：`待填写`
  `问题`：`待填写；若阻断来自脏工作树、残留临时分支或额外 worktree，必须先完成 Git 收口再继续。`
  `证据`：`待填写；至少记录当前批次报告路径、git status --short --branch 和相关日志/报告。`

## 11. 关联文档

- [docs/runtime/release-process.md](/D:/Projects/SiligenSuite/docs/runtime/release-process.md)
- [docs/runtime/release-readiness-standard.md](/D:/Projects/SiligenSuite/docs/runtime/release-readiness-standard.md)
- [docs/runtime/field-acceptance.md](/D:/Projects/SiligenSuite/docs/runtime/field-acceptance.md)
- [docs/runtime/external-migration-observation.md](/D:/Projects/SiligenSuite/docs/runtime/external-migration-observation.md)
- [docs/runtime/hardware-bring-up-smoke-sop.md](/D:/Projects/SiligenSuite/docs/runtime/hardware-bring-up-smoke-sop.md)
- [docs/runtime/long-run-stability.md](/D:/Projects/SiligenSuite/docs/runtime/long-run-stability.md)
