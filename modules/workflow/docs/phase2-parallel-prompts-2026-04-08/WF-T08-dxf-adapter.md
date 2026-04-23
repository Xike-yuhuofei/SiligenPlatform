# WF-T08 DXF Adapter Family

- Task type: parallel-worker
- Target residues: `WF-R026`
- Depends on: `WF-T00` shared target freeze
- Allowed scope: `modules/workflow/adapters/infrastructure/adapters/planning/dxf/**`
- Forbidden scope: shared single-writer files; `modules/dxf-geometry/**`; `apps/**`; `modules/workflow/application/**`

## Prompt

```text
你负责 `WF-T08 DXF Adapter Family`。
目标 residue：`WF-R026`。
只处理 workflow/adapters 内 DXF/PB parsing concrete clean-exit：workflow 不再编译 `PbPathSourceAdapter.cpp`、`AutoPathSourceAdapter.cpp`、`DXFAdapterFactory.cpp`，不再在 workflow/adapters 生成或链接 DXF protobuf concrete。
你不是集成器，不得修改共享文件：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
禁止事项：不要顺手处理 recipe / motion / tests；不要修改 `modules/dxf-geometry/**` 以外的 foreign owner。
完成标准：`WF-R026` 的 acceptance 可机械通过。
验证：targeted build；`rg` 或构建日志确认 workflow/adapters 不再编译 DXF/PB parsing concrete。
停止条件：若必须改共享文件或下游 consumer，停止并把挂点交给 `WF-T00` / `WF-T11`。
输出：修改文件清单、shared-file request、验证结果。
```
