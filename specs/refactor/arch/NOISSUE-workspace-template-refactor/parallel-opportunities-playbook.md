# Parallel Opportunities 落地手册

## 1. 目的

本手册将当前特性 `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor` 的 Parallel Opportunities 方案固化为可重复执行的文档流程。

适用场景：

- 已经完成 `spec.md`、`plan.md`、`tasks.md`，并准备进入并行执行阶段。
- `tasks.md` 中存在可并行的 `[P]` 任务。
- 需要把并行执行编排沉淀为文档资产，后续再收敛为项目内 skill，并接入 Speckit 工作流。

本手册覆盖范围从创建 `Prompts/` 目录开始，一直到：

- Phase 级并行 prompt 生成完成
- 子任务收口协议完成
- Phase 汇总面就绪
- 主线程 Phase 结束检查触发规则明确
- 后续 Skill 化接入路径明确

## 2. 当前目标产物

当前方案最终应产出以下资产：

- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\Prompts\README.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\Prompts\00-Multi-Task-Session-Summary-Protocol.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\Prompts\Phase-XX-*/Tasks-Execution-Summary.md`
- `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\Prompts\Phase-XX-*/T###.md`

其中职责边界固定如下：

- `tasks.md` 只由主线程维护。
- `T###.md` 是分发给并行子任务的独立 prompt。
- `Tasks-Execution-Summary.md` 是某个 Phase 的结果汇总面。
- `00-Multi-Task-Session-Summary-Protocol.md` 是所有子任务必须遵守的统一收口协议。

## 3. 前置条件

开始之前必须确认以下事实：

1. 当前特性目录已经固定，且不要与其它 feature 目录混用。
2. `tasks.md` 已存在，并且任务顺序、Phase 分组、`[P]` 标记已经稳定。
3. 已完成任务不应重复生成 prompt。
4. 主线程已经决定采用“按 Phase 推进”的执行方式，而不是全量跨 Phase 并发。
5. 主线程接受以下约束：
   - 子任务不得直接修改 `tasks.md`
   - 子任务不得提前触发 Phase 结束检查
   - 某个 Phase 的结束检查必须等待用户明确指令

如果后续将该流程 Skill 化，还应额外确认：

1. 目标 skill 应归档到 `.agents/skills/`。
2. 命名遵循项目内 `ss-` 前缀约定。
3. 该 skill 的定位应为 Speckit 的并行执行编排扩展，而不是替代 `speckit-tasks` 或 `speckit-implement`。

## 4. 总体设计原则

本方案采用以下固定原则：

1. 一次只推进一个 Phase。
2. 一个 Task 对应一份独立 prompt 文档。
3. 一个 Phase 对应一份汇总文档。
4. 子任务必须输出标准化 `SESSION_SUMMARY_BEGIN` / `SESSION_SUMMARY_END` block。
5. 子任务除了业务 scope files 外，只允许修改自己所属 Phase 汇总文档中的预分配 slot。
6. 主线程独占以下面：
   - `tasks.md`
   - 各 Phase 汇总文档中的 `Main Thread Control`
   - 各 Phase 汇总文档中的 `Phase End Check`

不要采用“所有子任务把结果追加到同一个文件末尾”的设计。这样会引入共享尾部写入冲突，不适合并行执行。

## 5. 详细执行步骤

### Step 1. 创建 `Prompts/` 根目录

在特性目录下创建：

`D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\Prompts`

注意事项：

- `Prompts/` 必须位于当前特性目录内，不能放到仓库根目录。
- 该目录只承载当前 feature 的并行执行资产，不能跨 feature 复用。
- 如果目录已存在，应先审查已有内容，避免覆盖其它会话生成物。

### Step 2. 从 `tasks.md` 提取并行任务

读取：

- `spec.md`
- `plan.md`
- `tasks.md`
- 已存在的补充文档，如 `wave-mapping.md`、`module-cutover-checklist.md`、`validation-gates.md`

只提取当前仍未完成的 `[P]` 任务，并按 Phase 分组。

注意事项：

- 不要为已完成的 `[P]` 任务重复生成 prompt。
- 任务是否完成，以当前特性目录内的 `tasks.md` 为准。
- 只识别真实 Task 文件名模式 `T###`，不要把其它以 `T` 开头的文件误判为任务。

### Step 3. 为每个 Phase 创建子目录

按 Phase 创建目录，例如：

