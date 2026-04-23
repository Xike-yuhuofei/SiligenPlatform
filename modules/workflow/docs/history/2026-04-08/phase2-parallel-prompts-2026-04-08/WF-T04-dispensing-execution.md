# WF-T04 Dispensing Execution Family

- Task type: parallel-worker
- Target residues: `WF-R006`
- Depends on: `WF-T00` shared target freeze
- Allowed scope: `modules/workflow/domain/domain/dispensing/**`
- Forbidden scope: shared single-writer files; `modules/workflow/application/**`; `modules/workflow/adapters/**`; `apps/**`

## Prompt

```text
你负责 `WF-T04 Dispensing Execution Family`。
目标 residue：`WF-R006`。
只处理 workflow domain 内 dispensing concrete clean-exit：workflow 不再编译 `PurgeDispenserProcess.cpp`、`SupplyStabilizationPolicy.cpp`、`ValveCoordinationService.cpp`。
你不是集成器，不得修改共享文件：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
禁止事项：不要顺手处理 valve application、DXF adapter、tests 或 apps；不要用新的 bridge 保留 dispensing concrete。
完成标准：`WF-R006` 的 acceptance 可机械通过。
验证：targeted build；`rg` 或构建日志确认 workflow domain target 不再编译上述 dispensing concrete。
停止条件：若必须同步改 `domain/domain/CMakeLists.txt` 或外部 consumer，停止并把挂点交给 `WF-T00` / `WF-T11`。
输出：修改文件清单、shared-file request、验证结果。
```
