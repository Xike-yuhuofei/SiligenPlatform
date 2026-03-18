# 项目问题档案库实施指南

## 快速开始

本指南帮助您快速建立和使用项目问题档案库系统。

## 第一步：创建基础目录结构

执行以下命令创建知识库目录结构：

```bash
# 在 docs 目录下创建知识库目录
mkdir -p docs/knowledge-base/index
mkdir -p docs/knowledge-base/templates
mkdir -p docs/knowledge-base/patterns
mkdir -p docs/knowledge-base/patterns/compilation
mkdir -p docs/knowledge-base/patterns/runtime
mkdir -p docs/knowledge-base/patterns/hardware
mkdir -p docs/knowledge-base/patterns/integration
mkdir -p docs/knowledge-base/automation
```

## 第二步：初始化索引文件

创建问题数据库索引：

```json
// docs/knowledge-base/index/problems.json
{
  "version": "1.0",
  "last_updated": "2025-01-09",
  "total_problems": 0,
  "problems": []
}
```

创建标签分类文件：

```json
// docs/knowledge-base/index/tags.json
{
  "categories": {
    "compilation": ["compiler", "linker", "build", "cmake", "qt-moc"],
    "runtime": ["crash", "exception", "memory", "performance"],
    "hardware": ["connection", "multicard", "driver", "io"],
    "integration": ["api", "module", "dependency", "compatibility"],
    "config": ["settings", "environment", "path", "permission"]
  },
  "severity": ["critical", "high", "medium", "low"],
  "status": ["active", "solved", "archived", "false_positive"]
}
```

## 第三步：创建问题记录模板

### 标准问题记录模板

使用 `docs/knowledge-base/templates/problem-record.md`：

```markdown
# 问题记录：[问题标题]

## 基本信息
- **问题ID**: PRB-YYYY-XXX
- **创建日期**: YYYY-MM-DD
- **创建者**: [姓名]
- **严重程度**: critical/high/medium/low
- **状态**: active/solved/archived
- **类别**: compilation/runtime/hardware/integration/config

## 问题描述
### 症状表现
- 症状1
- 症状2

### 环境信息
- **操作系统**: Windows/Linux
- **编译器**: MSVC/GCC/Clang
- **Qt版本**: [版本号]
- **硬件型号**: [型号]

### 复现步骤
1. 步骤1
2. 步骤2
3. 步骤3

## 根本原因分析
### 直接原因
[直接原因描述]

### 深层原因
[深层原因分析]

## 解决方案
### 修复步骤
1. [具体步骤1]
2. [具体步骤2]
3. [具体步骤3]

### 代码变更
```cpp
// 相关代码变更
```

### 验证方法
- [ ] 编译成功
- [ ] 测试通过
- [ ] 功能验证

## 相关资源
- **相关文档**: [链接]
- **相关代码**: [文件路径]
- **相关提交**: [commit hash]

## 预防措施
- [ ] 措施1
- [ ] 措施2

## 经验教训
- [ ] 教训1
- [ ] 教训2

## 后续跟进
- **负责人**: [姓名]
- **完成日期**: YYYY-MM-DD
- **跟进事项**: [事项列表]
```

## 第四步：迁移现有问题

### 从 docs/debug 迁移

将现有的调试问题标准化：

```bash
# 示例：迁移现有问题
cp docs/debug/2025-12-06-logger-include-error.md docs/knowledge-base/patterns/compilation/PRB-2025-001-logger-include-error.md
```

然后使用标准模板格式化内容。

### 创建问题索引

在 `problems.json` 中添加条目：

```json
{
  "problem_id": "PRB-2025-001",
  "title": "Logger头文件包含路径错误",
  "file": "patterns/compilation/PRB-2025-001-logger-include-error.md",
  "category": "compilation",
  "severity": "high",
  "status": "solved",
  "tags": ["logger", "include-path", "cmake"],
  "created_date": "2025-12-06",
  "resolved_date": "2025-12-07"
}
```

## 第五步：自动化工具使用

### 问题解析器

创建 `docs/knowledge-base/automation/problem-parser.py`：

```python
#!/usr/bin/env python3
"""
问题解析器 - 从日志和错误信息中提取结构化问题信息
"""

import re
import json
from datetime import datetime
from pathlib import Path

class ProblemParser:
    def __init__(self, knowledge_base_path):
        self.kb_path = Path(knowledge_base_path)
        self.problems_file = self.kb_path / "index" / "problems.json"

    def parse_build_log(self, log_file):
        """解析构建日志，提取错误信息"""
        errors = []
        with open(log_file, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                # 匹配常见编译错误模式
                if 'error:' in line.lower():
                    error_info = self._extract_error_info(line, line_num)
                    if error_info:
                        errors.append(error_info)
        return errors

    def _extract_error_info(self, line, line_num):
        """提取错误信息"""
        # 示例错误模式匹配
        patterns = {
            'undefined_reference': r'undefined reference to [`\']([^`\']+)[`\']',
            'file_not_found': r'([^\s:]+):(\d+):\d+: error: ([^:]+): No such file',
            'syntax_error': r'([^\s:]+):(\d+):\d+: error: ([^:]+)',
        }

        for error_type, pattern in patterns.items():
            match = re.search(pattern, line)
            if match:
                return {
                    'type': error_type,
                    'line': line_num,
                    'message': line.strip(),
                    'details': match.groups()
                }
        return None

