# WF-T10 Safety Domain Family

- Task type: parallel-worker
- Target residues: `WF-R003`, `WF-R009`
- Depends on: Wave A outputs merged by `WF-T00`
- Allowed scope: `modules/workflow/domain/motion-core/**`; `modules/workflow/domain/domain/safety/**`
- Forbidden scope: shared single-writer files; `modules/runtime-execution/**`; `apps/**`; `modules/workflow/application/**`

## Prompt

```text
你负责 `WF-T10 Safety Domain Family`。
目标 residue：`WF-R003`, `WF-R009`。
只处理 workflow domain 内 safety family clean-exit：`siligen_motion_core` 不再编译 `InterlockPolicy.cpp`，workflow bridge-domain 不再编译 `EmergencyStopService.cpp`、`InterlockPolicy.cpp`、`SafetyOutputGuard.cpp`、`SoftLimitValidator.cpp`，也不再通过 `siligen_motion_core` 维持 safety 依赖。
你不是集成器，不得修改共享文件：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
禁止事项：不要顺手处理 motion application 或 `runtime-execution`；不要通过新的 compat target 保留 safety concrete。
完成标准：`WF-R003` 与 `WF-R009` 的 acceptance 可机械通过。
验证：targeted build；`rg` 或构建日志确认上述 safety concrete 不再由 workflow target 编译。
停止条件：若必须改 shared graph、runtime consumer 或 app wiring，停止并把挂点交给 `WF-T00` / `WF-T12`。
输出：修改文件清单、shared-file request、验证结果。
```
