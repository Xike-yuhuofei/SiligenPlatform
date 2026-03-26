# trace-diagnostics

`modules/trace-diagnostics/` 是 `M10 trace-diagnostics` 的 canonical owner 入口。

## Owner 范围

- 追溯事件、诊断日志与审计线索的归档语义。
- 面向 HMI、验证与运维读取场景的追溯查询口径。
- 汇聚 `M10` owner 专属 contracts 与日志边界。

## Owner 入口

- 构建入口：`modules/trace-diagnostics/CMakeLists.txt`（target：`siligen_module_trace_diagnostics`）。
- 模块契约入口：`modules/trace-diagnostics/contracts/README.md`。

## 结构归位

- `modules/trace-diagnostics/contracts/`：`M10` owner 专属追溯与诊断契约。
- `shared/logging/`：跨模块复用日志基础设施（非 `M10` owner 实现）。

## 边界约束

- `docs/validation/` 仅承载验证说明，不承载追溯业务 owner 实现。
- `tests/` 与 `apps/trace-viewer/` 只消费 `M10` 事实，不承载 `M10` owner 实现。

## 当前协作面

- `docs/validation/`
- `apps/trace-viewer/`
- `shared/logging/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `adapters/logging/spdlog/` 是当前明确的日志适配实现承载面。
- 终态实现必须继续落在该模块 canonical 骨架内，不再引入 bridge 目录。

