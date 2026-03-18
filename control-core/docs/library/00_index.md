---
Owner: @Xike
Status: active
Last reviewed: 2026-03-15
Scope: 本项目全局
---

# 文档入口(Library Index)

> 这里是**权威文档库**入口。过程文档在 `docs/_archive/`。
>
> **需要更详细的文档导航？** 查看 [详细索引](./DETAILED_INDEX.md)

## 三条路径

### ① 我是新同学(30 分钟上手)
1. [当前项目复现指南](./04_guides/project-reproduction.md)
2. [项目概览](./01_overview.md)
3. [系统架构](./02_architecture.md)
4. [参考手册(IO/配置/阈值)](./06_reference.md)

### ② 我要开发/改代码
- Guides(按任务)
  - [Guides 目录说明](./04_guides/README.md)
  - [当前项目复现指南](./04_guides/project-reproduction.md) ⭐
- 关键参考
  - [参考手册(配置/环境变量/端口)](./06_reference.md)
  - [硬件限位配置与验证](./hardware-limit-configuration.md)
  - [工具集成清单(构建/依赖/验证)](./00_toolchain.md) ⭐ NEW
  - [错误码统一规范](./error-codes.md)
  - [错误码清单(自动生成)](./error-codes-list.md)
- 关键决策(为什么这样做)
  - [ADR 决策记录](../adr/README.md)

### ③ 线上/现场故障了(先救火)
- [Runbook 排障手册](./05_runbook.md)
- 常见高频故障:
  - 控制卡连接失败
  - 电机无响应
  - 点胶阀无响应
  - 供胶阀无响应

## 最近变更(手动维护 5~10 条就够)
- 2026-03-17: 收口 docs/plans 历史方案，新增“排胶流程与参数说明”“文档分层与收口规则”，并确认 `docs/adr/` 为 ADR 唯一权威入口 (Owner: @Codex)
- 2026-03-15: 新增“当前项目复现指南”，补齐新人接手、构建验证、运行入口、数据目录与当前仓库风险 (Owner: @Xike)
- 2026-02-04: 架构补充：工艺/测试结果归属 Diagnostics，结构化与校验集中在 Domain (Owner: @Xike)
- 2026-01-13: 新增工具集成清单 (00_toolchain.md),完整记录构建工具链、第三方库、验证协议 (Owner: @Xike)
- 2026-01-11: 初始化文档库骨架,建立两层文档结构(Owner: @Xike)

## 文档规范(极简版)

### 元数据要求(小团队最小规则)
- **强制**(CI会检查): `Owner` / `Last reviewed`
- **推荐**(CI只警告不拦截): `Status` / `Scope`

### 文档流程
- **新增文档默认先进 archive**,稳定后再提炼进 library
- 文档写作目标: 让新同学"照着做就能成功"
