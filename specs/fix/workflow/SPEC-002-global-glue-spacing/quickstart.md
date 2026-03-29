# Quickstart: 胶点全局均布与真值链一致性

## 1. 目标

在当前 feature 分支上完成连续路径全局胶点布局的 owner 收敛，使 `planned_glue_snapshot + glue_points` 继续作为唯一成功主预览，同时保证同一批 authority points 被 preview、校验和 execution gate 共用。

## 2. 关键输入

开始前确认以下资产存在：

1. 规格与设计产物
   `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-002-global-glue-spacing\spec.md`
   `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-002-global-glue-spacing\plan.md`
   `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-002-global-glue-spacing\research.md`
   `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-002-global-glue-spacing\data-model.md`
2. 关键 owner / consumer 代码
   `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp`
   `D:\Projects\SiligenSuite\modules\dispense-packaging\domain\dispensing\planning\domain-services\DispensingPlannerService.cpp`
   `D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\PlanningUseCase.cpp`
   `D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\DispensingWorkflowUseCase.cpp`
   `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\src\tcp\TcpCommandDispatcher.cpp`
   `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py`
3. 正式对外契约源
   `D:\Projects\SiligenSuite\shared\contracts\application\commands\dxf.command-set.json`
   `D:\Projects\SiligenSuite\shared\contracts\application\fixtures\responses\dxf.preview.snapshot.success.json`
   `D:\Projects\SiligenSuite\shared\contracts\application\mappings\protocol-mapping.md`

## 3. 最小实施顺序

1. 先在 `modules/dispense-packaging` 落地 authority layout 真值
   - 新增连续 span 布局、闭环相位和可见路径定位服务
   - 输出同一批 `LayoutTriggerPoint`、validation outcomes 和 execution bindings
2. 再改 `DispensePlanningFacade` / `DispensingPlannerService`
   - 去掉按段真值与 preview 二次重建热路径
   - 让 `glue_points` 直接来自 authority layout
3. 然后收紧 `modules/workflow`
   - `PlanningUseCase`、`DispensingWorkflowUseCase` 和 `WorkflowPreviewSnapshotService` 只消费 authority layout
   - 允许显式例外通过，但阻止 authority 缺失、失配或阻塞性违规
4. 最后验证 transport / HMI 契约
   - gateway 继续只接受 `planned_glue_snapshot + glue_points`
   - HMI 继续把 `planned_glue_snapshot` 呈现为主预览，并明确区分 `runtime_snapshot`

## 4. 本地验证命令

在仓库根目录 `D:\Projects\SiligenSuite\` 执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite all
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite all
```

面向本特性的快速回归：

```powershell
cmake --build .\build --config Debug --target siligen_dispense_packaging_unit_tests siligen_unit_tests -- /m:1
& .\build\bin\Debug\siligen_dispense_packaging_unit_tests.exe --gtest_filter="DispensePlanningFacadeTest.*:TriggerPlannerTest.*:AuthorityTriggerLayoutPlannerTest.*:PathArcLengthLocatorTest.*"
& .\build\bin\Debug\siligen_unit_tests.exe --gtest_filter="DispensingWorkflowUseCaseTest.*:PlanningUseCaseExportPortTest.*"
python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q
python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q
python -m pytest .\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py -q
```

说明：根级 `build.ps1` / `test.ps1` 用于 workspace 健康门；上面的定向回归用于覆盖“连续路径全局均布 + preview/execution 同源 + transport/HMI 契约”。

## 5. 通过标准

实施完成后，至少应能证明：

1. 几何上等价但分段不同的输入，得到相同数量、相同顺序且等价位置的 `glue_points`。
2. 闭合路径重复生成时，胶点数量、整体分布和通过/失败结论稳定一致，不出现缝口跳变。
3. `glue_points` 直接来自 authority layout，而不是从 execution trajectory 二次重建。
4. `dxf.preview.snapshot` 返回的成功主语义仍是 `planned_glue_snapshot + glue_points`，且 `glue_points` 非空。
5. `runtime_snapshot`、空 `glue_points`、authority 缺失或阻塞性校验失败时，当前计划不能继续执行。
6. 允许的间距例外会被显式标记并附带原因，而不是被误判为成功或被静默掩盖。

## 6. 进入 `speckit-tasks` 的条件

满足以下条件后即可进入 `speckit-tasks`：

1. `plan.md`、`research.md`、`data-model.md`、`contracts\*`、`quickstart.md` 已冻结。
2. authority layout owner、closed-loop phase 规则、curve 定位策略、validation 三态语义已经写清楚。
3. 预览、校验与执行如何共享同一批 authority points 已在设计层明确。
4. 验证入口已明确到根级脚本和定向 C++ / Python 回归。
