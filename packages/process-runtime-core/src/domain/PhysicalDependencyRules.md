# CMake物理依赖隔离方案

## 目标
通过CMake的`target_include_directories()`作用域限制，在编译时强制执行架构依赖方向：
- **Domain层**: 只能访问Shared层，禁止访问Infrastructure/Adapters层
- **Application层**: 只能访问Domain和Shared层
- **Infrastructure层**: 可以访问Domain和Shared层
- **Adapters层**: 可以访问Application、Domain、Shared层

## 实现策略

### 1. Domain层严格隔离
```cmake
# src/domain/CMakeLists.txt

# ✅ 正确: 只暴露当前层和Shared层
target_include_directories(siligen_motion PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/motion          # 当前子模块
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types # Shared层（允许）
)

# ❌ 错误: 暴露Infrastructure层（破坏依赖方向）
target_include_directories(siligen_motion PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../infrastructure/drivers/multicard  # 禁止！
)
```

**关键原则**:
- Domain层库的`PUBLIC` include目录只能包含`src/domain`和`src/shared`
- 其他层的include目录必须使用`PRIVATE`作用域

### 2. 创建跨层访问检测脚本

**文件**: `scripts/analysis/detect-cross-layer-includes.ps1`

```powershell
<#
.SYNOPSIS
    检测C++代码中的跨层非法include

.DESCRIPTION
    扫描源代码，检测违反架构依赖方向的#include语句:
    - Domain层禁止include Infrastructure/Adapters层
    - Application层禁止include Adapters层
#>

param(
    [string]$SourceDir = "src",
    [switch]$Verbose
)

$violations = @()

# 扫描所有.h/.cpp文件
Get-ChildItem -Path $SourceDir -Include "*.h","*.cpp" -Recurse | ForEach-Object {
    $file = $_
    $content = Get-Content $file.FullName -Raw

    # 识别文件所属层
    if ($file.FullName -like "*\domain\*") {
        $layer = "domain"
        $forbiddenPatterns = @(
            'infrastructure/',
            'adapters/',
            'application/'  # Domain也不应依赖Application
        )
    }
    elseif ($file.FullName -like "*\application\*") {
        $layer = "application"
        $forbiddenPatterns = @(
            'adapters/',
            'infrastructure/drivers/multicard',  # 应通过Port访问
            'infrastructure/adapters'
        )
    }
    else {
        # 其他层不检查
        return
    }

    # 检测非法include
    $forbiddenPatterns | ForEach-Object {
        $pattern = $_
        if ($content -match [regex]::Escape($pattern)) {
            $violations += [PSCustomObject]@{
                File = $file.FullName.Replace((Get-Location).Path + "\", "")
                Layer = $layer
                Violation = "非法include: $pattern"
            }
        }
    }
}

# 输出结果
if ($violations.Count -gt 0) {
    Write-Host "❌ 发现 $($violations.Count) 个跨层依赖违规:" -ForegroundColor Red
    $violations | Format-Table -AutoSize
    exit 1
} else {
    Write-Host "✅ 未发现跨层依赖违规" -ForegroundColor Green
    exit 0
}
```

### 3. CMake编译时检查（通过compiler warnings）

**策略**: 利用CMake的`include_directories()`和`target_include_directories()`的visibility控制：

```cmake
# 主CMakeLists.txt顶层配置

# ========================================
# 架构物理隔离配置
# ========================================

# 定义各层的include访问规则
function(siligen_layer_target layer_name target_name)
    cmake_parse_arguments(ARG "PUBLIC;PRIVATE" "" "" ${ARGN})

    if (layer_name STREQUAL "domain")
        # Domain层: 最严格，只能include自己和Shared
        set(allowed_include_regex
            "^${CMAKE_SOURCE_DIR}/src/domain/.*"
            "^${CMAKE_SOURCE_DIR}/src/shared/.*"
        )

    elseif (layer_name STREQUAL "application")
        # Application层: 可以include Domain和Shared
        set(allowed_include_regex
            "^${CMAKE_SOURCE_DIR}/src/domain/.*"
            "^${CMAKE_SOURCE_DIR}/src/shared/.*"
            "^${CMAKE_SOURCE_DIR}/src/application/.*"
        )

    elseif (layer_name STREQUAL "infrastructure")
        # Infrastructure层: 可以include Domain和Shared
        set(allowed_include_regex
            "^${CMAKE_SOURCE_DIR}/src/domain/.*"
            "^${CMAKE_SOURCE_DIR}/src/shared/.*"
            "^${CMAKE_SOURCE_DIR}/src/infrastructure/.*"
        )

    elseif (layer_name STREQUAL "adapters")
        # Adapters层: 可以include所有层
        set(allowed_include_regex
            "^${CMAKE_SOURCE_DIR}/src/.*"
        )
    endif()

    # TODO: 使用CMake的file(GENERATE)创建编译时检查头文件
    # 这个头文件会被每个源文件include，如果检测到违规include则触发#error
endfunction()
```

