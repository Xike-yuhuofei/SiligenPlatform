# scripts/check-naming-convention.ps1
<#
.SYNOPSIS
检查命名规范违规

.DESCRIPTION
检查代码库中的命名规范违规，包括：
- 函数/方法命名应该是 PascalCase（大驼峰）
- 类/结构体命名应该是 PascalCase（大驼峰）
- 参数/变量命名应该是 camelCase（小驼峰）
- 常量命名应该是 UPPER_CASE
- 命名空间应该是 PascalCase

.OUTPUTS
退出码：0 表示通过，1 表示发现违规
#>

[CmdletBinding()]
param()

$errors = 0
$warnings = 0

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  命名规范检查" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ========== 辅助函数 ==========
function Test-IsValidPascalCase {
    param([string]$name)
    return $name -cmatch '^[A-Z][a-zA-Z0-9]*$'
}

function Test-IsValidCamelCase {
    param([string]$name)
    return $name -cmatch '^[a-z][a-zA-Z0-9]*$'
}

function Test-IsValidUpperSnakeCase {
    param([string]$name)
    return $name -cmatch '^[A-Z][A-Z0-9_]*$'
}

# ========== 检查函数/方法命名（应该是 PascalCase）==========
Write-Host "检查函数/方法命名..." -ForegroundColor Yellow

Get-ChildItem -Path src -Filter "*.h" -Recurse | Where-Object { $_.FullName -notmatch 'vendor\\|vendor/' } | ForEach-Object {
    $content = Get-Content $_.FullName -Raw

    # 匹配函数定义（排除 operator, ~ 等特殊情况）
    $matches = [regex]::Matches($content, '(?m)^\s*(?!.*operator)(?!.*~)(?:virtual\s+)?(?:static\s+)?(?:inline\s+)?[\w<>:&\s]+\s+([a-z_][a-zA-Z0-9_]*)\s*\(')

    foreach ($match in $matches) {
        $funcName = $match.Groups[1].Value

        # 排除标准库函数和特殊函数
        if ($funcName -match '^(main|if|while|for|switch|sizeof|typeid)$') {
            continue
        }

        # 检查是否以小写字母或下划线开头
        if ($funcName -cmatch '^[a-z_]' -and $funcName -cnotmatch '^[A-Z]') {
            Write-Host "  ⚠️  $($_.FullName):$funcName - 函数应使用 PascalCase (大驼峰)" -ForegroundColor Yellow
            $errors++
        }
    }
}

# ========== 检查类/结构体命名（应该是 PascalCase）==========
Write-Host "检查类/结构体命名..." -ForegroundColor Yellow

Get-ChildItem -Path src -Filter "*.h" -Recurse | Where-Object { $_.FullName -notmatch 'vendor\\|vendor/' } | ForEach-Object {
    $content = Get-Content $_.FullName -Raw

    # 检测小写开头的类/结构体
    $matches = [regex]::Matches($content, '\b(class|struct)\s+([a-z][a-zA-Z0-9_]*)\b')

    foreach ($match in $matches) {
        $typeName = $match.Groups[2].Value

        # 排除模板参数
        if ($typeName -match '^[a-z]$') {
            continue
        }

        Write-Host "  ⚠️  $($_.FullName):$typeName - 类型应使用 PascalCase (大驼峰)" -ForegroundColor Yellow
        $errors++
    }
}

# ========== 检查枚举命名（应该是 PascalCase）==========
Write-Host "检查枚举命名..." -ForegroundColor Yellow

Get-ChildItem -Path src -Filter "*.h" -Recurse | Where-Object { $_.FullName -notmatch 'vendor\\|vendor/' } | ForEach-Object {
    $content = Get-Content $_.FullName -Raw

    # 检测小写开头的枚举
    $matches = [regex]::Matches($content, '\benum\s+(class\s+)?([a-z][a-zA-Z0-9_]*)\b')

    foreach ($match in $matches) {
        $enumName = $match.Groups[2].Value
        Write-Host "  ⚠️  $($_.FullName):$enumName - 枚举应使用 PascalCase (大驼峰)" -ForegroundColor Yellow
        $errors++
    }
}

