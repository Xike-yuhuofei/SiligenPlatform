# Domain Ports 子域下沉 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 将 `src/domain/<subdomain>/ports` 下的端口迁移到对应子域（system/planning/motion/dispensing/diagnostics/configuration），更新所有引用并删除根目录。

**Architecture:** 仅做结构迁移与命名空间替换，不改接口语义；新增子域 ports 目录与 CMake 绑定，使用全局搜索确保无旧路径残留。

**Tech Stack:** C++17, CMake, Markdown, ripgrep, PowerShell

---

### Task 1: 新建子域 ports 目录与 CMake 接入

**Files:**
- Create: `src/domain/system/ports/` (new directory)
- Create: `src/domain/planning/ports/` (new directory)
- Modify: `src/domain/system/CMakeLists.txt` (if missing, add)
- Modify: `src/domain/planning/CMakeLists.txt` (if missing, add)
- Modify: `src/domain/CMakeLists.txt`

**Step 1: 创建目录与 CMake 骨架**

创建 `system/ports` 与 `planning/ports`，并在各自 `CMakeLists.txt` 添加 `add_library`/`target_sources`（与现有子域保持一致）。

**Step 2: 在 `src/domain/CMakeLists.txt` 中 include 子域**

确保 `add_subdirectory(system)` 与 `add_subdirectory(planning)` 已存在或新增。

**Step 3: 验证 CMake 配置**

Run: `cmake -S D:\Projects\SiligenSuite\control-core -B D:\Projects\SiligenSuite\control-core\build\stage4-all-modules-check -DSILIGEN_BUILD_TESTS=ON`
Expected: 生成成功，无新报错

---

### Task 2: 移动端口文件到子域

**Files:**
- Move: `src/domain/<subdomain>/ports/IEventPublisherPort.h` → `src/domain/system/ports/IEventPublisherPort.h`
- Move: `src/domain/planning/ports/IDXFFileParsingPort.h` → `src/domain/planning/ports/IDXFFileParsingPort.h`
- Move: `src/domain/planning/ports/IVisualizationPort.h` → `src/domain/planning/ports/IVisualizationPort.h`
- Move: `src/domain/<subdomain>/ports/IPositionControlPort.h` → `src/domain/motion/ports/IPositionControlPort.h`
- Move: `src/domain/motion/ports/MotionControllerPortBase.h` → `src/domain/motion/ports/MotionControllerPortBase.h`
- Move: `src/domain/<subdomain>/ports/IIOControlPort.h` → `src/domain/motion/ports/IIOControlPort.h`
- Move: `src/domain/<subdomain>/ports/IValvePort.h` → `src/domain/dispensing/ports/IValvePort.h`
- Move: `src/domain/diagnostics/ports/ITestConfigManager.h` → `src/domain/diagnostics/ports/ITestConfigManager.h`
- Move: `src/domain/<subdomain>/ports/ITestRecordRepository.h` → `src/domain/diagnostics/ports/ITestRecordRepository.h`
- Move: `src/domain/diagnostics/ports/ICMPTestPresetPort.h` → `src/domain/diagnostics/ports/ICMPTestPresetPort.h`
- Move: `src/domain/configuration/ports/ValveConfig.h` → `src/domain/configuration/ports/ValveConfig.h`

**Step 1: 执行文件移动**

使用 `Move-Item` 批量移动文件。

**Step 2: 更新头文件内部 include 路径**

修正相对路径/引用路径（例如 `#include "IIOControlPort.h"` 改为 `#include "domain/motion/ports/IIOControlPort.h"` 或同目录相对 include）。

---

### Task 3: 全局替换 include 与命名空间

**Files:**
- Modify: 全工程 C++ 源码与头文件

**Step 1: 批量替换 include 路径**

