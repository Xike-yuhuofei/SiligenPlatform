# WF-T01 Application Public Surface

- Task type: parallel-worker
- Target residues: `WF-R018`, `WF-R019`
- Depends on: `WF-T00` first-pass shared target freeze
- Allowed scope: `modules/workflow/application/include/**`
- Forbidden scope: shared single-writer files; `modules/workflow/application/usecases/**`; `apps/**`; `modules/runtime-execution/**`

## Prompt

```text
你负责 `WF-T01 Application Public Surface`。
目标 residue：`WF-R018`, `WF-R019`。
只处理 `modules/workflow/application/include/**` 下的 canonical public header clean-exit：`planning-trigger` / `phase-control` 头不再回指旧 `usecases/dispensing/*`，deprecated shell 头不再导出 planning export / motion runtime provider 的真实业务语义。
你不是集成器，不得修改共享文件：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
禁止事项：不要改 recipe / motion / system concrete；不要新增 alias 头继续透传旧 surface；不要改 apps 或 `runtime-execution`。
完成标准：`WF-R018` 与 `WF-R019` 的 acceptance 可机械通过。
验证：include path check；`rg` 确认 canonical 头不再包含旧 `usecases/dispensing/*` 或 workflow 下的 runtime provider / planning export 兼容入口。
停止条件：若必须改共享文件或外部消费者路径，停止并把挂点交给 `WF-T00` / `WF-T11` / `WF-T12`。
输出：修改文件清单、需要 `WF-T00` 合入的 shared-file request、验证结果。
```
