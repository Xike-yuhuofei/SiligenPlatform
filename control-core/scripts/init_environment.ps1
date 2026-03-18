#!/usr/bin/env pwsh
# 项目环境变量初始化脚本

$ErrorActionPreference = "Stop"

function Set-ProjectEnvironment {
    <#
    .SYNOPSIS
        设置项目所需的环境变量
    .DESCRIPTION
        配置 BACKEND_CPP_ROOT 环境变量供脚本使用
    #>

    $projectRoot = $PSScriptRoot | Split-Path -Parent

    # 设置会话级环境变量（当前PowerShell会话有效）
    $env:BACKEND_CPP_ROOT = $projectRoot

    # 可选：设置用户级环境变量（持久化）
    $setPersistent = Read-Host "是否设置为用户级环境变量（持久化）？(y/N)"
    if ($setPersistent -eq 'y' -or $setPersistent -eq 'Y') {
        [Environment]::SetEnvironmentVariable(
            "BACKEND_CPP_ROOT",
            $projectRoot,
            "User"
        )
        Write-Host "[SUCCESS] 已设置用户级环境变量 BACKEND_CPP_ROOT = $projectRoot" -ForegroundColor Green
    } else {
        Write-Host "[INFO] 会话级环境变量已设置（仅在当前窗口有效）" -ForegroundColor Cyan
    }

    # 验证设置
    Write-Host "`n[INFO] 当前环境变量:" -ForegroundColor Cyan
    Write-Host "BACKEND_CPP_ROOT = $env:BACKEND_CPP_ROOT"
}

# 执行设置
Set-ProjectEnvironment
