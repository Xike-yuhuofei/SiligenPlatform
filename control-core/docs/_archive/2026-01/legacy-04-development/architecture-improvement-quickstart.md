# 架构改进快速实施指南

> **适用人群**: 开发者、DevOps工程师
> **前置条件**: 熟悉CMake、PowerShell、Python、C++
> **预计时间**: 2-3小时

---

## 1. 环境准备

### 1.1 验证工具链

```powershell
# 检查Python环境
python --version  # 应该是3.11+

# 检查PowerShell版本
$PSVersionTable.PSVersion  # 应该是7.0+

# 检查CMake
cmake --version  # 应该是3.20+

# 安装Python依赖
pip install pyyaml rich click tabulate
```

### 1.2 克隆配置文件

```bash
# 创建架构配置文件
cat > .claude/arch-config.json << 'EOF'
{
  "moduleStructure": {
    "domain": {
      "path": "src/domain",
      "allowedOutgoing": ["shared"]
    },
    "application": {
      "path": "src/application",
      "allowedOutgoing": ["domain", "shared"]
    },
    "infrastructure": {
      "path": "src/infrastructure",
      "allowedOutgoing": ["domain", "shared"]
    },
    "adapters": {
      "path": "src/adapters",
      "allowedOutgoing": ["application", "domain", "shared"]
    },
    "shared": {
      "path": "src/shared",
      "allowedOutgoing": []
    }
  },
  "dependencyRules": {
    "forbiddenIncludes": [
      {
        "from": "domain",
        "to": ["infrastructure", "adapters"],
        "severity": "critical"
      },
      {
        "from": "application",
        "to": ["infrastructure", "adapters"],
        "severity": "high"
      },
      {
        "from": "shared",
        "to": ["domain", "application", "infrastructure", "adapters"],
        "severity": "critical"
      }
    ]
  },
  "namespaceRules": {
    "domain": ["Siligen", "Domain"],
    "application": ["Siligen", "Application"],
    "infrastructure": ["Siligen", "Infrastructure"],
    "adapters": ["Siligen", "Adapters"],
    "shared": ["Siligen", "Shared"]
  }
}
EOF
```

---

## 2. 立即可执行的任务

### Task 1: 验证当前架构合规性

```powershell
# 运行Python架构分析器
python .claude/agents/arch_analyzer.py `
  --src src `
  --config .claude/arch-config.json `
  --output reports/arch-analysis.json `
  --format json `
  --verbose

# 查看结果
Get-Content reports/arch-analysis.json | ConvertFrom-Json | ConvertTo-Json -Depth 10
```

**预期输出**:
```json
{
  "score": 75,
  "violations": {
    "critical": 0,
    "high": 2,
    "medium": 5,
    "low": 3
  }
}
```

### Task 2: 手动检查跨层依赖

```powershell
# 搜索Domain层中的Infrastructure依赖
Select-String -Path "src/domain\**\*.h","src/domain\**\*.cpp" `
  -Pattern '#include.*infrastructure' `
  -CaseSensitive:$false
```

**预期**: 应该没有结果（如果Domain层依赖Infrastructure违规已修复）

### Task 3: 创建CMake宏文件

```cmake
# 文件: cmake/ArchitectureGuard.cmake
# 创建时间: 2026-01-08

macro(siligen_set_layer_includes target layer)
    if (layer STREQUAL "domain")
        # Domain层: 最严格，只能include自己和Shared
        target_include_directories(${target} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared/utils
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared/interfaces
        )
        message(STATUS "[${layer}] 设置严格include路径隔离")

    elseif (layer STREQUAL "application")
        # Application层: 可以include Domain和Shared
        target_include_directories(${target} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared
            ${CMAKE_CURRENT_SOURCE_DIR}/../domain
        )
        message(STATUS "[${layer}] 允许访问Domain层")

    elseif (layer STREQUAL "infrastructure")
        # Infrastructure层: 可以include Domain和Shared
        target_include_directories(${target} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared
            ${CMAKE_CURRENT_SOURCE_DIR}/../domain
        )
        message(STATUS "[${layer}] 允许访问Domain层")

    elseif (layer STREQUAL "adapters")
        # Adapters层: 可以include所有层
        target_include_directories(${target} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared
            ${CMAKE_CURRENT_SOURCE_DIR}/../domain
            ${CMAKE_CURRENT_SOURCE_DIR}/../application
            ${CMAKE_CURRENT_SOURCE_DIR}/../infrastructure
        )
        message(STATUS "[${layer}] 允许访问所有层")
    endif()

    # 添加架构守卫定义（用于编译时检查）
    target_compile_definitions(${target} PRIVATE
        SILIGEN_LAYER_${layer}=1
    )
