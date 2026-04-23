# WF-T18 Doc Contract Sync

- Task type: serial-finisher
- Target residues: `WF-R028`
- Depends on: `WF-T00` to `WF-T13` complete; trace tasks concluded as needed
- Allowed scope: `modules/workflow/README.md`; `modules/workflow/module.yaml`; workflow `CMakeLists.txt` comments only
- Forbidden scope: no new functional cleanup; no build graph rework disguised as docs

## Prompt

```text
你负责 `WF-T18 Doc Contract Sync`。
目标 residue：`WF-R028`。
这不是前置任务，而是最后的文档与注释对齐任务。只在前面 execution tasks 已经真实关闭对应 residue 后，同步 `README.md`、`module.yaml` 和 workflow 内 CMake 注释，使其与真实 target / include / link 关系一致。
禁止事项：不要在 docs 中提前宣称 bridge-only closeout；不要把尚未完成的 residue 写成已完成；不要借文档修改掩盖未完成的 build graph 问题。
完成标准：`WF-R028` 的 acceptance 可机械通过，且文档表述与 live build graph 一致。
验证：逐条对照已完成任务的 acceptance；检查文档声明与真实 target / include / link 关系。
停止条件：若任一前置 residue 未关闭或 required validation 未全绿，停止，不得更新为“已清洁”口径。
输出：文档修改清单、对应的前置任务证据引用、仍未满足的口径差异。
```
