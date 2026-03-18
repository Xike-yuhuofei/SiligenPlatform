---
Owner: @Xike
Status: active
Last reviewed: 2026-03-15
Scope: 本项目全局
---

# 工具集成清单

## 构建工具链

| 工具 | 版本要求 | 用途 |
|------|----------|------|
| Windows | 10/11 64位 | 主开发平台 |
| CMake | 建议 3.20+ | 构建系统 |
| Clang-CL / LLD | LLVM 安装于 `C:/Program Files/LLVM/bin` | C++ 编译与链接 |
| Windows SDK | 需提供 `mt.exe` | Manifest 工具 |
| Python | 3.12+ | 脚本和工具 |
| Git | 最新版 | 版本控制 |

## 依赖库

| 库 | 版本 | 用途 |
|----|------|------|
| `third_party/googletest` | vendored | 单元测试 |
| `third_party/protobuf` | vendored | DXF PB 协议与 `protoc` |
| `third_party/ruckig` | vendored | 轨迹时间参数化 |
| `third_party/nlohmann` | vendored | JSON 处理 |
| `third_party/spdlog` | vendored | 日志系统 |
| `third_party/boost` | vendored | TCP / Asio 等 |

## 构建命令

```powershell
# 推荐：始终从项目根目录执行
.\scripts\build.ps1 clean
.\scripts\build.ps1

# 构建测试
.\scripts\build.ps1 tests

# 调试构建
.\scripts\build.ps1 all -BuildType Debug
```

说明：

- 项目宪章要求优先使用 `scripts/build.ps1`
- 当前 `tests` 模式会切换 CMake 选项，但不等于只编译单一目标
- 为避免 `build/bin` 混入旧产物，切换目标前建议先执行 `clean`

## Python 依赖

```powershell
python -m pip install -r requirements.txt
python -m pip install -e D:\Projects\DXF
python -m pip install -r tools/dxf_pipeline/requirements.txt
python -m pip install -r src/adapters/hmi/requirements.txt
```

说明：

- 当前 `Backend_CPP` 会优先通过 `DxfPbPreparationService` 自动发现并调用同级 `D:\Projects\DXF`
- 如果外部 DXF 仓库不可用，才回退到本仓库内 `tools/dxf_pipeline/`

## 运行入口

| 入口 | 路径 | 说明 |
|------|------|------|
| TCP Server | `build/bin/siligen_tcp_server.exe` | HMI 后端服务，默认端口 `9527` |
| HMI | `src/adapters/hmi/main.py` | Python GUI，独立于 CMake |

## 验证协议（当前仓库真实状态）

### 最小验证

```powershell
.\build\bin\siligen_tcp_server.exe
```

### DXF 无硬件验证

```powershell
python src/adapters/hmi/main.py
```

### 单元测试

```powershell
ctest --test-dir build -C Release --output-on-failure
```

当前实测结果（2026-03-15）：

- TCP Server 构建成功
- tests 构建成功
- `ctest` 非全绿
- 已知失败：`MotionPlannerConstraintTest.AppliesVelocityFactorConstraint`

## 相关文档

- [当前项目复现指南](./04_guides/project-reproduction.md)
- [vcpkg 使用指南](./04_guides/vcpkg-usage-guide.md)
- [参考手册](./06_reference.md)