- `Prompts\Phase-02-Foundational`
- `Prompts\Phase-03-US1`
- `Prompts\Phase-04-US2`

命名规则：

- 使用稳定目录名，不把完整中文标题放进目录名。
- 目录名应能映射回 `tasks.md` 中的 Phase 顺序。

注意事项：

- 目录名应稳定，便于脚本生成和后续 skill 自动定位。
- 不要把标题中的标点、表情或反引号直接带入目录名。

### Step 4. 创建根级索引文档 `Prompts/README.md`

该文件用于主线程快速查看：

- 一共有多少个并行 prompt
- 每个 Phase 对应哪个目录
- 每个 Phase 包含哪些 Task
- 汇总文档统一叫什么
- 主线程应如何按 Phase 推进

至少应包含：

1. 目录用途说明
2. 只为未完成 `[P]` 任务生成 prompt 的规则
3. `tasks.md` 只由主线程维护的规则
4. 每个 Phase 均有 `Tasks-Execution-Summary.md`
5. Index 表格
6. `total_prompt_count`
7. `phase_summary_file`
8. 协议文件路径

注意事项：

- Phase 标题如果包含反引号，不要整体包裹为代码样式，否则 Markdown 表格会破版。
- 统计 prompt 数量时，只统计 `T###.md`。

### Step 5. 创建统一协议 `00-Multi-Task-Session-Summary-Protocol.md`

该文件定义所有并行子任务统一遵循的收口合同。

必须覆盖：

1. 协议用途
2. 产物所有权边界
3. Worker Rules
4. 状态枚举
5. 写回步骤
6. Mandatory Final Output Contract
7. 主线程消费规则
8. 按 Phase 推进规则

协议中必须明确：

- 子任务结束时既要在会话里输出总结，也要把同一份总结写回所属 Phase 汇总文档。
- 子任务只能替换自己的 slot，不能改动别人的 slot。
- 主线程只有在用户明确通知后，才能启动某个 Phase 的结束检查。

注意事项：

- 协议文件名要排在前面，便于浏览器和文件系统排序。
- 协议中的字段名要固定，不能在不同 Task prompt 中改写。

### Step 6. 为每个 Phase 创建 `Tasks-Execution-Summary.md`

每个 Phase 目录内都创建一份汇总文档，例如：

`D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-workspace-template-refactor\Prompts\Phase-02-Foundational\Tasks-Execution-Summary.md`

该文档至少应包含 3 个区块：

1. `Main Thread Control`
2. `Phase End Check`
3. `Task Slots`

每个 Task 都要预分配一个 slot，例如：

```md
### T007
<!-- TASK_SUMMARY_SLOT: T007 BEGIN -->
_PENDING_SESSION_SUMMARY_
<!-- TASK_SUMMARY_SLOT: T007 END -->
```

注意事项：

- 不要使用“统一追加到文件尾部”的模式。
- 必须使用 slot marker，便于脚本或 agent 精确替换。
- `Main Thread Control` 和 `Phase End Check` 只能由主线程更新。
- 子任务只能修改自己的 slot 内部内容，不能删除 marker。

### Step 7. 为每个并行 Task 创建独立 prompt 文档

每个 Task 生成一份 `T###.md`。

建议固定结构：

1. `Task Card`
2. `Scope Files`
3. `Allowed Coordination Writeback`
4. `Phase Context`
5. `Execution Instructions`
6. `Required Context Files`
7. `Done Definition`
8. `Mandatory Final Output`

其中 `Allowed Coordination Writeback` 必须显式写明：

- `summary_doc`
- `task_section`
- `slot_markers`
- 只允许改自己的 slot 的规则

注意事项：

- Prompt 中必须写死本 Task 唯一允许写回的汇总文件和 slot。
- Prompt 中必须再次强调不得修改 `tasks.md`。
- Prompt 中必须把最终 block 同时用于“会话输出”和“汇总文档回写”。

### Step 8. 将上下文文件显式写入每个 Task prompt

每个 `T###.md` 的 `Required Context Files` 中至少应包含：

- 当前特性目录下的 `spec.md`
- `plan.md`
- `tasks.md`
- 辅助设计文档
- 对应 Phase 的 `Tasks-Execution-Summary.md`
- 本 Task 的业务 scope files

注意事项：

- 只列出必要上下文，避免 prompt 无上限膨胀。
- 共享协调面只能是本 Phase 汇总文档中的本 Task slot，不是整个目录。

