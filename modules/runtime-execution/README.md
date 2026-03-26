# runtime-execution

`modules/runtime-execution/` 是 `M9 runtime-execution` 的 canonical owner 入口。

## Owner 范围

- 运行时执行链调度、执行状态收敛与失败归责语义。
- 消费 `M4-M8` 规划结果并驱动执行，不回写上游规划事实。
- 汇聚运行时执行域的 owner 专属 contracts 与 adapters 边界。

## Owner 入口

- 构建入口：`modules/runtime-execution/CMakeLists.txt`（target：`siligen_module_runtime_execution`）。
- 模块契约入口：`modules/runtime-execution/contracts/README.md`。

## 结构归位

- `modules/runtime-execution/contracts/`：`M9` owner 专属运行时契约。
- `modules/runtime-execution/adapters/`：设备与协议接入适配实现。
- `shared/contracts/device/`：跨模块稳定设备契约（不含实现）。

## 边界约束

- `apps/runtime-service/`、`apps/runtime-gateway/` 仅承担宿主和协议入口职责，不承载 `M9` 终态 owner 事实。
- `modules/runtime-execution/{runtime/host,adapters/device,contracts/device}` 与 `shared/contracts/device/` 构成当前 canonical owner 面。
- `M9 runtime-execution` 只能消费上游对象，不得回写 `M0/M4-M8` 事实。
## 当前事实来源

- `modules/runtime-execution/runtime/host/`
- `modules/runtime-execution/adapters/device/`
- `modules/runtime-execution/contracts/device/`
- `apps/runtime-service/`
- `apps/runtime-gateway/transport-gateway/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `contracts/device`、`adapters/device`、`runtime/host` 已成为唯一真实实现根。
- `runtime/host`、`adapters/device`、`planner-cli`、`runtime-gateway/transport-gateway` 与根 `security_module` 已统一转向 `workflow` canonical public surface。
- canonical CMake 已移除对 bridge vendor 路径的默认回退；仅保留显式 `SILIGEN_MULTICARD_VENDOR_DIR` 覆盖能力。
- 首轮硬件 smoke 主链所需的 `HardwareTestAdapter`、`HardwareConnectionAdapter`、`MultiCardMotionAdapter`、`InterpolationAdapter`、`HomingPortAdapter`、`MotionRuntimeFacade`、`ValveAdapter`、`TriggerControllerAdapter` 等 owner 资产已迁入 `adapters/device/src/adapters/**` canonical roots，相关 public headers 不再回指 `src/legacy`。
- `adapters/device` 当前仅保留 vendor 边界所需的 legacy driver residual：`src/legacy/drivers/multicard/MultiCardAdapter.cpp`，以及在无真实卡库时启用的 `src/legacy/drivers/multicard/MultiCardStub_Class.cpp`；这些残留不再承载硬件 smoke 的公开 owner 面。
- `planner-cli` 与 `transport-gateway` 已完成串行 target build 验证，确认外部消费者可通过新的 `workflow` public surface 构建。

