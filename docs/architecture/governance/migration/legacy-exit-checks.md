# Legacy Exit Checks

更新时间：`2026-03-23`

## 1. 目的

本文件定义“旧目录删除前自动验证”的仓内执行口径。

目标只有两个：

1. 阻止 legacy 引用回流。
2. 为目录级最终删除建立可重复、可在本地门禁中执行的归零验证。

## 2. 自动化入口

- 根级命令：`.\legacy-exit-check.ps1`
- 严格验证：`.\ci.ps1`
- 本地统一门禁：`powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\run-local-validation-gate.ps1`
- GitHub Actions workflow 已禁用：`.github/workflows-disabled/workspace-validation.yml.disabled`
- 报告输出：
  - `integration/reports/legacy-exit/legacy-exit-checks.json`
  - `integration/reports/legacy-exit/legacy-exit-checks.md`
  - 历史 CI 输出目录仍可保留：`integration/reports/ci/legacy-exit/`

## 3. 门禁分层

| 目录/范围 | 门禁类型 | 自动化口径 |
|---|---|---|
| `apps/dxf-editor-app` / `dxf-editor` / `packages/editor-contracts` | 已删除归零门禁 | 目录必须不存在；build/test/CI/源码不得再引用；活动文档仅允许在显式白名单中作为“已删除/历史说明”出现 |
| 顶层 `simulation-engine/` | 已删除归零门禁 | 顶层目录必须不存在；代码和活动文档不得再把旧顶层目录写回执行链路 |
| `hmi-client` | 已删除归零门禁 | 目录必须不存在；代码/自动化不得重新引用 `hmi-client/`；活动文档仅允许在显式白名单中作为“已删除/历史说明”出现 |
| `control-core/modules/shared-kernel` / `control-core/src/domain` / `control-core/src/application` / `control-core/modules/process-core` / `control-core/modules/motion-core` | 已删除归零门禁 | 目录必须不存在；代码/脚本/CMake 不得再把这些 legacy 子树写回执行链路 |
| `dxf-pipeline` | 兼容壳归零门禁 | `dxf-pipeline` 目录、`dxf_pipeline.*` import、`proto.dxf_primitives_pb2`、legacy CLI alias 都不得重新进入代码、自动化或活动文档默认入口 |
| `control-core` 兼容壳 | 兼容例外白名单 | 直接路径、PowerShell 根目录拼接、Python 根目录引用、CMake legacy include/link 只能出现在登记白名单内 |

## 4. 显式白名单

### 4.1 `control-core` 兼容例外

| 规则 | 白名单 | 原因 |
|---|---|---|
| PowerShell 拼接 `control-core` 根目录 | `apps/control-cli/run.ps1`、`apps/control-runtime/run.ps1`、`apps/control-tcp-server/run.ps1`、`tools/build/build-validation.ps1` | 当前 wrapper / build 入口仍需识别兼容产物或兼容源码根 |
| Python 引用 `control-core` 根目录 | `packages/transport-gateway/tests/test_transport_gateway_compatibility.py`、`tools/migration/validate_device_split.py` | 分别用于 legacy anti-regression 测试、device split 迁移校验 |
| 直接 control-core 兼容壳路径 | `packages/transport-gateway/tests/test_transport_gateway_compatibility.py` | 该测试只保留历史 thin-shell / anti-regression 证据，不代表 alias 仍是活跃默认链路 |
| CMake 硬编码 control-core legacy 依赖 | `packages/process-runtime-core/CMakeLists.txt`、`packages/simulation-engine/CMakeLists.txt` | 这些是当前已登记、尚未切完的阻塞项；新增文件一律失败 |

### 4.2 活动文档中的兼容例外

下列文档允许出现 legacy 路径，因为它们承担“兼容说明、发布/回滚、已删除声明”职责：

- `README.md`
- `WORKSPACE.md`
- `docs/onboarding/developer-workflow.md`
- `docs/onboarding/workspace-onboarding.md`
- `docs/runtime/deployment.md`
- `docs/runtime/external-dxf-editing.md`
- `docs/runtime/field-acceptance.md`
- `docs/runtime/long-run-stability.md`
- `docs/runtime/release-process.md`
- `docs/runtime/rollback.md`
- `docs/troubleshooting/canonical-runbook.md`

