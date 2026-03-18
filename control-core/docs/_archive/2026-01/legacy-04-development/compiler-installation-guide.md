# 编译工具安装指南

## 系统要求

- **操作系统**: Windows 10/11 (64位)
- **权限**: 管理员权限（安装工具时需要）

## 必需工具清单

| 工具 | 版本要求 | 用途 | 下载地址 |
|------|---------|------|---------|
| LLVM/Clang | 18+ | C/C++编译器 (clang-cl) | https://llvm.org/builds/ |
| CMake | 3.16+ | 构建系统生成器 | https://cmake.org/download/ |
| Ninja | 1.10+ | 构建工具 | https://github.com/ninja-build/ninja/releases |
| Git | 最新版 | 版本控制 | https://git-scm.com/downloads |

## 安装步骤

### 1. 安装 LLVM/Clang

**下载地址**: https://llvm.org/builds/

**步骤**:
1. 下载 Windows 64-bit 安装器：`LLVM-18.1.8-win64.exe`
2. 运行安装器，使用默认安装路径：`C:\Program Files\LLVM`
3. 安装时勾选 "Add LLVM to the system PATH for all users"
4. 完成后重启命令提示符

**验证安装**:
```cmd
clang-cl --version
```

**预期输出**:
```
clang-cl version 18.1.8
Target: x86_64-pc-windows-msvc
Thread model: posix
```

### 2. 安装 CMake

**下载地址**: https://cmake.org/download/

**步骤**:
1. 下载 Windows x64 Installer：`cmake-3.29.0-windows-x86_64.msi`
2. 运行安装器，选择 "Add CMake to the system PATH for all users"
3. 完成后重启命令提示符

**验证安装**:
```cmd
cmake --version
```

**预期输出**:
```
cmake version 3.29.0
CMake suite maintained and supported by Kitware (kitware.com/cmake).
```

### 3. 安装 Ninja

**下载地址**: https://github.com/ninja-build/ninja/releases

**步骤**:
1. 下载 `ninja-win.zip`
2. 解压到：`C:\Tools\Ninja`
3. 将 `C:\Tools\Ninja` 添加到系统 PATH

**方法 A - 临时添加（当前会话）**:
```cmd
set PATH=%PATH%;C:\Tools\Ninja
```

**方法 B - 永久添加（推荐）**:
```cmd
setx PATH "%PATH%;C:\Tools\Ninja"
```

**验证安装**:
```cmd
ninja --version
```

**预期输出**:
```
1.12.1
```

### 4. 安装 Git

**下载地址**: https://git-scm.com/downloads

**步骤**:
1. 下载 Windows 64-bit 安装器
2. 运行安装器，使用默认设置
3. 完成后重启命令提示符

**验证安装**:
```cmd
git --version
```

**预期输出**:
```
git version 2.45.0.windows.1
```

## 环境变量配置

### 自动配置脚本

项目提供了环境初始化脚本：

```powershell
.\scripts\init_environment.ps1
```

该脚本会自动设置：
- `BACKEND_CPP_ROOT` 环境变量（指向项目根目录）

### 手动配置 PATH

确保以下路径已添加到系统 PATH：

```
C:\Program Files\LLVM\bin
C:\Program Files\CMake\bin
C:\Tools\Ninja
C:\Program Files\Git\cmd
```

**查看当前 PATH**:
```cmd
echo %PATH%
```

## 验证安装

运行完整的环境检查：

```powershell
# 检查所有工具
clang-cl --version
cmake --version
ninja --version
git --version

# 检查项目环境
echo $env:BACKEND_CPP_ROOT
```

## 常见问题

### Q1: clang-cl 命令不存在

**原因**: LLVM 未添加到 PATH

**解决**:
1. 检查 LLVM 安装路径：`C:\Program Files\LLVM\bin`
2. 手动添加到 PATH 或重新运行 LLVM 安装器，勾选 "Add to PATH"

### Q2: CMake 找不到 Ninja

**原因**: Ninja 不在 PATH 中

**解决**:
```cmd
where ninja
```
如果返回空，检查 `C:\Tools\Ninja` 是否在 PATH 中

### Q3: 提示缺少 MSVC 运行时

**原因**: Windows 缺少 Visual C++ 运行时库

**解决**: 安装 [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)

### Q4: PowerShell 执行策略错误

**错误**: 无法加载文件，因为在此系统上禁止运行脚本

**解决**:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

## 一键安装脚本（可选）

创建 `install_tools.ps1`:

```powershell
# 管理员权限运行
if (!([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "请以管理员权限运行此脚本"
    exit 1
}

Write-Host "=== 编译工具自动安装脚本 ===" -ForegroundColor Cyan

# 检查已安装工具
Write-Host "`n检查工具安装状态..." -ForegroundColor Yellow

$tools = @{
    "Clang-CL"   = "clang-cl"
    "CMake"      = "cmake"
    "Ninja"      = "ninja"
    "Git"        = "git"
}

foreach ($tool in $tools.GetEnumerator()) {
    $cmd = Get-Command $tool.Value -ErrorAction SilentlyContinue
    if ($cmd) {
        Write-Host "[$($tool.Key)] 已安装: $($cmd.Source)" -ForegroundColor Green
    } else {
        Write-Host "[$($tool.Key)] 未安装" -ForegroundColor Red
    }
}

Write-Host "`n请手动安装未安装的工具（参考本文档）" -ForegroundColor Yellow
```

## 下一步

安装完所有工具后：

1. **配置项目环境**:
   ```powershell
   .\scripts\init_environment.ps1
   ```

2. **首次构建**:
   ```powershell
   .\scripts\build.ps1 all
   ```

3. **验证编译**:
   ```powershell
   cd build
   dir bin
   ```

## 技术支持

如遇问题，请检查：
- LLVM 版本是否 >= 18
- CMake 版本是否 >= 3.16
- Ninja 是否正确添加到 PATH
- PowerShell 执行策略是否允许脚本运行

---

**最后更新**: 2026-01-01
**维护者**: Backend_CPP Team
