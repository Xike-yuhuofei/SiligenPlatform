# tests/e2e

正式 e2e 验证承载面。

## 正式层级

- `tests/e2e/simulated-line/`：`L4` 模拟全链路 E2E
- `tests/e2e/hardware-in-loop/`：`L5` HIL 闭环测试

## 说明

- `L4` 负责在 simulated-line 下跑完整主路径和关键异常路径。
- `L5` 只在 `L0-L4` 已通过后执行，角色是闭环确认，不是低级 bug 主发现层。
- simulated-line 和 HIL 都必须额外产出 `case-index.json`、`validation-evidence-bundle.json`、`evidence-links.md`。
- HIL evidence 必须声明前置离线层、样本、operator 介入点与 abort metadata。
