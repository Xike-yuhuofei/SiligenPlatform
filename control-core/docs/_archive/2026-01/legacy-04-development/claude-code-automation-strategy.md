# Claude Code + GitHub Actions 自动化工作流集成方案

> **文档版本**: v1.0
> **创建日期**: 2025-12-30
> **适用范围**: siligen-motion-controller 项目
> **状态**: 方案设计阶段

---

## 📋 文档概述

本文档提供 Claude Code 与 GitHub Actions 深度集成的完整方案思路，涵盖自动化代码审查、自动修复、Issue 驱动开发三大核心场景。方案基于项目现有的 41 条架构规则和六边形架构约束。

### 相关文档

- [Claude Code 集成指南](./claude-code-integration-guide.md)
- [静态分析部署指南](./static-analysis-deployment-guide.md)
- [.github/workflows/claude-code-review.yml](../../.github/workflows/claude-code-review.yml)
- [.claude/rules/](../../.claude/rules/)

---

## 🎯 核心架构：三层自动化策略

### **架构图示**

```
┌─────────────────────────────────────────────────────────────┐
│                     触发层 (Trigger Layer)                    │
├──────────────┬──────────────────┬───────────────────────────┤
│ Workflow A:  │  Workflow B:      │  Workflow C:              │
│ PR 审查       │  Issue → PR      │  定时巡检                  │
│ (被动模式)    │  (主动模式)       │  (预防模式)                │
└──────┬───────┴──────┬───────────┴──────┬────────────────────┘
       │              │                   │
       ▼              ▼                   ▼
┌─────────────────────────────────────────────────────────────┐
│                   执行层 (Execution Layer)                   │
├──────────────┬──────────────────┬───────────────────────────┤
│ 阶段 1:       │  阶段 2:          │  阶段 3:                  │
│ 分析          │  决策             │  执行 + PR                │
│ (41 条规则)   │  (可修复性判断)    │  (自动修复)               │
└──────┬───────┴──────┬───────────┴──────┬────────────────────┘
       │              │                   │
       ▼              ▼                   ▼
┌─────────────────────────────────────────────────────────────┐
│                   反馈层 (Feedback Layer)                    │
├──────────────┬──────────────────┬───────────────────────────┤
│ 自动标签      │  标准化评论       │  失败重试                 │
│ 管理          │  (模板化)         │  (3 次策略)               │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔄 Workflow A: PR 触发审查（被动模式）

### **设计目标**
- 在 PR 创建时自动审查代码
- 检测违反 41 条规则的代码
- 提供具体修复建议
- 严重违规阻止合并

### **触发条件**
```yaml
on:
  pull_request:
    branches: [main, develop, '*-test', '010-*', 'bugfix/*']
    types: [opened, synchronize, reopened]
  pull_request_review_comment:
    types: [created]
  issue_comment:
    types: [created]
```

### **执行流程**
```
1. PR 触发事件
   ↓
