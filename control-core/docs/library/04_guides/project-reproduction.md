---
Owner: @Xike
Status: active
Last reviewed: 2026-03-15
Scope: 新人接手 / 当前项目复现
---

# 当前项目复现指南

> 目标不是介绍“理想中的项目”，而是帮助新人基于**当前仓库真实状态**完成上手、构建、验证和继续开发。

## 1. 使用原则

这份指南的判断优先级如下：

1. 源码与构建脚本
2. 本指南
3. `docs/library/`
4. `README.md`
5. `docs/_archive/`

原因很简单：当前仓库处于持续重构期，部分历史文档已经落后于代码。

## 2. 当前仓库快照（实测于 2026-03-15）

- 当前分支：`019-trajectory-unification`
- 最近提交：`f5f9d0eb9 docs: update library documentation and architecture guides`
- 工作区不是干净状态，存在大量已修改和未跟踪文件
- `git submodule status` 当前会报错：
  - `fatal: no submodule mapping found in .gitmodules for path '.claude/plugins/loop-master/Demo/ralph-claude-code'`

本次实测结论：

- `.\scripts\build.ps1` 可成功构建
- `.\scripts\build.ps1 tests` 可成功构建测试程序
- `ctest --test-dir build -C Release --output-on-failure` **不是全绿**
- 当前失败用例：
  - `MotionPlannerConstraintTest.AppliesVelocityFactorConstraint`
- 已验证的无硬件命令：
  - `.\build\bin\siligen_tcp_server.exe`
- 未实测：
  - HMI GUI 全流程
  - 真实控制卡连接
  - 真实点胶执行

这意味着：**当前仓库可构建、可做部分无硬件验证，但不能宣称“测试全部通过”或“运行链路完全已验证”。**

## 3. 新人先看什么，先忽略什么

### 先看

- `README.md`
- 本文档
- `docs/library/02_architecture.md`
- `docs/library/00_toolchain.md`
- `apps/control-runtime/container/README.md`
- `src/application/usecases/README.md`
- `src/infrastructure/resources/config/files/machine_config.ini`

### 先忽略

- `third_party/`
  - 体量大，是供应商/开源依赖，不是第一轮上手入口
- `tools/third_party_audit/`
  - 主要是第三方审计材料，不是主系统运行链路
- `docs/_archive/`
  - 只在追溯设计背景或历史问题时再看
- `build/`、`logs/`、`uploads/`、`__pycache__/`
  - 这些目录混有运行产物，不能直接当作源码事实

## 4. 顶层目录怎么理解

| 目录 | 作用 | 新人建议 |
|------|------|----------|
| `src/` | 主系统源码 | 核心关注区 |
| `tests/` | GoogleTest 单元测试 | 用于理解行为边界 |
| `docs/` | 文档体系 | 优先看 `library/` |
| `scripts/` | PowerShell / Python 辅助脚本 | 构建和环境入口 |
| `tools/` | DXF 与审计相关工具 | DXF 功能必看，第三方审计先忽略 |
| `specs/` | 功能规格、计划、任务 | 新功能开发前必须参考 |
| `third_party/` | vendored 第三方依赖 | 出现构建/链接问题再深入 |
| `data/recipes/` | 配方数据 | 无硬件复现时很有用 |
| `uploads/dxf/` | DXF 及衍生产物 | DXF 规划/预览验证入口 |
| `logs/` | 运行日志 | 排错必看 |

## 5. 系统入口与实际运行链路

### C++ 可执行入口

- TCP 服务：`build/bin/siligen_tcp_server.exe`

### Python GUI 入口

- HMI：`src/adapters/hmi/main.py`

### 两种入口的关系

1. `siligen_tcp_server.exe`
   - 提供 JSON over TCP 服务
   - 默认监听 `127.0.0.1:9527`