### Step 9. 校验交叉引用和统计结果

生成后至少做以下检查：

1. 每个 `T###.md` 都含有 `Allowed Coordination Writeback`
2. 每个 `T###.md` 都引用 `Tasks-Execution-Summary.md`
3. 每个 `T###.md` 都引用统一协议文件
4. 每个 Phase 目录都存在 `Tasks-Execution-Summary.md`
5. `README.md` 中统计的 prompt 数量与实际 `T###.md` 数量一致
6. 文档中不存在空字符、错误路径或重复空行导致的可读性问题

注意事项：

- 如果采用脚本批量生成或回写，必须特别检查转义字符和换行格式。
- 不要让 `Tasks-Execution-Summary.md` 被误计为 Task prompt。

### Step 10. 主线程分发当前 Phase 的 Task prompt

主线程进入某个 Phase 时：

1. 只分发该 Phase 的 `T###.md`
2. 不跨 Phase 并发
3. 等待该 Phase 的 Task 总结逐步写满

注意事项：

- 即使下个 Phase 的 prompt 已生成，也不能提前执行。
- Phase 边界以 `tasks.md` 依赖关系为准，而不是以“当前没人占用”判断。

### Step 11. 子任务执行与写回

每个子任务完成后必须做两件事：

1. 在当前会话输出标准化 `SESSION_SUMMARY_BEGIN` / `SESSION_SUMMARY_END`
2. 将同一份 block 写回所属 Phase 汇总文档中自己的 slot

回写顺序：

1. 重新读取 `Tasks-Execution-Summary.md` 最新内容
2. 定位自己的 slot markers
3. 只替换 markers 之间的内容
4. 保留 markers

注意事项：

- 如果需要重跑，只覆盖自己的旧总结，不新增第二份。
- 不能改动其它 Task slot。
- 不能改动 `Main Thread Control` 或 `Phase End Check`。

### Step 12. 主线程收口当前 Phase

主线程要做的是“观察汇总面”，不是“代替子任务补写总结”。

主线程检查重点：

1. 每个 slot 是否已填入真实总结
2. `status` 是否为 `done`
3. `completion_recommendation` 是否允许标记完成
4. 是否存在越权文件修改
5. 是否存在 `validation.fail` 或 `validation.not_run`
6. 是否存在阻塞项

注意事项：

- 所有 slot 写满，不等于可以自动结束该 Phase。
- Phase 结束检查必须等待用户明确触发。

### Step 13. 等待用户触发 Phase 结束检查

当某个 Phase 的所有 Task 都已完成时，主线程保持以下状态：

- `phase_status: collecting_task_summaries`
- `phase_end_check_requested_by_user: false`
- `phase_end_check_status: not_started`

只有在用户明确告知主线程“启动该 Phase 结束检查”后，主线程才可：

1. 更新 `Main Thread Control`
2. 执行 Phase 结束检查
3. 回写 `Phase End Check`
4. 统一更新 `tasks.md`

注意事项：

- 这是人为门禁，不允许自动跳过。
- 该规则的目的，是确保按 Phase 渐进推进，而不是让流水线自行跨阶段滚动。

### Step 14. 更新 `tasks.md` 并推进下一 Phase

只有在当前 Phase 的结束检查通过后，主线程才可以：

1. 在 `tasks.md` 中标记该 Phase 对应任务完成
2. 处理遗留阻塞或补充说明
3. 进入下一 Phase 的并行分发

注意事项：

- 子任务永远不应修改 `tasks.md`。
- `tasks.md` 的状态变更只能发生在主线程确认收口之后。

## 6. 并行执行注意事项

### 6.1 文件写面隔离

必须同时满足：

- 业务文件按 Task scope 隔离
- 汇总文档按 slot 隔离

如果两个 Task 会改动同一业务文件，则该两个 Task 不能并行。

### 6.2 汇总文档不是共享自由编辑面

`Tasks-Execution-Summary.md` 只是一个“受限共享面”：

- 允许共享文件
- 不允许共享写区

这两点必须同时成立。

### 6.3 子任务失败也必须输出总结

即使任务未完成，也必须输出：

- `status: partial` 或 `blocked`
- 真实阻塞项
- 主线程建议动作

不能静默终止。

### 6.4 不要让 Phase 结束检查与任务执行重叠

