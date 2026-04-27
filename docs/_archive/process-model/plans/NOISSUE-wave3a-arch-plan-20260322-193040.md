# NOISSUE Wave 3A Architecture Plan - TCP/Gateway Boundary First

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-193040`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave3a-scope-20260322-193040.md`

## 1. 目标结构

`Wave 3A` 只收紧这条链路：

```text
apps/hmi-app
  -> gateway launch contract
  -> apps/control-tcp-server
       -> packages/runtime-host
       -> packages/transport-gateway
            -> packages/application-contracts
```

## 2. 组件职责

### 2.1 `apps/control-tcp-server`

只允许承接：

1. 命令行参数解析
2. `BuildContainer(...)`
3. `BuildTcpFacadeBundle(...)`
4. `TcpServerHost` 启停
5. 进程生命周期、信号与日志 boot marker

禁止承接：

1. `TcpCommandDispatcher` 直接构造
2. TCP session / server / JSON envelope 解析实现
3. `control-core/modules/control-gateway/src/*` 或同类 legacy internal include

### 2.2 `packages/transport-gateway`

继续作为唯一实现 owner 承接：

1. JSON envelope codec
2. TCP session / accept loop
3. dispatcher 注册点
4. facade DTO 映射
5. app 侧 builder/host 公共头

公开构建接口只允许：

1. `siligen_transport_gateway`
2. `siligen_transport_gateway_protocol`

以下 target 在 `Wave 3A` 进入终态：

1. `siligen_control_gateway_tcp_adapter`
2. `siligen_tcp_adapter`
3. `siligen_control_gateway`

结论：以上 3 个 alias 不是“继续保留的兼容壳”，而是需要删除并通过测试防回流的 residual。

### 2.3 `apps/hmi-app`

继续只通过显式 launch contract 消费 gateway：

1. `SILIGEN_GATEWAY_LAUNCH_SPEC`
2. app 默认 `config/gateway-launch.json`
3. `SILIGEN_GATEWAY_EXE` + `ARGS/CWD/PATH/ENV_JSON`

优先级固定为：

1. 显式 `SILIGEN_GATEWAY_LAUNCH_SPEC`
2. app 默认 `gateway-launch.json`
3. env 组装契约

本阶段不新增新的 fallback 来源。

## 3. 实施批次

### Batch 1：gateway canonical target 收口

1. 从 `packages/transport-gateway/CMakeLists.txt` 删除 3 个 legacy alias target。
2. 把兼容测试改为验证“canonical target 存在，legacy alias 不得回流”。
3. 更新 gateway README 与相关 architecture 文档，消除口径冲突。

### Batch 2：app shell / HMI contract gate 收紧

1. 复用并收紧 `control-tcp-server` 的薄入口静态门禁。
2. 为 HMI launch contract 增加优先级与坏 spec 场景单测。
3. 保持 app 层行为不变，只增加防回流证据。

### Batch 3：阶段文档归并

1. 落盘 `Wave 3A` scope / arch / test
2. 在 closeout 前统一 README / architecture / test 口径

## 4. 失败模式与补救

1. 失败模式：删除 alias 后暴露隐藏 consumer
   - 补救：先跑 `transport-gateway` compatibility test，再串行跑 `build.ps1 -Profile CI -Suite apps`
2. 失败模式：app shell 门禁过严，误伤正常 wiring
   - 补救：只校验“不得出现 internal include / dispatcher 直连 / legacy alias”，不限制 `BuildContainer(...)` 与 `TcpServerHost` 合法调用
3. 失败模式：HMI launch contract 又回到硬编码 exe 路径
   - 补救：用默认 spec 和 env fallback 的优先级测试锁死当前合同
4. 失败模式：文档仍存在新旧两套结论
   - 补救：把 `canonical-paths`、gateway README、phase 文档统一到“alias 已删除，不得回流”

## 5. 回滚策略

1. alias 清理失败时，只回滚 gateway alias 清理批次，不动 HMI launch contract 测试。
2. HMI contract 测试若暴露既有问题，只回滚新增测试，不回滚 gateway alias 清理。
3. 任一批次若引起 `apps` / `packages` suite 新失败，按批次独立回滚，不跨批次混回。
