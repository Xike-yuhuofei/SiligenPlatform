# Phase 15 A4: hardware smoke test entry gate

更新时间：`2026-03-25`

## Gate 结论

**结论：`Go with follow-ups`**

当前已满足首轮真实硬件 smoke 的进入门条件，之前阻断 `A4` 的 vendor 资产缺口已清零。  
仍未完成的事项不再是架构/构建阻塞，而是**必须在现场执行前完成的人工安全确认与证据采集动作**。这些 follow-ups 若任一失败，结论应立即回退为 `No-Go`。

本报告覆盖并替代本日较早版本中基于 `MultiCard.dll` / `MultiCard.lib` 缺失而形成的 `No-Go` 结论。

## 前置门槛复核

以下前置专项报告均已存在并可读取：

- `phase15-a3-runtime-execution-hardware-adapter-ownerization.md`
- `phase15-b2-runtime-execution-integration-regression-surface.md`
- `phase15-c2-bring-up-sop.md`
- `phase15-c3-observation-and-recording-scripts.md`

因此，`A4` gate review 的前置文档门槛已满足。

## Gate 评审结果

### 1. 硬件测试主链 owner 面

结果：**通过**

依据：

- `A3` 已完成 `HardwareTestAdapter`、`HardwareConnectionAdapter`、`MultiCardMotionAdapter`、`InterpolationAdapter`、`HomingPortAdapter`、`MotionRuntimeFacade`、`ValveAdapter`、`TriggerControllerAdapter` 等首轮硬件 smoke 主链资产的 canonical owner 化。
- 本次复核执行：
  - `rg -n "src/legacy|process-runtime-core/src" D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\include D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\src D:\Projects\SS-dispense-align\modules\runtime-execution\runtime\host`
  - 结果：无匹配

结论：

- 首轮硬件 smoke 经过的关键 public 面已退出 `src/legacy` / `process-runtime-core/src` 公开回退。

### 2. runtime host 非硬件 smoke 稳定性

结果：**通过**

依据：

- `A3` 已通过：
  - `cmake --build ... --target siligen_device_adapters siligen_runtime_host siligen_runtime_host_unit_tests`
  - `ctest --test-dir ... -C Debug -R "siligen_runtime_host_unit_tests" --output-on-failure`
- `B2` 已补齐正式 integration/regression surface。
- 本次复核执行：
  - `ctest --test-dir C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build -C Debug -N`
  - 可见测试项包含：
    - `siligen_runtime_host_unit_tests`
    - `runtime_execution_integration_host_bootstrap_smoke`
- 本次复核执行：
  - `python D:\Projects\SS-dispense-align\scripts\migration\validate_workspace_layout.py`
  - 结果：
    - `runtime_execution_public_header_failure_count=0`
    - `runtime_execution_bridge_shell_failure_count=0`
    - `runtime_execution_active_reference_failure_count=0`
    - `phase15_active_reference_failure_count=0`

结论：

- 非真实硬件 smoke 的 host/bootstrap/integration 入口当前稳定，不构成进入门阻塞。

### 3. bring-up SOP 完整性

结果：**通过**

依据：

- `C2` 已形成 `docs/runtime/hardware-bring-up-smoke-sop.md`。
- SOP 已覆盖：
  - 通电前环境检查
  - canonical config 与 network/interlock/homing 关键字段核对
  - MultiCard vendor 资产检查
  - `runtime-service` / `runtime-gateway` dry-run 检查
  - 急停、限位、回零前检查
  - 点动 / I/O / 触发 smoke 顺序
  - 停止条件
  - 回滚动作
  - 证据采集要求

结论：

- 现场执行流程定义完整，文档面不构成进入门阻塞。

### 4. 观察/记录脚本可用性

结果：**通过**

依据：

- `C3` 已提供：
  - `scripts/validation/run-hardware-smoke-observation.ps1`
  - `scripts/validation/register-hardware-smoke-observation.ps1`
  - `scripts/validation/README-hardware-smoke-observation.md`
