# NOISSUE Wave 3B Architecture Plan - Gateway Config Boundary + Preview Consumer Cleanup

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-200303`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave3b-scope-20260322-200303.md`

## 1. 目标结构

```text
apps/control-tcp-server
  -> BuildContainer(resolved_config_path)
  -> BuildTcpFacadeBundle(...)
  -> TcpServerHost{port}
       -> TcpCommandDispatcher(..., IConfigurationPort)

apps/hmi-app
  -> CommandProtocol.dxf_preview_snapshot / confirm
  -> DispensePreviewGate

packages/engineering-contracts
  -> PreviewArtifact canonical consumer contract
```

## 2. 组件职责

### 2.1 `packages/transport-gateway`

1. 只承接协议、会话、dispatcher、facade 映射。
2. 不再保存、默认化或回退 `config_path`。
3. 只通过 `IConfigurationPort` 读取需要的配置事实。

### 2.2 `apps/control-tcp-server`

1. 继续负责命令行配置输入与解析。
2. 继续在进入 gateway 前完成 `resolved_config_path` 解析并传给 `BuildContainer(...)`。
3. 不把解析后的路径再向 gateway 公共接口透传。

### 2.3 `apps/hmi-app` / `packages/engineering-contracts`

1. HMI 运行时 preview gate 只走 gateway snapshot/confirm。
2. preview artifact contract 的 consumer 断言改为 canonical `PreviewArtifact`，不再借 HMI 本地 dataclass 做兼容层。
3. `preview_client` 删除后，不再保留 HMI 本地 preview fallback。

## 3. 失败模式与补救

1. 失败模式：gateway 删除 `config_path` 后暴露隐藏 consumer
   - 补救：先跑 `transport-gateway` compatibility test，再跑 `build.ps1 -Profile CI -Suite apps`
2. 失败模式：删除 `preview_client` 导致 contract gate 或 HMI 单测断裂
   - 补救：把 preview fixture consumer 统一改到 `PreviewArtifact`，不再让测试依赖 HMI 包路径
3. 失败模式：文档仍把 HMI 本地 preview 当成活跃链路
   - 补救：仅更新当前事实文档，不改历史 review 证据

## 4. 回滚边界

1. gateway config 合同收窄回滚单元：
   - `packages/transport-gateway`
   - `apps/control-tcp-server/main.cpp`
2. preview residual 删除回滚单元：
   - `apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/*`
   - `apps/hmi-app/tests/unit/test_preview_client.py`
   - `packages/engineering-contracts/tests/test_engineering_contracts.py`
