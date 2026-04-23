# WF-T03 Retired Management Family

- Historical note: 本文件保留 `2026-04-08` 的执行提示快照，仅供追溯。
- Historical note: 下文若出现已退役历史管理面的旧名称或旧路径，请按历史执行语境理解。

- Task type: parallel-worker
- Target residues: `WF-R002`, `WF-R008`, `WF-R010`
- Depends on: `WF-T00` shared target freeze
- Allowed scope: `modules/workflow/domain/process-core/**`; deleted historical-management concrete under workflow domain/application
- Forbidden scope: shared single-writer files; `apps/**`; `modules/runtime-execution/**`

## Prompt

```text
你负责 `WF-T03 Retired Management Family`。
目标 residue：`WF-R002`, `WF-R008`, `WF-R010`。
只处理 workflow 内已退役历史管理面的 leaf 级 clean-exit：`siligen_process_core` 不再编译该 family concrete，workflow domain 不再编译对应 validation/activation service，workflow application 不再编译对应 CRUD / bundle usecases。
你不是集成器，不得修改共享文件：`CMakeLists.txt`（repo root）, `modules/workflow/CMakeLists.txt`, `modules/workflow/domain/CMakeLists.txt`, `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/application/CMakeLists.txt`。
禁止事项：不要改 valve / motion / system / DXF family；不要修改 apps 或 `runtime-execution` consumer；不要通过新增 wrapper 保留已退役管理面的 owner drift。
完成标准：`WF-R002`, `WF-R008`, `WF-R010` 的 acceptance 可机械通过。
验证：targeted build；`rg` 或构建日志确认上述已删除 `.cpp` / usecase 不再由 workflow target 编译。
停止条件：若必须改共享聚合文件或该退役 family 的 canonical owner 路径未冻结，停止并把挂点交给 `WF-T00`。
输出：修改文件清单、shared-file request、验证结果、未迁净清单。
```