### 4. 创建架构违规检测头文件

**文件**: `src/build/architecture_guard.h`（自动生成）

```cpp
// architecture_guard.h - 架构依赖方向编译时检查
// 此文件由CMake自动生成，用于在编译时检测跨层依赖违规

#pragma once

// 检测Domain层是否包含了Infrastructure层
#ifdef SILIGEN_LAYER_DOMAIN
    #if defined(_INC_INFRASTRUCTURE) || defined(INFRASTRUCTURE_GUARD_INCLUDED)
        #error "架构违规: Domain层不能include Infrastructure层头文件"
    #endif
#endif

// 检测Application层是否包含了Adapters层
#ifdef SILIGEN_LAYER_APPLICATION
    #if defined(_INC_ADAPTERS) || defined(ADAPTERS_GUARD_INCLUDED)
        #error "架构违规: Application层不能include Adapters层头文件（应通过Port接口）"
    #endif
#endif
```

## 实施步骤

### Step 1: 修改Domain层CMakeLists.txt
```cmake
# 移除所有Infrastructure层的include路径
# 确保target_include_directories只包含:
# - ${CMAKE_CURRENT_SOURCE_DIR}及子目录
# - ${CMAKE_CURRENT_SOURCE_DIR}/../shared
```

### Step 2: 添加编译时检查到CI
```yaml
# .github/workflows/architecture-validation.yml

- name: Run Cross-Layer Include Detection
  run: |
    pwsh scripts/analysis/detect-cross-layer-includes.ps1 \
      -SourceDir src \
      -Verbose
```

### Step 3: 创建CMake宏简化配置
```cmake
# cmake/ArchitectureGuard.cmake

macro(siligen_set_layer_includes target layer)
    if (layer STREQUAL "domain")
        target_include_directories(${target} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared/utils
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared/interfaces
        )
        target_compile_definitions(${target} PRIVATE SILIGEN_LAYER_DOMAIN=1)

    elseif (layer STREQUAL "application")
        target_include_directories(${target} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared
            ${CMAKE_CURRENT_SOURCE_DIR}/../domain
        )
        target_compile_definitions(${target} PRIVATE SILIGEN_LAYER_APPLICATION=1)
    endif()

    # 添加架构守卫头文件
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src/build)
endmacro()
```

## 验证方法

### 1. 单元测试验证
创建测试用例尝试违规include，确保编译失败：

```cpp
// tests/architecture/domain_infrastructure_violation_test.cpp

// 故意违反架构规则，应该编译失败
#include "domain/motion/LinearInterpolator.h"
#include "infrastructure/drivers/multicard/MultiCardAdapter.h"  // ❌ 应该触发#error

int main() {
    // 此代码不应编译通过
    return 0;
}
```

### 2. 静态分析集成
在`.github/workflows/claude-code-review.yml`中添加：

```yaml
- name: Enforce Architecture Rules
  run: |
    pwsh scripts/analysis/detect-cross-layer-includes.ps1
```

## 风险与缓解

### 风险1: 现有代码编译失败
**缓解**: 渐进式迁移
1. 第1周: 添加检测脚本，仅报告违规，不阻止构建
2. 第2周: 修复严重违规（Domain->Infrastructure）
3. 第3周: 启用编译时检查，强制执行规则

### 风险2: 误报
**缓解**: 使用白名单机制：
```cmake
set(ARCHITECTURE_VIOLATION_WHITELIST
)
```

## 预期效果

1. **编译时强制执行**: 违规include导致编译失败
2. **CI自动检测**: PR合并前自动运行检测脚本
3. **零运行时开销**: 纯编译时检查，不影响性能
4. **清晰的错误消息**: 编译错误明确指出违规原因
