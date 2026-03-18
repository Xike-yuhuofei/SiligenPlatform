# 激进式重构目录迁移映射

**日期**: 2026-01-02
**目的**: 记录所有目录和文件的迁移路径
**原则**: 最小化业务中断，保持向后兼容

---

## 📊 当前状态

**根目录数量**: 19个
**目标数量**: 15个
**预计减少**: 4个 (-21%)

---

## 🔄 完整迁移映射

### 1. 运行时目录 → `output/`

| 原目录 | 新位置 | 状态 | 说明 |
|--------|--------|------|------|
| `logs/` | `output/logs/` | ⏳ 待迁移 | 运行时日志 |
| `data/` | `output/data/` | ⏳ 待迁移 | 运行时数据文件 |
| `reports/` | `output/reports/` | ⏳ 待迁移 | 测试报告、分析报告 |
| `buildlogs/` | `output/buildlogs/` | ⏳ 待迁移 | 构建日志 |

**行动**:
```powershell
# 创建output子目录
mkdir -p output/logs output/data output/reports output/buildlogs

# 迁移内容（如果存在）
Move-Item -Path logs/* -Destination output/logs/ -ErrorAction SilentlyContinue
Move-Item -Path data/* -Destination output/data/ -ErrorAction SilentlyContinue
Move-Item -Path reports/* -Destination output/reports/ -ErrorAction SilentlyContinue
Move-Item -Path buildlogs/* -Destination output/buildlogs/ -ErrorAction SilentlyContinue

# 删除原目录
Remove-Item -Path logs,data,reports,buildlogs -Recurse -Force -ErrorAction SilentlyContinue
```

---

### 2. 配置目录合并 → `.config/`

| 原目录 | 新位置 | 状态 | 说明 |
|--------|--------|------|------|
| `config/` | `.config/` | ⏳ 待合并 | 主配置目录 |
| `configs/` | `.config/` | ⏳ 待合并 | 重复的配置目录 |

**调查**:
```powershell
# 检查两个目录内容差异
Compare-Object (Get-ChildItem config/) (Get-ChildItem configs/)
```

**行动**:
```powershell
# 合并configs/到config/
Copy-Item -Path configs/* -Destination config/ -Recurse -Force

# 重命名config/为.config/
Rename-Item -Path config -NewName .config

# 删除configs/
Remove-Item -Path configs -Recurse -Force
```

---

### 3. 测试目录合并 → `tests/`

| 原目录 | 新位置 | 状态 | 说明 |
|--------|--------|------|------|
| `test/` | `tests/` | ⏳ 待合并 | 旧的测试目录 |
| `tests/` | `tests/` | ✅ 保留 | 主要测试目录 |

**调查**:
```powershell
# 检查test/目录是否有独特内容
Get-ChildItem test/ -Recurse
```

**行动**:
```powershell
# 合并test/到tests/
Copy-Item -Path test/* -Destination tests/ -Recurse -Force

# 删除test/
Remove-Item -Path test -Recurse -Force
```

---

### 4. Web适配器迁移 → `src/adapters/web/`

| 原目录 | 新位置 | 状态 | 说明 |
|--------|--------|------|------|
| `web/` | `src/adapters/web/` | ⏳ 待迁移 | Web前端和API适配器 |

**调查**:
- 检查`web/`目录结构
- 确认是否包含前端代码和后端适配器
- 验证CMakeLists.txt依赖关系

**行动**:
```powershell
# 迁移web/到src/adapters/web/
Move-Item -Path web/* -Destination src/adapters/web/ -Recurse -Force

# 删除web/
Remove-Item -Path web -Recurse -Force
```

---

### 5. 备份目录处理

| 原目录 | 新位置 | 状态 | 说明 |
|--------|--------|------|------|
| `backups/` | 保持不变 | ⚠️ 需清理 | 建议迁移到外部存储 |

**行动**:
```powershell
# 可选：将backups/移到项目外部
# Move-Item -Path backups -Destination D:/Backups/Backend_CPP/ -Recurse
```

---

### 6. 废弃目录清理

| 原目录 | 处理方式 | 状态 | 说明 |
|--------|----------|------|------|
| `protos/` | 删除 | ⏳ 待确认 | gRPC已于2025-12-30移除 (ADR-004) |