# ========== 检查参数命名（应该是 camelCase）==========
Write-Host "检查参数命名..." -ForegroundColor Yellow

Get-ChildItem -Path src -Filter "*.h" -Recurse | Where-Object { $_.FullName -notmatch 'vendor\\|vendor/' } | ForEach-Object {
    $content = Get-Content $_.FullName -Raw

    # 匹配函数参数（检测大写开头或下划线开头）
    $matches = [regex]::Matches($content, '\(([^)]*)\)')

    foreach ($match in $matches) {
        $params = $match.Groups[1].Value

        # 跳过空参数或 void
        if ([string]::IsNullOrWhiteSpace($params) -or $params -match '^\s*void\s*$') {
            continue
        }

        # 分割参数
        $paramList = $params -split ','
        foreach ($param in $paramList) {
            # 提取参数名（最后一个标识符）
            if ($param -match '(\w+)\s*$') {
                $paramName = $matches[1]

                # 跳过类型名
                if ($param -match '^\s*(const\s+)?\w+') {
                    $typePart = $matches[0]
                    if ($paramName -eq $typePart.Trim()) {
                        continue
                    }
                }

                # 检查参数名是否以大写字母开头（除非是缩写）
                if ($paramName -cmatch '^[A-Z][A-Z0-9_]*$') {
                    # 全大写的可能是常量，给出警告而不是错误
                    Write-Host "  ℹ️  $($_.FullName):$paramName - 参数使用常量命名（可能是合理的）" -ForegroundColor Cyan
                    $warnings++
                }
            }
        }
    }
}

# ========== 检查常量命名（应该是 UPPER_CASE）==========
Write-Host "检查常量命名..." -ForegroundColor Yellow

Get-ChildItem -Path src -Filter "*.h" -Recurse | Where-Object { $_.FullName -notmatch 'vendor\\|vendor/' } | ForEach-Object {
    $content = Get-Content $_.FullName -Raw

    # 匹配 constexpr 或 static const 定义
    $matches = [regex]::Matches($content, '(?:constexpr|static\s+const)\s+[\w<>:&\s]+\s+([a-z][a-zA-Z0-9_]*)\s*[=;]')

    foreach ($match in $matches) {
        $constName = $match.Groups[1].Value

        # 排除明显是函数的
        if ($constName -match '^[a-z][a-zA-Z0-9_]*\(') {
            continue
        }

        Write-Host "  ⚠️  $($_.FullName):$constName - 常量应使用 UPPER_CASE (大写下划线)" -ForegroundColor Yellow
        $errors++
    }
}

# ========== 检查命名空间命名（应该是 PascalCase）==========
Write-Host "检查命名空间命名..." -ForegroundColor Yellow

Get-ChildItem -Path src -Filter "*.h" -Recurse | Where-Object { $_.FullName -notmatch 'vendor\\|vendor/' } | ForEach-Object {
    $content = Get-Content $_.FullName -Raw

    # 检测小写开头的命名空间
    $matches = [regex]::Matches($content, 'namespace\s+([a-z][a-zA-Z0-9_]*)\b')

    foreach ($match in $matches) {
        $namespaceName = $match.Groups[1].Value

        # 排除标准命名空间
        if ($namespaceName -match '^(std|detail|impl)$') {
            continue
        }

        Write-Host "  ⚠️  $($_.FullName):$namespaceName - 命名空间应使用 PascalCase (大驼峰)" -ForegroundColor Yellow
        $errors++
    }
}

# ========== 输出总结 ==========
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  检查结果" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if ($errors -gt 0) {
    Write-Host "❌ 发现 $errors 个命名规范违规" -ForegroundColor Red
    Write-Host "ℹ️  发现 $warnings 个建议" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "请修复以上问题后重试。" -ForegroundColor Red
    exit 1
} else {
    Write-Host "✅ 命名规范检查通过" -ForegroundColor Green
    if ($warnings -gt 0) {
        Write-Host "ℹ️  有 $warnings 个建议（非错误）" -ForegroundColor Cyan
    }
    exit 0
}
