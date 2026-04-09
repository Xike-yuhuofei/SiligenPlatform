# WF-T03 Recipe Family

- Task type: parallel-worker
- Target residues: `WF-R002`, `WF-R008`, `WF-R010`
- Depends on: `WF-T00` shared target freeze
- Allowed scope: `modules/workflow/domain/process-core/**`; `modules/workflow/domain/domain/recipes/**`; `modules/workflow/application/usecases/recipes/**`
- Forbidden scope: shared single-writer files; `apps/**`; `modules/runtime-execution/**`

## Prompt

```text
你负责 `WF-T03 Recipe Family`。
目标 residue：`WF-R002`, `WF-R008`, `WF-R010`。
只处理 workflow 内 recipe family 的 leaf 级 clean-exit：`siligen_process_core` 不再编译 recipe concrete，workflow domain 不再编译 `RecipeActivationService.cpp` / `RecipeValidationService.cpp`，workflow application 不再编译 recipe CRUD / bundle usecases。
你不是集成器，不得修改共享文件：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
禁止事项：不要改 valve / motion / system / DXF family；不要修改 apps 或 `runtime-execution` consumer；不要通过新增 wrapper 保留 recipe owner drift。
完成标准：`WF-R002`, `WF-R008`, `WF-R010` 的 acceptance 可机械通过。
验证：targeted build；`rg` 或构建日志确认上述 recipe `.cpp` / usecase 不再由 workflow target 编译。
停止条件：若必须改共享聚合文件或 recipe canonical owner 路径未冻结，停止并把挂点交给 `WF-T00`。
输出：修改文件清单、shared-file request、验证结果、未迁净清单。
```
