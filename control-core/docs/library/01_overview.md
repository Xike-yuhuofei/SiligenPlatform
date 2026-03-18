---
Owner: @Xike
Status: active
Last reviewed: 2026-01-21
Scope: 本项目全局
---

# 系统概览

## 项目简介

Siligen 是基于 C++17 的工业点胶控制系统，采用六边形架构设计，支持高精度运动控制和位置触发。

## 核心功能

- **多轴协调运动**: 通过 MultiCard 控制卡实现多轴协调运动
- **高精度点胶**: 广泛应用于精密点胶、涂胶等工业自动化场景
- **位置触发 (CMP)**: 使用规划位置（Profile Position）作为比较源，不使用编码器反馈

## 技术栈

| 类别 | 技术 |
|------|------|
| 语言 | C++17 |
| 架构 | 六边形架构 (Ports & Adapters) |
| 平台 | Windows 7/8/10/11 (64位) |
| 构建 | CMake 3.12+ + Ninja |
| 编译器 | Clang-CL |
| 硬件 | GAS MultiCard 系列控制卡 |

## 当前目录主视角

```
apps/
├── control-runtime/     # 当前 runtime 宿主与装配入口
└── control-tcp-server/  # 当前 TCP 可执行入口

modules/
├── control-gateway/     # TCP 协议、会话、分发与 facade
├── device-hal/          # 硬件适配器、驱动与厂商 SDK
├── motion-core/         # 运动/安全核心
├── process-core/        # 工艺/配方核心
└── shared-kernel/       # 共享内核

src/
├── domain/              # 领域层源码
├── application/         # 用例聚合与兼容入口
├── infrastructure/      # 基础设施聚合与兼容入口
├── adapters/            # 剩余外部接口适配层
└── shared/              # 共享基础能力
```

## 快速开始

1. **了解架构** - 阅读 [系统架构](./02_architecture.md)
2. **环境搭建** - 参考 [项目根目录 README](../../README.md)
3. **参考手册** - 查看 [参考手册](./06_reference.md)
4. **故障排查** - 查看 [Runbook](./05_runbook.md)

## 相关文档

- [系统架构](./02_architecture.md) - 六边形架构设计详解
- [参考手册](./06_reference.md) - 配置、API、端口等技术参考
- [故障排查手册](./05_runbook.md) - 线上故障快速响应指南
