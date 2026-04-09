# WF-T04 / WF-R006 执行记录与阻塞移交

## 1. 任务与边界
- worker: `WF-T04 Dispensing Execution Family`
- target residue: `WF-R006`
- 用户限制:
  - 只处理 workflow domain 内 dispensing concrete clean-exit
  - 不得修改 shared single-writer files:
    - `CMakeLists.txt`
    - `modules/workflow/CMakeLists.txt`
    - `modules/workflow/domain/CMakeLists.txt`
    - `modules/workflow/domain/domain/CMakeLists.txt`
    - `modules/workflow/application/CMakeLists.txt`
  - 不处理 valve application、DXF adapter、tests、apps
  - 不允许用新的 bridge 保留 dispensing concrete
- task prompt stop condition:
  - 若必须同步改 `domain/domain/CMakeLists.txt` 或外部 consumer，停止并把挂点交给 `WF-T00` / `WF-T11`

## 2. 本 worker 实际改动
- 修改文件清单:
  - `modules/workflow/docs/session-handoffs/2026-04-08-1513-wf-t04-wf-r006-blocked-handoff.md`
- 未修改:
  - `modules/workflow/domain/domain/CMakeLists.txt`
  - `modules/workflow/application/usecases/dispensing/valve/**`
  - `apps/**`
  - 任何 tests / adapters / shared files

## 3. 机械证据

### 3.1 workflow domain 当前仍直接编译 residue concrete
- `modules/workflow/domain/domain/CMakeLists.txt:127-130`
  - `siligen_dispensing_execution_services` 直接加入:
    - `dispensing/domain-services/PurgeDispenserProcess.cpp`
    - `dispensing/domain-services/SupplyStabilizationPolicy.cpp`
    - `dispensing/domain-services/ValveCoordinationService.cpp`
- live build 生成文件再次确认:
  - `build/modules/workflow/domain/bridge-domain/siligen_dispensing_execution_services.vcxproj:285-287`
  - 仍存在这 3 个 `ClCompile Include`

### 3.2 shared target freeze 仍未完成
- `modules/workflow/docs/phase2-parallel-prompts-2026-04-08/README.md:5`
  - `WF-T00` 是唯一允许修改 shared aggregator files 的 task
- `modules/workflow/docs/phase2-parallel-prompts-2026-04-08/WF-T04-dispensing-execution.md:5`
  - `WF-T04` 明确依赖 `WF-T00 shared target freeze`

### 3.3 consumer 仍未迁出
- workflow 内 consumer:
  - `modules/workflow/application/usecases/dispensing/valve/CMakeLists.txt:19-22`
    - `siligen_valve_core` 仍链接 `siligen_dispensing_execution_services`
  - `modules/workflow/application/usecases/dispensing/valve/ValveCommandUseCase.h:4-5`
    - 仍包含 `PurgeDispenserProcess.h` / `ValveCoordinationService.h`
  - `modules/workflow/application/usecases/dispensing/valve/ValveCommandUseCase.cpp:43-50`
    - 仍直接构造 `PurgeDispenserProcess`
- workflow 外 consumer:
  - `apps/runtime-service/container/ApplicationContainer.Dispensing.cpp:9`
    - 仍包含 `ValveCoordinationService.h`
  - `apps/runtime-service/container/ApplicationContainer.Dispensing.cpp:92-96`
    - 仍直接构造并注册 `ValveCoordinationService`

### 3.4 owner 侧当前并未承接这 3 个 execution concrete
- `modules/dispense-packaging/domain/dispensing/CMakeLists.txt:1-4`
  - 文件内明确声明 `Purge/ValveCoordination/SupplyStabilization 已不在本 target 源列表中`
- `modules/dispense-packaging/domain/dispensing/CMakeLists.txt:38-59`
  - `siligen_dispense_packaging_domain_dispensing` 源列表未包含上述 3 个 `.cpp`

## 4. 已执行验证
- targeted build:
  - 命令:
    - `cmake --build D:\Projects\SiligenSuite\build --config Debug --target siligen_dispensing_execution_services -- /m:1 /v:minimal`
  - 结果:
    - 成功生成 `D:\Projects\SiligenSuite\build\lib\Debug\siligen_dispensing_execution_services.lib`
- target source evidence:
  - 命令:
    - `rg -n "PurgeDispenserProcess.cpp|SupplyStabilizationPolicy.cpp|ValveCoordinationService.cpp" D:\Projects\SiligenSuite\build\modules\workflow\domain\bridge-domain\siligen_dispensing_execution_services.vcxproj`
  - 结果:
    - 第 285-287 行仍列出这 3 个 concrete source

## 5. 结论
- `WF-R006` 在当前用户约束下不能由 `WF-T04` 单独闭合。
- 原因不是实现细节未改，而是入口和 consumer 都落在本 worker 禁改范围之外。
- 继续在 `modules/workflow/domain/domain/dispensing/**` 内做任何局部删除、重命名或 stub，都无法让 acceptance 机械通过，且会违反“不要用新的 bridge 保留 dispensing concrete”。

## 6. Shared-file Request

### 6.1 发给 `WF-T00`
- 处理 shared single-writer file:
  - `modules/workflow/domain/domain/CMakeLists.txt`
- 必要动作:
  - 从 `siligen_dispensing_execution_services` 移除:
    - `dispensing/domain-services/PurgeDispenserProcess.cpp`
    - `dispensing/domain-services/SupplyStabilizationPolicy.cpp`
    - `dispensing/domain-services/ValveCoordinationService.cpp`
- 合入前检查:
  - 不得在 workflow 内新增 compat target / bridge target / forwarding static lib 继续承载这 3 个 concrete
  - 需要与 canonical owner target freeze 一起落地，否则只会把问题从编译期移到链接期

### 6.2 发给 `WF-T11`
- retarget consumer:
  - `apps/runtime-service/container/ApplicationContainer.Dispensing.cpp`
- 当前挂点:
  - `ValveCoordinationService` 直接实例化与注册
- 需要的后续动作:
  - 改为依赖 canonical public surface
  - 不再从 workflow domain public headers 获取 `ValveCoordinationService`

### 6.3 发给相关 valve application worker
- 当前 workflow 内挂点:
  - `modules/workflow/application/usecases/dispensing/valve/CMakeLists.txt`
  - `modules/workflow/application/usecases/dispensing/valve/ValveCommandUseCase.h`
  - `modules/workflow/application/usecases/dispensing/valve/ValveCommandUseCase.cpp`
- 说明:
  - 这些 consumer 不在本 worker 允许修改范围内
  - 若 `WF-T00` 先撤掉 shared target 编译入口，而 valve application 未同步 retarget，将产生新的链接缺口

## 7. 未解决挂点
- canonical owner target 里尚未看到这 3 个 execution concrete 的 live build 接管点
- `workflow` 与 `apps` 仍存在对 workflow headers / target 的直接依赖
- `modules/workflow/WORKFLOW_RESIDUE_LEDGER.md` 中 `WF-R006` 的旧定义与本轮 prompt 不一致；本轮以 `modules/workflow/docs/workflow-residue-ledger-2026-04-08.md` 和 `WF-T04` prompt 为准

## 8. 建议后续顺序
1. `WF-T00` 先完成 shared target freeze，并确认 canonical owner build graph。
2. valve application 相关 worker 同步移除 workflow 内 direct consumer。
3. `WF-T11` 再处理 apps consumer retarget。
4. 最后重新执行 targeted build，确认 workflow domain target 不再编译这 3 个 `.cpp`。
