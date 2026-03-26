# NOISSUE Wave 3A Test Plan - TCP/Gateway Boundary First

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-193040`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave3a-scope-20260322-193040.md`
- 关联 arch：`docs/process-model/plans/NOISSUE-wave3a-arch-plan-20260322-193040.md`

## 1. 验证原则

1. 所有构建与根级 suite 串行执行，不在同一 build root 上并行。
2. 先跑静态门禁，再跑单元/包级验证，最后跑 app build 与根级 suites。
3. 只验证 `Wave 3A` 收紧的边界，不扩展到 `control-runtime` / `control-cli` owner 调整。

## 2. 静态与单元验证

### 2.1 transport-gateway 边界

命令：

```powershell
python packages/transport-gateway/tests/test_transport_gateway_compatibility.py
```

预期：

1. `control-tcp-server` main 仍只通过公开 builder/host wiring。
2. `packages/transport-gateway/CMakeLists.txt` 仍导出 `siligen_transport_gateway` 与 `siligen_transport_gateway_protocol`。
3. `siligen_control_gateway_tcp_adapter`、`siligen_tcp_adapter`、`siligen_control_gateway` 不再出现。

### 2.2 HMI gateway launch contract

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File apps/hmi-app/scripts/test.ps1
```

重点断言：

1. 显式 `SILIGEN_GATEWAY_LAUNCH_SPEC` 优先于 env 组装契约。
2. app 默认 `config/gateway-launch.json` 优先于 env 组装契约。
3. 非对象 `SILIGEN_GATEWAY_ENV_JSON` 被拒绝。
4. 缺少 `executable` 的 spec 文件被拒绝。

## 3. 构建与根级回归

### 3.1 app build

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps
```

预期：

1. `siligen_tcp_server` 成功构建
2. 不因 gateway alias 删除产生 link failure

### 3.2 packages suite

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -FailOnKnownFailure
```

预期：

1. `transport-gateway` compatibility test 通过
2. 不引入新的 `known_failure` 或 `failed`

### 3.3 apps suite

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -FailOnKnownFailure
```

预期：

1. `control-tcp-server-dryrun`
2. `control-tcp-server-help`
3. `control-tcp-server-subdir-canonical-config`
4. `control-tcp-server-compat-alias-rejected`
5. `hmi-app-dryrun`
6. `hmi-app-unit`
7. `hmi-app-offline-smoke`
8. `hmi-app-online-smoke`
9. `hmi-app-online-ready-timeout`

以上全部继续通过。

## 4. 失败判定

以下任一出现即判定 `Wave 3A` 未通过：

1. gateway legacy alias 在实现或测试中重新出现
2. `control-tcp-server` app shell 开始直接依赖 gateway internal 实现
3. HMI launch contract 优先级回退到硬编码 exe 或 build/bin 扫描
4. `apps` / `packages` suite 出现新失败
