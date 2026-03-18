# 构建目录整合指南

## 目录结构

项目现在使用以下构建目录结构：

```
Backend_CPP/
├── build/          # 主构建目录 (Visual Studio)
├── build_clang/    # Clang编译器专用
├── build_new/      # 实验性配置
└── build/          # 清理重复后的唯一主目录
```

## 构建命令

### 标准构建 (Visual Studio)
```bash
cmake -B build -S .
cmake --build build --config Release
```

### Clang编译器构建
```bash
cmake -G Ninja -B build_clang -S . -DCMAKE_CXX_COMPILER="C:\Program Files\LLVM\bin\clang++.exe"
cmake --build build_clang
```

### 多配置构建
```bash
# Debug版本
cmake -B build_debug -S . -DCMAKE_BUILD_TYPE=Debug

# Release版本
cmake -B build_release -S . -DCMAKE_BUILD_TYPE=Release

# 测试版本
cmake -B build_test -S . -DSILIGEN_BUILD_TESTS=ON
```

## 维护建议

1. **定期清理**：每月清理未使用的构建目录
2. **命名规范**：使用描述性名称（如 build_msvc、build_clang、build_debug）
3. **配置分离**：不同配置使用不同目录
4. **自动化**：使用脚本管理构建目录

## 快速清理脚本

```powershell
# 清理所有构建目录
Remove-Item -Recurse -Force build_*
```

## 磁盘空间节省

清理后节省了约 2.2GB 磁盘空间（删除了 build2 和 build3）。