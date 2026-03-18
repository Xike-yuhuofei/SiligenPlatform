# 架构审查清单

## 概述

本文档为Siligen工业控制系统项目的架构审查提供标准化的检查清单，确保所有代码变更都符合六边形架构设计原则。

## 六边形架构原则

### 依赖方向规则
```
adapters → application → domain ← infrastructure ← shared
```

- ✅ **Domain层**：仅依赖shared层，不依赖任何技术实现层
- ✅ **Application层**：仅依赖domain层和shared层
- ✅ **Infrastructure层**：仅依赖domain层和shared层，实现端口接口
- ✅ **Adapters层**：可依赖application、domain、shared层
- ✅ **Shared层**：独立内核，不依赖任何业务层

## 审查清单

### 🚨 高优先级检查（必须通过）

#### Domain层纯净性检查
- [ ] Domain层文件不包含 `#include "../../infrastructure/"`
- [ ] Domain层文件不包含 `#include "../../adapters/"`
- [ ] Domain层不直接调用硬件API或系统调用
- [ ] Domain层仅通过端口接口与外部交互
- [ ] Domain类不继承自技术实现类

#### Application层隔离检查
- [ ] Application层不直接实现硬件操作
- [ ] Application层通过端口接口调用基础设施服务
- [ ] Application用例不包含UI或数据库代码

#### 命名空间规范检查
- [ ] 所有业务代码使用 `Siligen::` 前缀命名空间
- [ ] Domain层使用 `Siligen::Domain::*`
- [ ] Application层使用 `Siligen::Application::*`
- [ ] Infrastructure层使用 `Siligen::Infrastructure::*`

### 🔍 中优先级检查（建议遵循）

#### 端口接口设计
- [ ] 端口接口定义为纯虚函数
- [ ] 端口接口使用 `Result<T>` 模式进行错误处理
- [ ] 端口接口位于 `src/*/ports/` 目录

#### 适配器实现
- [ ] 适配器实现具体的端口接口
- [ ] 适配器位于 `src/infrastructure/adapters/`
- [ ] 适配器不包含业务逻辑

#### 共享内核使用
- [ ] 基础类型定义在 `src/shared/types/`
- [ ] 工具类位于 `src/shared/utils/`
- [ ] 共享接口位于 `src/shared/interfaces/`

### 📋 低优先级检查（优化项）

#### 代码组织
- [ ] 文件目录结构符合六边形架构分层
- [ ] 每个模块有清晰的职责边界
- [ ] 模块间依赖关系最小化

#### 性能考虑
- [ ] 避免不必要的跨层调用
- [ ] 合理使用缓存机制
- [ ] 接口设计考虑性能影响

## 审查流程

### 1. 提交前检查
开发者应在提交代码前运行：
```bash
# 运行架构分析器
python .claude/agents/arch_analyzer.py --src src --verbose

# 检查依赖违规
grep -r "#include.*infrastructure" src/domain/
grep -r "#include.*adapters" src/domain/
```

### 2. Pull Request审查
审查者应检查：
- [ ] PR模板完整填写
- [ ] 架构分数 >= 80分
- [ ] 无严重级别违规
- [ ] 依赖方向正确
- [ ] 命名空间规范

### 3. CI/CD自动检查
- [ ] 架构分析器自动运行
- [ ] 构建验证通过
- [ ] 分数阈值检查（>=80分）

## 常见违规模式

### ❌ 禁止模式
```cpp
// Domain层直接依赖Infrastructure
#include "../../infrastructure/logging/Logger.h"

// Application层直接调用硬件
class MyUseCase {
    HardwareAPI api; // ❌ 错误
};

// 错误的命名空间
namespace MyProject { // ❌ 应该是 Siligen::
```

### ✅ 正确模式
```cpp
// Domain层通过端口接口
#include "../ports/ILoggingPort.h"

// Application层通过端口调用
class MyUseCase {
    IHardwarePort& hardware; // ✅ 正确
};

// 正确的命名空间
namespace Siligen::Domain::Motion { // ✅ 正确
```

## 工具支持

### 架构分析器
- 位置：`.claude/agents/arch_analyzer.py`
- 用法：`python .claude/agents/arch_analyzer.py --src src --verbose`
- 功能：自动检测依赖违规、命名空间问题、模块边界违规

### CI/CD集成
- GitHub Actions工作流：`.github/workflows/architecture-validation.yml`
- 自动架构评分和违规检查
- PR自动评论架构分析结果

## 紧急修复流程

当发现严重架构违规时：

1. **立即停止合并**相关PR
2. **创建修复Issue**，标记为高优先级
3. **制定重构计划**，确保向后兼容
4. **执行重构**，保持架构分数 >= 80
5. **验证结果**，确认违规已修复

## 参考资料

- [六边形架构设计文档](../architecture/)
- [编码规范](coding-guidelines.md)
- [模块约束](../../../src/*/CLAUDE.md)
- [项目结构](../../PROJECT_STRUCTURE.md)

---

**版本**: v1.0.0
**最后更新**: 2025-12-15
**维护者**: 架构团队