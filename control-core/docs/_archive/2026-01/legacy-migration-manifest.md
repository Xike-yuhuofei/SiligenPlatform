# Legacy文档迁移清单

> **迁移日期**: 2026-01-11
> **迁移人**: @Xike
> **迁移原因**: 整合历史文档至统一的归档系统

---

## 迁移映射表

### 保留在 _legacy/ 的文档

以下文档因已在 `library/` 有对应版本,保留原位置作为历史参考:

| 原路径 | 迁移至 | 理由 | 对应新文档 |
|--------|--------|------|------------|
| `_legacy/01-architecture/` | `_archive/2026-01/legacy-01-architecture/` | 架构设计历史 | `library/02_architecture.md` |
| `_legacy/01-getting-started/` | `_archive/2026-01/legacy-01-getting-started/` | 快速开始旧版 | `library/04_guides/` |
| `_legacy/02-api/multicard/` | `_archive/2026-01/legacy-02-api/` | 博派API文档 | `library/06_reference.md` + `docs/02-api/` |
| `_legacy/03-hardware/` | `_archive/2026-01/legacy-03-hardware/` | 硬件集成历史 | `library/06_reference.md` (硬件章节) |
| `_legacy/04-development/` | `_archive/2026-01/legacy-04-development/` | 开发流程历史 | `docs/CLAUDE.md` |
| `_legacy/05-testing/` | `_archive/2026-01/legacy-05-testing/` | 测试文档历史 | `docs/04-development/` |
| `_legacy/06-user-guides/` | `_archive/2026-01/legacy-06-user-guides/` | 用户指南旧版 | `library/04_guides/` |
| `_legacy/08-troubleshooting/` | `_archive/2026-01/legacy-08-troubleshooting/` | 故障排查历史 | `library/05_runbook.md` |
| `_legacy/09-reports/` | `_archive/2026-01/legacy-09-reports/` | 历史报告 | `_archive/2026-01/reports/` |

### 已迁移至主题目录的文档

| 原路径 | 迁移至 | 分类 |
|--------|--------|------|
| `_legacy/analysis/` | `_archive/2026-01/analysis/` | 分析文档 |
| `_legacy/configuration/` | `_archive/2026-01/hardware-integration/` | 硬件集成 |
| `_legacy/features/` | `_archive/2026-01/exploration/` | 探索草稿 |
| `_legacy/fixes/` | `_archive/2026-01/refactoring/` | 重构记录 |
| `_legacy/deployment/` | `_archive/2026-01/exploration/deployment/` | 部署探索 |
| `_legacy/development/` | `_archive/2026-01/exploration/development/` | 开发探索 |
| `_legacy/architecture/` | `_archive/2026-01/exploration/architecture/` | 架构探索 |
| `_legacy/user-guide/` | `_archive/2026-01/exploration/user-guide/` | 用户指南探索 |
| `_legacy/root-level-md-files/` | `_archive/2026-01/` (根目录) | 零散指南 |
| `_legacy/10-archives/` | `_archive/2026-01/` (合并) | 历史归档 |
| `_legacy/reports/` | `_archive/2026-01/reports/` | 验证报告 |
| `_legacy/03-api-reference/` | `_archive/2026-01/legacy-03-api-reference/` | API参考历史 |
| `_legacy/05-observability/` | `_archive/2026-01/legacy-05-observability/` | 可观测性历史 |

---

## 迁移统计

- **总文件数**: 298个
- **迁移至主题目录**: ~50个
- **迁移至编号目录**: ~232个
- **保留在library**: ~48个(核心知识)

### 详细统计

| 主题目录 | 文件数 |
|----------|--------|
| `analysis/` | 5 |
| `exploration/` | 8 |
| `refactoring/` | 1 |
| `hardware-integration/` | 1 |
| `reports/` | 9 |
| `legacy-01-architecture/` | 11 |
| `legacy-01-getting-started/` | 2 |
| `legacy-02-api/` | 40+ |
| `legacy-03-api-reference/` | 待统计 |
| `legacy-03-hardware/` | 待统计 |
| `legacy-04-development/` | 待统计 |
| `legacy-05-observability/` | 待统计 |
| `legacy-05-testing/` | 待统计 |
| `legacy-06-user-guides/` | 待统计 |
| `legacy-08-troubleshooting/` | 待统计 |
| `legacy-09-reports/` | 待统计 |

---

## 访问指南

### 查找历史文档

1. **博派MultiCard API**:
   - 新文档: `library/06_reference.md` (第7节)
   - 旧文档: `_archive/2026-01/legacy-02-api/multicard/`

2. **故障排查**:
   - 新文档: `library/05_runbook.md`
   - 旧文档: `_archive/2026-01/legacy-08-troubleshooting/`

3. **架构设计**:
   - 新文档: `library/02_architecture.md`
   - 旧文档: `_archive/2026-01/legacy-01-architecture/`

4. **探索草稿和分析**:
   - 位置: `_archive/2026-01/analysis/`, `_archive/2026-01/exploration/`

---

## 三分分类法说明

根据 `docs/_archive/README.md` 的分类标准:

### Strong(代码可验证) - 已迁移至 library/
- API参考手册
- IO映射表
- 错误码定义
- 配置参数

### Medium(流程经验) - 已迁移至 library/
- 部署步骤
- 排障手册
- 用户指南
- 测试指南

### Weak(过程记录) - 迁移至 _archive/
- 探索草稿
- 对比分析
- 会议记录
- 临时排查记录

---

## 维护说明

1. **不要删除** `_archive/` 下的历史文档
2. **不要修改** `_legacy/` 目录(现已空)
3. 新的探索文档直接放入 `_archive/YYYY-MM/`
4. 有价值的知识应提炼至 `library/`

---

**迁移完成标志**: `_legacy/` 目录已清空或仅保留README
