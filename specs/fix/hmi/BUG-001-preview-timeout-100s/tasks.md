---
description: "Task list for HMI 胶点预览超时窗口调整"
---

# Tasks: HMI 胶点预览超时窗口调整

**Input**: 设计文档来自 `D:\Projects\SiligenSuite\specs\fix\hmi\BUG-001-preview-timeout-100s\`  
**Prerequisites**: `plan.md`、`spec.md`、`research.md`、`data-model.md`、`contracts\`、`quickstart.md`

**Tests**: 本特性在 `plan.md` 与 `quickstart.md` 中已经显式冻结了 Python 定向回归，因此每个用户故事都包含先行测试任务。  
**Organization**: 任务按用户故事分组，确保 `US1` 和 `US2` 可以按“更长等待成功展示预览”与“真正超时才失败且文案正确”两个增量独立实施和验证。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 可并行执行（不同文件、无未完成前置依赖）
- **[Story]**: 任务对应的用户故事（`US1`、`US2`）
- 每条任务都包含可直接落地的精确文件路径

## Phase 1: Setup (Shared Timeout Scaffolding)

**Purpose**: 为自动预览链路建立共享的超时策略锚点和模块级测试支撑

- [X] T001 在 `D:\Projects\SiligenSuite\modules\hmi-application\application\hmi_application\preview_session.py` 定义 DXF 打开自动预览专用的 `300.0s` timeout policy 常量与 worker 作用域注释
- [X] T002 [P] 在 `D:\Projects\SiligenSuite\modules\hmi-application\tests\unit\test_preview_session.py` 建立可断言 prepare/snapshot timeout 传参的 worker fake 与测试夹具

---

## Phase 2: Foundational (Blocking Preview Timeout Surface)

**Purpose**: 固定所有用户故事共用的 preview timeout override surface

**CRITICAL**: 本阶段完成前，不得开始任何用户故事实现

- [X] T003 [P] 在 `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py` 添加 preview timeout override 和默认值保留边界的协议回归
- [X] T004 在 `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\client\protocol.py` 为 `dxf_prepare_plan(...)`、`dxf_preview_snapshot(...)` 和 `dxf_preview_snapshot_with_error_details(...)` 增加显式 timeout override 参数
- [X] T005 在 `D:\Projects\SiligenSuite\modules\hmi-application\application\hmi_application\preview_session.py` 把新的 override surface 限定给 `PreviewSnapshotWorker`，不向 runtime resync 或其他命令默认扩散

**Checkpoint**: preview timeout override surface 已冻结，后续用户故事只需要在该边界上分别落成功路径与失败路径

---

## Phase 3: User Story 1 - 打开较慢图纸时继续等待预览 (Priority: P1) 🎯 MVP

**Goal**: 让 DXF 打开自动预览链路在 prepare/snapshot 处理时间大于 `15s` 但小于 `300s` 时继续等待并成功展示预览

**Independent Test**: 打开一个预览生成时间大于 `15s` 且小于 `300s` 的有效 DXF，验证自动预览链路不会在 `15.0s` 时提前失败，并能继续展示 authority preview payload

### Tests for User Story 1

> 先写这些回归，并确认它们在实现前失败

- [X] T006 [P] [US1] 在 `D:\Projects\SiligenSuite\modules\hmi-application\tests\unit\test_preview_session.py` 添加 `PreviewSnapshotWorker.run()` 对 `dxf.plan.prepare` 和 `dxf.preview.snapshot` 都使用 `300.0s` 的回归
- [X] T007 [P] [US1] 在 `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_main_window.py` 添加自动预览成功路径回归，锁定返回 authority payload 后的渲染与状态更新不受更长等待预算影响

### Implementation for User Story 1

- [X] T008 [US1] 在 `D:\Projects\SiligenSuite\modules\hmi-application\application\hmi_application\preview_session.py` 将 `PreviewSnapshotWorker.run()` 改为调用 `dxf_prepare_plan(..., timeout=300.0)` 和 `dxf_preview_snapshot(..., timeout=300.0)`
- [X] T009 [US1] 在 `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\client\protocol.py` 实现 override-aware 的 preview prepare/snapshot 调用，同时保持成功 payload 解析与现有 authority 字段语义不变

**Checkpoint**: 到这里，`US1` 应能独立证明“自动预览链路不再在 15 秒时过早失败”

---

## Phase 4: User Story 2 - 仅在真正超出等待上限时才报超时 (Priority: P2)

**Goal**: 让自动预览链路只在真实超过 `300s` 时才返回 timeout，并确保用户可见错误明确反映 `300.0s`，同时保留非 timeout 失败原语义

**Independent Test**: 打开一个超过 `300s` 的有效 DXF，验证 timeout 只在完整等待窗口后出现且提示包含 `300.0s`；再验证非 timeout 错误仍按原文透传

### Tests for User Story 2

> 先写这些回归，并确认它们在实现前失败

- [X] T010 [P] [US2] 在 `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_main_window.py` 添加 timeout 失败显示 `Request timed out (300.0s)` 且非 timeout 失败保留原始 detail 的 UI 回归
- [X] T011 [P] [US2] 在 `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py` 添加 runtime resync `dxf_preview_snapshot_with_error_details(...)`、`dxf_preview_confirm(...)` 和 `dxf_job_start(...)` 仍保留既有默认预算的边界回归

### Implementation for User Story 2

- [X] T012 [US2] 在 `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\client\tcp_client.py`、`D:\Projects\SiligenSuite\modules\hmi-application\application\hmi_application\preview_session.py` 和 `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py` 保持 transport-originated timeout 文本原样透传，并确保 `300.0s` 只出现在 worker 驱动的 DXF 打开自动预览失败链路

**Checkpoint**: 到这里，`US2` 应能独立证明“只有真正超过 300 秒才会 timeout，且用户看到的新阈值与真实等待预算一致”

---

## Phase 5: Polish & Cross-Cutting Concerns

**Purpose**: 跑定向回归与根级验证，确认 timeout 范围没有扩散

- [X] T013 [P] 运行 `D:\Projects\SiligenSuite\specs\fix\hmi\BUG-001-preview-timeout-100s\quickstart.md` 中的定向 Python 回归：`python -m pytest .\modules\hmi-application\tests\unit\test_preview_session.py -q`、`python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q`、`python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q`
- [X] T014 运行 `D:\Projects\SiligenSuite\specs\fix\hmi\BUG-001-preview-timeout-100s\quickstart.md` 中的根级验证命令：`powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite all` 和 `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite all`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: 无依赖，可立即开始
- **Phase 2 (Foundational)**: 依赖 Phase 1，阻塞全部用户故事
- **Phase 3 (US1)**: 依赖 Phase 2，是建议 MVP
- **Phase 4 (US2)**: 依赖 Phase 2；因与 `US1` 共享 `protocol.py` 与 `preview_session.py`，建议在 `US1` 稳定后合入
- **Phase 5 (Polish)**: 依赖全部目标用户故事完成

### User Story Dependencies

- **User Story 1 (P1)**: 只依赖 Foundational phase，可独立交付为 MVP
- **User Story 2 (P2)**: 只依赖 Foundational phase，但与 `US1` 共享相同 timeout owner 文件，推荐按 `US1 -> US2` 顺序落地以减少冲突

### Within Each User Story

- 测试任务必须先写并先失败，再进入对应实现任务
- `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\client\protocol.py` 是 `US1` 与 `US2` 共享边界文件，应在同一 timeout policy 下连续修改
- `D:\Projects\SiligenSuite\modules\hmi-application\application\hmi_application\preview_session.py` 是 worker owner 文件，`US1` 与 `US2` 都依赖它来限定 300 秒作用范围
- `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py` 只负责消费错误，不应重新定义 timeout 数值

### Parallel Opportunities

- Phase 1 的 `T001` 与 `T002` 可并行
- Phase 2 的 `T003` 可与 `T004` 并行准备，但 `T005` 依赖 `T004`
- `US1` 的 `T006` 与 `T007` 可并行
- `US2` 的 `T010` 与 `T011` 可并行
- Phase 5 的 `T013` 完成后再执行 `T014`

---

## Parallel Example: User Story 1

```text
T006 + T007
```

## Parallel Example: User Story 2

```text
T010 + T011
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. 完成 Phase 1: Setup
2. 完成 Phase 2: Foundational
3. 完成 Phase 3: User Story 1
4. 按 `US1` 的 Independent Test 验证自动预览链路是否已经摆脱 `15.0s` 早超时
5. 仅在 `US1` 稳定后再继续 `US2`

