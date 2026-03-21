# Findings

## P0

1. 运行模式（生产/空跑）切换后未使预览签核失效，存在“界面显示空跑但后端按生产计划执行”的安全风险（阻断）。
- 文件：
  - apps/hmi-app/src/hmi_client/ui/main_window.py:685
  - apps/hmi-app/src/hmi_client/ui/main_window.py:2172
  - apps/hmi-app/src/hmi_client/ui/main_window.py:2371
  - apps/hmi-app/src/hmi_client/client/protocol.py:293
  - packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:1612
- 风险：
  - 预览计划在生成时绑定 dry_run 参数（_generate_dxf_preview 读取当前模式并参与 plan.prepare）。
  - 启动时仅提交 plan_id + plan_fingerprint，不再提交运行模式。
  - UI 的模式切换仅更新按钮文案（生产按钮 toggled 绑定 _update_start_button_state），未触发 mark_input_changed 及计划失效。
  - 结果是：若先在“生产模式”签核预览，再切到“空跑”直接启动，UI 显示“空跑运行中”，但后端执行的仍是生产计划，存在误点胶风险。
- 触发条件：
  - 默认生产模式下生成并确认预览。
  - 用户切换到空跑模式但不刷新预览。
  - 直接点击启动。
- 建议修复：
  - 把运行模式纳入“预览输入参数变更”集合；模式切换必须触发预览状态 STALE、清空 current_plan_id/current_plan_fingerprint，强制重新预览并重新确认。
  - 启动前增加一致性校验：当前 UI 模式必须与 plan 上下文模式一致，否则拒绝启动。
  - 增加回归用例：prod-preview-confirm -> switch-dryrun -> start 必须被门禁拦截。

## P1

1. 未发现新增 P1 阻断项（限本次抽检范围：HMI 预览门禁、预览确认链路、dxf.job.* 启停链路）。

## P2

1. 文档漂移风险：docs/architecture/dispense-trajectory-preview-scope-baseline-v1.md 强调“预览与执行共享参数语义”，但当前实现未把“运行模式切换”纳入预览失效触发条件，需补齐一致性说明与状态图。

# Open Questions

1. 产品语义是否明确要求“模式切换必定重新签核预览”？若是，当前实现需按阻断缺陷处理；若否，需要明确“模式不影响计划语义”的业务前提并同步到契约文档。
2. dry_run 是否应作为后端 dxf.job.start 的显式请求参数参与最终执行判定？若继续仅依赖 plan，则前端必须保证模式切换导致 plan 失效。

# Residual Risks

1. 当前分支工作树非干净，QA 流程已被门禁中止（见同批次 QA 报告 No-Go），尚未完成完整回归闭环。
2. 已执行的单测与兼容性检查覆盖了协议与门禁主链路，但未覆盖“模式切换后启动”这一关键负向场景。

# 建议下一步（QA 或返工）

1. 先返工修复 P0（模式切换触发预览失效 + 启动一致性校验）。
2. 补充并通过对应单测后，重新执行 ss-qa-regression（完整门禁）再进入下一轮 premerge。