2. 加载 .claude/rules/*.md (41 条规则)
   ├─ INDUSTRIAL.md: 5 条工业安全规则
   ├─ HARDWARE.md: 5 条硬件抽象规则
   ├─ NAMESPACE.md: 5 条命名空间规则
   └─ 其他 8 个文件: 26 条规则
   ↓
3. 运行验证命令 (rg - ripgrep)
   ↓
4. 生成审查报告
   ├─ ABORT 级别: 阻止合并 🚫
   └─ WARNING 级别: 仅评论 ⚠️
   ↓
5. 发布 PR 评论 + 标签
```

### **ABORT 级别阻止合并的技术实现**

**目标**: 确保 ABORT 级别违规真正阻止 PR 合并，而非仅仅评论。

**实现方式**:

#### **1. Workflow Job 失败机制**
```yaml
# .github/workflows/claude-code-review.yml
jobs:
  architecture-review:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout code
        uses: actions/checkout@v4

      - name: Run architecture validation
        id: validate
        run: |
          # 运行 41 条规则验证
          abort_count=0
          warning_count=0

          # 示例: 检测 HARDWARE_DOMAIN_ISOLATION
          if rg "#include.*MultiCard|#include.*lnxy|#include.*driver" src/domain/; then
            echo "::error file=src/domain/system/StatusMonitor.h,line=15::ABORT: Domain 层不应直接包含硬件头文件"
            ((abort_count++))
          fi

          # 检测其他 ABORT 规则...
          # ...

          # 输出结果
          echo "abort_violations=$abort_count" >> $GITHUB_OUTPUT
          echo "warning_violations=$warning_count" >> $GITHUB_OUTPUT

          # 生成详细报告
          echo "发现 $abort_count 个 ABORT 级别违规"
          echo "发现 $warning_count 个 WARNING 级别违规"

      - name: Post PR comment
        uses: actions/github-script@v7
        with:
          script: |
            // 发布详细的审查评论
            // ...

      - name: Fail if ABORT violations found
        if: steps.validate.outputs.abort_violations != '0'
        run: |
          echo "::error::发现 ${{ steps.validate.outputs.abort_violations }} 个 ABORT 级别违规,阻止合并"
          exit 1  # 关键: 让 job 失败
```

**关键点**:
- `exit 1` 会让整个 job 状态变为 **failed**
- GitHub Actions 的 `::error::` 注解会在 PR 的 "Checks" 标签页显示错误

#### **2. Branch Protection 配置**

在 GitHub 仓库设置中配置分支保护:

```
Settings → Branches → Branch protection rules (for main/develop)

✅ Require status checks to pass before merging
   ✅ Require branches to be up to date before merging

   Required status checks:
   ✅ architecture-review  (对应 Workflow job 名称)

✅ Require conversation resolution before merging (可选)
```

**效果**:
- 如果 `architecture-review` job 失败 (exit 1)，GitHub 会**物理阻止**合并按钮
- PR 页面显示: "Required status check 'architecture-review' has failed"
- 合并按钮变灰，无法点击

#### **3. 验证方式**

测试阻止机制是否生效:

```bash
# 1. 创建测试 PR (故意引入 ABORT 违规)
git checkout -b test/abort-violation
echo '#include "MultiCardInterface.h"' >> src/domain/test.cpp
git add src/domain/test.cpp
git commit -m "test: 引入 ABORT 违规"
git push origin test/abort-violation

# 2. 创建 PR
gh pr create --title "Test ABORT blocking" --body "测试 ABORT 阻止机制"

# 3. 查看 Workflow 运行
gh run list --workflow=claude-code-review.yml

# 4. 验证 PR 页面
# - "Checks" 标签页应显示 architecture-review 失败 ❌
# - "Conversation" 标签页合并按钮应为灰色
# - 鼠标悬停显示: "Required status check has failed"

# 5. 清理测试
gh pr close {PR_NUMBER}
git branch -D test/abort-violation
```

#### **4. 渐进式启用策略**

**问题**: 41 条规则中 31 条为 ABORT，立即全部启用可能导致大量现有代码无法合并。

**建议**:

```yaml
# Week 1-2: 观察模式
- name: Fail if ABORT violations found
  if: steps.validate.outputs.abort_violations != '0' && github.event_name != 'pull_request'
  run: exit 1
  # 仅在非 PR 事件时失败，PR 事件仅评论

# Week 3-4: 关键规则启用
ABORT_RULES_ENABLED:
  - HARDWARE_DOMAIN_ISOLATION
  - HEXAGONAL_DEPENDENCY_DIRECTION
  - INDUSTRIAL_EMERGENCY_STOP

# Week 5+: 全部启用
```

### **优先级审查维度**

| 优先级 | 维度 | 规则文件 | 严重级别 | 示例违规 |
|--------|------|----------|----------|----------|
| 1 | ARCHITECTURE | HEXAGONAL.md | ABORT | Domain 层依赖 Infrastructure |
| 2 | INDUSTRIAL SAFETY | INDUSTRIAL.md | ABORT | 实时控制路径使用动态内存 |
| 3 | HARDWARE ABSTRACTION | HARDWARE.md | ABORT | Domain 层直接包含 MultiCard 头文件 |
| 4 | DOMAIN PURITY | DOMAIN.md | ABORT | Domain 层使用 std::vector |
| 5 | NAMESPACE | NAMESPACE.md | WARNING | 缺失 Siligen::* 命名空间 |
| 6 | CODE QUALITY | 各规则文件 | WARNING | 资源泄漏、线程安全问题 |
| 7 | SECURITY | 各规则文件 | WARNING | 命令注入、未校验输入 |

### **输出示例**
```markdown
## 🔍 Claude Code 审查报告

### 违规类型: HARDWARE_DOMAIN_ISOLATION
### 严重级别: ABORT 🚫

**位置**: `src/domain/system/StatusMonitor.h:15`
**问题**: Domain 层不应直接包含硬件头文件
**规则**: HARDWARE.md → HARDWARE_DOMAIN_ISOLATION
**验证命令**: `rg "#include.*MultiCard|#include.*lnxy|#include.*driver" src/domain/`

**修复建议**:
1. 删除 `#include "MultiCardInterface.h"`
2. 通过 `IHardwareControllerPort` 接口访问硬件功能
3. 将硬件相关逻辑移至 Infrastructure 层

**参考文档**: `.claude/rules/HARDWARE.md`
```

---

## 🚀 Workflow B: Issue 驱动开发（主动模式）

### **设计目标**
- 将 GitHub Issue 自动转换为功能代码
- Claude 分析需求并生成符合架构规范的实现
- 自动创建包含代码、测试、文档的完整 PR

### **触发条件**
```yaml
on:
  issues:
    types: [opened, edited]
  issue_comment:
    types: [created]
```

**激活方式**:
- Issue 标签: `claude` 或 `automated`
- Issue 评论: `@claude please implement`

### **执行流程**
```
1. 用户创建 Issue
   Title: "添加 CMP 触发器配置验证"
   Body: "需要验证位置范围、输出极性和卡连接..."
   Labels: ["claude", "feature"]
   ↓
2. Workflow B 触发
   ↓
3. Claude 分析 Issue
   ├─ 提取功能需求
   ├─ 加载相关规则 (.claude/rules/INDUSTRIAL.md)
   └─ 检查现有代码库上下文
   ↓
4. 生成实现代码
   ├─ 符合六边形架构
   ├─ 遵循 INDUSTRIAL_CMP_TEST_SAFETY 规则
   └─ 使用 Siligen::Application::* 命名空间
   ↓
5. 创建完整 PR
   ├─ 源代码修改
   ├─ 单元测试 (tests/unit/)
   ├─ 集成测试 (tests/integration/)
   └─ 文档更新 (docs/)
   ↓
6. 自动关联原 Issue
```

### **能力边界**
✅ **可以自动实现**:
- CRUD 操作
- 配置验证
- 简单业务逻辑
- API 端点添加
- 单元测试生成

❌ **需要人工干预**:
- 复杂算法设计
- 性能优化
- 架构重构
- 安全敏感功能

### **输出示例**
```markdown
## 🤖 自动生成的 PR

**原 Issue**: #123 "添加 CMP 触发器配置验证"
**生成者**: Claude Code Automation
**分支**: `feature/issue-123-cmp-trigger-validation`

### 📦 包含内容

- [x] 源代码: `src/application/usecases/CMPTestUseCase.cpp`
- [x] 单元测试: `tests/unit/CMPTestUseCaseTest.cpp`
- [x] 集成测试: `tests/integration/CMPValidationTest.cpp`
- [x] 文档更新: `docs/03-hardware/configuration/cmp-validation.md`
- [x] 架构验证: ✅ 通过 HEXAGONAL 依赖检查

### ✅ 代码合规性

- [x] 符合 INDUSTRIAL_CMP_TEST_SAFETY 规则
- [x] 使用 Siligen::Application::* 命名空间
- [x] 通过 IHardwareControllerPort 访问硬件
- [x] Result<T> 错误处理
- [x] noexcept 约束

### 📊 代码统计

- 新增代码: 342 行
- 测试覆盖: 94%
- 编译状态: ✅ 通过
- 静态分析: ✅ 无违规

---

**请人工审核此 PR，确认后合并。**
```

---

## 🕒 Workflow C: 定时健康巡检（预防模式）

### **设计目标**
- 定期扫描整个代码库的架构合规性
- 发现技术债务并自动跟踪
- 自动修复简单违规
- 生成健康度趋势报告

### **触发条件**
```yaml
on:
  schedule:
    - cron: '0 2 * * *'  # 每日凌晨 2 点
  workflow_dispatch:      # 手动触发
```

### **多阶段 Pipeline 设计**

参考 [OpenAI Codex Action](https://github.com/openai/codex-action) 的实现模式：

#### **Job 1: 全库分析**
```yaml
analyze-codebase:
  runs-on: ubuntu-latest
  outputs:
    violations: ${{ steps.scan.outputs.findings }}
    fixable: ${{ steps.scan.outputs.can-auto-fix }}
    tech_debt: ${{ steps.scan.outputs.debt-items }}
```

**输出**:
```json
{
  "violations": [
    {
      "file": "src/domain/motion/TrajectoryExecutor.cpp",
      "rule": "INDUSTRIAL_REALTIME_SAFETY",
      "severity": "ABORT",
      "line": 145,
      "issue": "使用 std::vector 在关键路径"
    }
  ],
  "fixable": [
    {
      "file": "src/shared/types/Common.h",
      "rule": "NAMESPACE_HEADER_PURITY",
      "issue": "using namespace std",
      "auto_fix": true
    }
  ],
  "tech_debt": [
    {
      "component": "BufferManager",
      "debt_type": "缺少边界检查",
      "priority": "HIGH"
    }
  ]
}
```

#### **Job 2: 优先级排序**
```yaml
prioritize:
  needs: analyze-codebase
  outputs:
    fix_plan: ${{ steps.prioritize.outputs.plan }}
    track_items: ${{ steps.prioritize.outputs.issues }}
```

**逻辑**:
- 可自动修复 → 立即修复
- 高风险 + 易修复 → 创建 Issue 标记 `high-priority`
- 低风险 + 难修复 → 记录到技术债务报告

#### **Job 3: 自动修复**
```yaml
auto-fix:
  needs: prioritize
  if: needs.prioritize.outputs.fix_plan != '[]'
  permissions:
    contents: write
    pull-requests: write
```

**操作**:
1. 在新分支上应用修复
2. 运行完整测试套件
3. 创建 PR: `fix: auto-fix architecture violations by claude-code`
4. 标签: `auto-fixed`, `bot`

#### **Job 4: 跟踪 Issue**
```yaml
track-debt:
  needs: prioritize
  if: needs.prioritize.outputs.track_items != '[]'
  permissions:
    issues: write
```

**Issue 模板**:
```markdown
## 🏗️ 架构违规报告

**发现时间**: 2025-12-30 02:00 UTC
**扫描范围**: 整个代码库
**检测工具**: Claude Code + .claude/rules/

### 🚨 高优先级违规 (2)

1. **[TRAJECTORY_EXECUTOR] 实时安全违规**
   - 文件: `src/domain/motion/TrajectoryExecutor.cpp:145`
   - 规则: `INDUSTRIAL_REALTIME_SAFETY`
   - 问题: 关键路径使用 `std::vector`
   - 建议: 使用固定容量数组 `FixedCapacityVector<T, N>`

2. **[STATUS_MONITOR] 硬件隔离违规**
   - 文件: `src/domain/system/StatusMonitor.h:15`
   - 规则: `HARDWARE_DOMAIN_ISOLATION`
   - 问题: 直接包含 MultiCard 头文件
   - 建议: 通过 `IHardwareControllerPort` 抽象

### 📋 技术债务

- 组件 `BufferManager`: 缺少边界检查 (优先级: HIGH)
- 组件 `ConfigAdapter`: 错误处理不完整 (优先级: MEDIUM)

---

**标签**: `architecture-violation`, `technical-debt`, `needs-review`
```

---

## 🔁 反馈层：闭环机制

### **1. 自动标签管理**

| 标签 | 触发条件 | 行为 |
|------|----------|------|
| `critical-violation` | ABORT 级别违规 | 阻止合并，要求修复 |
| `needs-review` | WARNING 级别违规 | 仅评论，不阻止合并 |
| `auto-fixed` | 自动修复成功 | 自动合并（可选） |
| `retry-1` | 第 1 次失败 | 评论提示修复方法 |
| `retry-2` | 第 2 次失败 | 增加详细修复示例 |
| `manual-review` | 第 3 次失败 | 阻止合并，强制人工介入 |

### **2. 标准化评论模板**

#### **模板 A: 违规检测**
```markdown
## 🔍 Claude Code 审查报告

### 违规类型: {{RULE_NAME}}
### 严重级别: {{SEVERITY}} {{ICON}}

**位置**: `{{FILE_PATH}}:{{LINE_NUMBER}}`
**问题**: {{ISSUE_DESCRIPTION}}
**规则**: {{RULE_FILE}} → {{RULE_SECTION}}
**验证命令**: `{{VERIFY_COMMAND}}`

**修复建议**:
{{FIX_SUGGESTIONS}}

**参考文档**:
- 规则文件: `.claude/rules/{{RULE_FILE}}.md`
- 相关文档: `{{DOC_LINK}}`

---

**🤖 此评论由 Claude Code 自动生成**
```

#### **模板 B: 自动修复完成**
```markdown
## ✅ 自动修复完成

**原始违规**: {{ORIGINAL_VIOLATION}}
**修复类型**: {{FIX_TYPE}}
**修改文件**:
- {{FILE_1}}
- {{FILE_2}}

**验证结果**:
- [x] 编译通过
- [x] 单元测试通过 ({{TEST_COUNT}} tests)
- [x] 静态分析通过
- [x] 架构合规性验证通过

**请审核以下变更**:
{{DIFF_SUMMARY}}

---

**🤖 此修复由 Claude Code 自动生成，请人工审核后合并**
```

### **3. 失败重试策略**

```
┌─────────────────────────────────────────────────────────┐
│ 第 1 次检测到违规                                        │
├─────────────────────────────────────────────────────────┤
│ ✅ 添加评论 + 标签 "needs-review"                        │
│ ✅ 提供修复建议                                          │
│ ✅ 不阻止合并                                            │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ 第 2 次检测到相同违规 (用户提交新 commit)                 │
├─────────────────────────────────────────────────────────┤
│ ✅ 更新评论: "⚠️ 违规仍然存在"                            │
│ ✅ 标签升级: "retry-2"                                   │
│ ✅ 提供详细修复示例代码                                  │
│ ✅ 仍不阻止合并                                          │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│ 第 3 次检测到相同违规 (用户仍未修复)                      │
├─────────────────────────────────────────────────────────┤
│ ❌ 标签升级: "critical-violation"                        │
│ ❌ 阻止合并 (PR status checks 失败)                      │
│ ❌ 评论: "🚫 必须修复此违规才能合并"                      │
│ ❌ @mention 团队负责人                                   │
└─────────────────────────────────────────────────────────┘
```

### **4. 规则抑制/例外机制**

**设计目标**: 允许在特殊情况下有限地抑制特定规则检查，同时确保可追溯性和定期审查。

**适用场景**:
- 架构规则在某些合理场景下需要例外
- 例如: 轨迹插补的输出点数量运行时确定，无法使用固定容量数组
- 必须显式声明、有充分理由、并接受定期审查

#### **使用方式**

在代码中添加标准化的抑制注释:

```cpp
// CLAUDE_SUPPRESS: RULE_NAME
// Reason: 详细说明为什么需要例外 (至少 20 字符)
// Approved-By: 审批人姓名
// Date: YYYY-MM-DD
[被抑制的代码]
```

**示例**:
```cpp
// src/domain/motion/CMPCoordinatedInterpolator.cpp

// CLAUDE_SUPPRESS: DOMAIN_NO_STL_CONTAINERS
// Reason: CMP 协同插补的触发点数量取决于多轴路径,运行时确定。
//         FixedCapacityVector 会导致栈溢出(最大可能 10000+ 点)
//         或容量不足(无法处理复杂路径)。
// Approved-By: 张三
// Date: 2025-12-30
std::vector<CMPTriggerPoint> CalculateTriggerPoints(
    const std::vector<AxisTrajectory>& trajectories) noexcept {
    // ... 插补逻辑
}
```

#### **Workflow 检测逻辑**

```yaml
# .github/workflows/claude-code-review.yml
- name: Check for valid suppressions
  run: |
    # 1. 运行规则验证,检测到违规
    violations=$(rg "forbidden_pattern" src/ --json)

    for violation in $violations; do
      file=$(echo $violation | jq -r '.data.path.text')
      line=$(echo $violation | jq -r '.data.line_number')

      # 2. 检查违规位置上方 10 行内是否有 CLAUDE_SUPPRESS
      suppression=$(head -n $line $file | tail -n 10 | grep "CLAUDE_SUPPRESS")

      if [ -n "$suppression" ]; then
        rule_name=$(echo $suppression | grep -oP 'CLAUDE_SUPPRESS: \K\w+')

        # 3. 验证抑制格式完整性
        has_reason=$(head -n $line $file | tail -n 10 | grep "Reason:")
        has_approved=$(head -n $line $file | tail -n 10 | grep "Approved-By:")
        has_date=$(head -n $line $file | tail -n 10 | grep "Date:")

        if [ -n "$has_reason" ] && [ -n "$has_approved" ] && [ -n "$has_date" ]; then
          # 4. 检查规则是否允许抑制
          if grep -q "$rule_name" .claude/rules/EXCEPTIONS.md; then
            echo "::warning file=$file,line=$line::Suppressed violation (needs review)"
            # 记录到审计日志
            echo "| $(date +%Y-%m-%d) | $file:$line | $rule_name | ... | ... | $(date -d '+3 months' +%Y-%m-%d) |" >> .claude/suppression-log.md
          else
            echo "::error file=$file,line=$line::This rule cannot be suppressed"
            exit 1
          fi
        else
          echo "::error file=$file,line=$line::Invalid suppression format"
          exit 1
        fi
      else
        # 无抑制注释,按原规则处理
        echo "::error file=$file,line=$line::Unsuppressed violation"
        exit 1
      fi
    done
```

#### **可抑制 vs 禁止抑制的规则**

| 规则 | 可抑制 | 理由 |
|------|--------|------|
| `DOMAIN_NO_STL_CONTAINERS` | ✅ | 特定场景（如插补）需要运行时大小 |
| `NAMESPACE_ROOT_ENFORCEMENT` | ✅ | 第三方库集成时可能需要 |
| `NAMESPACE_LAYER_ISOLATION` | ✅ | 跨层工具类可能需要 |
| `HARDWARE_DOMAIN_ISOLATION` | ❌ | 硬件隔离绝对不可妥协 |
| `INDUSTRIAL_REALTIME_SAFETY` | ❌ | 实时安全绝对不可妥协 |
| `INDUSTRIAL_EMERGENCY_STOP` | ❌ | 紧急停止绝对不可妥协 |
| `HEXAGONAL_DEPENDENCY_DIRECTION` | ❌ | 架构依赖方向绝对不可妥协 |

详细规则列表: `.claude/rules/EXCEPTIONS.md`

#### **PR 评论模板（抑制的违规）**

```markdown
## ⚠️ 抑制的违规 (需审查)

**位置**: `src/domain/motion/CMPCoordinatedInterpolator.cpp:145`
**规则**: DOMAIN_NO_STL_CONTAINERS
**抑制理由**: CMP 协同插补的触发点数量取决于多轴路径,运行时确定...
**审批人**: 张三
**审批日期**: 2025-12-30
**下次审查**: 2026-03-30 (3 个月后)

**此违规已被抑制,但仍需人工审查确认:**
- [ ] 理由仍然有效
- [ ] 没有更好的替代方案
- [ ] 已记录到审计日志 (`.claude/suppression-log.md`)

**参考**: `.claude/rules/EXCEPTIONS.md`

---
**🤖 此评论由 Claude Code 自动生成**
```

#### **审计与跟踪**

**审计日志**: `.claude/suppression-log.md`

| Date | File | Rule | Reason | Approved-By | Review-Date |
|------|------|------|--------|-------------|-------------|
| 2025-12-30 | src/domain/motion/LinearInterpolator.cpp:45 | DOMAIN_NO_STL_CONTAINERS | 运行时大小不确定 | 张三 | 2026-03-30 |
| 2025-12-30 | src/domain/motion/CMPCoordinatedInterpolator.cpp:145 | DOMAIN_NO_STL_CONTAINERS | CMP 多轴路径点数运行时确定 | 张三 | 2026-03-30 |

**审查要求**:
- **频率**: 每季度审查所有抑制
- **内容**:
  - 抑制理由是否仍然有效
  - 是否有更好的解决方案
  - 是否可以移除抑制
- **指标**:
  - 抑制总数 (目标: <10)
  - 抑制/总规则检查 比率 (目标: <1%)
  - 过期未审查抑制数 (目标: 0)

#### **告警阈值**

```yaml
metrics_alert:
  - condition: suppression_count > 10
    action: 团队会议讨论
  - condition: suppression_ratio > 1%
    action: 架构审查
  - condition: overdue_reviews > 3
    action: 阻止新 PR 合并
```

#### **实施检查清单**

- [ ] 创建 `.claude/suppression-log.md`
- [ ] 更新 `.github/workflows/claude-code-review.yml` 添加抑制检测逻辑
- [ ] 团队培训: 如何正确使用抑制机制
- [ ] 设置季度审查提醒 (日历事件)
- [ ] 监控抑制使用趋势 (仪表板)

**关键原则**:
> 例外是最后手段,不是常规做法
> 所有抑制必须有充分的技术理由
> 抑制趋势应该下降,而非上升

---

## 🛡️ 安全与权限控制

### **权限分级策略**

参考 [GitHub Actions 安全实践](https://github.com/context7/github_en_actions):

```yaml
# Workflow A: PR 审查（只读权限）
permissions:
  contents: read
  pull-requests: write  # 仅用于评论

# Workflow B: Issue → PR（写权限）
permissions:
  contents: write
  pull-requests: write
  issues: write

# Workflow C: 定时巡检（写权限）
permissions:
  contents: write
  pull-requests: write
  issues: write
```

### **Secret 管理**

```yaml
env:
  GLM_API_KEY: ${{ secrets.GLM_API_KEY }}  # Claude API 密钥
  GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }} # 自动注入
```

**安全注意事项**:
- ✅ `GLM_API_KEY` 存储在 Repository Secrets
- ✅ 使用 `pull_request_target` 时需格外小心（有 Secret 访问权限）
- ✅ Fork PR 触发的工作流自动限制权限（无 Secret 访问）

### **Fork PR 安全**

| 事件 | 权限 | Secret 访问 | 适用场景 |
|------|------|-------------|----------|
| `pull_request` (from fork) | 受限 | ❌ | 代码审查（只读） |
| `pull_request_target` | 完整 | ✅ | 需要访问 Secret 的操作 |
| `pull_request` (from same repo) | 完整 | ✅ | 正常开发流程 |

**推荐配置**:
```yaml
on:
  pull_request:
    types: [opened, synchronize]
    # Fork PR 安全运行，无 Secret 访问
```

---

## 📊 实施路线图

### **Phase 1: 基础审查（已完成 ✅）**

**目标**: 建立 PR 代码审查机制

**清单**:
- [x] 配置 `.github/workflows/claude-code-review.yml`
- [x] 添加 41 条架构规则到 `.claude/rules/`
- [x] 集成 GLM API (Claude Code)
- [x] PR 触发自动评论

**验证方法**:
```bash
# 1. 创建测试 PR
git checkout -b test/pr-review
git push origin test/pr-review

# 2. 检查 Workflow 运行
gh run list --workflow=claude-code-review.yml

# 3. 查看审查评论
gh pr view {PR_NUMBER} --comments
```

---

### **Phase 2: 智能修复（谨慎实施）**

**目标**: 自动修复极其机械性的架构违规

**⚠️ 风险控制原则**:
- 工业项目代码修改必须严格可控
- AI 缺乏完整上下文，可能引入隐蔽 bug
- 所有自动修复 PR 必须人工审核后才能合并

**实现要点**:
1. **可修复性判断逻辑**（严格限制）
   ```yaml
   is_fixable:
     if: |
       violation.type == 'delete_using_namespace' ||
       (violation.type == 'delete_forbidden_include' &&
        violation.confidence == '100%')
   ```

2. **支持的自动修复类型**（仅机械性操作）
   - ✅ 删除 `using namespace` 声明（完全匹配）
   - ✅ 删除 forbidden includes（完全匹配，无副作用）

3. **不支持的修复**（需要语义理解或架构调整）
   - ❌ 添加 `Siligen::*` 命名空间（需要理解代码语义）
   - ❌ 调整代码结构
   - ❌ 任何涉及业务逻辑的修改
   - ❌ 任何可能影响执行路径的修改
   - ❌ ABORT 级别违规（必须人工处理）

**输出要求**:
```yaml
auto-fix-pr:
  标签: ["auto-fixed", "manual-review-required"]
  合并策略: 人工审核后合并（禁止自动合并）
  测试要求: 必须通过完整测试套件
```

**文件结构**:
```
.github/
  workflows/
    claude-code-review.yml        # Phase 1（已存在）
    claude-auto-fix.yml            # Phase 2（新增，限制范围）
.claude/
  fix-templates/                   # 修复模板目录
    delete-using-namespace.md      # 仅删除操作
    delete-forbidden-include.md    # 仅删除操作
```

---

### **Phase 3: Issue 辅助设计（可选，不推荐自动 PR）**

**⚠️ 重要提示**: 工业运动控制器项目**不适合**自动将 Issue 转换为 PR。

**理由**:
1. **安全边界**: AI 无法完全理解硬件约束和实时安全要求
2. **责任边界**: 自动生成的代码无法明确责任归属
3. **审查成本**: 审查 AI 生成的复杂代码 > 人工实现
4. **法规要求**: 工业控制软件变更需要严格的追溯性

**替代方案: Issue 设计辅助**

**目标**: Claude 提供实现建议和架构设计，人工参考并实现

**触发条件**:
```yaml
on:
  issues:
    types: [opened, edited]
  issue_comment:
    types: [created]
```

**激活方式**:
- Issue 标签: `design-help` 或 `claude-assist`
- Issue 评论: `@claude please provide design suggestions`

**执行流程**:
```
1. 用户创建 Issue
   Title: "添加 CMP 触发器配置验证"
   Labels: ["design-help", "feature"]
   ↓
2. Claude 分析需求
   ├─ 提取功能需求
   ├─ 加载相关规则 (.claude/rules/INDUSTRIAL.md)
   ├─ 分析现有代码库上下文
   └─ 设计符合六边形架构的实现方案
   ↓
3. 生成设计文档（非代码）
   ├─ 架构设计说明
   ├─ 需要修改的文件清单
   ├─ 接口定义建议
   ├─ 测试策略
   └─ 潜在风险点
   ↓
4. 输出到 Issue 评论（供人工参考）
   ↓
5. 开发人员参考建议手动实现
```

**输出示例**:
```markdown
## 🎨 Claude 设计建议

**Issue**: #123 "添加 CMP 触发器配置验证"

### 📐 架构设计

**推荐的实现方式**:
1. 在 `src/application/usecases/` 创建 `CMPTestUseCase.cpp`
2. 通过 `IHardwareControllerPort` 访问硬件（符合 HARDWARE.md 规则）
3. 使用 `Result<T>` 错误处理
4. 添加单元测试到 `tests/unit/CMPTestUseCaseTest.cpp`

**关键设计决策**:
- ✅ 遵循 INDUSTRIAL_CMP_TEST_SAFETY 规则
- ✅ 使用 Siligen::Application::* 命名空间
- ✅ noexcept 约束
- ⚠️ 注意：验证逻辑必须在非实时路径执行

**潜在风险**:
- 硬件状态竞争 → 使用 IHardwareControllerPort 的互斥保护
- 配置验证超时 → 设置合理的超时时间

### 📋 实施清单

- [ ] 创建 CMPTestUseCase.h/cpp
- [ ] 实现位置范围验证
- [ ] 实现输出极性验证
- [ ] 添加单元测试（目标覆盖率 >80%）
- [ ] 集成测试验证硬件行为
- [ ] 更新文档

### 📚 参考文档

- `.claude/rules/INDUSTRIAL.md`
- `.claude/rules/HARDWARE.md`
- `docs/03-hardware/configuration/cmp-validation.md`

---
**🤖 此建议由 Claude 提供，请人工审核后实施**
```

**与原方案的关键区别**:

| 维度 | 原方案（Issue → PR） | 新方案（设计辅助） |
|------|---------------------|-------------------|
| 输出 | 完整代码 + PR | 设计建议 + 实施清单 |
| 风险 | 高（AI 生成代码） | 低（人工实现） |
| 责任 | 不明确 | 明确（开发人员） |
| 适用性 | 简单 CRUD | 架构设计指导 |
| 审查成本 | 高（需逐行检查） | 低（参考建议即可） |

---

### **Phase 4: 手动巡检（可选）**

**目标**: 按需扫描代码库架构合规性，支持技术债务管理

**⚠️ 调整理由**:
- 定时全库扫描成本高（API 调用 + CI 时间）
- 对于已有 CI/CD 的项目价值有限
- 可能产生大量噪音 Issue
- 手动触发更灵活，按需使用

**触发条件**:
```yaml
on:
  workflow_dispatch:  # 仅手动触发
    inputs:
      scan_scope:
        description: '扫描范围'
        required: true
        type: choice
        options:
          - recent-changes  # 仅近期变更文件（推荐）
          - full-repo       # 全库扫描（谨慎使用）
          - specific-path   # 指定路径

      target_branch:
        description: '目标分支'
        required: false
        type: string
        default: 'main'

      create_issues:
        description: '是否自动创建 Issue'
        required: false
        type: boolean
        default: false  # 默认不自动创建，避免噪音
```

**使用场景**:

| 场景 | 扫描范围 | 频率 | 自动创建 Issue |
|------|---------|------|---------------|
| 发布前检查 | recent-changes | 按需 | ✅ |
| 里程碑回顾 | full-repo | 每月/季度 | ❌（仅报告） |
| 特定模块审查 | specific-path | 按需 | ✅ |
| 日常开发 | - | - | 不使用 |

**多阶段 Pipeline 设计**（参考 OpenAI Codex Action）:

#### **Job 1: 增量分析**
```yaml
analyze-codebase:
  runs-on: ubuntu-latest
  outputs:
    violations: ${{ steps.scan.outputs.findings }}
    fixable: ${{ steps.scan.outputs.can-auto-fix }}
    tech_debt: ${{ steps.scan.outputs.debt-items }}
```

**优化策略**:
- 仅扫描 `git diff --name-only main~10..HEAD`（最近 10 个 commit）
- 缓存已扫描文件的结果
- 增量分析避免重复计算

**输出**:
```json
{
  "scan_scope": "recent-changes",
  "commits_analyzed": 10,
  "files_scanned": 15,
  "violations": [
    {
      "file": "src/domain/motion/TrajectoryExecutor.cpp",
      "rule": "INDUSTRIAL_REALTIME_SAFETY",
      "severity": "ABORT",
      "line": 145,
      "issue": "使用 std::vector 在关键路径"
    }
  ],
  "fixable": [
    {
      "file": "src/shared/types/Common.h",
      "rule": "NAMESPACE_HEADER_PURITY",
      "issue": "using namespace std",
      "auto_fix": true
    }
  ]
}
```

#### **Job 2: 报告生成与存储（默认）**
```yaml
generate-report:
  needs: analyze-codebase
  if: |
    inputs.create_issues == false ||
    needs.analyze-codebase.outputs.violations == '[]'
  steps:
    - name: Generate report
      id: report
      run: |
        # 生成 Markdown 格式报告
        cat > health-check-report.md <<EOF
        ## 📊 代码健康度扫描报告

        **扫描时间**: $(date -u +"%Y-%m-%d %H:%M UTC")
        **触发方式**: 手动触发
        **扫描范围**: 最近 10 个 commits (recent-changes)

        ### 🔍 发现的问题
        ${{ needs.analyze-codebase.outputs.violations }}

        ### ✅ 可自动修复
        ${{ needs.analyze-codebase.outputs.fixable }}
        EOF

    - name: Upload artifact
      uses: actions/upload-artifact@v4
      with:
        name: health-check-report-${{ github.run_number }}
        path: health-check-report.md
        retention-days: 90  # 保留 90 天

    - name: Update rolling issue
      uses: actions/github-script@v7
      with:
        script: |
          const fs = require('fs');
          const report = fs.readFileSync('health-check-report.md', 'utf8');

          // 查找或创建 "Health Check Dashboard" Issue
          const issues = await github.rest.issues.listForRepo({
            owner: context.repo.owner,
            repo: context.repo.repo,
            labels: ['health-check', 'automated'],
            state: 'open'
          });

          let issueNumber;
          if (issues.data.length > 0) {
            // 更新现有 Issue
            issueNumber = issues.data[0].number;
            const currentBody = issues.data[0].body;
            const newBody = `${report}\n\n---\n\n## 历史记录\n\n${currentBody}`;

            await github.rest.issues.update({
              owner: context.repo.owner,
              repo: context.repo.repo,
              issue_number: issueNumber,
              body: newBody.substring(0, 65535)  // GitHub Issue body 限制
            });
          } else {
            // 创建新 Issue
            const newIssue = await github.rest.issues.create({
              owner: context.repo.owner,
              repo: context.repo.repo,
              title: '📊 代码健康度仪表板 (Health Check Dashboard)',
              body: report,
              labels: ['health-check', 'automated', 'documentation']
            });
            issueNumber = newIssue.data.number;
          }

          console.log(`Updated health check dashboard: #${issueNumber}`);

    - name: Generate JSON report
      run: |
        # 生成 JSON 格式用于程序化处理
        cat > health-check-report.json <<EOF
        {
          "timestamp": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
          "scan_scope": "${{ inputs.scan_scope }}",
          "violations": ${{ needs.analyze-codebase.outputs.violations }},
          "fixable": ${{ needs.analyze-codebase.outputs.fixable }},
          "metrics": {
            "abort_count": $(echo '${{ needs.analyze-codebase.outputs.violations }}' | jq '[.[] | select(.severity=="ABORT")] | length'),
            "warning_count": $(echo '${{ needs.analyze-codebase.outputs.violations }}' | jq '[.[] | select(.severity=="WARNING")] | length')
          }
        }
        EOF

    - name: Upload JSON artifact
      uses: actions/upload-artifact@v4
      with:
        name: health-check-report-json-${{ github.run_number }}
        path: health-check-report.json
        retention-days: 90
```

**报告存储位置**:

| 存储方式 | 位置 | 用途 | 保留期限 |
|---------|------|------|---------|
| **GitHub Actions Artifact** | Actions → Run → Artifacts | 下载完整报告 | 90 天 |
| **滚动 Issue** | 固定 Issue (标签: `health-check`) | 易访问，趋势查看 | 永久 |
| **JSON 格式** | Actions → Run → Artifacts | 程序化处理，趋势分析 | 90 天 |

**访问方式**:

```bash
# 1. 查看最新健康度仪表板 Issue
gh issue list --label health-check --state open

# 2. 下载最近一次运行的报告
gh run list --workflow=claude-health-check.yml --limit 1
gh run download {RUN_ID} -n health-check-report-{RUN_NUMBER}

# 3. 查看 JSON 数据（用于趋势分析）
gh run download {RUN_ID} -n health-check-report-json-{RUN_NUMBER}
cat health-check-report.json | jq '.metrics'
```

**报告格式**:
```markdown
## 📊 代码健康度扫描报告

**扫描时间**: 2025-12-30 14:30 UTC
**触发方式**: 手动触发
**扫描范围**: 最近 10 个 commits (recent-changes)
**扫描文件**: 15 个文件

### 🔍 发现的问题

**ABORT 级别**: 1
- `TrajectoryExecutor.cpp:145` - 实时路径使用 std::vector

**WARNING 级别**: 2
- `Common.h:23` - using namespace std
- `ConfigAdapter.cpp:56` - 缺少错误处理

### ✅ 可自动修复

- `Common.h:23` - 删除 using namespace（需人工审核）

### 📋 技术债务

无新增技术债务项。

### 🎯 建议

1. 立即修复 TrajectoryExecutor.cpp 中的实时安全违规
2. 审核并应用 Common.h 的自动修复 PR
3. 补充 ConfigAdapter.cpp 的错误处理

---
**🤖 此报告由 Claude Code 自动生成**
```

#### **Job 3: Issue 跟踪（可选）**
```yaml
track-debt:
  needs: analyze-codebase
  if: |
    inputs.create_issues == true &&
    needs.analyze-codebase.outputs.track_items != '[]'
  permissions:
    issues: write
```

**Issue 模板**:
```markdown
## 🏗️ 架构违规报告

**发现时间**: 2025-12-30 14:30 UTC
**扫描范围**: recent-changes（最近 10 个 commits）
**检测工具**: Claude Code + .claude/rules/

### 🚨 高优先级违规 (1)

1. **[TRAJECTORY_EXECUTOR] 实时安全违规**
   - 文件: `src/domain/motion/TrajectoryExecutor.cpp:145`
   - 规则: `INDUSTRIAL_REALTIME_SAFETY`
   - 问题: 关键路径使用 `std::vector`
   - 建议: 使用固定容量数组 `FixedCapacityVector<T, N>`

---
**标签**: `architecture-violation`, `needs-review`
```

**成本对比**:

| 模式 | API 调用 | CI 时间 | 适用场景 |
|------|---------|---------|---------|
| 定时全库（原方案） | ~50k tokens/day | 15-20 min | 大型团队 |
| 手动增量（新方案） | ~5k triggers | 2-3 min | 所有团队 |

**推荐配置**:
```yaml
# .github/workflows/claude-health-check.yml
on:
  workflow_dispatch:  # ✅ 推荐
  # schedule:         # ❌ 删除
  #   - cron: '0 2 * * *'
```

---

## 🎯 关键决策点

### **1. 自动修复边界（严格限制）**

**⚠️ 工业项目原则**: 可预测性 > 自动化效率

| 类别 | 自动修复 | 需人工 | 不支持 | 理由 |
|------|----------|--------|--------|------|
| 删除 using namespace | ✅ 仅删除 | | | 机械操作，无副作用 |
| 删除 forbidden includes | ✅ 100% 匹配 | | | 完全匹配时安全 |
| 添加 Siligen::* 命名空间 | | ✅ | | 需要理解代码语义 |
| 调整代码结构 | | ✅ | | 需要架构理解 |
| ABORT 级别违规 | | ✅ | | 安全关键，必须人工 |
| 架构重构 | | | ❌ | 超出 AI 能力 |
| 性能调优 | | | ❌ | 需要实际测试验证 |
| 业务逻辑修改 | | | ❌ | 需要领域知识 |

**禁止自动修复的场景**:
1. 涉及硬件交互的代码
2. 实时控制路径
3. 可能影响执行时序的修改
4. 任何需要理解代码语义的操作

### **2. PR 自动合并策略（不推荐）**

**工业项目推荐: 人工审核**
```
所有 PR（包括自动修复）→ 人工审核 → 确认合并
```

**理由**:
1. **架构合规性 ≠ 功能正确性**
   - 通过架构检查的代码仍可能有逻辑错误
2. **责任追溯**
   - 工业软件需要明确的变更责任人
3. **隐性风险**
   - AI 无法预见所有边界情况
4. **团队学习**
   - 人工审核过程有助于团队理解架构规则

**配置建议**:
```yaml
# .github/.auto-merge.yml（不存在，即禁用自动合并）
# 工业项目不推荐配置自动合并

# 替代方案: 使用标签标记
allowed-auto-merge: []  # 空列表 = 禁用所有自动合并
```

**仅在以下场景考虑自动合并**（需要团队讨论）:
- ✅ 仅删除 `using namespace` 声明
- ✅ 仅修改注释或文档
- ✅ 测试代码格式化（不影响逻辑）
- ❌ 其他任何情况都需要人工审核

### **3. 成本控制**

**GLM API 调用优化**:
- 使用缓存（相同的文件修改不重复分析）
- 批量处理（多个文件合并为一次请求）
- 增量分析（仅分析修改的部分）

**预算估算**:
```
每日 PR 数量: 10
每次审查 token: ~2000
每日 token 消耗: 20,000
每月 token 消耗: 600,000

成本（GLM-4.7）: ~￥30/月
```

### **4. 团队接受度策略**

**渐进式引入**:
1. **Week 1-2**: 仅 WARNING 级别，熟悉流程
2. **Week 3-4**: 升级部分规则为 ABORT
3. **Week 5-6**: 启用自动修复（需人工审核）
4. **Week 7+**: 全面启用自动化

**培训要点**:
- 如何解读审查评论
- 如何修复常见违规
- 如何配置例外规则（如需要）

---

## 🛡️ 风险控制与适用性分析

### **工业项目的特殊约束**

**本项目的特点**:
1. **实时性要求**: 运动控制需要确定性的执行时间
2. **安全性要求**: 设备损坏和人员伤害风险
3. **硬件依赖**: 与特定硬件平台紧密耦合
4. **法规要求**: 可能需要符合工业标准（如 IEC 61508）

**这些约束对自动化的影响**:
- ❌ 不能自动修改实时控制路径代码
- ❌ 不能自动修改硬件交互代码
- ❌ 不能依赖 AI 理解硬件约束
- ✅ 可以进行架构合规性检查
- ✅ 可以进行机械性的代码清理

### **适用性矩阵**

| 自动化功能 | 通用 Web 项目 | 本项目（工业控制） | 推荐度 | 理由 |
|-----------|-------------|------------------|--------|------|
| **PR 审查** | ✅ | ✅ | ⭐⭐⭐⭐⭐ | 纯检查，无副作用 |
| **自动修复（机械性）** | ✅ | ⚠️ | ⭐⭐⭐ | 仅限删除操作 |
| **自动修复（语义性）** | ✅ | ❌ | ⭐ | 需要理解硬件约束 |
| **Issue → PR** | ✅ | ❌ | ❌ | 安全关键，风险过高 |
| **定时全库扫描** | ✅ | ⚠️ | ⭐⭐ | 改为手动触发 |
| **自动合并** | ✅ | ❌ | ❌ | 需要责任追溯 |

**结论**: 本项目**仅适合**实施 Phase 1（PR 审查）和 Phase 2（受限的自动修复）。

### **风险评估与缓解措施**

| 风险 | 概率 | 影响 | 缓解措施 | Phase |
|------|------|------|----------|-------|
| AI 生成代码引入 bug | 中 | 高 | 禁止自动生成代码，仅提供设计建议 | Phase 3 |
| 自动修复破坏架构 | 低 | 高 | 仅支持删除操作，必须人工审核 | Phase 2 |
| 审查噪音过多 | 中 | 中 | 渐进式启用，调整规则敏感度 | Phase 1 |
| API 成本超预算 | 低 | 低 | 增量分析，使用缓存 | All |
| 团队依赖 AI | 中 | 中 | 保留人工审核，AI 仅辅助 | All |
| Fork PR 安全问题 | 低 | 中 | 使用 `pull_request` 限制权限 | Phase 1 |

### **实施前提条件**

**必须满足以下条件才能实施各 Phase**:

#### **Phase 1 (PR 审查) - 已满足 ✅**
- [x] 41 条架构规则已定义
- [x] 规则验证命令已编写
- [x] Workflow 配置已部署
- [x] 团队了解如何解读审查评论

#### **Phase 2 (自动修复) - 需评估**
- [ ] 团队对自动修复的信任度已建立
- [ ] 有完善的测试套件（覆盖率 >80%）
- [ ] 有明确的审核流程
- [ ] 团队成员愿意承担审核成本

#### **Phase 3 (Issue 辅助设计) - 可选**
- [ ] 团队需要设计辅助（而非代码生成）
- [ ] 有明确的审核责任归属
- [ ] 不影响现有开发流程

#### **Phase 4 (手动巡检) - 按需**
- [ ] 有明确的扫描触发时机
- [ ] 有 Issue 管理流程（避免噪音）
- [ ] 团队认同其价值

### **不推荐的场景**

以下场景**不适合**使用 Claude Code 自动化:

1. **硬件驱动开发**
   - 需要深入理解硬件时序
   - AI 无法验证寄存器配置的正确性

2. **实时控制算法**
   - 执行时间确定性要求高
   - AI 无法预测运行时行为

3. **安全关键功能**
   - 紧急停止、故障检测等
   - 需要正式的验证和测试

4. **性能优化**
   - 需要实际性能测试数据
   - AI 无法预测缓存、分支预测等

5. **复杂业务逻辑**
   - 涉及领域知识
   - AI 缺乏项目背景

---

## 📚 参考资源

### **官方文档**
- [Claude Code GitHub Actions 官方文档](https://code.claude.com/docs/en/github-actions)
- [Claude Code 常见工作流](https://code.claude.com/docs/en/common-workflows)
- [GitHub Actions 官方文档](https://github.com/context7/github_en_actions)
- [anthropics/claude-code-action](https://github.com/anthropics/claude-code-action)

### **实战指南**
- [Claude Code + GitHub Actions: 再也不用改BUG](https://zhuanlan.zhihu.com/p/1888999194720702630)
- [如何用 Claude Code 搭建安全的 GitHub CI](https://poloapi.com/poloapi-blog/Claude%2520Code-11)
- [How to Integrate Claude Code with CI/CD](https://skywork.ai/blog/how-to-integrate-claude-code-ci-cd-guide-2025/)
- [Faster Claude Code agents in GitHub Actions - Depot](https://depot.dev/blog/claude-code-in-github-actions)

### **最佳实践**
- [Claude Code: Best practices for agentic coding](https://www.anthropic.com/engineering/claude-code-best-practices)
- [GitHub in 2025: Mastering Advanced Workflows](https://medium.com/@beenakawat002/github-in-2025-mastering-advanced-workflows-tools-and-best-practices-be6693e5061e)
- [State of AI Code Review Tools in 2025](https://www.devtoolsacademy.com/blog/state-of-ai-code-review-tools-in-2025/)
- [Top 5 GitHub Best Practices for Teams in 2025](https://red9systech.com/top-5-github-best-practices-for-teams-in-2025/)

### **工具参考**
- [OpenAI Codex Action (多阶段 Pipeline 示例)](https://github.com/openai/codex-action)
- [Presubmit - AI Code Review & Fixes](https://github.com/marketplace/actions/presubmit-ai-code-review-fixes)
- [GitHub Code Review Features](https://github.com/features/code-review)

### **项目相关文档**
- [Claude Code 集成指南](./claude-code-integration-guide.md)
- [静态分析部署指南](./static-analysis-deployment-guide.md)
- [静态分析用户指南](./static-analysis-user-guide.md)
- [架构审查清单](./architecture-review-checklist.md)

---

## 📝 附录

### **A. 规则文件清单**

| 文件 | 规则数 | 作用 | ABORT 规则 |
|------|--------|------|-----------|
| BUILD.md | 4 | 构建系统 | 2 |
| CONTEXT_MAP.md | 1 | 上下文加载 | 0 |
| DOMAIN.md | 5 | Domain 层约束 | 5 |
| FORBIDDEN.md | 3 | 全局禁止操作 | 3 |
| HARDWARE.md | 5 | 硬件抽象 | 3 |
| HEXAGONAL.md | 3 | 六边形架构 | 3 |
| INDUSTRIAL.md | 5 | 工业安全 | 3 |
| NAMESPACE.md | 5 | 命名空间 | 2 |
| PORTS_ADAPTERS.md | 3 | 端口适配器 | 3 |
| REALTIME.md | 4 | 实时控制 | 4 |
| UI_PLAYWRIGHT.md | 3 | UI 测试 | 3 |
| **合计** | **41** | | **31** |

### **B. 现有架构违规**

**检测时间**: 2025-12-30

```
1. src/domain/system/StatusMonitor.h:15
   违规: #include "MultiCardInterface.h"
   规则: HARDWARE_DOMAIN_ISOLATION
   严重级别: ABORT

2. src/domain/<subdomain>/ports/README.md
   违规: 文档中包含硬件头文件引用
   规则: HARDWARE_DOMAIN_ISOLATION
   严重级别: WARNING (文档，可接受)
```

### **C. 快速命令参考**

```bash
# 手动触发 Workflow
gh workflow run claude-code-review.yml

# 查看 Workflow 运行状态
gh run list --workflow=claude-code-review.yml

# 查看 Workflow 日志
gh run view {RUN_ID} --log

# 重新运行失败的 Workflow
gh run rerun {RUN_ID}

# 验证规则
rg "#include.*MultiCard" src/domain/
rg "^using namespace" src/ --include="*.h"
rg "(new |delete )" src/domain/
```

---

## 📊 总结与建议

### **修改说明**

本文档基于 ChatGPT 的原始方案进行了**针对工业控制项目的调整**：

| 维度 | 原方案 | 修改后方案 | 理由 |
|------|--------|-----------|------|
| **Phase 2 自动修复** | 支持命名空间、头文件等多类修复 | 仅支持删除操作（using namespace、forbidden includes） | AI 缺乏完整上下文，可能引入隐蔽 bug |
| **Phase 3 Issue 处理** | 自动生成功能 PR | 提供设计建议，人工实现 | 工业项目需要明确责任追溯和严格变更控制 |
| **Phase 4 巡检** | 定时全库扫描（每日凌晨 2 点） | 手动触发，增量扫描 | 降低成本，避免噪音，更灵活 |
| **PR 自动合并** | 支持条件自动合并 | 禁止，必须人工审核 | 架构合规性 ≠ 功能正确性 |
| **风险控制** | 较少 | 新增专门章节 | 工业项目有特殊的安全和实时性要求 |

### **最终建议**

#### **立即实施（高优先级）**
- ✅ **Phase 1 (PR 审查)**: 已完成，持续优化
  - 保持运行，收集团队反馈
  - 调整规则敏感度，减少噪音
  - 完善评论模板，提高可读性

#### **谨慎实施（中优先级）**
- ⚠️ **Phase 2 (自动修复)**: 需评估团队接受度
  - 仅支持机械性删除操作
  - 必须人工审核后合并
  - 前提：完善的测试套件（覆盖率 >80%）

#### **可选实施（低优先级）**
- 📋 **Phase 3 (设计辅助)**: 按团队需求决定
  - 仅提供设计建议，不生成代码
  - 适合新团队成员上手
  - 不替代人工设计

- 🔍 **Phase 4 (手动巡检)**: 按需使用
  - 发布前检查
  - 里程碑回顾
  - 不作为日常流程

#### **禁止实施**
- ❌ **Issue → PR 自动生成**: 风险过高
- ❌ **PR 自动合并**: 不符合工业项目要求
- ❌ **定时全库扫描**: 成本高，收益低
- ❌ **实时控制代码自动修改**: 安全关键

### **实施检查清单**

在实施各 Phase 前，请确认：

#### **Phase 1 (当前)**
- [x] Workflow 已部署
- [x] 团队了解如何解读审查评论
- [x] 规则误报率在可接受范围
- [ ] 团队反馈收集机制已建立

#### **Phase 2 (如需实施)**
- [ ] 团队成员同意尝试自动修复
- [ ] 测试覆盖率 >80%
- [ ] 有明确的审核流程
- [ ] 准备好回滚方案

#### **Phase 3 (如需实施)**
- [ ] 团队需要设计辅助
- [ ] 不影响现有开发流程
- [ ] 有明确的责任归属

#### **Phase 4 (如需实施)**
- [ ] 有明确的触发时机
- [ ] 有 Issue 管理流程
- [ ] 成本预算已审批

### **关键原则**

> **可预测性 > 自动化效率**
> **人工审核 > AI 自动生成**
> **责任追溯 > 开发速度**
> **安全可靠 > 功能丰富**

### **联系方式**

如有疑问或需要调整，请：
1. 查阅 `.claude/rules/` 中的架构规则
2. 参考本文档的风险控制章节
3. 与团队成员讨论达成共识

---

**文档维护**: 本文档应随项目发展持续更新，记录每个 Phase 的实施进展和经验教训。

**最后更新**: 2025-12-30
**更新内容**: 基于 Claude Code 的评估调整方案，增加风险控制和适用性分析