2. HMI
   - 不由 CMake 管理
   - 通过 TCP 客户端连接 `siligen_tcp_server.exe`
   - `BackendManager` 会优先寻找 `build/bin/siligen_tcp_server.exe`

## 6. 架构和代码阅读最短路径

仓库遵循六边形架构，当前重点层次如下：

- `apps/control-tcp-server/`
  - 当前 TCP 可执行入口
- `apps/control-runtime/`
  - 当前 runtime 装配与宿主实现
- `modules/control-gateway/`
  - TCP 协议、会话、命令分发与 facade 主实现
- `modules/device-hal/`
  - 配置、硬件、存储与设备驱动主实现
- `src/domain/`
  - 纯业务规则
  - 当前重点子域：`motion`、`dispensing`、`recipes`、`trajectory`、`safety`
- `src/application/` / `src/infrastructure/`
  - 仍可用于追踪聚合关系，但不再是优先阅读入口

如果只想快速理解“系统怎么跑起来”，建议按这个顺序阅读：

1. `apps/control-tcp-server/main.cpp`
2. `apps/control-runtime/ContainerBootstrap.cpp`
3. `apps/control-runtime/bootstrap/InfrastructureBindingsBuilder.cpp`
4. `apps/control-runtime/container/ApplicationContainer*.cpp`

## 7. 环境要求

### 操作系统

- Windows 10/11 64 位

### 编译工具链

- CMake
  - 根工程声明 `3.16+`
  - 子目录存在 `cmake_minimum_required(VERSION 3.20)`，**建议直接使用 3.20+**
- LLVM
  - 需要 `clang-cl` 与 `lld-link`
  - 根 `CMakeLists.txt` 当前硬编码查找路径：`C:/Program Files/LLVM/bin`
- Windows SDK
  - 需要 `mt.exe`
- Git

### Python

- Python 3.12+

需要按用途安装依赖：

- 仓库根：`requirements.txt`
- HMI：`src/adapters/hmi/requirements.txt`
- DXF 管线：优先 `D:\Projects\DXF`，回退 `tools/dxf_pipeline/requirements.txt`

### 硬件

- 默认配置文件中 `[Hardware] mode=Real`
- 代码支持 `Mock` 模式
- **推断**：无硬件环境可通过 `Mock` 模式完成大部分非硬件验证
- **本次未实际切换 `Mock` 模式验证**

## 8. 第一次上手的标准步骤

### 步骤 1：检查仓库状态

```powershell
git branch --show-current
git status --short
```

目的：

- 确认当前分支
- 知道仓库不是干净基线，避免误把本地产物当源码问题

### 步骤 2：初始化环境变量（可选）

```powershell
.\scripts\init_environment.ps1
```

说明：

- 当前只会设置 `BACKEND_CPP_ROOT`
- 仅供脚本使用
- 脚本会交互询问是否写入用户级环境变量

### 步骤 3：安装 Python 依赖

```powershell
python -m pip install -r requirements.txt
python -m pip install -e D:\Projects\DXF
python -m pip install -r tools/dxf_pipeline/requirements.txt
python -m pip install -r src/adapters/hmi/requirements.txt
```

### 步骤 4：清理旧构建（建议）

```powershell
.\scripts\build.ps1 clean
```

原因：

- `build/bin` 中可能残留旧目标
- 当前仓库已经观察到历史二进制残留，如 `limit_switch_test.exe`

### 步骤 5：构建

```powershell
.\scripts\build.ps1 tests
```

### 步骤 6：验证

```powershell
ctest --test-dir build -C Release --output-on-failure
```

预期：

- 当前不是全绿
- 你应该看到 `MotionPlannerConstraintTest.AppliesVelocityFactorConstraint` 失败

## 9. 构建系统的真实行为

### 唯一推荐入口

按项目宪章，应优先使用：

```powershell
.\scripts\build.ps1
```

而不是手动执行 `cmake` / `ninja`。

### 输出目录

