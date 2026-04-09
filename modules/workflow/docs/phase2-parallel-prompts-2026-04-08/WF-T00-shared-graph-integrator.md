# WF-T00 Shared Graph Integrator

- Task type: serial-integrator
- Target residues: `WF-R001`, `WF-R004`, `WF-R005`, `WF-R014`, `WF-R015`, `WF-R016`, `WF-R017`
- Depends on: ledger baseline only; then consumes outputs from all worker tasks
- Allowed scope: shared single-writer files; minimal integration edits needed to merge worker results
- Forbidden scope: do not own family-specific concrete migration work inside leaf directories unless required only for integration wiring

## Prompt

```text
你负责 `WF-T00 Shared Graph Integrator`。
背景：唯一基线是 `modules/workflow/docs/workflow-residue-ledger-2026-04-08.md`。当前目标是为 phase 2 clean-exit 执行提供唯一的共享文件写入口。
目标 residue：`WF-R001`, `WF-R004`, `WF-R005`, `WF-R014`, `WF-R015`, `WF-R016`, `WF-R017`。
你是唯一允许修改共享聚合文件的执行者：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
你的职责：冻结共享 target 归属、接收各 worker 的挂点请求、合并必要的 build graph / aggregate / alias 变更，并保持每个 family worker 的 write set 不重叠。
禁止事项：不要顺手承担 recipe / motion / safety / DXF 等 leaf concrete 迁移；不要新增 wrapper / alias / bridge 来掩盖 residue。
完成标准：root 不再把 workflow 当 legacy process-runtime-core 提供者；`siligen_domain` 不再充当 foreign super-aggregate；`domain/domain` 不再作为 live bridge-domain 装配根；`siligen_control_application` / `siligen_application` 不再隐藏多 owner concrete；`siligen_module_workflow` 不再静默透传 foreign surface。
验证：targeted build；target link / include graph 检查；`rg` 确认共享文件不再保留上述 residue 的旧 wiring。
停止条件：若某个 worker 的 family 语义未冻结、consumer 未完成 retarget、或 acceptance 无法机械判断，停止并记录为 blocked integration。
输出：共享文件修改清单、消费的 worker 输出、每个 residue 的关闭证据、剩余 blocker。
```
