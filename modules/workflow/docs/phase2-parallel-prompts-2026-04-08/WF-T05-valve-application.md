# WF-T05 Valve Application Family

- Task type: parallel-worker
- Target residues: `WF-R011`
- Depends on: `WF-T00` shared target freeze
- Allowed scope: `modules/workflow/application/usecases/dispensing/valve/**`
- Forbidden scope: shared single-writer files; `modules/workflow/domain/**`; `modules/workflow/adapters/**`; `apps/**`

## Prompt

```text
你负责 `WF-T05 Valve Application Family`。
目标 residue：`WF-R011`。
只处理 workflow application 内 valve family clean-exit：workflow application 不再编译 `ValveCommandUseCase.cpp`、`ValveQueryUseCase.cpp`。
你不是集成器，不得修改共享文件：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
禁止事项：不要顺手处理 dispensing domain、motion、system 或 apps；不要通过新的 facade 保留 valve concrete。
完成标准：`WF-R011` 的 acceptance 可机械通过。
验证：targeted build；`rg` 或构建日志确认 workflow application target 不再编译 valve command/query concrete。
停止条件：若必须改共享聚合文件或外部 include consumer，停止并把挂点交给 `WF-T00` / `WF-T11`。
输出：修改文件清单、shared-file request、验证结果。
```