- 可执行文件：`build/bin/`
- 库文件：`build/lib/`
- 运行配置：`build/config/machine_config.ini`

### 一个容易误解的点

`build.ps1` 的 `tests` 是通过 CMake 变量切换模式，而不是通过 `cmake --build --target <name>` 精准构建单目标。

结果是：

- `tests` 模式不等于“只生成测试”
- 不清理 `build/` 时，旧二进制会继续留在 `build/bin/`

## 10. 当前第三方依赖现状

### 已 vendored 到仓库

- `boost`
- `googletest`
- `nlohmann`
- `protobuf`
- `ruckig`
- `spdlog`
- `dxflib`
- `replxx`
- `cares`

### 需要特别知道的事实

- GoogleTest 真实使用的是 **GoogleTest**，不是旧文档里提到的 Catch2
- Protobuf 源码已 vendored，构建时会生成 `protoc.exe`
- DXF 的 `.proto` 文件位于 `proto/dxf_primitives.proto`
- `src/infrastructure/adapters/planning/dxf/CMakeLists.txt` 会在构建期生成 PB C++ 源文件

## 11. 配置、数据、日志和产物目录

### 配置文件

主配置源文件：

- `src/infrastructure/resources/config/files/machine_config.ini`

构建后会被复制到：

- `build/config/machine_config.ini`

TCP 默认使用：

- `config/machine_config.ini`

但代码会尝试：

1. 当前工作目录
2. 项目根目录

去解析相对路径。

### 日志目录

当前默认日志主要写到项目根下：

- `logs/tcp_server.log`
- `logs/hmi_tcp_client.log`
- `logs/hmi_ui.log`

### DXF 上传与衍生产物

当前目录：

- `uploads/dxf/`

这里已经有大量现成样例和派生产物：

- `.dxf`
- `.pb`
- `_preview.html`
- `_glue_points.csv`

补充说明：

- `.pb` 是 DXF 运行链路的中间格式，不是人工维护的源文件。
- DXF 上传、预览、执行前都会先确认对应 `.pb` 已就绪；若 `.pb` 缺失、为空，或时间戳落后于源 DXF，会先重新生成。
- PB 预处理默认优先调用外部 `D:\Projects\DXF\scripts\dxf_core_launcher.py`；只有找不到外部 DXF 仓库时才回退到本仓库 `tools/dxf_pipeline/dxf_to_pb.py`。
- 上传阶段会实际执行 DXF 预处理来做可解析校验；损坏 DXF、空模型空间，或仅包含不受支持实体导致导出 0 个图元时，会在上传阶段直接失败。
- 当前支持导出的实体：`LINE`、`ARC`、`CIRCLE`、`LWPOLYLINE`、2D `POLYLINE`、`SPLINE`、`ELLIPSE`、`POINT`。
- 当前不支持的常见实体：`INSERT`、`TEXT/MTEXT/ATTRIB`、`HATCH`、`DIMENSION/LEADER/MLEADER`、`IMAGE`、`XLINE/RAY`、`REGION`、`3DFACE/SOLID/TRACE`、3D `POLYLINE`/mesh 变体，以及其他未显式处理实体。
- 样条当前默认尽量保留为 `SPLINE` 写入 `.pb`；启用 `approx_splines` 后会在预处理阶段离散为折线。其精度受 `spline_samples`、`spline_max_step`、`chordal` 等参数影响，不保证与原始 NURBS 完全等价。

### 配方数据

当前目录：

- `data/recipes/recipes/`
- `data/recipes/versions/`
- `data/recipes/templates/`
- `data/recipes/audit/`
- `data/recipes/schemas/`

Schema 还有一个回退来源：

- `src/infrastructure/resources/config/files/recipes/schemas/default.json`

## 12. 当前已经确认可跑通的无硬件路径

### 1) TCP 服务启动

```powershell
.\build\bin\siligen_tcp_server.exe
```

### 2) DXF 规划

先选一个现有样例 DXF：

