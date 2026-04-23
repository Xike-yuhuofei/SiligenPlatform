# WF-T02 Domain Public Surface

- Task type: parallel-worker
- Target residues: `WF-R020`, `WF-R021`, `WF-R022`, `WF-R023`
- Depends on: `WF-T00` first-pass shared target freeze
- Allowed scope: `modules/workflow/domain/include/**`
- Forbidden scope: shared single-writer files; `modules/workflow/domain/domain/**`; `apps/**`; `modules/runtime-execution/**`

## Prompt

```text
你负责 `WF-T02 Domain Public Surface`。
目标 residue：`WF-R020`, `WF-R021`, `WF-R022`, `WF-R023`。
只处理 `modules/workflow/domain/include/**` 下的 canonical public header clean-exit：workflow 不再通过 compat 头暴露 dispensing planning / trigger、motion planning、旧 `planning` 路径或旧 `system/redundancy` 路径的真实语义。
你不是集成器，不得修改共享文件：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
禁止事项：不要迁移 domain concrete；不要改 tests、apps、`runtime-execution`；不要保留新的 compat alias。
完成标准：`WF-R020` 到 `WF-R023` 的 acceptance 可机械通过。
验证：include path check；`rg` 确认 workflow `domain/include/domain/**` 不再回指 foreign owner 真实语义。
停止条件：若发现 repo 级 include consumer 未明确，需要把 trace 交给 `WF-T15`，不要顺手全仓扩散。
输出：修改文件清单、shared-file request、验证结果、未决 consumer 列表。
```
