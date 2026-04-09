# WF-T09 Motion Application Family

- Task type: parallel-worker
- Target residues: `WF-R013`
- Depends on: Wave A outputs merged by `WF-T00`
- Allowed scope: `modules/workflow/application/usecases/motion/**`; `modules/workflow/application/execution-supervision/runtime-consumer/**`
- Forbidden scope: shared single-writer files; `modules/runtime-execution/**`; `apps/**`; `modules/workflow/domain/**`

## Prompt

```text
你负责 `WF-T09 Motion Application Family`。
目标 residue：`WF-R013`。
只处理 workflow application 内 motion family clean-exit：workflow application 不再编译 motion execution/control concrete，不再负责 `MotionRuntimeAssemblyFactory` 或 `DefaultMotionRuntimeValidationService` 这类 runtime assembly 职责。
你不是集成器，不得修改共享文件：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
禁止事项：不要顺手处理 `runtime-execution` 反向依赖；不要新增 runtime-consumer wrapper；不要处理 safety domain。
完成标准：`WF-R013` 的 acceptance 可机械通过。
验证：targeted build；`rg` 或构建日志确认 workflow application 不再编译 motion execution/control concrete，也不再拥有 runtime assembly factory。
停止条件：若必须改 `runtime-execution/**`、shared files 或 apps consumer，停止并把挂点交给 `WF-T00` / `WF-T11` / `WF-T12`。
输出：修改文件清单、shared-file request、验证结果、与 `runtime-execution` 的待合并接口点。
```