**行动**:
```powershell
# 确认protos/不再需要后删除
Remove-Item -Path protos -Recurse -Force -ErrorAction SilentlyContinue
```

---

## 📁 最终目录结构

```
Backend_CPP/
├── .config/          # 统一配置目录（合并config/ + configs/）
├── .gitignore        # 更新以忽略output/
├── build/            # 构建输出（保持不变）
├── cmake/            # CMake模块（保持不变）
├── docs/             # 文档（保持不变）
├── images/           # 图片资源（保持不变）
├── output/           # 新增：运行时输出
│   ├── logs/         # 运行时日志
│   ├── data/         # 运行时数据
│   ├── reports/      # 测试/分析报告
│   └── buildlogs/    # 构建日志
├── scripts/          # 构建和工具脚本（保持不变）
├── src/              # 源代码
│   ├── adapters/
│   │   └── web/      # ← 迁移自web/
│   ├── application/
│   ├── domain/
│   ├── infrastructure/
│   └── shared/
├── tests/            # 统一测试目录（合并test/ + tests/）
├── third_party/      # 第三方库（保持不变）
└── REFACTORING_MIGRATION_MAP.md  # 本文档
```

**根目录数量**: 15个 ✓

---

## 🔄 依赖更新清单

### CMakeLists.txt 更新

1. **web/ → src/adapters/web/**
```cmake
# 旧路径
add_subdirectory(web)

# 新路径
add_subdirectory(src/adapters/web)
```

2. **test/ + tests/ → tests/**
```cmake
# 旧路径
add_subdirectory(test)
add_subdirectory(tests)

# 新路径
add_subdirectory(tests)
```

3. **config/ + configs/ → .config/**
```cmake
# 旧路径
set(CONFIG_DIR ${CMAKE_SOURCE_DIR}/config)
set(CONFIGS_DIR ${CMAKE_SOURCE_DIR}/configs)

# 新路径
set(CONFIG_DIR ${CMAKE_SOURCE_DIR}/.config)
```

---

### 构建脚本更新

**scripts/build.ps1**:
```powershell
# 更新日志路径
$LOG_DIR = "$PROJECT_ROOT/output/logs"

# 更新构建输出路径
$BUILDLOG_DIR = "$PROJECT_ROOT/output/buildlogs"
```

---

### IDE 配置更新

**VS Code (.vscode/settings.json)**:
```json
{
  "files.exclude": {
    "output/**": true,
    "build/**": true,
    "**/*.exe": true
  }
}
```

---

## ✅ 验证清单

迁移完成后，验证以下项目：

- [ ] 根目录数量为15个
- [ ] `output/`目录包含4个子目录
- [ ] `.config/`目录包含合并后的配置文件
- [ ] `tests/`目录包含合并后的测试
- [ ] `src/adapters/web/`包含原`web/`内容
- [ ] CMakeLists.txt中所有路径引用已更新
- [ ] scripts/build.ps1路径已更新
- [ ] `.gitignore`已添加`output/`
- [ ] 构建成功 (`.\scripts\build.ps1`)
- [ ] 所有测试通过 (`.\scripts\build.ps1 tests`)
- [ ] CI/CD流水线通过（如适用）

---

## 🚨 回滚计划

如果迁移失败，按以下步骤回滚：

```powershell
# 1. 从git恢复
git checkout HEAD -- .

# 2. 或从备份恢复
# 假设已创建完整备份
Copy-Item -Path ../Backend_CPP_Backup/* -Destination . -Recurse -Force
```

---

## 📝 执行日志

| 时间 | 操作 | 状态 | 备注 |
|------|------|------|------|
| 2026-01-02 09:00 | 创建迁移映射文档 | ✅ | 初始版本 |
| 2026-01-02 09:15 | 更新.gitignore | ⏳ | 添加output/目录 |
| 2026-01-02 09:30 | 备份当前结构 | ⏳ | 创建完整备份 |
| 2026-01-02 10:00 | 执行目录迁移 | ⏳ | 按阶段执行 |

---

**维护者**: Claude Code
**审查者**: 待定
**下次审查**: 2026-02-02
