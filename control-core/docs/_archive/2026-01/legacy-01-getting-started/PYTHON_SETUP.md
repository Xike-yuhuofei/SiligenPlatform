# Python 环境配置指南

## 快速开始

### 1. 自动安装（推荐）

```powershell
# 运行自动安装脚本
.\scripts\install_python.ps1

# 强制重新安装
.\scripts\install_python.ps1 -Force

# 跳过虚拟环境创建
.\scripts\install_python.ps1 -SkipVirtualEnv
```

### 2. 手动安装

访问 https://www.python.org/downloads/ 下载Python 3.12安装包

**安装注意事项：**
- ✅ 勾选 "Add python.exe to PATH"
- ✅ 勾选 "Install for all users"
- 推荐安装路径：`C:\Python312`

## 虚拟环境

### 激活虚拟环境

```powershell
# 方式1：使用便捷脚本
.\scripts\activate_python.ps1

# 方式2：直接激活
.\.venv\Scripts\Activate.ps1
```

### 退出虚拟环境

```powershell
deactivate
```

### 重建虚拟环境

```powershell
# 删除旧环境
Remove-Item -Path .\.venv -Recurse -Force

# 创建新环境
python -m venv .venv

# 激活并安装依赖
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

## 常用包管理命令

### 安装包

```powershell
# 从requirements.txt安装
pip install -r requirements.txt

# 安装单个包
pip install <package-name>

# 指定版本
pip install <package-name>=1.0.0
```

### 更新包

```powershell
# 更新单个包
pip install -U <package-name>

# 更新所有包
pip list --outdated
pip install -U <package1> <package2> ...
```

### 查看已安装包

```powershell
# 列出所有包
pip list

# 显示包信息
pip show <package-name>

# 导出依赖
pip freeze > requirements.txt
```

## 开发工具配置

### 代码格式化（Black）

```powershell
# 格式化单个文件
black <file.py>

# 格式化整个项目
black scripts/

# 配置文件：pyproject.toml
[tool.black]
line-length = 100
target-version = ['py312']
```

### 代码检查（Pylint）

```powershell
# 检查单个文件
pylint <file.py>

# 检查整个项目
pylint scripts/

# 配置文件：.pylintrc
```

### 类型检查（MyPy）

```powershell
# 检查单个文件
mypy <file.py>

# 检查整个项目
mypy scripts/

# 配置文件：mypy.ini
```

## C++开发相关工具

### Conan（C++包管理器）

```powershell
# 创建Conan配置
conan profile detect

# 搜索包
conan search <package-name> --remote=conancenter

# 安装依赖
conan install . --output-folder=build --build=missing
```

### CMake格式化

```powershell
# 格式化CMake文件
cmake-format -i CMakeLists.txt

# 验证格式
cmake-format --check CMakeLists.txt
```

## 测试工具（Pytest）

```powershell
# 运行所有测试
pytest

# 运行特定文件
pytest tests/test_example.py

# 并行执行
pytest -n auto

# 生成覆盖率报告
pytest --cov=src --cov-report=html
```

## 故障排查

### 权限问题

```powershell
# 以管理员身份运行PowerShell
# Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### PATH问题

```powershell
# 查看当前PATH
$env:Path -split ';'

# 手动添加Python到PATH（临时）
$env:Path += ";C:\Python312;C:\Python312\Scripts"

# 永久添加（需要管理员权限）
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";C:\Python312", "Machine")
```

### pip SSL错误

```powershell
# 升级pip并添加信任证书
pip install --upgrade pip --trusted-host pypi.org --trusted-host files.pythonhosted.org
```

## 最佳实践

1. **始终使用虚拟环境**
   - 避免全局环境污染
   - 便于项目依赖隔离

2. **定期更新依赖**
   ```powershell
   pip list --outdated
   pip install -U <package-name>
   ```

3. **使用pre-commit钩子**
   ```powershell
   pre-commit install
   pre-commit run --all-files
   ```

4. **保持requirements.txt更新**
   ```powershell
   pip freeze > requirements.txt
   ```

## 相关文档

- [Conan文档](https://docs.conan.io/)
- [CMake文档](https://cmake.org/documentation/)
- [Pytest文档](https://docs.pytest.org/)
- [Black文档](https://black.readthedocs.io/)
- [Pylint文档](https://pylint.readthedocs.io/)

## 环境变量

### 可选配置

```powershell
# 设置Python缓存目录
$env:PYTHONPYCACHEPREFIX = "D:\Temp\PythonCache"

# 设置pip缓存
$env:PIP_CACHE_DIR = "D:\Temp\PipCache"

# 设置Conan数据目录
$env:CONAN_USER_HOME = "D:\Conan"
```

## 卸载

```powershell
# 卸载Python（通过Scoop）
scoop uninstall python

# 删除虚拟环境
Remove-Item -Path .\.venv -Recurse -Force

# 删除配置文件
Remove-Item -Path $env:USERPROFILE\.pip -Recurse -Force
```
