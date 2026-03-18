---
Owner: @Xike
Status: draft
Created: 2026-01-11
Scope: ADR 工具自动化能力分析
---

# ADR 工具自动化能力对比

> **问题**：adr (npm) 是自动化的吗？
>
> **答案**：**部分自动化**（半自动化工具）

---

## 📊 自动化程度分级

| 级别 | 自动化程度 | 定义 |
|------|-----------|------|
| **L1 - 手动** | ❌ 完全手动，无工具辅助 |
| **L2 - 半自动** | 🟡 辅助创建，需手动维护 |
| **L3 - 高度自动** | 🟢 自动创建，部分自动维护 |
| **L4 - 全自动** | ✅ 端到端自动化，无需人工干预 |

---

## 🎯 adr (npm) 的自动化能力

### 自动化功能清单

| 功能 | 自动化程度 | 说明 |
|------|-----------|------|
| **自动编号** | ✅ L3 | 自动分配 0001, 0002... |
| **自动创建文件** | ✅ L3 | 根据模板自动生成 Markdown 文件 |
| **自动填充模板** | ✅ L3 | 自动填充标题、日期、状态等字段 |
| **自动生成目录** | ✅ L2 | 创建 `docs/adr/` 目录 |
| **列表显示** | 🟡 L2 | 显示所有 ADR（但无标题） |
| **链接更新** | ❌ L1 | 修改 ADR 后需手动更新相关链接 |
| **索引生成** | ❌ L1 | 需手动维护 README.md 索引 |
| **状态同步** | ❌ L1 | ADR 被替代后需手动更新状态 |
| **格式验证** | ❌ L1 | 无自动格式检查 |
| **交叉引用** | ❌ L1 | 相关 ADR 需手动链接 |

**总体评级**：**🟡 L2 - 半自动化工具**

---

## 🔍 详细示例

### ✅ 自动化功能示例

**1. 自动编号**
```bash
$ adr new "采用六边形架构"
✅ Created ADR: docs/adr/0001-adopt-hexagonal-architecture.md

$ adr new "使用 Result<T> 错误处理"
✅ Created ADR: docs/adr/0002-use-result-error-handling.md
```
- ✅ 无需手动编号
- ✅ 自动递增编号

**2. 自动创建文件**
```bash
$ adr new "添加日志系统"
```
生成的文件：
```markdown
# 添加日志系统

## Status
Proposed

## Date
2026-01-11

## Context
[需手动填写]

## Decision
[需手动填写]

## Consequences
- **正面**：
- **负面**：
```
- ✅ 文件自动创建
- ✅ 模板自动填充
- ⚠️ 核心内容需手动填写

**3. 自动目录创建**
```bash
$ adr init en
✅ Created directory: doc/adr/
✅ Created template: doc/adr/template.md
```
- ✅ 无需手动创建目录

---

### ❌ 非自动化功能示例

**1. 链接更新（需手动）**

场景：ADR-002 替代了 ADR-001

```markdown
# docs/adr/0002-new-approach.md

## Supersedes
- [ADR-0001](./0001-old-approach.md)  # ❌ 需手动添加
```

同时需要手动更新 ADR-0001：
```markdown
# docs/adr/0001-old-approach.md

## Status
Superseded by [ADR-0002](./0002-new-approach.md)  # ❌ 需手动添加
```

**2. 索引维护（需手动）**

`docs/adr/README.md` 需要手动更新：
```markdown
## ADR 索引

| 编号 | 标题 | 状态 | 日期 |
|------|------|------|------|
| [ADR-0001](./0001-...) | 采用六边形架构 | Accepted | 2026-01-11 |  # ❌ 需手动添加
| [ADR-0002](./0002-...) | 使用 Result<T> | Proposed | 2026-01-12 |  # ❌ 需手动添加
```

**3. 状态同步（需手动）**

决策变更后需手动更新：
```markdown
## Status
Accepted  # ❌ 需从 Proposed 改为 Accepted
```

---

## 🆚 更自动化的工具对比

### 1. @lordcraymen/adr-toolkit

