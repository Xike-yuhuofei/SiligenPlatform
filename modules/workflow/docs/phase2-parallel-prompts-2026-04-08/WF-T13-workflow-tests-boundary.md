# WF-T13 Workflow Tests Boundary

- Task type: parallel-worker
- Target residues: `WF-R027`
- Depends on: `WF-T11` and `WF-T12` green, plus relevant integration by `WF-T00`
- Allowed scope: `modules/workflow/tests/canonical/**`
- Forbidden scope: `modules/workflow/domain/**`; `modules/workflow/application/**`; `apps/**`; `modules/runtime-execution/**`

## Prompt

```text
你负责 `WF-T13 Workflow Tests Boundary`。
目标 residue：`WF-R027`。
只处理 workflow canonical tests 的边界收口：tests 不再引入 `domain/process-core/include`、`domain/motion-core/include`、`runtime-execution` concrete/public、`dxf-geometry` / `job-ingest` public 作为 workflow 单测依赖。
你不是集成器，不得修改 workflow 实现代码或 apps / `runtime-execution`。
禁止事项：不要为了让测试通过而重新引入 foreign owner include；不要新增 wrapper test helper 掩盖边界 drift。
完成标准：`WF-R027` 的 acceptance 可机械通过。
验证：targeted test build；`rg` 确认 tests/canonical 不再包含上述 foreign include roots / link targets。
停止条件：若 workflow canonical contract 尚未稳定或必须回改实现层，停止并把挂点交给 `WF-T00`。
输出：修改文件清单、验证结果、仍需仓库级跨 owner smoke / 端到端证明的行为列表。
```