Phase 结束检查应发生在：

- 所有 slot 已收口之后
- 用户明确触发之后

不能边执行子任务边做 Phase 终验。

### 6.5 不要依赖旧 feature 目录

本仓库曾出现 prerequisite 脚本仍解析到旧 feature 目录的问题。后续如果要把本方案 Skill 化，必须在脚本层显式校验当前 feature 路径，避免把输出写到旧目录。

## 7. 推荐的主线程操作顺序

1. 读取当前特性目录内的 `tasks.md`
2. 识别未完成的 `[P]` 任务
3. 生成 `Prompts/` 及其下全部产物
4. 校验 prompt 数量、路径和 slot 完整性
5. 只启动当前目标 Phase 的任务分发
6. 等待该 Phase 的 Task 汇总填满
7. 等待用户命令启动该 Phase 结束检查
8. 主线程执行结束检查并更新 `tasks.md`
9. 切换到下一 Phase

## 8. 后续 Skill 化方案

### 8.1 建议 Skill 名称

建议命名为：

`speckit-parallel`

理由：

- 符合项目内 `ss-` 前缀约束
- 语义直指“并行机会编排”
- 不与现有 `speckit-*` skill 冲突

### 8.2 建议在 Speckit 工作流中的位置

推荐位置：

1. `speckit-specify`
2. `speckit-plan`
3. `speckit-tasks`
4. `speckit-parallel`
5. `speckit-implement`

定位说明：

- `speckit-tasks` 负责生成 `tasks.md`
- `speckit-parallel` 负责把 `tasks.md` 中的 `[P]` 任务转化为并行执行资产
- `speckit-implement` 或主线程再基于这些资产执行实现

### 8.3 建议 Skill 目录结构

```text
.agents/skills/speckit-parallel/
├── SKILL.md
├── scripts/
│   ├── generate_parallel_prompts.py
│   ├── validate_parallel_prompt_workspace.py
│   └── refresh_phase_summary_slots.py
└── references/
    ├── prompt-template.md
    ├── phase-summary-template.md
    └── session-summary-protocol-template.md
```

### 8.4 建议 Skill 输入

Skill 运行时至少要消费：

- 当前 feature 目录
- `tasks.md`
- `spec.md`
- `plan.md`
- 可选辅助文档列表
- 当前已完成任务状态

### 8.5 建议 Skill 输出

Skill 应自动生成或更新：

- `Prompts/README.md`
- `Prompts/00-Multi-Task-Session-Summary-Protocol.md`
- `Prompts/Phase-XX-*/Tasks-Execution-Summary.md`
- `Prompts/Phase-XX-*/T###.md`

### 8.6 建议 Skill 行为边界

Skill 不应：

- 直接修改业务代码
- 直接执行实现任务
- 自动勾选 `tasks.md`
- 自动启动 Phase 结束检查

Skill 应只负责：

- 建模并行执行面
- 生成 prompt 资产
- 校验并发写面是否安全
- 为主线程提供可分发的任务文档

### 8.7 建议与 Speckit 的接入方式

优先建议两种方式之一：

1. 作为独立 skill，由主线程在 `speckit-tasks` 之后显式调用
2. 作为 `speckit-tasks` 的后置扩展 hook，在确认当前 feature 需要并行执行时生成 `Prompts/`

更稳妥的初始方案是第一种，因为：

- 边界更清晰
- 失败回滚面更小
- 不会影响现有 `speckit-tasks` 主路径

## 9. Skill 化前的最小验收标准

后续把本方案正式开发为 skill 之前，至少要满足：

1. 可以从 `tasks.md` 稳定识别未完成 `[P]` 任务
2. 可以稳定按 Phase 生成目录和文件
3. 可以稳定为每个 Task 分配唯一 slot
4. 可以检测并阻止共享业务文件写冲突
5. 可以检测 README 中的统计结果是否正确
6. 可以检测 prompt 中是否缺少协议、汇总文档或 slot 配置

## 10. 当前手工方案与未来 Skill 方案的关系

当前文档化方案不是临时说明，而是未来 skill 的真实工作流蓝本。

后续 Skill 开发时应遵循：

- 先保持文档流程与手工执行结果一致
- 再把重复性步骤脚本化
- 最后再把脚本封装进 `.agents/skills/speckit-parallel/`

不要先写 skill，再倒推文档。正确顺序是先固定工作流，再把工作流程序化。