endmacro()
```

**应用宏到Domain层**:

```cmake
# 文件: src/domain/CMakeLists.txt (修改)

# 在文件顶部引入宏
include(ArchitectureGuard)

# 修改 siligen_motion 库
add_library(siligen_motion STATIC
    motion/InterpolationBase.cpp
    motion/LinearInterpolator.cpp
    # ... 其他源文件
)

# 使用宏设置include路径
siligen_set_layer_includes(siligen_motion domain)

target_link_libraries(siligen_motion
    siligen_types
    siligen_utils
)
```

### Task 4: 应用CMake宏到所有Domain层库

```powershell
# 批量修改src/domain/CMakeLists.txt
# 将所有 target_include_directories 替换为宏调用

$content = Get-Content "src/domain/CMakeLists.txt" -Raw

# 替换 siligen_motion
$content = $content -replace `
  'target_include_directories\(siligen_motion PUBLIC.*?\)',
  'siligen_set_layer_includes(siligen_motion domain)'

# 替换 siligen_triggering
$content = $content -replace `
  'target_include_directories\(siligen_triggering PUBLIC.*?\)',
  'siligen_set_layer_includes(siligen_triggering domain)'

# ... 依此类推

$content | Set-Content "src/domain/CMakeLists.txt" -NoNewline
```

### Task 5: 验证构建

```powershell
# 清理构建目录
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue

# 重新配置
cmake -G Ninja -B build -S . -DCMAKE_BUILD_TYPE=Release

# 构建
cmake --build build --config Release --parallel

# 如果构建失败，检查错误消息
# 预期可能出现的错误:
#   "fatal error: infrastructure/hardware/...: No such file or directory"
# 这说明架构检查生效了！
```

---

## 3. 集成到CI/CD

### 3.1 更新架构验证工作流

```yaml
# 文件: .github/workflows/architecture-validation.yml (更新)
# 添加检测步骤

- name: Detect Cross-Layer Includes
  run: |
    python .claude/agents/arch_analyzer.py `
      --src src `
      --config .claude/arch-config.json `
      --format json `
      --output reports/arch-report.json

    # 检查是否有严重违规
    $report = Get-Content reports/arch-report.json | ConvertFrom-Json
    if ($report.violations.critical -gt 0) {
      Write-Host "❌ 发现 $($report.violations.critical) 个严重违规"
      exit 1
    }
  shell: pwsh
```

### 3.2 添加PR质量门控

```yaml
# 文件: .github/workflows/pr-quality-gate.yml

name: PR Quality Gate

on:
  pull_request:
    branches: [main, develop]

jobs:
  architecture-check:
    runs-on: windows-latest

    steps:
    - uses: actions/checkout@v4
      with:
        fetch-depth: 0  # 获取完整历史

    - name: Setup Python
      uses: actions/setup-python@v4
      with:
        python-version: '3.11'

    - name: Install Dependencies
      run: |
        pip install pyyaml rich click tabulate

    - name: Run Architecture Analysis
      run: |
        python .claude/agents/arch_analyzer.py `
          --src src `
          --config .claude/arch-config.json `
          --format json `
          --output reports/arch-report.json

    - name: Check Architecture Score
      run: |
        $report = Get-Content reports/arch-report.json | ConvertFrom-Json
        $score = $report.score
        Write-Host "架构合规性评分: $score/100"

        if ($score -lt 80) {
          Write-Host "❌ 架构评分低于80分，请检查违规"
          exit 1
        }
      shell: pwsh

    - name: Comment Results on PR
      if: always()
      uses: actions/github-script@v6
      with:
        script: |
          const fs = require('fs');
          const report = JSON.parse(fs.readFileSync('reports/arch-report.json', 'utf8'));

          const body = `## 🔍 架构合规性检查

          | 指标 | 值 |
          |------|-----|
          | 评分 | ${report.score}/100 |
          | 严重违规 | ${report.violations.critical} |
          | 高危违规 | ${report.violations.high} |
          | 中危违规 | ${report.violations.medium} |

          ${report.score >= 80 ? '✅ 架构检查通过' : '❌ 架构检查失败'}
          `;

          await github.rest.issues.createComment({
            issue_number: context.issue.number,
            owner: context.repo.owner,
            repo: context.repo.repo,
            body: body
          });