- 本次直接执行默认入口：
  - `powershell -NoProfile -ExecutionPolicy Bypass -File D:\Projects\SS-dispense-align\scripts\validation\run-hardware-smoke-observation.ps1`
  - 结果：成功产出结构化报告：
    - `tests/reports/hardware-smoke-observation/20260325-185102/hardware-smoke-observation-summary.md`
    - `tests/reports/hardware-smoke-observation/20260325-185102/hardware-smoke-observation-summary.json`
  - 关键结论：
    - `observation_result = ready_for_gate_review`
    - `a4_signal = NeedsManualReview`
    - `dominant_gap = none`

结论：

- 观察脚本当前已可按默认入口执行，且自动化观察结果已明确支持进入 `A4` gate review。

### 5. canonical 配置与 vendor 资产路径

结果：**通过**

依据：

- canonical config 路径存在且字段齐备：
  - `config/machine/machine_config.ini`
- canonical vendor 目录存在：
  - `modules/runtime-execution/adapters/device/vendor/multicard`
- 本次复核目录内容包含：
  - `MultiCard.def`
  - `MultiCard.exp`
  - `MultiCard.dll`
  - `MultiCard.lib`
  - `README.md`
- 本次复核 strict dry-run 日志：
  - `tests/reports/hardware-smoke-observation/20260325-185102/logs/runtime-service-dryrun-strict.log`
  - `tests/reports/hardware-smoke-observation/20260325-185102/logs/runtime-gateway-dryrun-strict.log`
  - 两者当前都返回标准 `DRYRUN ... preflight_skipped=False` 成功信息

结论：

- 之前阻断 `A4` 的 vendor 资产路径问题已清零。
- canonical vendor 路径当前明确、可用，且与 SOP/脚本口径一致。

## 现场 follow-ups

以下项**不是进入门 blocker**，但必须在真实上板前由现场执行并形成证据：

### Follow-up 1

- 类型：**人工安全确认**
- 内容：确认急停按钮、`X7` 输入与 `emergency_stop_active_low=false` 的现场极性一致。
- 来源：`manual-emergency-stop`

### Follow-up 2

- 类型：**机械安全确认**
- 内容：确认 X/Y 轴当前位置在软限位内，工作区无干涉物。
- 来源：`manual-soft-limit-clearance`

### Follow-up 3

- 类型：**回零方向确认**
- 内容：确认 Axis1/Axis2 回零方向与 `search_distance` 不会直接撞向机械硬限位。
- 来源：`manual-homing-direction`

### Follow-up 4

- 类型：**I/O 映射确认**
- 内容：确认 `ValveSupply.do_bit_index=2` 与 `ValveDispenser.cmp_channel=1 / cmp_axis_mask=3` 对应现场接线。
- 来源：`manual-io-mapping`

### Follow-up 5

- 类型：**触发 smoke 条件确认**
- 内容：确认触发 smoke 在空跑、无产品、无供胶风险条件下执行。
- 来源：`manual-trigger-smoke`

### Follow-up 6

- 类型：**证据采集**
- 内容：在执行前创建现场 evidence 目录，并保留 SOP 与 observation 要求的日志、配置快照和现场照片/视频。

## 推荐执行顺序

1. 先运行 observation 脚本，确认本轮 `observation_result = ready_for_gate_review`。
2. 创建现场 evidence 目录，并记录 `machine_config` 快照与 vendor 资产快照。
3. 严格按 `docs/runtime/hardware-bring-up-smoke-sop.md` 完成急停、限位、回零方向、I/O 映射和触发条件的人工确认。
4. 启动 `runtime-service`。
5. 启动 `runtime-gateway`。
6. 如需要，启动 HMI 在线界面。
7. 按 SOP 顺序执行首轮 smoke：
   - 点动
   - I/O
   - 触发