# 使用示例
if __name__ == "__main__":
    parser = ProblemParser("docs/knowledge-base")
    errors = parser.parse_build_log("build.log")
    for error in errors:
        print(f"Found {error['type']}: {error['message']}")
```

### 索引构建器

创建 `docs/knowledge-base/automation/index-builder.py`：

```python
#!/usr/bin/env python3
"""
索引构建器 - 重建问题数据库索引
"""

import json
from pathlib import Path
from datetime import datetime

class IndexBuilder:
    def __init__(self, knowledge_base_path):
        self.kb_path = Path(knowledge_base_path)
        self.problems_file = self.kb_path / "index" / "problems.json"

    def rebuild_index(self):
        """重建完整的问题索引"""
        problems = []

        # 扫描所有问题文件
        for pattern_dir in self.kb_path.glob("patterns/*"):
            if pattern_dir.is_dir():
                for problem_file in pattern_dir.glob("PRB-*.md"):
                    problem_info = self._parse_problem_file(problem_file)
                    if problem_info:
                        problems.append(problem_info)

        # 写入索引文件
        index_data = {
            "version": "1.0",
            "last_updated": datetime.now().isoformat(),
            "total_problems": len(problems),
            "problems": problems
        }

        with open(self.problems_file, 'w', encoding='utf-8') as f:
            json.dump(index_data, f, indent=2, ensure_ascii=False)

        print(f"索引构建完成，共 {len(problems)} 个问题")

    def _parse_problem_file(self, file_path):
        """解析问题文件，提取元数据"""
        # 实现文件解析逻辑
        pass

# 使用示例
if __name__ == "__main__":
    builder = IndexBuilder("docs/knowledge-base")
    builder.rebuild_index()
```

## 第六步：工作流集成

### Git Hook 集成

在 `.git/hooks/pre-commit` 中添加：

```bash
#!/bin/sh
# 提交前检查是否有关联的问题记录

changed_files=$(git diff --cached --name-only)
if echo "$changed_files" | grep -q "src/"; then
    # 检查提交信息是否包含问题引用
    commit_msg=$(git log -1 --pretty=%B)
    if ! echo "$commit_msg" | grep -qE "(PRB-[0-9]+|Fixes|Closes)"; then
        echo "警告：代码修改建议关联相应的问题记录"
        echo "请在提交信息中包含问题ID，例如：PRB-2025-001"
        exit 1
    fi
fi
```

### CMake 集成

在 `CMakeLists.txt` 中添加问题检查：

```cmake
# 构建失败时自动创建问题记录
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_target(check-problems
        COMMAND python ${CMAKE_SOURCE_DIR}/docs/knowledge-base/automation/problem-parser.py
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "检查已知问题模式"
    )
endif()
```

## 第七步：团队使用指南

### 问题记录流程

1. **发现问题**
   - 使用标准模板创建问题记录
   - 放置在对应的 patterns 子目录
   - 文件命名：`PRB-YYYY-XXX-简短描述.md`

2. **分析问题**
   - 详细记录症状和环境
   - 进行根本原因分析
   - 记录尝试过的解决方案

3. **解决问题**
   - 记录最终解决方案
   - 包含代码变更和验证步骤
   - 更新问题状态为 "solved"

4. **总结经验**
   - 提取可复用的模式
   - 制定预防措施
   - 更新相关文档

### 问题检索方法

1. **按类别浏览**
   - 编译问题：`docs/knowledge-base/patterns/compilation/`
   - 运行时问题：`docs/knowledge-base/patterns/runtime/`
   - 硬件问题：`docs/knowledge-base/patterns/hardware/`

2. **关键词搜索**
   ```bash
   # 在所有问题文件中搜索关键词
   grep -r "关键词" docs/knowledge-base/patterns/
   ```

3. **索引查询**
   ```bash
   # 使用Python脚本查询索引
   python -c "
   import json
   with open('docs/knowledge-base/index/problems.json') as f:
       data = json.load(f)
   for problem in data['problems']:
       if 'qt' in problem.get('tags', []):
           print(f\"{problem['problem_id']}: {problem['title']}\")
   "
   ```

## 最佳实践

### 问题记录质量

- **完整性**：包含所有必要信息
- **准确性**：确保技术细节正确
- **可复现**：提供清晰的复现步骤
- **可搜索**：使用适当的关键词和标签

### 持续改进

- **定期回顾**：每周回顾新增问题
- **模式总结**：识别和总结常见模式
- **预防措施**：基于问题记录改进开发流程
- **知识分享**：在团队中分享有价值的解决方案

### 工具使用

- **IDE插件**：安装问题搜索插件（如果可用）
- **脚本自动化**：使用提供的Python脚本
- **版本控制**：将问题记录纳入版本管理
- **备份策略**：定期备份知识库数据

---

*文档版本: 1.0*
*创建日期: 2025-01-09*
*维护者: 开发团队*