**项目**: [@lordcraymen/adr-toolkit](https://www.npmjs.com/package/@lordcraymen/adr-toolkit)

**自动化特性**：
- ✅ **自动创建** ADR 文档
- ✅ **自动验证/检查**（Linting）
- ✅ **自动发布**（Publishing）
- ✅ **专为 AI Agent 设计**，无需手动 Markdown 格式化

**与 adr (npm) 的区别**：

| 功能 | adr (npm) | @lordcraymen/adr-toolkit |
|------|----------|------------------------|
| 自动编号 | ✅ | ✅ |
| 自动创建 | ✅ | ✅ |
| 格式验证 | ❌ | ✅ |
| 自动发布 | ❌ | ✅ |
| AI 友好 | ❌ | ✅ |

**安装**：
```bash
npm install -g @lordcraymen/adr-toolkit
```

---

### 2. @adr-manager/cli

**项目**: [@adr-manager/cli](https://www.npmjs.com/package/@adr-manager/cli)

**自动化特性**：
- ✅ **自动文档生成**
- ✅ **结构化格式化**
- ✅ **命令简单**：`adr create 'Decision title'`

**使用示例**：
```bash
$ npm install -g @adr-manager/cli
$ adr create '使用 PostgreSQL 作为数据库'
✅ Generated document inside adr/ folder
✅ Named as 0001-use-postgresql-as-database.md
```

---

### 3. adr-log

**项目**: [adr-log](https://www.npmjs.com/package/adr-log)

**自动化特性**：
- ✅ **自动生成决策日志**（从多个 ADR 聚合）
- ✅ **显示标题**（改进自 adr-tools）

**使用示例**：
```bash
$ npm install -g adr-log
$ adr-log list
0001: 采用六边形架构 (accepted)  # ✅ 显示标题
0002: 使用 Result<T> (proposed)  # ✅ 显示标题
```

---

## 📊 工具自动化能力对比表

| 工具 | 自动编号 | 自动创建 | 格式验证 | 自动发布 | 标题显示 | 日志生成 | AI 友好 | 总体评级 |
|------|---------|---------|---------|---------|---------|---------|---------|---------|
| **adr (npm)** | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | 🟡 L2 |
| **@lordcraymen/adr-toolkit** | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ✅ | 🟢 L3 |
| **@adr-manager/cli** | ✅ | ✅ | ❌ | ❌ | ✅ | ❌ | ❌ | 🟡 L2+ |
| **adr-log** | ✅ | ✅ | ❌ | ❌ | ✅ | ✅ | ❌ | 🟢 L3 |
| **Python 脚本（自研）** | ✅ | ✅ | ✅ | ❌ | ✅ | ✅ | ❌ | 🟢 L3 |

---

## 🤔 应该选择哪个工具？

### 决策树

```
需要最高的自动化？
├─ 是 → @lordcraymen/adr-toolkit（AI 友好，格式验证）
└─ 否 → 需要显示 ADR 标题？
    ├─ 是 → adr-log（列表显示标题）
    └─ 否 → adr（简单轻量）
```

### 针对当前项目的推荐

**项目特征**：
- ✅ 小团队（2-5 人）
- ✅ 已有 Node.js
- ✅ 追求简洁
- ⚠️ 需要一定的自动化

**推荐方案**：

#### 方案A：保持 adr (npm) + 自定义脚本（推荐）

**理由**：
- ✅ 简单轻量，学习曲线平缓
- ✅ 通过自定义脚本补充自动化

**补充脚本**：创建 `scripts/adr_helpers.py`

```python
#!/usr/bin/env python3
"""
ADR 自动化辅助脚本
补充 adr (npm) 的自动化功能
"""

import re
from pathlib import Path
from datetime import datetime

ADR_DIR = Path("docs/adr")

def generate_index():
    """自动生成 README.md 索引"""
    adrs = sorted(ADR_DIR.glob("adr-*.md"))

    index_content = """# Architecture Decision Records (ADR)

本目录包含 siligen-motion-controller 项目的所有重要架构决策记录。

## ADR 索引

| 编号 | 标题 | 状态 | 日期 |
|------|------|------|------|
"""
    for adr_file in adrs:
        # 提取信息
        with open(adr_file, 'r', encoding='utf-8') as f:
            content = f.read()
            title_match = re.search(r'# (.+?)\n', content)
            title = title_match.group(1) if title_match else adr_file.stem
            status_match = re.search(r'Status: (.+?)\n', content)
            status = status_match.group(1) if status_match else "unknown"
            date_match = re.search(r'Date: (\d{4}-\d{2}-\d{2})', content)
            date = date_match.group(1) if date_match else "unknown"

        index_content += f"| [{adr_file.stem}]({adr_file.name}) | {title} | {status} | {date} |\n"

    # 写入 README.md
    readme_path = ADR_DIR / "README.md"
    with open(readme_path, 'w', encoding='utf-8') as f:
        f.write(index_content)

    print(f"✅ 更新索引: {len(adrs)} 个 ADR")

def update_superseded_links(superseded_adr, new_adr):
    """自动更新被替代的 ADR 链接"""
    old_file = ADR_DIR / superseded_adr
    new_file = ADR_DIR / new_adr

    if not old_file.exists():
        print(f"❌ 文件不存在: {old_file}")
        return

    # 更新旧 ADR
    with open(old_file, 'r', encoding='utf-8') as f:
        content = f.read()

    # 添加 Superseded by 链接
    if "Superseded by" not in content:
        content += f"\n\n## Superseded by\n[ADR-{new_adr.split('-')[1]}](./{new_adr})\n"

        with open(old_file, 'w', encoding='utf-8') as f:
            f.write(content)

        print(f"✅ 更新 {superseded_adr}：添加 Superseded by 链接")
    else:
        print(f"⚠️ {superseded_adr} 已经有 Superseded by 链接")

def validate_adr_format(adr_file):
    """验证 ADR 格式是否符合规范"""
    required_sections = ["Status", "Date", "Context", "Decision", "Consequences"]

    with open(adr_file, 'r', encoding='utf-8') as f:
        content = f.read()

    missing = []
    for section in required_sections:
        if section not in content:
            missing.append(section)

    if missing:
        print(f"❌ {adr_file.name} 缺少必需章节: {', '.join(missing)}")
        return False
    else:
        print(f"✅ {adr_file.name} 格式正确")
        return True

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("用法:")
        print("  python scripts/adr_helpers.py index              # 生成索引")
        print("  python scripts/adr_helpers.py supersede <old> <new>  # 更新替代链接")
        print("  python scripts/adr_helpers.py validate <adr-file>   # 验证格式")
        return

    command = sys.argv[1]

    if command == "index":
        generate_index()
    elif command == "supersede":
        if len(sys.argv) < 4:
            print("❌ 请提供旧 ADR 和新 ADR 文件名")
            return
        update_superseded_links(sys.argv[2], sys.argv[3])
    elif command == "validate":
        if len(sys.argv) < 3:
            print("❌ 请提供 ADR 文件名")
            return
        validate_adr_format(ADR_DIR / sys.argv[2])
```

**使用示例**：
```bash
# 自动生成索引
python scripts/adr_helpers.py index

# 自动更新替代链接
python scripts/adr_helpers.py supersede adr-001-old.md adr-002-new.md

# 验证格式
python scripts/adr_helpers.py validate adr-001-hexagonal-architecture.md
```

---

#### 方案B：升级到 @lordcraymen/adr-toolkit（备选）

**适用场景**：
- ✅ 需要 AI Agent 辅助创建 ADR
- ✅ 需要格式自动验证
- ✅ 需要自动发布功能

**安装**：
```bash
npm uninstall -g adr  # 卸载旧工具
npm install -g @lordcraymen/adr-toolkit
```

---

## 🔧 Git Hooks 集成（自动化补充）

### Pre-commit Hook：自动更新索引

创建 `.git/hooks/pre-commit`：

```bash
#!/bin/bash
# ADR Pre-commit Hook

# 检查是否有 ADR 变更
if git diff --cached --name-only | grep -q "docs/adr/"; then
    echo "📋 检测到 ADR 变更"

    # 自动生成索引
    python scripts/adr_helpers.py index

    # 验证所有 ADR 格式
    for adr_file in docs/adr/adr-*.md; do
        python scripts/adr_helpers.py validate "$(basename $adr_file)"
    done

    # 将索引添加到提交
    git add docs/adr/README.md

    echo "✅ ADR 索引已自动更新"
fi
```

---

### GitHub Actions：自动化验证

创建 `.github/workflows/adr-check.yml`：

```yaml
name: ADR Check

on:
  pull_request:
    paths:
      - 'docs/adr/**'

jobs:
  validate-adrs:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3

      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.12'

      - name: Validate ADR format
        run: |
          python scripts/adr_helpers.py index
          for adr_file in docs/adr/adr-*.md; do
            python scripts/adr_helpers.py validate "$(basename $adr_file)"
          done
```

---

## 📊 最终推荐总结

### 对于当前项目（Siligen）

**推荐：adr (npm) + 自定义脚本**

**理由**：
1. ✅ **简单轻量** - 团队学习成本低
2. ✅ **足够自动化** - 自动编号、创建、模板
3. ✅ **可控性强** - 通过 Python 脚本补充自动化
4. ✅ **成本最低** - 无需更换工具

**自动化覆盖**：
- ✅ 自动编号（adr 原生）
- ✅ 自动创建（adr 原生）
- ✅ 自动索引生成（自定义脚本）
- ✅ 自动格式验证（自定义脚本）
- ✅ 自动链接更新（自定义脚本 + Git Hooks）

---

## 🔗 参考资料

- **adr (npm)**: https://www.npmjs.com/package/adr
- **@lordcraymen/adr-toolkit**: https://www.npmjs.com/package/@lordcraymen/adr-toolkit
- **@adr-manager/cli**: https://www.npmjs.com/package/@adr-manager/cli
- **adr-log**: https://www.npmjs.com/package/adr-log
- **ADR GitHub**: https://github.com/phodal/adr
- **Using ADR with adr-tools**: https://jon.sprig.gs/blog/post/2101

---

**文档创建时间**：2026-01-11
**最后更新**：2026-01-11
**维护者**：@Xike