8. 完成后归档证据；若 observation/intake 需要留痕，再执行 observation register。

## 建议命令

```powershell
Set-Location D:\Projects\SS-dispense-align

.\scripts\validation\run-hardware-smoke-observation.ps1 `
  -BuildConfig Debug `
  -VendorDir D:\Projects\SS-dispense-align\modules\runtime-execution\adapters\device\vendor\multicard

.\apps\runtime-service\run.ps1 `
  -BuildConfig Debug `
  -ConfigPath "config\machine\machine_config.ini" `
  -VendorDir "modules\runtime-execution\adapters\device\vendor\multicard"

.\apps\runtime-gateway\run.ps1 `
  -BuildConfig Debug `
  -ConfigPath "config\machine\machine_config.ini" `
  -VendorDir "modules\runtime-execution\adapters\device\vendor\multicard" -- --port 9527
```

如需归档 observation intake：

```powershell
Set-Location D:\Projects\SS-dispense-align

.\scripts\validation\register-hardware-smoke-observation.ps1 `
  -EvidenceDir tests\reports\hardware-smoke-observation\20260325-185102 `
  -MachineId <machine-id> `
  -Operator <operator>
```

## 证据收集要求

现场证据至少应保留：

- `tests/reports/hardware-smoke-observation/20260325-185102/hardware-smoke-observation-summary.md`
- `tests/reports/hardware-smoke-observation/20260325-185102/hardware-smoke-observation-summary.json`
- `tests/reports/hardware-smoke-observation/20260325-185102/logs/runtime-service-dryrun-strict.log`
- `tests/reports/hardware-smoke-observation/20260325-185102/logs/runtime-gateway-dryrun-strict.log`
- `tests/reports/hardware-smoke/<timestamp>/machine_config.snapshot.ini`
- `tests/reports/hardware-smoke/<timestamp>/vendor-assets.txt`
- `logs/control_runtime.log`
- `logs/tcp_server.log`
- `D:\Projects\SiligenSuite\logs\velocity_trace\`
- 现场照片/视频：急停状态、轴初始位置、I/O 指示灯、异常现象

## 证据索引

- A3 报告：
  - `specs/refactor/arch/NOISSUE-module-skeleton-alignment/phase15-a3-runtime-execution-hardware-adapter-ownerization.md`
- B2 报告：
  - `specs/refactor/arch/NOISSUE-module-skeleton-alignment/phase15-b2-runtime-execution-integration-regression-surface.md`
- C2 报告：
  - `specs/refactor/arch/NOISSUE-module-skeleton-alignment/phase15-c2-bring-up-sop.md`
- C3 报告：
  - `specs/refactor/arch/NOISSUE-module-skeleton-alignment/phase15-c3-observation-and-recording-scripts.md`
- bring-up SOP：
  - `docs/runtime/hardware-bring-up-smoke-sop.md`
- 最新 observation 证据：
  - `tests/reports/hardware-smoke-observation/20260325-185102/hardware-smoke-observation-summary.md`
  - `tests/reports/hardware-smoke-observation/20260325-185102/hardware-smoke-observation-summary.json`
  - `tests/reports/hardware-smoke-observation/20260325-185102/logs/runtime-service-dryrun-strict.log`
  - `tests/reports/hardware-smoke-observation/20260325-185102/logs/runtime-gateway-dryrun-strict.log`

## 最终判断

- `runtime-execution` 首轮硬件 smoke 主链 owner 化：**已达标**
- `runtime host` 非硬件 smoke 稳定性：**已达标**
- bring-up SOP：**已达标**
- 观察/记录脚本：**已达标**
- canonical config / vendor 资产：**已达标**
- 现场人工安全确认：**待执行**

因此，本轮 `A4` 的正式进入门结论为：

**`Go with follow-ups`**

只要现场按 SOP 完成上述人工确认并保留证据，即可进入 `hardware_smoke_test`。
