---
name: speckit-parallel
description: 基于当前 feature 的 spec.md、plan.md、tasks.md 识别未完成 [P] 任务并按 Phase 生成/校验 Prompts 并行执行资产（README、统一协议、Phase 汇总、T### Prompt）。在 speckit-tasks 之后、speckit-implement 之前使用。
---

## User Input

```text
$ARGUMENTS
```

优先把 `$ARGUMENTS` 解释为 feature 目录绝对路径。未提供时，默认使用当前工作目录下最近一次 active 的 `specs/<type>/<scope>/<ticket>-<desc>` 目录。

## 与 Speckit 工作流衔接

执行顺序固定为：

1. `speckit-specify`
2. `speckit-plan`
3. `speckit-tasks`
4. `speckit-parallel`（本技能）
5. `speckit-implement`

禁止把本技能用于替代 `speckit-tasks` 或直接执行实现任务。

## 强制约束

1. 在任何生成动作前，先运行：

```powershell
scripts/validation/resolve-workflow-context.ps1
```

2. 高风险命令必须通过：

```powershell
scripts/validation/invoke-guarded-command.ps1 -Command "<your command>"
```

3. `tasks.md` 仅允许主线程维护，子任务不得修改。
4. 只推进当前一个 Phase，不做跨 Phase 并发。
5. 子任务只能改自己在 `Tasks-Execution-Summary.md` 中的 slot。
6. Phase End Check 必须等待用户明确触发，不能自动执行。

## 执行步骤

1. 读取 feature 目录中的 `spec.md`、`plan.md`、`tasks.md`。
2. 从 `tasks.md` 提取未完成 `[P]` 任务，并按 Phase 分组。
3. 生成或更新 `Prompts/` 资产：
   - `Prompts/README.md`
   - `Prompts/00-Multi-Task-Session-Summary-Protocol.md`
   - `Prompts/Phase-XX-*/Tasks-Execution-Summary.md`
   - `Prompts/Phase-XX-*/T###.md`
4. 校验交叉引用、slot marker、任务统计和协议链接。
5. 进入按 Phase 分发与收口流程。

## 标准命令

在仓库根目录执行：

```powershell
python .agents/skills/speckit-parallel/scripts/generate_parallel_prompts.py --feature-dir "<ABS_FEATURE_DIR>"
python .agents/skills/speckit-parallel/scripts/validate_parallel_prompt_workspace.py --feature-dir "<ABS_FEATURE_DIR>"
```

在 `tasks.md` 有新增/删减 `[P]` 任务但不想全量重生时执行：

```powershell
python .agents/skills/speckit-parallel/scripts/refresh_phase_summary_slots.py --feature-dir "<ABS_FEATURE_DIR>"
```

## 产物契约

每个 `T###.md` 必须显式声明：

1. `Allowed Coordination Writeback`
2. `summary_doc`
3. `task_section`
4. `slot_markers`
5. 禁止修改 `tasks.md`

每个子任务必须同时输出并回写同一份 block：

```text
SESSION_SUMMARY_BEGIN
task_id: T###
status: done|partial|blocked
changed_files:
validation:
risks_or_blockers:
completion_recommendation:
SESSION_SUMMARY_END
```

## 校验清单

执行后至少确认：

1. 每个 `T###.md` 包含 `Allowed Coordination Writeback`
2. 每个 `T###.md` 同时引用统一协议与本 Phase 汇总文档
3. 每个 Phase 目录存在 `Tasks-Execution-Summary.md`
4. `README.md` 中 `total_prompt_count` 与实际 `T###.md` 数量一致
5. 汇总文档 slot marker 完整且可定位

## 参考模板

按需读取：

- `references/prompt-template.md`
- `references/phase-summary-template.md`
- `references/session-summary-protocol-template.md`