不在这份白名单里的 root/onboarding/runtime/troubleshooting/validation 文档，只要再出现 legacy 路径，就会被 `legacy-exit-check.ps1` 直接打失败。

### 4.3 历史迁移审计脚本例外

以下脚本允许保留少量 legacy 路径文本，但仅用于历史 residual needle / payoff 校验，不能作为默认入口或实现来源：

- `tools/migration/validate_wave2b_residual_payoff.py`

## 5. 当前已自动化门禁

1. 已删除目录实体是否重新出现。
2. build/test/CI/源码是否重新引用 `dxf-editor`、`apps/dxf-editor-app`、`packages/editor-contracts`。
3. 顶层旧 `simulation-engine/` 路径是否回流。
4. `hmi-client/` 是否重新出现，或其路径是否重新进入代码、自动化与活动文档默认链路。
5. `dxf-pipeline/src`、`dxf-pipeline/proto`、`dxf_pipeline` import、`proto.dxf_primitives_pb2` 与 legacy CLI alias 是否重新进入代码或自动化链路。
6. 已删除的 `control-core/modules/shared-kernel`、`src/domain`、`src/application`、`modules/process-core`、`modules/motion-core` 是否重新出现，或是否被代码/脚本/CMake 重新写回执行链路。
7. `control-core/build/bin`、`control-core/apps/*`、`control-core/src/adapters/tcp`、`control-core/modules/control-gateway` 是否被未登记文件直接引用。
8. 代码、脚本、自动化是否重新把 `control-core/config/machine_config.ini`、`control-core/src/infrastructure/resources/config/files/machine_config.ini` 写回默认 fallback。
9. 代码、脚本、自动化是否重新把 `control-core/data/recipes/*`、`control-core/src/infrastructure/resources/config/files/recipes/schemas/*` 写回默认 fallback。
10. `control-core` 默认库图是否重新注册 `adapters/tcp` 或 `control-gateway`。
11. canonical `apps/*`、`packages/runtime-host`、`packages/process-runtime-core`、`packages/transport-gateway` 是否重新隐式回落到 `${CMAKE_SOURCE_DIR}/src` 或 `${CMAKE_SOURCE_DIR}/third_party`。
12. PowerShell 是否在白名单之外重新拼接 `control-core` 根目录。
13. Python 是否在白名单之外重新依赖 `control-core` 根目录。
14. CMake 是否在白名单之外新增 `control-core` include/link/source 依赖。
15. 活动文档是否在白名单之外重新把 legacy 路径写回默认入口说明。

## 6. 还无法自动化的门禁

1. 仓库外 sibling repo 或现场脚本是否仍直接依赖 `hmi-client`、`dxf-pipeline`、`control-core`。
2. `control-cli` 的全量命令面是否已经完成迁移，而不仅是最小 canonical 承载面存在。
3. 外部用户是否仍通过旧 Python 包名、旧 CLI 名称或手工脚本依赖兼容壳。
4. 文档语义是否真的把 canonical 说清楚。当前自动化只能限制 legacy 路径出现位置，不能理解整段语义。
5. repo 内 HIL 默认入口虽然已经通过 `hardware-smoke` 验证，但现场部署、回滚链路是否已经完全摆脱 `control-core` / `dxf-pipeline` 工作区外兼容产物，仍需人工确认。

## 7. 删除前必须人工确认

1. 现场/HIL/发布负责人确认：没有工作区外脚本仍依赖待删目录。
2. 兼容壳 owner 确认：白名单中的每个例外都已有明确下线前置条件和责任人。
3. 文档 owner 确认：默认 onboarding / runtime / rollback 口径已经以 canonical 路径为第一入口。
4. 对已删除的 `hmi-client` 仍需继续确认仓外消费者是否已经迁移；`dxf-pipeline` 的兼容壳虽已退出，但仓外脚本是否仍缓存旧入口仍需人工确认。
5. 若准备删除 `control-core` 下某个兼容壳，需先确认对应 canonical 产物、测试、发布链路都已闭环。

## 8. 已记录缺口

- `legacy-exit-check.ps1` 当前只扫描 repo 内文件，不扫描工作区外 sibling repo、CI secrets 或人工运维命令历史。
- 活动文档白名单采用“文件级例外”，可以有效阻止扩散，但不能细化到行级语义审查。
- `control-core` 真实实现面是否已经完全被 canonical 承接，仍需依赖 `legacy-deletion-gates.md` 与人工评审共同判定。
