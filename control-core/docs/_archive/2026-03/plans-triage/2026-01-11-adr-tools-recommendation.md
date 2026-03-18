---
Owner: @Xike
Status: draft
Created: 2026-01-11
Scope: ADR 工具选型
---

# ADR 工具推荐方案

> **背景**：选择方案1（统一到 `docs/adr/`），需要合适的 ADR 工具支持文档化管理
>
> **目标**：为 Windows + 小团队 + C++ 项目推荐最适合的 ADR 工具

---

## 📋 当前项目环境

| 环境 | 版本 | 可用性 |
|------|------|--------|
| **操作系统** | Windows 11 | ✅ |
| **Python** | 3.12.1 | ✅ 可用 |
| **Node.js** | v24.12.0 | ✅ 可用 |
| **Rust** | 未安装 | ❌ 不可用 |
| **Git** | 已安装 | ✅ 可用 |
| **团队规模** | 小团队 | - |

---

## 🎯 工具推荐总览

| 排名 | 工具 | 类型 | 适用度 | 推荐理由 |
|------|------|------|--------|----------|
| **🥇 首选** | **adr (npm)** | Node.js CLI | ⭐⭐⭐⭐⭐ | 成熟稳定、跨平台、社区活跃 |
| **🥈 次选** | **adr-log (npm)** | Node.js CLI | ⭐⭐⭐⭐ | 功能增强版、包含标题 |
| **🥉 备选** | **Python 脚本** | 自定义工具 | ⭐⭐⭐ | 完全可控、无依赖 |
| **不推荐** | adr-tools (Bash) | Bash 脚本 | ⭐ | Windows 支持差（需 WSL） |
| **不推荐** | ADRust (Rust) | Rust 编译型 | ⭐ | 需要编译环境、安装复杂 |

---

## 🥇 首选方案：adr (npm)

### 项目信息