```

---

## 4. 本地开发工作流

### 4.1 预提交检查

```powershell
# 文件: scripts/pre-commit-check.ps1

param(
    [string]$BaseBranch = "HEAD~1"
)

# 1. 获取变更文件
$changedFiles = git diff --name-only $BaseBranch HEAD -- '*.h' '*.cpp' 2>$null
if ($changedFiles) {
    $changedFiles = $changedFiles -split "`n" | Where-Object { $_ -and (Test-Path $_) }
}

Write-Host "发现 $($changedFiles.Count) 个变更文件" -ForegroundColor Cyan

# 2. 运行Python架构分析
if ($changedFiles.Count -gt 0) {
    Write-Host "正在分析架构合规性..." -ForegroundColor Yellow

    python .claude/agents/arch_analyzer.py `
      --src src `
      --config .claude/arch-config.json `
      --format json `
      --output reports/arch-check.json

    $report = Get-Content reports/arch-check.json | ConvertFrom-Json

    if ($report.violations.critical -gt 0) {
        Write-Host "❌ 发现严重架构违规，提交被阻止" -ForegroundColor Red
        exit 1
    } else {
        Write-Host "✅ 架构检查通过" -ForegroundColor Green
    }
}

# 3. 尝试构建
Write-Host "正在构建验证..." -ForegroundColor Yellow
cmake --build build --config Release --parallel

if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ 构建成功" -ForegroundColor Green
    exit 0
} else {
    Write-Host "❌ 构建失败，提交被阻止" -ForegroundColor Red
    exit 1
}
```

### 4.2 Git Hook配置

```bash
# 安装pre-commit hook
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
pwsh scripts/pre-commit-check.ps1
EOF

chmod +x .git/hooks/pre-commit
```

---

## 5. 故障排除

### 问题1: CMake找不到ArchitectureGuard.cmake

**错误**:
```
CMake Error at src/domain/CMakeLists.txt:10 (include):
  could not find requested module: ArchitectureGuard
```

**解决**:
```cmake
# 在主CMakeLists.txt中添加cmake/模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
```

### 问题2: Python分析器导入yaml失败

**错误**:
```
ModuleNotFoundError: No module named 'yaml'
```

**解决**:
```bash
pip install pyyaml
```

### 问题3: 构建失败，但不知道哪个文件违规

**错误**:
```
fatal error: infrastructure/hardware/MultiCardAdapter.h: No such file or directory
```

**解决**:
```powershell
# 使用grep找到违规文件
Select-String -Path "src\domain\**\*.cpp" `
  -Pattern 'infrastructure/hardware/MultiCardAdapter.h'
```

然后移除违规include，改用Port接口。

---

## 6. 下一步行动

### 立即执行（今天）
- [ ] 运行Python架构分析器，获取当前评分
- [ ] 创建`cmake/ArchitectureGuard.cmake`
- [ ] 应用宏到Domain层CMakeLists.txt
- [ ] 验证构建

### 本周完成
- [ ] 应用宏到Application和Infrastructure层
- [ ] 创建PowerShell跨层检测脚本
- [ ] 集成到CI工作流

### 下周计划
- [ ] 实现组合根机制
- [ ] 创建架构测试
- [ ] 统一Port接口命名

---

## 7. 参考链接

- **完整改进方案**: `docs/04-development/architecture-improvement-plan-2026-01-08.md`
- **架构规则**: `.claude/rules/HEXAGONAL.md`
- **Python分析器**: `.claude/agents/arch_analyzer.py`
- **示例PR**: #123 (包含架构检查评论示例)

---

**文档版本**: v1.0.0
**最后更新**: 2026-01-08
**维护者**: Coder Agent