### Incremental Delivery

1. 先完成 Setup + Foundational，固定 preview timeout override surface
2. 交付 `US1`，解决“有效 DXF 在 15-300 秒之间仍能成功预览”
3. 交付 `US2`，解决“只有真正超过 300 秒才 timeout 且文案正确”
4. 执行 Polish，确认 timeout 范围没有扩散到 resync 或其他命令

### Suggested MVP Scope

1. 建议 MVP 仅包含 `US1`
2. `US2` 复用 `US1` 建立的 override surface 与 worker timeout policy
3. 若 `US1` 仍未稳定，不应提前扩大到 UI timeout 文案和其他命令边界验证

---

## Notes

- `[P]` 仅表示可并行，不表示可以跳过 preview timeout surface 的前置冻结
- 本特性不要求修改 `D:\Projects\SiligenSuite\shared\contracts\application\commands\dxf.command-set.json` 的正式 schema
- `300.0s` 只属于“打开 DXF 自动预览”链路，不应被默认扩散到 runtime resync、`dxf.preview.confirm`、`dxf.job.start`、`connect` 或 `dxf.load`
- 用户可见 timeout 文案必须继续源自 `TcpClient.send_request(...)` 的真实 timeout 值
- 分支必须持续保持 `<type>/<scope>/<ticket>-<short-desc>` 合规