```powershell
Get-ChildItem uploads/dxf -Filter *.dxf | Select-Object -First 1
```

然后执行：

```powershell
python src/adapters/hmi/main.py
```

本次建议通过 HMI + TCP 后端验证 DXF 链路。

## 13. HMI 复现路径

HMI 不在 CMake 里，单独按 Python 项目处理。

### 启动前提

1. 已构建出 `build/bin/siligen_tcp_server.exe`
2. 已安装 `src/adapters/hmi/requirements.txt`
3. 当前工作目录最好是项目根

### 启动方式

```powershell
python src/adapters/hmi/main.py
```

### 当前已知行为

- HMI 侧 `BackendManager` 会尝试自动拉起后端 TCP Server
- 默认连接 `127.0.0.1:9527`
- 会把日志写到项目根 `logs/`

### 当前边界

- 本次没有进行 GUI 交互级别实测
- 因此知识库只能确认“启动链路设计”和“代码路径”，不能确认当前 GUI 全流程可用

## 14. 当前最重要的坑

### 坑 1：文档与代码并非完全同步

典型例子：

- `docs/library/00_toolchain.md` 历史版本提到 `Catch2` / `vcpkg`
- 实际构建当前使用的是 vendored `GoogleTest`、`protobuf`、`ruckig`

### 坑 2：默认 DXF 文件路径对新机器不可靠

`machine_config.ini` 里的 `[DXF] dxf_file_path` 当前指向：

- `D:\Projects\Backend_CPP\uploads\dxf\I_LOVE_YOU.dxf`

但当前仓库里并不存在这个固定名字的文件，现有样例带有时间戳前缀。

另外，当前配置与历史残留说明里仍可见同一固定路径：

- `D:\Projects\Backend_CPP\uploads\dxf\I_LOVE_YOU.dxf`

这类固定路径属于 Backend_CPP / 旧 CLI 时代残留，不应视为当前受支持入口约定。

这意味着新人不要依赖默认 DXF 路径，应该在当前 HMI 上传、TCP 请求或调试链路里显式指定待处理 DXF 文件。

### 坑 3：配置里存在机器路径

例如 `[VelocityTrace] output_path` 当前写死为：

- `D:\Projects\Backend_CPP\logs\velocity_trace`

换机器后要手动改配置，或在命令行覆盖输出路径。

### 坑 4：构建目录会残留旧产物

如果你看到 `build/bin` 里出现和当前 `examples/CMakeLists.txt` 不一致的旧程序，不一定是本次构建生成的，可能只是残留文件。

### 坑 5：当前仓库不是“全通过基线”

不要把“有 1 个已知失败测试”误判成你本地环境坏了。先复现实测失败，再决定是否修。

## 15. 使用 AI 助手时，最小上下文包

如果让 AI 快速进入有效状态，先给它这些文件：

1. 本文档
2. `CMakeLists.txt`
3. `scripts/build.ps1`
4. `src/infrastructure/resources/config/files/machine_config.ini`
5. `apps/control-runtime/bootstrap/InfrastructureBindingsBuilder.cpp`
6. `apps/control-runtime/container/ApplicationContainer.cpp`
7. `apps/control-runtime/container/ApplicationContainer.*.cpp`
8. `apps/control-tcp-server/main.cpp`
9. 相关模块对应的 `specs/<feature>/spec.md`

再补一句上下文：

- 当前分支是 `019-trajectory-unification`
- `ctest` 当前有 1 个已知失败测试
- 需要区分“无硬件复现”和“真实硬件联调”

## 16. 新人的第一天建议

如果你只想先建立正确心智，不要一上来接硬件，按下面顺序就够：

1. 读完本文档
2. 构建后端和 tests
3. 跑一次 `ctest`
4. 启动一次 TCP 服务
5. 打开一次 HMI
6. 再去读架构和具体模块

这样能先把“仓库真实状态”与“理想设计描述”分开。

