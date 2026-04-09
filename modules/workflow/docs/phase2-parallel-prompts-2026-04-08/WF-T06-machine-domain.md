# WF-T06 Machine Domain Family

- Task type: parallel-worker
- Target residues: `WF-R007`
- Depends on: `WF-T00` shared target freeze
- Allowed scope: `modules/workflow/domain/domain/machine/**`
- Forbidden scope: shared single-writer files; `modules/coordinate-alignment/**`; `apps/**`; `modules/runtime-execution/**`

## Prompt

```text
你负责 `WF-T06 Machine Domain Family`。
目标 residue：`WF-R007`。
只处理 workflow domain 内 machine family clean-exit：workflow 不再同时承载 `CalibrationProcess` 与 `DispenserModel` 这类不属于 M0 orchestration 的 machine aggregate concrete。
你不是集成器，不得修改共享文件：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
禁止事项：不要修改 `modules/coordinate-alignment/**` 的 canonical owner；不要顺手处理 safety / motion / system。
完成标准：`WF-R007` 的 acceptance 可机械通过。
验证：targeted build；`rg` 确认 workflow domain 不再编译 `CalibrationProcess` / `DispenserModel` concrete。
停止条件：若 canonical owner 落点不清或必须跨模块改 shared graph，停止并把挂点交给 `WF-T00`。
输出：修改文件清单、shared-file request、验证结果、owner freeze 风险。
```
