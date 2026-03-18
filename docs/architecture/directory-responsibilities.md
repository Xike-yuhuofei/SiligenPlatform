# 目录职责

## 1. 目的

本文件定义根级 monorepo 目录的职责边界，避免后续任务继续把能力随意堆回旧子项目。

## 2. 顶层目录职责

| 目录 | 应放内容 | 不应放内容 | 当前兼容说明 |
|---|---|---|---|
| `apps/` | 可运行入口、壳层装配、启动封装 | 可复用业务逻辑、共享 schema | 当前承接 HMI 与控制侧入口 |
| `packages/` | 复用能力、契约、宿主支撑、基础模块 | 进程入口、部署脚本 | 当前不再包含 editor contracts |
| `integration/` | 跨包场景、协议兼容、HIL/SIL 回归 | 单包单元测试 | 当前只建分类目录，不搬现有测试 |
| `tools/` | 构建脚本、codegen、迁移工具、发布脚本 | 业务实现 | 当前不重写现有脚本 |
| `docs/` | 根级治理、架构、决策、onboarding | 仅对子项目局部有效的低层文档 | 当前承接外部 DXF 编辑说明 |
| `config/` | 机器配置、环境模板、CI 配置 | 运行时临时文件 | 当前只建目标目录 |
| `data/` | 配方、schema、基线等版本化资产 | 运行时 mutable 上传目录 | `uploads/*` 策略待定 |
| `examples/` | 演示样例、示例输入输出 | 真实运行时数据 | 当前只建目标目录 |

## 3. `apps/` 目录职责

| app | 职责 | 当前真实来源 |
|---|---|---|
| `apps/control-runtime` | 运行时宿主薄入口 | `packages/runtime-host` + `control-core/apps/control-runtime` 兼容壳 |
| `apps/control-tcp-server` | TCP/JSON 服务入口 | `packages/transport-gateway` + `control-core/apps/control-tcp-server` 薄入口 |
| `apps/control-cli` | CLI 入口 | 旧仓 `src/adapters/cli` |
| `apps/hmi-app` | 桌面 HMI 入口 | `hmi-client` |

## 4. `packages/` 目录职责

| package | 职责 | 当前真实来源 |
|---|---|---|
| `engineering-data` | 工程对象、几何预处理、离线规划/轨迹 | `dxf-pipeline` + `control-core` 离线切片 |
| `process-runtime-core` | 强收拢业务内核 | `control-core/src/domain/*` + `src/application/usecases/*` |
| `runtime-host` | 宿主装配、配置、生命周期、安全、后台任务 | `packages/runtime-host` + 旧 bootstrap/container 迁移来源 |
| `device-adapters` | 硬件驱动与设备适配 | `control-core/modules/device-hal` |
| `transport-gateway` | TCP/JSON 边界和传输映射 | `packages/transport-gateway` + `control-core/modules/control-gateway` 兼容壳 |
| `traceability-observability` | 日志、事件、回放、查询 | 当前散落在 runtime/device-hal/shared |
| `application-contracts` | HMI/CLI/TCP 应用契约 | 当前散落在 HMI 和 control-gateway |
| `device-contracts` | 核心与设备之间的能力契约 | 当前散落在 ports/include |
| `engineering-contracts` | 工程输出 schema/proto/codegen | `dxf-pipeline/proto` + contracts |
| `shared-kernel` | 小而稳的公共基础内核 | `control-core/modules/shared-kernel` |
| `packages/simulation-engine` | 运动仿真、虚拟设备桥接、回归支撑 | 原顶层 `simulation-engine` 已收口到 `packages/simulation-engine` |
| `test-kit` | 跨包测试支撑与回归工具箱 | 当前散落在各子项目和旧仓工具 |

## 5. DXF 编辑规则

DXF 编辑当前不再有专属 app 或 contracts 包。

相关需求统一遵循：

- 使用外部编辑器人工处理 DXF
- 说明文档落到 `docs/runtime/external-dxf-editing.md`
- 不再新增 `apps/dxf-editor-app`、`dxf-editor`、`packages/editor-contracts` 等同类边界
