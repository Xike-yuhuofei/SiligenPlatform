# Doc Sync Review

- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 时间戳：20260322-200303
- 范围：Wave 3B gateway config boundary + preview residual closeout
- 上游收口：`docs/process-model/plans/NOISSUE-wave3a-closeout-20260322-194649.md`

## 1. 本次同步的文件

1. `apps/hmi-app/DEPENDENCY_AUDIT.md`
   - 把 HMI 依赖边界从“仍有本地 preview residual”更新为“运行时主链路只走 gateway preview gate”。
2. `docs/architecture/dxf-pipeline-strangler-progress.md`
   - 把 HMI 预览口径改成 gateway snapshot/confirm 主链路，不再把本地 preview shim 当现状。
3. `docs/architecture/dxf-pipeline-final-cutover.md`
   - 记录 HMI 运行时 preview 已从本地 preview bridge 切到 gateway 主链路。
4. `docs/architecture/redundancy-candidate-audit.md`
   - 将 `dxf-pipeline` 和 `apps/dxf-editor-app` 的判定依据更新到当前事实。
5. `packages/engineering-contracts/README.md`
   - 把 preview JSON consumer 从 HMI 本地 shim 改为 canonical consumer / HMI preview gate 辅助校验。
6. `packages/transport-gateway/README.md`
   - 明确 gateway 不负责 workspace config 路径解析，配置解析 owner 固定在 app/runtime-host 边界。

## 2. 代码与测试写集

1. `apps/control-tcp-server/main.cpp`
2. `packages/transport-gateway/include/siligen/gateway/tcp/tcp_server_host.h`
3. `packages/transport-gateway/src/tcp/TcpCommandDispatcher.h`
4. `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp`
5. `packages/transport-gateway/src/wiring/TcpServerHost.cpp`
6. `packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
7. `packages/engineering-contracts/tests/test_engineering_contracts.py`
8. 删除：
   - `apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/__init__.py`
   - `apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/preview_client.py`
   - `apps/hmi-app/tests/unit/test_preview_client.py`

## 3. 新增治理工件

1. `docs/process-model/plans/NOISSUE-wave3b-scope-20260322-200303.md`
2. `docs/process-model/plans/NOISSUE-wave3b-arch-plan-20260322-200303.md`
3. `docs/process-model/plans/NOISSUE-wave3b-test-plan-20260322-200303.md`
4. `docs/process-model/plans/NOISSUE-wave3b-closeout-20260322-200303.md`

## 4. 本次同步的原因

1. 如果不更新文档，工作区会同时存在两套冲突口径：
   - 代码已删除 HMI 本地 preview integration shim
   - 文档仍把它当现行 HMI consumer
2. gateway 公共接口已经去掉 `config_path` 合同面，README 与 architecture 文档必须同步收口。
3. `Wave 3B` 验证已落盘到 `integration/reports/verify/wave3b-closeout/`，当前文档需要与已验证终态一致。

## 5. 刻意未改的内容

1. 未改 `control-cli` 其余 path/resolver 文档，因为这不在 `Wave 3B` scope。
2. 未改历史 review / 历史 closeout 文档，因为它们属于证据归档，不应回写。
3. 未把 `dxf-pipeline` 的仓外兼容窗口直接判定为结束，因为这需要后续独立阶段处理。

## 6. 结论

1. 当前事实文档、README、compatibility test 与实现终态已经对齐到同一结论：
   - gateway 不再暴露 `config_path`
   - HMI 不再依赖本地 preview shim
   - preview contract gate 只验证 canonical preview consumer
2. 本轮文档同步足以支撑 `Wave 3B closeout` 与后续 `Wave 3C` 规划。
