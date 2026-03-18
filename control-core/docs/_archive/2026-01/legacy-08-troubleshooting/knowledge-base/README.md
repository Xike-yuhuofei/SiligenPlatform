# 项目问题档案库

## 概述

本知识库用于记录和管理 Siligen 工业控制系统开发过程中的问题和解决方案，旨在积累经验、提高效率、促进知识共享。

## 目录结构

```
knowledge-base/
├── index/                  # 索引和元数据
│   ├── problems.json      # 问题数据库
│   ├── tags.json          # 标签分类体系
│   └── search-index.json  # 搜索索引（自动生成）
├── templates/             # 文档模板
│   ├── problem-record.md   # 问题记录模板
│   ├── solution-pattern.md # 解决方案模式模板
│   └── lessons-learned.md  # 经验教训模板
├── patterns/              # 故障模式库
│   ├── compilation/       # 编译问题
│   ├── runtime/           # 运行时问题
│   ├── hardware/          # 硬件问题
│   └── integration/       # 集成问题
└── automation/            # 自动化工具
    ├── problem-parser.py   # 问题解析器
    ├── index-builder.py    # 索引构建器
    └── knowledge-graph.py  # 知识图谱生成器
```

## 快速开始

### 1. 记录新问题

1. 复制模板文件：
   ```bash
   cp templates/problem-record.md patterns/[category]/PRB-YYYY-XXX-brief-name.md
   ```

2. 填写问题信息：
   - 完整的问题描述和症状
   - 环境信息和复现步骤
   - 根本原因分析
   - 详细的解决方案

3. 更新索引：
   ```bash
   python automation/index-builder.py
   ```

### 2. 查找问题

#### 按类别浏览
- **编译问题**: `patterns/compilation/`
- **运行时问题**: `patterns/runtime/`
- **硬件问题**: `patterns/hardware/`
- **集成问题**: `patterns/integration/`

#### 关键词搜索
```bash
# 搜索包含特定关键词的问题
grep -r "关键词" patterns/
```

#### 使用索引查询
```python
import json
with open('index/problems.json') as f:
    data = json.load(f)

# 查找特定类别的问题
compilation_problems = [p for p in data['problems']
                       if p['category'] == 'compilation']
```

### 3. 贡献指南

#### 问题记录规范
- 使用标准模板
- 提供完整的复现步骤
- 包含具体的错误信息
- 记录详细的解决方案
- 提取可复用的模式

#### 命名规范
- 问题文件：`PRB-YYYY-XXX-简短描述.md`
- 格式模式：`PAT-YYYY-XXX-模式名称.md`
- 经验总结：`LES-YYYY-XXX-主题.md`

#### 标签使用
- 必须指定主要类别
- 选择适当的环境标签
- 添加相关的技术标签
- 标明严重程度和状态

## 自动化工具

### 问题解析器
```bash
python automation/problem-parser.py --file build.log
```
功能：从构建日志中自动提取错误信息

### 索引构建器
```bash
python automation/index-builder.py
```
功能：重建问题数据库索引

### 知识图谱生成器
```bash
python automation/knowledge-graph.py
```
功能：生成问题关联图和模式分析

## 最佳实践

### 问题记录
- **完整性**：包含所有必要信息
- **准确性**：确保技术细节正确
- **可复现**：提供清晰的复现步骤
- **可搜索**：使用适当的关键词和标签

### 解决方案
- **具体化**：提供详细的操作步骤
- **代码化**：包含必要的代码示例
- **验证化**：说明验证方法
- **通用化**：提炼可复用的模式

### 经验总结
- **定期性**：定期回顾和总结
- **系统化**：分类整理问题模式
- **前瞻性**：制定预防措施
- **共享性**：促进团队知识分享

## 维护

### 日常维护
- 及时更新问题状态
- 定期清理过时信息
- 维护索引的准确性
- 备份重要数据

### 定期回顾
- 月度问题分析会议
- 季度经验教训总结
- 年度知识库优化
- 工具和方法升级

## 联系方式

如有问题或建议，请联系：
- **知识库维护者**：[邮箱]
- **技术支持**：[邮箱]
- **文档反馈**：[Issue链接]

---

*版本*: 1.0
*创建日期*: 2025-01-09
*维护者*: 开发团队