示例：
```
domain/<subdomain>/ports/IEventPublisherPort.h -> domain/system/ports/IEventPublisherPort.h
domain/<subdomain>/ports/IDXFFileParsingPort.h -> domain/planning/ports/IDXFFileParsingPort.h
domain/<subdomain>/ports/IVisualizationPort.h -> domain/planning/ports/IVisualizationPort.h
domain/<subdomain>/ports/IIOControlPort.h -> domain/motion/ports/IIOControlPort.h
domain/<subdomain>/ports/IPositionControlPort.h -> domain/motion/ports/IPositionControlPort.h
domain/<subdomain>/ports/MotionControllerPortBase.h -> domain/motion/ports/MotionControllerPortBase.h
domain/<subdomain>/ports/IValvePort.h -> domain/dispensing/ports/IValvePort.h
domain/<subdomain>/ports/ITestConfigManager.h -> domain/diagnostics/ports/ITestConfigManager.h
domain/<subdomain>/ports/ITestRecordRepository.h -> domain/diagnostics/ports/ITestRecordRepository.h
domain/<subdomain>/ports/ICMPTestPresetPort.h -> domain/diagnostics/ports/ICMPTestPresetPort.h
domain/<subdomain>/ports/ValveConfig.h -> domain/configuration/ports/ValveConfig.h
```

**Step 2: 批量替换命名空间**

示例：
```
Domain::Ports::IEventPublisherPort -> Domain::System::Ports::IEventPublisherPort
Domain::Ports::IDXFFileParsingPort -> Domain::Planning::Ports::IDXFFileParsingPort
Domain::Ports::IVisualizationPort -> Domain::Planning::Ports::IVisualizationPort
Domain::Ports::IIOControlPort -> Domain::Motion::Ports::IIOControlPort
Domain::Ports::IPositionControlPort -> Domain::Motion::Ports::IPositionControlPort
Domain::Ports::MotionControllerPortBase -> Domain::Motion::Ports::MotionControllerPortBase
Domain::Ports::IValvePort -> Domain::Dispensing::Ports::IValvePort
Domain::Ports::ITestConfigManager -> Domain::Diagnostics::Ports::ITestConfigManager
Domain::Ports::ITestRecordRepository -> Domain::Diagnostics::Ports::ITestRecordRepository
Domain::Ports::ICMPTestPresetPort -> Domain::Diagnostics::Ports::ICMPTestPresetPort
Domain::Ports::ValveSupplyConfig -> Domain::Configuration::Ports::ValveSupplyConfig
```

**Step 3: 修正 using/namespace**

检查 `using namespace` 与 `using Siligen::Domain::Ports::*`，改为对应子域。

---

### Task 4: 清理根目录与文档同步

**Files:**
- Delete: `src/domain/<subdomain>/ports/README.md`
- Delete: `src/domain/<subdomain>/ports/`（空目录）
- Modify: `src/domain/README.md`
- Modify: `src/domain/CLAUDE.md`
- Modify: `src/domain/PhysicalDependencyRules.md`
- Modify: `src/infrastructure/README.md`
- Modify: `src/application/container/README.md`
- Modify: `src/application/usecases/README.md`
- Modify: `src/application/usecases/USECASE_ANALYSIS.md`
- Modify: `src/infrastructure/adapters/README.md`

**Step 1: 删除根 ports 目录与 README**

删除 `src/domain/<subdomain>/ports` 目录。

**Step 2: 更新文档引用**

将文档中 `src/domain/<subdomain>/ports/README.md` 指向改为子域 ports 说明或删除引用。

---

### Task 5: 验证与回归检查

**Step 1: 全局残留扫描**

Run:
```
rg -n "domain/<subdomain>/ports" D:\Projects\SiligenSuite\control-core\src
rg -n "Domain::Ports" D:\Projects\SiligenSuite\control-core\src
```
Expected: 无输出

**Step 2: 构建验证**

Run: `cmake --build D:\Projects\SiligenSuite\control-core\build\stage4-all-modules-check --config Debug --target siligen_control_application`
Expected: 构建成功（允许已有警告）

**Step 3: Commit**

```bash
git add src/domain src/application src/infrastructure
git commit -m "refactor: move domain ports into subdomains"
```


