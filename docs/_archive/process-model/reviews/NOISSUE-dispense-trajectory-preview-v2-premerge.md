# Premerge Review

## Findings

### P1 - `dxf.preview.snapshot` 在真实 gateway 上未兑现声明契约
- 契约要求 `dxf.preview.snapshot` 返回 `snapshot_id/snapshot_hash/trajectory_polyline/...`，且 `additionalProperties=false`。
- 真实 gateway 直接把 `dxf.preview.snapshot` 转发到 `HandleDxfPlanPrepare`，返回的是 `plan_id/plan_fingerprint/artifact_id/preview_request_signature` 等 `dxf.plan.prepare` 语义，不包含 `trajectory_polyline`，也不使用声明的 3010-3014 错误码。
- HMI 主窗口没有消费 `dxf.preview.snapshot`，而是在拿到 `dxf.plan.prepare` 摘要后继续调用本地 `engineering-data` 预览客户端生成 HTML，预览展示仍不是运行时权威快照。
- 证据：
  - `packages/application-contracts/commands/dxf.command-set.json:151`
  - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:1975`
  - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:1491`
  - `apps/hmi-app/src/hmi_client/ui/main_window.py:1955`
  - `apps/hmi-app/src/hmi_client/ui/main_window.py:1998`
  - `apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/preview_client.py:24`

### P1 - `plan_fingerprint` 不是端到端权威输入，调用方/被调用方语义已经漂移
- 契约把 `plan_fingerprint` 暴露为 `dxf.job.start` 输入，但运行时 `StartJobRequest` 只接收 `plan_id` 和 `target_count`；真实用例层没有接收或校验调用方提交的 fingerprint。
- gateway 只在“请求的 `plan_id` 等于当前缓存 plan_id”时做一次缓存比对；如果传入的是非当前缓存但仍存在的 `plan_id`，fingerprint 不再参与校验，契约门禁退化为 best-effort。
- mock 链路甚至完全忽略 `plan_fingerprint`，只检查 `plan_id`，导致仿真链和真实链对同一字段的约束不同。
- 证据：
  - `packages/application-contracts/commands/dxf.command-set.json:227`
  - `packages/process-runtime-core/src/application/usecases/dispensing/DispensingWorkflowUseCase.h:55`
  - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:1511`
  - `packages/process-runtime-core/src/application/usecases/dispensing/DispensingWorkflowUseCase.cpp:197`
  - `apps/hmi-app/src/hmi_client/tools/mock_server.py:583`

### P1 - `speed_mm_s` 兼容别名只存在于契约和 mock，真实 gateway 不接受
- `dxf.plan.prepare`/`dxf.preview.snapshot` 契约声明接受 `speed_mm_s` 作为 `dispensing_speed_mm_s` 别名。
- 真实 gateway 的 `BuildExecutionRequest` 只读取 `dispensing_speed_mm_s`，`HandleDxfPlanPrepare` 用该结果判定有效速度，因此调用方若按契约传 `speed_mm_s` 会被真实链路拒绝。
- mock 链路却接受 `speed_mm_s`，这进一步放大了仿真/真实链路的输入语义漂移。
- 证据：
  - `packages/application-contracts/commands/dxf.command-set.json:140`
  - `packages/application-contracts/commands/dxf.command-set.json:217`
  - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:436`
  - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:1458`
  - `apps/hmi-app/src/hmi_client/tools/mock_server.py:494`
  - `apps/hmi-app/src/hmi_client/tools/mock_server.py:535`

### P2 - mock 的状态/错误语义未和真实链路对齐
- `dxf.preview.snapshot` 的契约错误码定义为 3010-3014，真实 gateway 实际复用 2894-2897，mock 返回 `-32003/-32004`。
- `dxf.job.status` 契约要求返回 `dry_run`，真实链路从作业上下文返回该值；mock 却从状态查询请求参数读取 `dry_run`，默认会把所有状态查询都报告为 `false`。
- 这意味着测试和联调链路不能作为权威错误/状态语义来源。
- 证据：
  - `packages/application-contracts/commands/dxf.command-set.json:213`
  - `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp:1445`
  - `apps/hmi-app/src/hmi_client/tools/mock_server.py:492`
  - `packages/application-contracts/models/states.json:208`
  - `packages/process-runtime-core/src/application/usecases/dispensing/DispensingWorkflowUseCase.cpp:516`
  - `apps/hmi-app/src/hmi_client/tools/mock_server.py:631`

## Open Questions
- 当前是否允许 `dxf.preview.snapshot` 与 `dxf.plan.prepare` 继续共用一个 provider？如果允许，必须先把契约合并；如果不允许，真实链路需要补齐 snapshot 点集返回。
- `plan_fingerprint` 是否必须成为用例层强校验输入？如果答案是“必须”，现有 `StartJobRequest` 需要扩展。

## Residual Risks
- 现有 `application-contracts`、transport-gateway 和 HMI 协议测试都能通过，但它们没有验证“真实 provider 返回体是否符合 schema”，因此无法阻止上述漂移继续进入主干。

## Suggested Next Steps
1. 先冻结 `dxf.preview.snapshot` 与 `dxf.plan.prepare` 的边界：保留两个命令就让真实链路返回两套不同且完整的 schema；否则删除其中一个并收敛到单一契约。
2. 把 `plan_fingerprint` 下沉为用例层强校验输入，并让 mock/真实链路共用同一校验语义。
3. 增加 provider 级契约测试，直接对真实 dispatcher/mock server 的返回体与错误码做 schema 校验，而不是只检查 schema 文件或源码字符串。
