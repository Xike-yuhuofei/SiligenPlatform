# Simulation 受控生产测试发布结论模板 V1

更新时间：`2026-04-01`

## 1. 执行信息

- 执行日期：`待填写`
- 执行人：`待填写`
- Profile：`Local / CI / 待填写`
- 报告目录：`tests/reports/controlled-production-test`

## 2. 自动化证据

- `workspace-validation.json`：`待填写路径或版本`
- `workspace-validation.md`：`待填写路径或版本`
- `e2e/simulated-line/simulated-line-summary.json`：`待填写路径或版本`
- `e2e/simulated-line/simulated-line-summary.md`：`待填写路径或版本`
- `controlled-production-gate-summary.json`：`待填写路径或版本`
- `controlled-production-gate-summary.md`：`待填写路径或版本`
- `simulation-controlled-production-release-summary.md`：`待填写路径或版本`

## 3. 判定摘要

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| e2e workspace gate | `待填写` | failed / known_failure / skipped 是否为 0 |
| engineering regression | `待填写` | workspace case `engineering-regression` |
| simulated-line workspace case | `待填写` | workspace case `simulated-line` |
| simulated-line scheme C | `待填写` | `rect_diag` / `sample_trajectory` / `invalid_empty_segments` / `following_error_quantized` |
| deterministic replay | `待填写` | 是否全部为 `true` |
| fault matrix metadata | `待填写` | `fault-matrix.simulated-line.v1` 与 replay metadata 是否齐全 |
| known failure / skipped | `待填写` | 必须为 `0 / 0` |

## 4. 结论

只允许填写以下三种结论之一：

- `通过`
- `有条件通过`
- `阻塞`

填写规则：

- 所有自动化证据齐全，且 `e2e` workspace gate 无失败、无 `known_failure`、无 `skipped`：`通过`
- 所有自动化命令通过，但存在非阻断说明项：`有条件通过`
- 任一关键 case、fault matrix metadata 或报告链缺失：`阻塞`

## 5. 范围声明

本结论只代表：

- `e2e/simulated-line` 受控生产测试通过/未通过

本结论不代表：

- HIL 长稳通过
- 真机 smoke 通过
- 整机或工艺真实性签收完成
