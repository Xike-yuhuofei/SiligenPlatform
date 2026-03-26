# NOISSUE Wave 3B Scope - Config Contract Tightening + Preview Residual Removal

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-200303`
- 上游收口：`docs/process-model/plans/NOISSUE-wave3a-closeout-20260322-194649.md`

## 1. 阶段目标

1. 收紧 `packages/transport-gateway` 的 `config_path` 合同面，使 gateway 不再承接配置路径解析语义。
2. 删除 HMI 本地 `preview_client` residual，使 HMI 运行时预览链路只剩 gateway preview gate。
3. 把 README / architecture / compatibility test 统一到新的 canonical owner 口径。

## 2. in-scope

1. `packages/transport-gateway`
2. `apps/control-tcp-server`
3. `packages/engineering-contracts`
4. `apps/hmi-app` 中与 `preview_client` 删除直接相关的测试与依赖审计
5. `docs/architecture/`、`docs/process-model/plans/` 中与以上边界直接相关的文档

## 3. out-of-scope

1. `control-cli` 其余 `cwd`/path 旁路治理
2. `runtime-host` 配置解析 owner 再设计
3. HMI 生产页 preview UI/交互重构
4. `dxf-pipeline` 全部 legacy import shim 的仓外兼容窗口处理

## 4. 完成标准

1. gateway 公共头与实现中不再暴露 `config_path` 合同面。
2. `control-tcp-server` 仍保持薄入口，但配置解析责任明确停留在 app/runtime-host 边界。
3. `apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/preview_client.py` 与对应单测删除后，仓内活跃 consumer 不再依赖它。
4. `engineering-contracts` 的 preview contract gate 改为只验证 canonical preview artifact consumer。

## 5. 阶段退出门禁

1. `python packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
2. `python packages/engineering-contracts/tests/test_engineering_contracts.py`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File apps/hmi-app/scripts/test.ps1`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -FailOnKnownFailure`
6. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -FailOnKnownFailure`
7. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -FailOnKnownFailure`
