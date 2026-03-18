# build.ps1 - Siligen统一构建系统入口脚本
# 支持所有构建目标的统一管理
#
# 使用方法:
#   .\scripts\build.ps1 [目标] [选项]
#
# 示例:
#   .\scripts\build.ps1                    # 构建所有目标
#   .\scripts\build.ps1 tests              # 仅构建测试
#   .\scripts\build.ps1 clean              # 清理构建目录
#   .\scripts\build.ps1 all -Debug         # 调试构建

param(
    [ValidateSet("all", "tests", "clean")]
    [string]$Target = "all",

    [string]$BuildType = "Release",
    [switch]$Fast,
    [switch]$Help,
    [switch]$Clean,
    [ValidateSet("ERROR", "WARNING", "INFO")]
    [string]$LogLevel = "ERROR"
)

# 显示帮助信息
if ($Help) {
    Write-Host "Siligen统一构建系统" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "使用说明:" -ForegroundColor Yellow
    Write-Host "  .\scripts\build.ps1 [目标] [选项]"
    Write-Host ""
    Write-Host "构建目标:" -ForegroundColor Yellow
    Write-Host "  all         构建所有目标（默认）"
    Write-Host "  tests       仅构建测试"
    Write-Host "  clean       清理构建目录"
    Write-Host ""
    Write-Host "构建选项:" -ForegroundColor Yellow
    Write-Host "  -BuildType  Release|Debug 构建类型"
    Write-Host "  -Fast       启用快速编译模式"
    Write-Host "  -Clean      强制清理后构建"
    Write-Host "  -LogLevel   ERROR|WARNING|INFO 日志级别 (默认: ERROR)"
    Write-Host ""
    Write-Host "示例:" -ForegroundColor Yellow
    Write-Host "  .\scripts\build.ps1 all -Debug"
    Write-Host "  .\scripts\build.ps1 clean"
    Write-Host ""
    Write-Host "相关脚本:" -ForegroundColor Gray
    Write-Host "  .\scripts\optimize-build.ps1 - 性能优化构建"
    Write-Host "  .\scripts\clean-build.ps1 - 构建目录清理"
    exit 0
}

# 项目根目录和构建目录
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"

# 加载公共函数
if (Test-Path "$PSScriptRoot\common.ps1") {
    . "$PSScriptRoot\common.ps1"
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Siligen统一构建系统" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "项目路径: $ProjectRoot" -ForegroundColor Gray
Write-Host "构建目录: $BuildDir" -ForegroundColor Gray
Write-Host "构建目标: $Target" -ForegroundColor Gray
Write-Host "构建类型: $BuildType" -ForegroundColor Gray
Write-Host ""

# 清理模式处理
if ($Target -eq "clean") {
    Write-Host "[INFO] 执行构建目录清理..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Path $BuildDir -Recurse -Force
        Write-Host "[SUCCESS] 构建目录已清理: $BuildDir" -ForegroundColor Green
    } else {
        Write-Host "[INFO] 构建目录不存在，无需清理" -ForegroundColor Gray
    }
    exit 0
}

# 强制清理模式
if ($Clean) {
    Write-Host "[INFO] 强制清理构建目录..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Path $BuildDir -Recurse -Force
        Write-Host "[SUCCESS] 构建目录已清理" -ForegroundColor Green
    }
}

# 构建CMake配置参数
$cmakeArgs = @(
    "-B", $BuildDir,
    "-S", ".",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DSILIGEN_BUILD_TARGET=$Target",
    "-DSILIGEN_LOG_LEVEL=$LogLevel"
)

# 根据日志级别设置 CMake 输出级别
if ($LogLevel -eq "ERROR") {
    $cmakeArgs += "--log-level=ERROR"
} elseif ($LogLevel -eq "WARNING") {
    $cmakeArgs += "--log-level=WARNING"
}

# 快速编译模式
if ($Fast) {
    $cmakeArgs += "-DSILIGEN_USE_PCH=ON"
    $cmakeArgs += "-DSILIGEN_USE_UNITY_BUILD=ON"
    Write-Host "[INFO] 快速编译模式已启用" -ForegroundColor Green
}

Write-Host "`n[步骤1] CMake配置..." -ForegroundColor Cyan
Write-Host "命令: cmake $($cmakeArgs -join ' ')" -ForegroundColor Gray

$cmakeProcess = Start-Process -FilePath "cmake" -ArgumentList $cmakeArgs -Wait -PassThru -NoNewWindow
if ($cmakeProcess.ExitCode -ne 0) {
    Write-Host "[ERROR] CMake配置失败 (错误码: $($cmakeProcess.ExitCode))" -ForegroundColor Red
    exit $cmakeProcess.ExitCode
}

Write-Host "[SUCCESS] CMake配置完成" -ForegroundColor Green

# CMake构建
Write-Host "`n[步骤2] 编译目标..." -ForegroundColor Cyan

$buildArgs = @(
    "--build", $BuildDir,
    "--config", $BuildType,
    "--parallel", [Environment]::ProcessorCount
)

# 如果是静默模式，针对 Ninja 使用 --quiet，其它生成器不追加参数避免失败
if ($LogLevel -eq "ERROR" -or $LogLevel -eq "WARNING") {
    $ninjaBuildFile = Join-Path $BuildDir "build.ninja"
    if (Test-Path $ninjaBuildFile) {
        $buildArgs += @("--", "--quiet")
    }
}

Write-Host "命令: cmake $($buildArgs -join ' ')" -ForegroundColor Gray

$buildProcess = Start-Process -FilePath "cmake" -ArgumentList $buildArgs -Wait -PassThru -NoNewWindow
if ($buildProcess.ExitCode -ne 0) {
    Write-Host "[ERROR] 编译失败 (错误码: $($buildProcess.ExitCode))" -ForegroundColor Red
    exit $buildProcess.ExitCode
}

Write-Host "[SUCCESS] 编译完成" -ForegroundColor Green

# 构建总结
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "构建完成!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

# 显示输出文件
$binDir = Join-Path $BuildDir "bin"
if (Test-Path $binDir) {
    Write-Host "输出目录: $binDir" -ForegroundColor Gray
    Write-Host ""

    $tcpExe = Join-Path $binDir "siligen_tcp_server.exe"
    if (Test-Path $tcpExe) {
        Write-Host "TCP服务端程序: $tcpExe" -ForegroundColor Cyan
    }

    if ($Target -eq "tests" -or $Target -eq "all") {
        $testExes = Get-ChildItem -Path $binDir -Filter "*test*.exe"
        foreach ($exe in $testExes) {
            Write-Host "测试程序: $($exe.FullName)" -ForegroundColor Magenta
        }
    }
}

Write-Host ""
Write-Host "快速命令:" -ForegroundColor Cyan
Write-Host "  清理构建: .\scripts\build.ps1 clean" -ForegroundColor Gray
Write-Host "  快速构建: .\scripts\build.ps1 all -Fast" -ForegroundColor Gray
Write-Host "  调试构建: .\scripts\build.ps1 all -Debug" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan
