# vcpkg 使用指南

本文档说明如何在项目中使用 vcpkg 管理的依赖包。

## 安装位置

vcpkg 已安装到: `D:/Downloads/vcpkg`

## 已安装的包

- `abseil:x64-windows@20250814.1` - Google 基础库
- `nlohmann-json:x64-windows@3.12.0` - JSON 库

## 快速开始

### 方法 1: 使用项目工具链文件（推荐）

```bash
cd D:/Projects/Backend_CPP
mkdir -p build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/vcpkg-toolchain.cmake ..
cmake --build .
```

### 方法 2: 直接指定 vcpkg 工具链

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=D:/Downloads/vcpkg/scripts/buildsystems/vcpkg.cmake ..
```

### 方法 3: 设置环境变量

```bash
# 在 PowerShell 中
$env:VCPKG_ROOT="D:/Downloads/vcpkg"
cmake ..

# 在 Bash 中
export VCPKG_ROOT=D:/Downloads/vcpkg
cmake ..
```

## 添加新的 vcpkg 包

### 安装新包

```bash
cd D:/Downloads/vcpkg
./vcpkg install <package-name>:x64-windows
```

### 示例

```bash
# 安装 fmt 库
./vcpkg install fmt:x64-windows

# 安装 spdlog 日志库
./vcpkg install spdlog:x64-windows

# 安装 gRPC
./vcpkg install grpc:x64-windows
```

### 在项目中使用

在 CMakeLists.txt 中添加：

```cmake
find_package(<package-name> CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE <package-name>::<target>)
```

## 常用 vcpkg 命令

```bash
# 列出已安装的包
cd D:/Downloads/vcpkg
./vcpkg list

# 搜索包
./vcpkg search <package-name>

# 删除包
./vcpkg remove <package-name>:x64-windows

# 更新 vcpkg
cd D:/Downloads/vcpkg
git pull

# 升级所有包
./vcpkg upgrade

# 导出已安装的包（用于离线部署）
./vcpkg export <package-name>:x64-windows --output=exports
```

## 环境变量清理

vcpkg 安装时可能会检测到旧的 VCPKG_ROOT 环境变量指向 Visual Studio 的内置 vcpkg。如果看到警告：

```
warning: The vcpkg D:\Downloads\vcpkg\vcpkg.exe is using detected vcpkg root
D:\Downloads\vcpkg and ignoring mismatched VCPKG_ROOT environment value
C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\vcpkg
```

这个警告可以忽略，因为项目现在使用新的 vcpkg 安装。如果想去掉这个警告：

### PowerShell

```powershell
# 删除用户级环境变量
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", $null, "User")

# 删除系统级环境变量（需要管理员权限）
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", $null, "Machine")
```

### Bash

```bash
# 在 ~/.bashrc 或 ~/.bash_profile 中添加
unset VCPKG_ROOT
```

## 故障排查

### 问题: find_package 找不到包

**解决方案:**

1. 确保使用了正确的工具链文件：
   ```bash
   cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/vcpkg-toolchain.cmake ..
   ```

2. 检查包是否已安装：
   ```bash
   cd D:/Downloads/vcpkg
   ./vcpkg list | grep <package-name>
   ```

3. 重新安装包：
   ```bash
   ./vcpkg remove <package-name>:x64-windows
   ./vcpkg install <package-name>:x64-windows
   ```

### 问题: 编译器不匹配

vcpkg 默认使用 MSVC 编译器，但项目配置使用 Clang-CL。这可能导致 ABI 不兼容问题。

**解决方案:**

选项 1: 使用 Clang-CL 重新编译 vcpkg 包
```bash
cd D:/Downloads/vcpkg
./vcpkg install <package-name>:x64-windows --recurse
# 设置环境变量强制使用 Clang-CL
set CC=clang-cl
set CXX=clang-cl
```

选项 2: 项目也使用 MSVC（临时解决方案）
```bash
cmake -T v143 ..
```

### 问题: CMAKE_TOOLCHAIN_FILE 不生效

**可能原因:**

1. CMake 缓存已过期
2. 在错误的目录运行 cmake

**解决方案:**

```bash
# 清理构建目录
rm -rf CMakeCache.txt CMakeFiles/

# 重新配置
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/vcpkg-toolchain.cmake ..
```

## 参考

- [vcpkg 官方文档](https://learn.microsoft.com/en-us/vcpkg/)
- [vcpkg GitHub](https://github.com/microsoft/vcpkg)

## 维护者

- vcpkg 安装位置: `D:/Downloads/vcpkg`
- 工具链配置文件: `cmake/vcpkg-toolchain.cmake`
- 最后更新: 2025-01-13
