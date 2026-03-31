---
name: context-bootstrap
description: "Normalize the minimal repository context before analysis or implementation. Use to identify relevant modules, docs, entry points, validation paths, 上下文引导, and 上下文归一."
---

# Purpose

本 skill 的唯一职责是：在正式开展规格、定位、实现或审查前，先把本轮任务的最小上下文对齐。

它解决的问题不是“给方案”，而是“确保后续工作是在正确的代码、文档、目录、测试入口和边界之上进行”。

# Use this skill when

- 任务类型已经初步明确，但上下文尚未统一。
- 需要确定本轮应该优先阅读哪些文档、目录和入口文件。
- 需要明确哪些模块与本轮最相关。
- 需要判断本轮允许修改的初步范围。
- 需要为 `contract-freeze`、`feature-clarify`、`incident-intake`、`boundary-audit` 或 `online-validation-gate-and-evidence` 准备上下文。

# Do not use this skill when

- 已经明确掌握完整上下文，并且正在执行具体分析或实现。
- 当前只是做最终收尾。
- 当前问题已经收敛为单个函数或文件的局部实现问题，不需要重新梳理上下文。

# Inputs

尽可能收集和整理以下内容：

- 当前任务类型
- 仓库根与当前工作目录
- 当前分支 / 工作树
- 任务目标或故障现象
- 可能相关的模块
- 可能相关的文档
- 可能相关的脚本、测试、构建入口
- 用户明确提出的限制
- 当前不确定但必须确认的关键点

# Required outputs

1. 本轮相关目录 / 模块
2. 优先阅读的文档 / 契约
3. 关键代码入口
4. 测试 / 脚本 / 验证入口
5. 本轮边界
6. 证据缺口

# Hard prohibitions

- 不给设计方案。
- 不直接判根因。
- 不直接改代码。
- 不把“可能相关”写成“已经确认相关”。
- 不做无差别全仓库漫游。

# Procedure

1. 读取当前任务类型与目标。
2. 如仓库存在 `scripts/validation/resolve-workflow-context.ps1`，优先用它归一 `ticket`、`branchSafe`、`timestamp` 等上下文。
3. 识别与任务类型匹配的最小相关层：
   - 新功能：功能入口、契约、owner 模块、验证入口
   - 故障修复：现象链路、复现入口、日志入口、异常传播链
   - 架构治理：目录结构、模块边界、依赖图、架构说明
   - 联机验证：状态机、设备适配、联机脚本、观察点
4. 优先确定“从文档到实现”的最短阅读路径，而不是随机找文件。
5. 明确当前边界：
   - 先看哪些
   - 暂不看哪些
6. 标记正式验证入口，优先使用根级 `build.ps1`、`test.ps1`、`ci.ps1` 与仓库校验脚本。
7. 如果存在高风险命令，记录需要改走 `scripts/validation/invoke-guarded-command.ps1`。

# Output format

严格使用以下结构输出：

## 本轮上下文引导卡

### 1. 相关目录 / 模块
-
-

### 2. 优先阅读文档
- 文件 / 文档：
- 原因：

### 3. 关键代码入口
- 入口文件 / 类 / 服务：
- 作用：

### 4. 测试 / 脚本 / 验证入口
-
-

### 5. 本轮边界
- 先聚焦：
- 暂不扩展：

### 6. 证据缺口
-
-

## 建议下一步
- 推荐进入的下一个 skill：

# Handoff

- 上游：`task-classifier`
- 下游：
  - 新功能：`feature-clarify`
  - 故障修复：`incident-intake`
  - 架构治理：`boundary-audit`
  - 联机验证：`online-validation-gate-and-evidence`
  - 任何需要契约冻结的任务：`contract-freeze`
