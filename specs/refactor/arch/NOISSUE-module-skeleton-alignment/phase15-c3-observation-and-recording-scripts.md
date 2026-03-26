# Phase 15 C3 Observation And Recording Scripts

## 任务结论

- 已新增 `scripts/validation/run-hardware-smoke-observation.ps1`，用于输出硬件 smoke 观察报告、dry-run 日志和结构化摘要。
- 已新增 `scripts/validation/register-hardware-smoke-observation.ps1`，用于把一次观察结果登记为 intake 记录，便于 `A4` 汇总。
- 已新增 `scripts/validation/README-hardware-smoke-observation.md`，说明脚本入口、参数、默认输出目录和与 SOP 的配套方式。

## 脚本入口

- `scripts/validation/run-hardware-smoke-observation.ps1`
- `scripts/validation/register-hardware-smoke-observation.ps1`
- `scripts/validation/README-hardware-smoke-observation.md`

## 默认输出位置

- 观察报告根：`tests/reports/hardware-smoke-observation/<timestamp>/`
- 登记 intake 根：`tests/reports/hardware-smoke-observation/intake/`

## 证据格式

观察脚本会输出：

- `hardware-smoke-observation-summary.md`
- `hardware-smoke-observation-summary.json`
- `logs/runtime-service-dryrun-strict.log`
- `logs/runtime-service-dryrun-diagnostic.log`
- `logs/runtime-gateway-dryrun-strict.log`
- `logs/runtime-gateway-dryrun-diagnostic.log`

登记脚本会输出：

- `<timestamp>-<machine-id>.md`
- `<timestamp>-<machine-id>.json`

## 如何与 SOP 配套使用

- `C1` 提供 canonical config、vendor 目录与运行脚本 preflight 事实。
- `C2` 定义人工安全检查、停止条件、回滚和日志收集。
- `C3` 在此基础上补齐自动化观察层，用严格 dry-run 与诊断 dry-run 区分：
  - 配置问题
  - vendor 资产问题
  - owner / build / dry-run 边界问题

推荐顺序：

1. 运行 `run-hardware-smoke-observation.ps1`
2. 若 `observation_result=blocked`，先处理自动化阻塞
3. 若 `observation_result=ready_for_gate_review`，继续按 `docs/runtime/hardware-bring-up-smoke-sop.md` 做人工检查
4. 使用 `register-hardware-smoke-observation.ps1` 归档结果
5. 将观察报告和 intake 记录交给 `A4`

## 已知限制

- 当前仓库仍缺失 `MultiCard.dll` 与 `MultiCard.lib`，在未补齐前，严格 dry-run 会把主要阻塞归类为 `vendor-assets`。
- 本任务不直接执行真实硬件动作，因此急停、限位、回零方向、I/O 和触发链仍需要结合 `C2` SOP 做人工确认。
- `A4` 仍需综合 `C1`、`C2`、`C3` 和后续 ownerization / integration 结果给出最终 go/no-go。
