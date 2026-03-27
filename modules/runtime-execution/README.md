# runtime-execution

`modules/runtime-execution/` 是 `M9 runtime-execution` 的 canonical owner 入口。

## Owner 范围

- 运行时执行链调度、执行状态收敛与失败归责语义。
- 消费 `M4-M8` 规划结果并驱动执行，不回写上游规划事实。
- 汇聚运行时执行域的 owner 专属 contracts 与 adapters 边界。

## Owner 入口

- 构建入口：`modules/runtime-execution/CMakeLists.txt`（target：`siligen_module_runtime_execution`）。
- application public 入口：`modules/runtime-execution/application/CMakeLists.txt`（target：`siligen_runtime_execution_application_public`）。
- 模块契约入口：`modules/runtime-execution/contracts/README.md`。
- motion contracts 入口：`modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/`
- motion application 入口：`modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/`
- host motion provider 入口：`modules/runtime-execution/runtime/host/runtime/motion/`

## 结构归位

- `modules/runtime-execution/application/`：`M9` 对 app / host consumer 暴露的执行链 public surface。
- `modules/runtime-execution/contracts/`：`M9` owner 专属运行时契约。
- `modules/runtime-execution/adapters/`：设备与协议接入适配实现。
- `shared/contracts/device/`：跨模块稳定设备契约（不含实现）。

## Stage B Motion Owner

- `IMotionRuntimePort` 与 `IIOControlPort` 由 `runtime-execution/contracts/runtime` owner 持有。
- `MotionControlUseCase` 作为 Stage B 的最小 motion entry，统一承接 homing、ready-zero、jog、PTP control 与 monitoring 查询。
- `WorkflowMotionRuntimeServicesProvider` 位于 `runtime/host/runtime/motion/`，负责将 host 侧 runtime port 组装为 `MotionControlServiceImpl` / `MotionStatusServiceImpl`。
- `MotionControlUseCase` 继续沿用 `Siligen::Application::UseCases::Motion` 命名空间以保持兼容，但 owner 与公开 include root 已迁入 `runtime-execution/application/*`。

## 边界约束

- `apps/runtime-service/`、`apps/runtime-gateway/` 仅承担宿主和协议入口职责，不承载 `M9` 终态 owner 事实。
- `shared/contracts/device/` 是唯一设备稳定契约 canonical root；`modules/runtime-execution/contracts/device/` 仅保留兼容装配入口，不再反向定义 canonical target。
- `planner-cli`、`runtime-gateway`、`runtime-host` 的执行链 include 必须通过 `runtime_execution/application/*` 进入；上传与规划/编排则分别经 `job_ingest/application/*`、`workflow/application/*` 进入。
- `runtime-gateway` 的 motion facade/builder 只允许依赖 `MotionControlUseCase` 单入口，不得继续分别装配 `HomeAxesUseCase`、`EnsureAxesReadyZeroUseCase`、`ManualMotionControlUseCase`、`MotionMonitoringUseCase`。
- DXF/PB 准备、规划请求构建与 planning facade 调用归 `workflow/application/*`；`runtime_execution/application/*` 只消费已规划执行输入。
- `M9 runtime-execution` 只能消费上游对象，不得回写 `M0/M4-M8` 事实。
- 运行时不再保留 `siligen_runtime_execution_workflow_domain_compat`；残余 domain 类型依赖统一经 `siligen_domain` 解析，application/use-case 与 serializer 依赖继续通过 dedicated owner surfaces 收口。
- recipe 相关 consumer 必须通过 `siligen_workflow_recipe_domain_public`、`siligen_workflow_recipe_application_public`、`siligen_workflow_recipe_serialization_public` 接入，禁止再用 broad workflow public targets 回收能力。
## 当前事实来源

- `modules/runtime-execution/runtime/host/`
- `modules/runtime-execution/adapters/device/`
- `shared/contracts/device/`
- `apps/runtime-service/`
- `apps/runtime-gateway/transport-gateway/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `application/include/runtime_execution/application/**` 已成为执行链 consumer 的唯一公开 include 面。
- `shared/contracts/device`、`adapters/device`、`runtime/host` 已成为当前设备契约/消费的唯一真实构建根；`modules/runtime-execution/contracts/device` 仅作兼容入口。
- `planner-cli` 与 `runtime-gateway/transport-gateway` 已直接链接 `siligen_runtime_execution_application_public`；broad runtime-workflow compat target 已移除。
- `DispensingExecutionUseCase*.cpp` 已由 `modules/runtime-execution/application/usecases/dispensing/` 编译，`workflow/application` 仅保留规划与编排。
- `DispensingExecutionUseCase` 的 canonical 输入已切换为已规划执行输入；DXF 发起链路通过 `workflow/application/usecases/dispensing/DispensingExecutionWorkflowUseCase` 过渡编排后再调用 M9。
- canonical CMake 已移除对 bridge vendor 路径的默认回退；仅保留显式 `SILIGEN_MULTICARD_VENDOR_DIR` 覆盖能力。
- 首轮硬件 smoke 主链所需的 `HardwareTestAdapter`、`HardwareConnectionAdapter`、`MultiCardMotionAdapter`、`InterpolationAdapter`、`HomingPortAdapter`、`MotionRuntimeFacade`、`ValveAdapter`、`TriggerControllerAdapter` 等 owner 资产已迁入 `adapters/device/src/adapters/**` canonical roots，相关 public headers 不再回指 `src/legacy`。
- `adapters/device` 当前仅保留 vendor 边界所需的 legacy driver residual：`src/legacy/drivers/multicard/MultiCardAdapter.cpp`，以及在无真实卡库时启用的 `src/legacy/drivers/multicard/MultiCardStub_Class.cpp`；这些残留不再承载硬件 smoke 的公开 owner 面。
- `planner-cli` 与 `transport-gateway` 当前分别经 `workflow/application`、`workflow/adapters`、`job_ingest/application`、`runtime_execution/application` 组合消费所需 public surface，不得回退到 `siligen_module_workflow`。

## DXF Caller 对照

| 调用方 | 当前 DXF 入口 | 允许直接依赖 |
| --- | --- | --- |
| `apps/planner-cli` | `workflow/application/usecases/dispensing/DispensingExecutionWorkflowUseCase` | 可直接依赖 `runtime_execution/application` 仅用于 task/status 等已规划执行控制；DXF 发起不得直连 M9 |
| `apps/runtime-gateway/transport-gateway` | `ExecuteDxfAsync` 固定走 `DispensingExecutionWorkflowUseCase` | `UploadFileUseCase`、`PlanningUseCase`、`DispensingWorkflowUseCase` 用于上传/规划/预览；`DispensingExecutionUseCase` 仅用于 task status / pause / resume / cancel |
| `apps/runtime-service` / `runtime/host/container` | 容器同时装配 workflow 兼容入口与 M9 canonical execution API | legacy DXF forwarder 只允许转发到 `workflow/application`，不得在 M9 内部恢复 planning 逻辑 |

## 第一阶段回归守则

- `runtime-execution/application` 不得重新引入 `DispensePlanningFacade`。
- `runtime-execution/application` 不得重新引入 `DxfPbPreparationService`。
- `runtime-execution/application` 不得恢复 `planner_->Plan(...)`。
- `runtime-execution/application` 不得恢复 `BuildPlanRequest(...)`。
- `workflow/application` 是 legacy DXF compatibility owner；DXF/PB 准备、规划请求构建与 planning facade 调用必须停留在这里。
