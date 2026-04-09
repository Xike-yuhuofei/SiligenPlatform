# WF-T07 System Application Family

- Task type: parallel-worker
- Target residues: `WF-R012`
- Depends on: `WF-T00` shared target freeze
- Allowed scope: `modules/workflow/application/usecases/system/**`
- Forbidden scope: shared single-writer files; `modules/runtime-execution/**`; `apps/runtime-service/**`; `apps/**`

## Prompt

```text
你负责 `WF-T07 System Application Family`。
目标 residue：`WF-R012`。
只处理 workflow application 内 system family clean-exit：workflow application 不再编译 `InitializeSystemUseCase.cpp`、`EmergencyStopUseCase.cpp`，外部不再通过 workflow system 头获取 runtime/system 能力。
你不是集成器，不得修改共享文件：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
禁止事项：不要顺手处理 motion family 或 `runtime-execution` contract；不要通过 alias 保留 system concrete。
完成标准：`WF-R012` 的 acceptance 可机械通过。
验证：targeted build；`rg` 或构建日志确认上述 system usecase 不再由 workflow application 编译。
停止条件：若必须改 `runtime-execution` 或 apps consumer，停止并把挂点交给 `WF-T11` / `WF-T12` / `WF-T00`。
输出：修改文件清单、shared-file request、验证结果。
```
