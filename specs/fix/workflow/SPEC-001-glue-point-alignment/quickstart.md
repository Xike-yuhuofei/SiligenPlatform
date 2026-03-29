# Quickstart: HMI 胶点预览锚定分布一致性

## 1. 目标

在当前 feature 分支上完成 planning owner chain 的规则收敛，使 `planned_glue_snapshot + glue_points` 成为唯一权威预览，并验证其与 execution gate 共用同一份权威出胶结果。

## 2. 关键输入

开始前确认以下资产存在：

1. 规格与设计产物
   `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-001-glue-point-alignment\spec.md`
   `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-001-glue-point-alignment\plan.md`
   `D:\Projects\SiligenSuite\specs\fix\workflow\SPEC-001-glue-point-alignment\research.md`
2. 正式 preview 协议源
   `D:\Projects\SiligenSuite\shared\contracts\application\commands\dxf.command-set.json`
3. 关键 owner / consumer 代码
   `D:\Projects\SiligenSuite\modules\dispense-packaging\application\services\dispensing\DispensePlanningFacade.cpp`
   `D:\Projects\SiligenSuite\modules\workflow\application\usecases\dispensing\DispensingWorkflowUseCase.cpp`
   `D:\Projects\SiligenSuite\apps\runtime-gateway\transport-gateway\src\tcp\TcpCommandDispatcher.cpp`
   `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py`

## 3. 最小实施顺序

1. 先在 owner planning chain 落地显式 trigger authority：
   - `modules/dispense-packaging` 负责生成锚定分布、短线段例外和 grouped spacing validation
   - `modules/workflow` 负责保持 preview 与 execution gate 同源
2. 再收紧 preview transport / UI 契约：
   - gateway 只接受 `planned_glue_snapshot + glue_points`
   - HMI 继续把 `planned_glue_snapshot` 呈现为主预览
3. 最后补测试和协议映射文档

## 4. 本地验证命令

在仓库根目录 `D:\Projects\SiligenSuite\` 执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite all
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite all
```

面向本特性的快速回归：

```powershell
cmake --build .\build --config Debug --target siligen_dispense_packaging_unit_tests siligen_motion_planning_unit_tests siligen_unit_tests -- /m:1
& .\build\bin\Debug\siligen_dispense_packaging_unit_tests.exe --gtest_filter="DispensePlanningFacadeTest.*"
& .\build\bin\Debug\siligen_motion_planning_unit_tests.exe --gtest_filter="CMPCoordinatedInterpolatorPrecisionTest.*"
& .\build\bin\Debug\siligen_unit_tests.exe --gtest_filter="DispensingWorkflowUseCaseTest.*:PlanningUseCaseExportPortTest.*"
python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q
python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q
python -m pytest .\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py -q
```

说明：`build.ps1` / `test.ps1` 只作为 workspace 健康门，不替代上述 feature 定向回归。

## 5. 通过标准

实施完成后，至少应能证明：

1. `dxf.preview.snapshot` 返回的主预览来源仍为 `planned_glue_snapshot`，且 `glue_points` 非空。
2. 相同输入图形和相同规划参数下，`glue_points` 的数量和坐标稳定一致。
3. 角点、共享顶点和短线段场景都能按当前规格口径通过或显式失败。
4. 当前计划在 preview 缺失、来源不合规或 authority 失配时，无法继续执行。
5. 旧结果、旧格式和旧 fallback 路径不会被自动转换为成功预览。

## 6. 进入 `speckit-tasks` 的条件

满足以下条件后即可进入 `speckit-tasks`：

1. `plan.md`、`research.md`、`data-model.md`、`contracts\*`、`quickstart.md` 已冻结。
2. 真实 owner surface、preview authority、execution gate 和失败边界已经在设计层写清楚。
3. 验证入口已明确到根级脚本与定向 C++ / Python 回归，不再依赖口头说明。
