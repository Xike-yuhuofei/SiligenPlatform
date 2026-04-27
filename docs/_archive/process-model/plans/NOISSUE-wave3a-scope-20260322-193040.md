# NOISSUE Wave 3A Scope - TCP/Gateway Boundary First

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-193040`
- 上游收口：`docs/process-model/plans/NOISSUE-wave2b-closeout-20260322-191927.md`

## 1. 阶段目标

本阶段只做 `TCP/Gateway` 先行收口：

1. 收紧 `apps/control-tcp-server` 的 app shell 边界。
2. 统一 `packages/transport-gateway` 的 canonical target 与兼容口径。
3. 冻结 `apps/hmi-app` 的 gateway launch 契约，并补足防回流测试。
4. 把 `Wave 3A` 的实施、验证、回滚口径落盘为 authoritative 文档。

## 2. 明确 in-scope

1. `apps/control-tcp-server`
2. `packages/transport-gateway`
3. `apps/hmi-app` 的 gateway launch 契约与相关单测
4. `docs/architecture/`、`docs/process-model/plans/` 中与以上边界直接相关的文档

## 3. 明确 out-of-scope

1. `control-runtime` / `control-cli` 的 owner 切分或命令面重构
2. `runtime-host` / `process-runtime-core` / `simulation-engine` 的功能迁移
3. `config/`、`data/`、`examples/`、`integration/` 的默认路径再切换
4. `packages/transport-gateway` 的物理目录迁移
5. 新协议 schema、应用契约字段和 wire shape 变更

## 4. 当前需要解决的阻断

1. `packages/transport-gateway/CMakeLists.txt` 当前仍导出：
   - `siligen_control_gateway_tcp_adapter`
   - `siligen_tcp_adapter`
   - `siligen_control_gateway`
2. 但以下工件已经把这些 alias 定义为“已删除，不得回流”：
   - `docs/architecture/alias-consumer-audit.md`
   - `docs/architecture/control-core-fallback-removal.md`
   - `docs/architecture/compatibility-shell-audit.md`
   - `packages/transport-gateway/README.md`
3. `packages/transport-gateway/tests/test_transport_gateway_compatibility.py` 当前仍错误要求 alias 存在，导致测试口径与审计口径冲突。

## 5. 阶段完成标准

1. `control-tcp-server` 继续保持薄入口，且静态门禁能阻止 app 层吸入 gateway 内部实现。
2. gateway 只保留 `siligen_transport_gateway`、`siligen_transport_gateway_protocol` 两个 canonical target。
3. HMI 只通过显式 launch contract 拉起 gateway，contract 优先级有单测覆盖。
4. 相关 README / 架构文档 / 兼容测试口径一致，不再出现“文档说已删除、代码却仍导出”的冲突。

## 6. 阶段退出门禁

1. `python packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File apps/hmi-app/scripts/test.ps1`
3. `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -FailOnKnownFailure`
5. `powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -FailOnKnownFailure`

## 7. 回滚边界

1. gateway alias 清理回滚单元：
   - `packages/transport-gateway/CMakeLists.txt`
   - `packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
   - 相关 README / architecture 文档
2. HMI launch contract 测试回滚单元：
   - `apps/hmi-app/tests/unit/test_gateway_launch.py`
3. 若 app build 或 `apps/packages` suite 因边界收紧出现新失败，整批回滚到 `Wave 2B` closeout 口径。
