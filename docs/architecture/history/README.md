# Architecture History

本目录承接架构专项审计、阶段性评审、过程性 progress 和 closeout 记录。

读取规则：

1. 先看 `docs/architecture/README.md` 中列出的正式规则和正式契约
2. 只有在需要追溯某次专项分析、边界审查或收口过程时，才进入本目录
3. 若本目录中的结论仍然有效且会被持续依赖，应上收至正式区

当前分组：

- `module-boundary/`
- `audits/`
- `baselines/`
- `progress/`
- `closeouts/`

其中：

- `baselines/`：旧阶段的指标快照、历史基线与已退出正式区的 baseline 文档
- `closeouts/`：legacy 删除、cutover、专项 closeout 的执行记录
- `progress/`：strangler、实施清单、阶段性推进记录