- **GitHub**: [phodal/adr](https://github.com/phodal/adr)
- **npm**: [adr package](https://www.npmjs.com/package/adr)
- **类型**: Node.js CLI 工具
- **许可证**: MIT
- **活跃度**: ⭐⭐⭐⭐⭐

### 核心特性

✅ **跨平台支持** - Windows、Linux、macOS 完美支持
✅ **轻量级** - 专注于 ADR 管理，无额外依赖
✅ **Markdown 原生** - 生成标准 Markdown 格式
✅ **多语言支持** - 支持中英文模板
✅ **简单易用** - 3 个命令完成所有操作
✅ **HTML 报告** - 可生成 HTML 报告供非技术人员查看

### 安装步骤

```bash
# 1. 全局安装
npm install -g adr

# 2. 验证安装
adr --version
```

### 使用流程

```bash
# 1. 初始化 ADR 目录（在项目根目录）
cd D:\Projects\SiligenSuite\control-core
adr init en

# 输出：
# ✅ Created directory: doc/adr/
# ✅ Created template: doc/adr/template.md

# 2. 创建新 ADR
adr new "采用六边形架构"

# 输出：
# ✅ Created ADR: doc/adr/0001-use-hexagonal-architecture.md

# 3. 列出所有 ADR
adr list

# 输出：
# 0001: 采用六边形架构 (accepted)
# 0002: 使用 Result<T> 错误处理 (proposed)
```

### 生成的文件结构

```
doc/adr/
├── 0001-use-hexagonal-architecture.md
├── 0002-use-result-error-handling.md
├── 0003-add-logging-system.md
└── template.md
```

### ADR 文件模板

```markdown
# 0001 - 采用六边形架构

## Status
Accepted

## Context
我们需要...

## Decision
采用六边形架构...

## Consequences
- 正面：...
- 负面：...
```

### 高级功能

**生成 HTML 报告**：
```bash
adr generate > report.html
```

**配置目录**：
```bash
# 自定义 ADR 目录（默认是 doc/adr）
adr init en --directory docs/adr
```

### 优点分析

| 维度 | 评分 | 说明 |
|------|------|------|
| **安装简单性** | ⭐⭐⭐⭐⭐ | `npm install -g adr` 一行搞定 |
| **学习曲线** | ⭐⭐⭐⭐⭐ | 只需掌握 3 个命令（init、new、list） |
| **Windows 兼容** | ⭐⭐⭐⭐⭐ | 原生 Node.js，无兼容性问题 |
| **文件命名规范** | ⭐⭐⭐⭐ | 自动编号（0001、0002...） |
| **模板可定制** | ⭐⭐⭐⭐ | 可修改 template.md 自定义格式 |
| **社区支持** | ⭐⭐⭐⭐⭐ | GitHub 1.1k stars，活跃维护 |
| **文档完整性** | ⭐⭐⭐⭐ | README 和 Wiki 完整 |

### 缺点分析

| 缺点 | 影响 | 缓解措施 |
|------|------|----------|
| 默认目录是 `doc/adr/` | 与项目 `docs/adr/` 不一致 | 使用 `--directory` 参数指定 |
| 无自动链接更新 | 修改 ADR 编号后需手动更新链接 | 可编写脚本自动更新 |
| 无 GitHub Actions 集成 | 无法自动化 CI/CD | 可自行配置 GitHub Actions |

---

## 🥈 次选方案：adr-log (npm)

### 项目信息

- **npm**: [adr-log package](https://www.npmjs.com/package/adr-log)
- **类型**: Node.js CLI 工具（adr 的改进版）
- **特点**: 在输出中包含 ADR 标题（原始 adr-tools 不包含）

### 与 adr 的区别

| 特性 | adr | adr-log |
|------|-----|---------|
| **列出 ADR 标题** | ❌ 只显示编号 | ✅ 显示编号和标题 |
| **输出格式** | 简洁 | 详细 |
| **功能复杂度** | 简单 | 稍复杂 |

### 安装和使用

```bash
# 安装
npm install -g adr-log

# 使用（命令与 adr 相同）
adr-log init en
adr-log new "决策标题"
adr-log list
```

### 适用场景

- ✅ 需要在列表中查看 ADR 标题
- ✅ 需要 ADR 的详细元数据
- ✅ 团队规模稍大（5+ 人）

---

## 🥉 备选方案：Python 脚本

### 为什么考虑 Python？

1. **项目已有 Python 3.12.1** - 无需额外安装
2. **完全可控** - 可根据项目需求定制
3. **无外部依赖** - 不依赖 npm 包
4. **集成项目工具** - 可与现有 Python 脚本集成

### 实现方案

创建 `scripts/adr_tools.py`：

```python
#!/usr/bin/env python3
"""
ADR 管理工具 - Siligen 项目定制版
"""

import os
import re
from datetime import datetime
from pathlib import Path

ADR_DIR = Path("docs/adr")
TEMPLATE = """---
Status: {status}
Date: {date}
Deciders: {deciders}
Technical Story: {technical_story}
---

# {title} - {status}

## Context
{context}

## Decision
{decision}

## Consequences
- **正面**：
- **负面**：

"""

class ADRTool:
    def __init__(self, adr_dir=None):
        self.adr_dir = Path(adr_dir) if adr_dir else ADR_DIR
        self.adr_dir.mkdir(parents=True, exist_ok=True)

    def list_adrs(self):
        """列出所有 ADR"""
        adrs = sorted(self.adr_dir.glob("adr-*.md"))
        if not adrs:
            print("📋 当前没有 ADR")
            return

        print(f"📋 找到 {len(adrs)} 个 ADR：\n")
        for adr_file in adrs:
            # 提取标题
            with open(adr_file, 'r', encoding='utf-8') as f:
                content = f.read()
                title_match = re.search(r'# (.+?)\n', content)
                title = title_match.group(1) if title_match else adr_file.stem
                status_match = re.search(r'Status: (.+?)\n', content)
                status = status_match.group(1) if status_match else "unknown"

            print(f"  {adr_file.stem:30} [{status}] {title}")

    def new_adr(self, title, status="proposed", deciders="@Xike"):
        """创建新 ADR"""
        # 找到下一个编号
        existing_adrs = list(self.adr_dir.glob("adr-*.md"))
        next_num = len(existing_adrs) + 1
        filename = self.adr_dir / f"adr-{next_num:03d}-{self.slugify(title)}.md"

        # 生成内容
        content = TEMPLATE.format(
            title=title,
            status=status,
            date=datetime.now().strftime("%Y-%m-%d"),
            deciders=deciders,
            technical_story="为什么需要这个决策",
            context="描述决策的背景和上下文",
            decision="描述决策内容",
        )

        # 写入文件
        with open(filename, 'w', encoding='utf-8') as f:
            f.write(content)

        print(f"✅ 创建 ADR: {filename}")
        print(f"   标题: {title}")
        print(f"   状态: {status}")

    @staticmethod
    def slugify(text):
        """转换为文件名安全的格式"""
        text = text.lower()
        text = re.sub(r'[^\w\s-]', '', text)
        text = re.sub(r'[-\s]+', '-', text)
        return text.strip('-')

def main():
    import sys

    if len(sys.argv) < 2:
        print("用法:")
        print("  python scripts/adr_tools.py list              # 列出所有 ADR")
        print("  python scripts/adr_tools.py new <title>       # 创建新 ADR")
        print("  python scripts/adr_tools.py new <title> --status=accepted")
        return

    command = sys.argv[1]
    tool = ADRTool()

    if command == "list":
        tool.list_adrs()
    elif command == "new":
        if len(sys.argv) < 3:
            print("❌ 请提供 ADR 标题")
            return
        title = sys.argv[2]
        status = "proposed"
        if "--status=" in sys.argv[-1]:
            status = sys.argv[-1].split("=")[1]
        tool.new_adr(title, status=status)

if __name__ == "__main__":
    main()
```

### 使用示例

```bash
# 列出所有 ADR
python scripts/adr_tools.py list

# 创建新 ADR（默认状态：proposed）
python scripts/adr_tools.py new "采用六边形架构"

# 创建已接受的 ADR
python scripts/adr_tools.py new "统一命名空间" --status=accepted
```

### 优缺点

| 优点 | 缺点 |
|------|------|
| ✅ 完全可控 | ❌ 需要自己维护代码 |
| ✅ 无外部依赖 | ❌ 功能简单（需自行扩展） |
| ✅ 可集成项目工具 | ❌ 无社区支持 |
| ✅ 符合项目命名规范 | ❌ 学习曲线（如果需要修改） |

---

## ❌ 不推荐的方案

### 1. adr-tools (Bash)

**项目**: [npryce/adr-tools](https://github.com/npryce/adr-tools) - 原始 Bash 脚本

**不推荐理由**：
- ❌ Windows 支持差（需要 WSL 或 Git Bash）
- ❌ Bash 脚本在 Windows 上路径处理有问题
- ❌ 依赖 Unix 工具（sed、grep、find）

**适用场景**：
- ⚠️ 仅在 Linux/macOS 服务器上使用
- ⚠️ 团队所有人使用 WSL

---

### 2. ADRust (Rust)

**项目**: [omallassi/adrust](https://github.com/omallassi/adrust) - Rust 实现

**不推荐理由**：
- ❌ 需要安装 Rust 工具链（rustc、cargo）
- ❌ 需要编译（复杂度高）
- ❌ 项目中无 Rust 依赖（引入新技术栈）

**适用场景**：
- ⚠️ 团队已有 Rust 开发经验
- ⚠️ 需要极致性能（ADR 工具不需要）

---

### 3. log4brains

**项目**: [thomvaill/log4brains](https://github.com/thomvaill/log4brains)

**不推荐理由**：
- ❌ 过于复杂（小团队不需要）
- ❌ 需要额外配置和依赖
- ❌ 学习曲线陡峭

**适用场景**：
- ⚠️ 大型团队（20+ 人）
- ⚠️ 需要知识库和决策管理集成

---

## 🎯 最终推荐

### 对于 Siligen 项目（当前状态）

**推荐：adr (npm)**

**核心理由**：

1. **环境匹配** ✅
   - 已有 Node.js v24.12.0
   - Windows 原生支持
   - 一行命令安装

2. **团队匹配** ✅
   - 小团队（2-5 人）
   - 无需复杂功能
   - 学习曲线平缓

3. **功能匹配** ✅
   - 核心 ADR 功能完整
   - Markdown 原生支持
   - 符合项目 `docs/adr/` 结构

4. **维护成本** ✅
   - 社区活跃维护
   - 无需自研工具
   - 长期稳定可靠

---

## 📝 实施计划

### 第一阶段：安装和配置（5分钟）

```bash
# 1. 安装 adr
npm install -g adr

# 2. 备份现有 ADR
cp -r docs/adr docs/adr.backup

# 3. 初始化（指定目录）
adr init en --directory docs/adr

# 4. 验证
adr list
```

### 第二阶段：迁移现有 ADR（10分钟）

```bash
# 1. 检查现有 ADR
ls docs/adr/

# 2. 手动重命名（如果需要）
# adr-001-hexagonal-architecture.md → 0001-hexagonal-architecture.md

# 3. 更新模板（可选）
# 编辑 docs/adr/template.md 符合项目规范
```

### 第三阶段：团队培训（15分钟）

**培训内容**：
1. ADR 是什么？（5 分钟）
2. 如何使用 adr 工具？（5 分钟）
3. 实战演练：创建一个 ADR（5 分钟）

**实战演练**：
```bash
# 让团队成员创建自己的第一个 ADR
adr new "为什么我选择这个方案"
```

### 第四阶段：集成到工作流（持续）

**Pre-commit Hook**：
```bash
# .git/hooks/pre-commit
# 检查新 ADR 是否已添加到索引
if git diff --cached --name-only | grep -q "docs/adr/"; then
    echo "📋 检测到 ADR 变更，运行 adr list..."
    adr list
fi
```

**GitHub Actions**（可选）：
```yaml
name: ADR Check

on: [pull_request]

jobs:
  check-adr:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install ADR tool
        run: npm install -g adr
      - name: List ADRs
        run: adr list
```

---

## 📊 工具对比总结表

| 工具 | 安装难度 | 学习曲线 | Windows 支持 | 功能完整度 | 推荐度 |
|------|---------|---------|-------------|-----------|--------|
| **adr (npm)** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | 🥇 |
| **adr-log** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | 🥈 |
| **Python 脚本** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | 🥉 |
| **adr-tools (Bash)** | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐ | ⭐⭐⭐ | ❌ |
| **ADRust** | ⭐ | ⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ | ❌ |

---

## 🔗 参考资料

- [adr GitHub Repository](https://github.com/phodal/adr)
- [adr npm Package](https://www.npmjs.com/package/adr)
- [adr-log npm Package](https://www.npmjs.com/package/adr-log)
- [adr-tools Original (Bash)](https://github.com/npryce/adr-tools)
- [ADRust - Rust Implementation](https://github.com/omallassi/adrust)
- [log4brains - Advanced ADR Management](https://github.com/thomvaill/log4brains)
- [Markdown ADR Examples](https://github.com/joelparkerhenderson/architecture-decision-record)
- [Microsoft's ADR Guide](https://microsoft.github.io/code-with-engineering-playbook/design/design-reviews/decision-log/doc/adr/0001-record-architecture-decisions/)
- [ADR - Architecture Decision Records Guide](https://www.hascode.com/managing-architecture-decision-records-with-adr-tools/)

---

**文档创建时间**：2026-01-11
**最后更新**：2026-01-11
**维护者**：@Xike
