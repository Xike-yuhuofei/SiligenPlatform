# 当前源码到目标模块的迁移映射（已落地）

## 说明

本文件记录当前 canonical 落点，帮助阅读者直接定位主实现，而不是回头依赖阶段 0 规划草图或 legacy 目录。

总体原则已经执行完成：

- 主实现迁入 `apps/*` 与 `modules/*`
- 运行时装配、TCP gateway、device-hal 均已有稳定 canonical 路径
- canonical target 已切换完成，旧 target 名称仅以 alias 形式暂时保留

## Canonical 落点（当前实况）

| 职责域 | 当前 canonical 路径 | 当前状态 |
|---|---|---|
| 共享内核 | `modules/shared-kernel` | 已落地 |
| 工艺/配方核心 | `modules/process-core` | 已落地 |
| 运动/安全核心 | `modules/motion-core` | 已落地 |
| runtime 装配 | `apps/control-runtime/container` | 已落地 |
| runtime bootstrap | `apps/control-runtime/bootstrap` | 已落地 |
| runtime 事件/调度/配置/存储 | `apps/control-runtime/runtime` | 已落地 |
| system 用例 | `apps/control-runtime/usecases/system` | 已落地 |
| runtime 安全与监控 | `apps/control-runtime/security` + `apps/control-runtime/services/motion` | 已落地 |
| gateway 协议与 TCP 适配 | `modules/control-gateway/src/protocol` + `modules/control-gateway/src/tcp` | 已落地 |
| gateway TCP 门面 | `modules/control-gateway/src/facades/tcp` | 已落地 |
| TCP 程序入口 | `apps/control-tcp-server` | 已落地 |
| device-hal 硬件驱动源码 | `modules/device-hal/src/drivers/multicard` | 已落地 |
| device-hal 厂商 SDK 二进制 | `modules/device-hal/vendor/multicard` | 已落地 |
| device-hal motion/dispensing/diagnostics/recipes 适配器 | `modules/device-hal/src/adapters/*` | 已落地 |

## 当前阅读建议

- 阅读主实现时，优先从 `apps/*` 和 `modules/*` 进入
- 排查 `MultiCard` 链接、DLL 拷贝或安装逻辑时，优先看 `modules/device-hal/vendor/multicard`
- 优先使用 `siligen_control_application`、`siligen_control_runtime`、`siligen_control_gateway_tcp_adapter`、`siligen_device_hal_*` 与 `siligen_control_runtime_*` 等 canonical target 名称
- 遇到旧 target 名称时，先看对应 `CMakeLists.txt` 指向的新源码路径，而不是回 legacy 目录找 wrapper

## 当前兼容策略

- CMake 的真实 target 已切换到 canonical 命名，部分历史 target 名称仅作为 alias 保留
- 文档与说明文件中仍会少量提及 legacy 清理历史，但不应再作为扩展实现的入口依据

## 后续只做可选清理

后续如果继续推进，目标不再是“迁移主实现”，而是“继续统一认知入口”：

- 继续收缩 legacy CMake 入口
- 将文档、脚本、生成物完全切换到新目录
- CLI 已明确标记为历史/兼容能力；若未来需要重启独立 CLI，应另立项并补新的 ADR，而不是回退当前 canonical 入口描述

换言之，迁移地图已经从“规划目标”进入“canonical 路径统一”阶段。
