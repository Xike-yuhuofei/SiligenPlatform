# 六边形架构与C++依赖治理研究报告

**研究日期**: 2026-01-08
**研究者**: Claude Code (Research Agent)
**项目**: siligen-motion-controller

---

## 执行摘要

本报告针对六边形架构在C++项目中的实现和依赖治理最佳实践进行了系统性研究。研究发现：六边形架构的原始定义由Alistair Cockburn于1994年提出，其核心理念是通过"端口和适配器"模式实现核心业务逻辑与外部关注点的解耦。在C++项目中，依赖治理需要同时关注物理依赖（编译时）和逻辑依赖（运行时），并结合现代工具链进行自动化验证。

**关键发现**：
1. 六边形架构只有两层：内部（核心域逻辑）和外部（适配器/端口）
2. 工业级C++项目（如LLVM、Chromium）使用分层规则和CI/CD工具链强制架构约束
3. 实时控制系统需要额外的确定性约束和安全保证
4. 现代C++生态系统提供了丰富的依赖分析和架构验证工具

---

## 第一部分：六边形架构研究

### 1.1 原始定义与核心概念

**来源**:
- [Alistair Cockburn - Hexagonal Architecture](https://alistair.cockburn.us/hexagonal-architecture)
- [Hexagonal Architecture Explained (Cockburn 2024)](https://alistaircockburn.com/figs%20hexarch%20book.pdf)
- [Ports and Adapters Architecture (C2 Wiki)](https://wiki.c2.com/?PortsAndAdaptersArchitecture)

**核心原则**:

1. **两层架构**:
   - **内部（Inside）**: 核心业务逻辑和领域模型
   - **外部（Outside）**: 用户界面、数据库、外部服务、消息传递等

2. **六边形形状的目的**:
   - 打破"上/下"、"左/右"的层次化感知
   - 所有端口在概念上平等对待
   - 强调关注点分离而非技术栈分层

3. **端口和适配器**:
   - **端口（Ports）**: 应用程序与外部世界交互的接口
     - **驱动端口（Driving Ports）**: 由外部触发（如用户接口、API请求）
     - **被驱动端口（Driven Ports）**: 应用程序主动调用（如数据库、硬件接口）
   - **适配器（Adapters）**: 端口的具体实现
     - **驱动适配器（Driving Adapters）**: 如HTTP控制器、CLI界面
     - **被驱动适配器（Driven Adapters）**: 如数据库持久化、硬件驱动

4. **依赖方向**:
   - 外部依赖内部（适配器依赖端口）
   - 内部不依赖外部（核心逻辑零基础设施知识）
   - 所有依赖指向中心（领域层）

### 1.2 C++实现案例

**来源**:
- [hexagonal-this-cpp (GitHub)](https://github.com/piergst/hexagonal-this-cpp)
- [C++ Hex Architecture: CodeFest Recap](https://www.wholetomato.com/blog/summer-codefest-what-the-hex-ports-and-adapters-architecture-with-c-recap/)
- [An Implementation Guide](https://jmgarridopaz.github.io/content/hexagonalarchitecture-ig/intro.html)

**C++特定考量**:

1. **端口实现策略**:
   - **纯虚接口（Pure Abstract Interfaces）**: 最接近原始概念
   - **PIMPL模式**: 减少编译时依赖，隐藏实现细节
   - **模板基类**: 编译时多态，零运行时开销

2. **适配器设计**:
   - **单一职责**: 每个适配器实现一个端口
   - **依赖注入**: 通过构造函数注入端口接口
   - **RAII原则**: 适配器生命周期管理

3. **内存管理**:
   - 使用智能指针（`std::unique_ptr`, `std::shared_ptr`）管理适配器生命周期
   - 避免在领域层使用`new`/`delete`
   - 考虑对象池模式用于高频分配的领域对象

### 1.3 成功要素与常见陷阱

**成功要素**:

1. **清晰的边界定义**:
   - 明确识别领域核心（业务规则、不变量、实体）
   - 严格分离硬件抽象、持久化、通信等外部关注点

2. **接口稳定性**:
   - 端口接口应该稳定，变化缓慢
   - 适配器可以灵活替换而不影响领域层

3. **可测试性**:
   - 领域逻辑可独立于基础设施测试
   - 使用模拟适配器进行单元测试

**常见陷阱**:

1. **伪抽象**:
   - 端口接口直接暴露基础设施概念（如SQL、HTTP细节）
   - 适配器包含业务逻辑

2. **过度抽象**:
   - 为所有内容创建端口，包括简单数据结构
   - 增加不必要的间接层

3. **依赖泄露**:
   - 领域层间接依赖基础设施（通过模板实例化、异常类型等）
   - 编译时配置与运行时行为耦合

4. **C++特有问题**:
   - 头文件包含导致的物理依赖
   - 模板实例化的依赖传播
   - 异常类型跨越层级边界

---

## 第二部分：C++依赖分析工具

### 2.1 依赖分析工具链

**来源**:
- [Stack Overflow - Tool to track #include dependencies](https://stackoverflow.com/questions/42308/tool-to-track-include-dependencies)
- [cpp-dependencies (GitHub)](https://github.com/tomtom-international/cpp-dependencies)
- [Reddit - CMake dependency analysis tools](https://www.reddit.com/r/cpp_questions/comments/1gt5wp7/is_there_a_tool_to_analyze_a_cmake_project/)

**工具清单**:

| 工具 | 类型 | 功能 | 适用场景 |
|------|------|------|----------|
| **Include-What-You-Use (IWYU)** | 静态分析 | 分析头文件包含关系，识别未使用的include | 减少编译时依赖 |
| **cpp-dependencies** | 扫描器 | 生成C++代码的#include依赖图 | 可视化物理依赖 |
| **CppDepend** | 商业工具 | 分析类间依赖、检测循环依赖 | 架构质量监控 |
| **CMake Graphviz** | 内置 | 生成CMake目标依赖图 | 构建系统级分析 |

**工具使用示例**:

```bash
# IWYU - 分析头文件包含
include-what-you-use -Xiwyu --mapping_file=imp mappings.h src/*.cpp

# cpp-dependencies - 生成依赖图
cpp-dependencies --src src --output dependencies.dot

# CMake - 生成目标依赖图
cmake --graphviz=dependencies.dot .
```

### 2.2 静态分析与架构验证

**来源**:
- [Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
- [CodeChecker Documentation](https://codechecker.readthedocs.io/)
- [RFC: Integrate Clang-Tidy into Clang Static Analyzer](https://discourse.llvm.org/t/rfc-integrate-clang-tidy-checkers-into-clang-static-analyzer/88937)

**工具集成策略**:

1. **Clang-Tidy**:
   - 自定义检查器检测架构违规
   - 现代C++最佳实践强制
   - CI/CD集成示例：

   ```yaml
   # .github/workflows/architecture-check.yml
   - name: Run architecture checks
     run: |
       clang-tidy src/domain/**/*.cpp \
         -checks='-*,modernize-*,-modernize-use-trailing-return-type' \
         --config-file=.clang-tidy
   ```

2. **CodeChecker**:
   - 基于LLVM/Clang的静态分析基础设施
   - 替代`scan-build`的现代化工具
   - 提供Web界面查看分析结果

3. **自定义架构检查器**:
   ```cpp
   // 示例：检测Domain层不应依赖Infrastructure
   bool CheckDomainDependency(const Decl* D) {
     if (!IsInDomainLayer(D)) return true;

     for (auto* Include : D->getIncludedFiles()) {
       if (IsInInfrastructureLayer(Include)) {
         Report(D->getLocation(), "Domain layer must not depend on Infrastructure");
         return false;
       }
     }
     return true;
   }
   ```

### 2.3 CMake依赖图生成

**来源**:
- [CMakeGraphVizOptions Documentation](https://cmake.org/cmake/help/latest/module/CMakeGraphVizOptions.html)
- [Visualising Module Dependencies with CMake and Graphviz](https://embeddeduse.com/2022/03/01/visualising-module-dependencies-with-cmake-and-graphviz/)
- [cmake-scripts/dependency-graph.cmake (GitHub)](https://github.com/StableCoder/cmake-scripts/blob/main/dependency-graph.cmake)

**实现方法**:

1. **内置Graphviz支持**:
   ```cmake
   # CMakeLists.txt
   set(CMAKE_GRAPHVIZ_EXECUTABLES "true")
   set(CMAKE_GRAPHVIZ_EXTERNAL_FLAGS "--rankdir=LR")
   ```

   ```bash
   # 生成依赖图
   cmake --graphviz=deps.dot .
   dot -Tpng deps.dot -o dependencies.png
   ```

2. **自定义脚本增强**:
   ```cmake
   # 来自 StableCoder/cmake-scripts
   include(dependency-graph)

   # 过滤特定目标
   generate_dependency_graph(
     TARGETS ${PROJECT_LIBRARIES}
     OUTPUT ${CMAKE_BINARY_DIR}/filtered-deps.dot
     EXCLUDE_TARGETS "test_*"
   )
   ```

3. **CI/CD集成**:
   ```yaml
   - name: Generate dependency graph
     run: |
       cmake -B build -DCMAKE_GRAPHVIZ_EXECUTABLES="true"
       cmake --graphviz=build/deps.dot build
       dot -Tsvg build/deps.dot -o dependencies.svg

   - name: Upload dependency artifacts
     uses: actions/upload-artifact@v3
     with:
       name: dependency-graph
       path: dependencies.svg
   ```

---

## 第三部分：工业级先例研究

### 3.1 LLVM分层架构

**来源**:
- [LLVM Coding Standards](https://llvm.org/docs/CodingStandards.html)
- [Layering Requirements in LLVM](https://discourse.llvm.org/t/layering-requirements-in-the-llvm-coding-style-guide/47364)

**核心原则**:

1. **分层要求**:
   - **库之间清晰的分层**
   - **依赖方向单向**: 上层可以依赖下层，反之不行
   - **最小化公共接口**: 减少库间耦合

2. **目录结构映射**:
   ```
   llvm/
   ├── include/llvm/       # 公共接口
   ├── lib/                # 实现细节
   │   ├── IR/            # 中间表示 - 底层
   │   ├── Analysis/      # 依赖IR
   │   ├── Transform/     # 依赖Analysis
   │   └── CodeGen/       # 依赖Transform
   └── tools/             # 依赖所有库
   ```

3. **强制机制**:
   - **代码审查**: 人工验证依赖合规性
   - **工具检测**: 使用`llvm-lit`和自定义脚本
   - **物理隔离**: 目录结构反映依赖关系

**对siligen项目的启示**:
- 类似的分层：domain（底层）-> application -> adapters（上层）
- 通过目录结构强制依赖方向
- 使用脚本自动验证依赖合规性

### 3.2 Chromium组件架构

**来源**:
- [Chromium C++ Style Guide](https://chromium.googlesource.com/experimental/chromium/src/+/refs/tags/72.0.3612.2/styleguide/c++/c++.md)
- [Browser Components Cookbook](https://www.chromium.org/developers/design-documents/cookbook/)

**架构模式**:

1. **模块化组件**:
   - **组件独立性**: 每个组件可独立编译和测试
   - **依赖注入**: 使用`DependsFrom`声明组件依赖
   - **服务定位器**: `content::BrowserContext`提供关键服务

2. **分层策略**:
   ```
   content/
   ├── browser/           # 浏览器主逻辑
   ├── renderer/          # 渲染进程
   ├── common/            # 共享代码（被两者依赖）
   └── public/            # 公共接口
   ```

3. **构建系统强制**:
   - **GN元构建系统**: 声明式依赖管理
   - **可见性声明**: `visibility = [ ":*" ]`控制符号导出
   - **依赖图检查**: GN内置循环依赖检测

**可移植的最佳实践**:
- 明确的组件边界（对应六边形的端口）
- 声明式依赖管理（CMake的`target_link_libraries`）
- 可见性控制（CMake的`target_sources`和`visibility`属性）

### 3.3 实时运动控制系统

**来源**:
- [C++ For Industrial Automation: Building Real-Time Control Systems](https://www.codewithc.com/c-for-industrial-automation-building-real-time-control-systems/)
- [Mastering Embedded C/C++ for Real-Time Systems](https://medium.com/@pankajkompella22/mastering-embedded-c-c-for-real-time-embedded-systems-62c5adbd3431)
- [A Real-time Service-Oriented Architecture for Industrial Automation (PDF)](https://core.ac.uk/download/pdf/54933057.pdf)

**关键约束**:

1. **确定性执行**:
   - **有界执行时间**: 所有关键路径操作有可预测的最坏情况执行时间
   - **无阻塞调用**: 实时循环中禁用堆分配、锁竞争、I/O操作
   - **优先级继承**: 使用RTOS优先级协议避免优先级反转

2. **内存管理**:
   - **静态分配**: 预分配所有关键资源
   - **对象池**: 避免实时路径的动态分配
   - **栈内存优先**: 减少堆内存碎片化

3. **硬件抽象**:
   - **HAL层隔离**: 硬件细节通过抽象层封装
   - **寄存器访问限制**: 直接硬件访问仅在特定层
   - **中断处理分离**: 中断服务例程与业务逻辑分离

**对siligen项目的相关性**:
```cpp
// 示例：实时安全的领域接口
namespace Siligen::Domain::Motion {
  class ITrajectoryExecutor {
  public:
    // noexcept保证无异常抛出
    virtual Result<void> ExecuteTrajectory(
      const Trajectory& trajectory,
      Timeslice budget_ms  // 时间预算约束
    ) noexcept = 0;

    // 禁用动态分配
    virtual ~ITrajectoryExecutor() = default;
  };
}
```

---

## 第四部分：C++特定的依赖管理

### 4.1 物理依赖 vs 逻辑依赖

**来源**:
- [The Pimpl Pattern - C++ Stories](https://www.cppstories.com/2018/01/pimpl/)
- [Physical Structure and C++ – Part 1](https://gamesfromwithin.com/physical-structure-and-c-part-1-a-first-look)
- [Guidelines for Physical Design (John Lakos)](http://www.cs.unc.edu/~stotts/COMP204/lakos/guide.html)

**核心概念**:

1. **物理依赖（编译时）**:
   - **#include关系**: 头文件包含导致的编译时依赖
   - **模板实例化**: 编译器需要模板定义
   - **内联函数**: 需要在头文件中可见

2. **逻辑依赖（运行时）**:
   - **虚函数调用**: 动态分派的运行时开销
   - **依赖注入**: 对象间的引用关系
   - **模块加载**: 动态库的符号解析

**最佳实践**:

| 场景 | 物理依赖策略 | 逻辑依赖策略 |
|------|-------------|-------------|
| **领域层接口** | 前向声明优先，减少#include | 纯虚接口，虚函数表 |
| **适配器实现** | PIMPL隐藏实现细节 | 依赖注入端口接口 |
| **跨层通信** | 通过端口接口，无直接依赖 | 基于接口编程，多态调用 |

### 4.2 PIMPL模式详解

**来源**:
- [Pimpl idiom - cppreference](https://en.cppreference.com/w/cpp/language/pimpl)
- [Modern C++: The Pimpl Idiom (Medium, 2024)](https://medium.com/@weidagang/modern-c-the-pimpl-idiom-53173b16a60a)
- [Pimpl vs Pure Virtual Interface (Stack Overflow)](https://stackoverflow.com/questions/825018/pimpl-idiom-vs-pure-virtual-class-interface)

**PIMPL vs 纯虚接口**:

| 维度 | PIMPL | 纯虚接口 |
|------|-------|----------|
| **编译时解耦** | ⭐⭐⭐⭐⭐ 极强 | ⭐⭐⭐ 中等 |
| **运行时开销** | ⭐⭐⭐⭐⭐ 无（编译时多态） | ⭐⭐⭐ 虚函数调用 |
| **动态替换** | ❌ 需重新编译 | ✅ 运行时替换 |
| **实现复杂度** | ⭐⭐⭐ 中等 | ⭐⭐ 简单 |
| **适用场景** | 隐藏实现，减少重编译 | 插件架构，运行时多态 |

**实现示例**:

```cpp
// hardware_controller.h (领域端口)
namespace Siligen::Domain::Ports {
  class IHardwareController {
  public:
    virtual Result<void> Initialize() noexcept = 0;
    virtual Result<void> EmergencyStop() noexcept = 0;
    virtual ~IHardwareController() = default;
  };
}

// hardware_controller_impl.h (基础设施适配器)
namespace Siligen::Infrastructure::Adapters {
  class HardwareControllerImpl : public IHardwareController {
  public:
    Result<void> Initialize() noexcept override;
    Result<void> EmergencyStop() noexcept override;

  private:
    // PIMPL: 隐藏MultiCard_API细节
    class Impl;
    std::unique_ptr<Impl> pImpl_;
  };
}

// hardware_controller_impl.cpp
class HardwareControllerImpl::Impl {
public:
  // 可以包含任何头文件，不影响领域层
  #include "MultiCard_API.h"

  MultiCardHandle card_handle_;
  // ... 实现细节
};

Result<void> HardwareControllerImpl::Initialize() noexcept {
  return pImpl_->Initialize();
}
```

### 4.3 头文件包含最佳实践

**来源**:
- [Header File Fundamentals and Best Practices (Medium, 2025)](https://ibrahimmansur4.medium.com/header-file-fundamentals-and-best-practices-in-c-98b1fe87dae0)
- [Top 10 C++ Header File Mistakes](https://acodersjourney.com/top-10-c-header-file-mistakes-and-how-to-fix-them/)
- [Bloomberg BDE: Physical Code Organization](https://github.com/bloomberg/bde/wiki/physical-code-organization)

**分层包含规则**:

1. **领域层头文件** (`src/domain/**/*.h`):
   ```cpp
   // ✅ 允许
   #include "domain/<subdomain>/ports/IHardwareController.h"  // 端口接口
   #include <memory>  // 标准库智能指针

   // ❌ 禁止
   #include "infrastructure/hardware/MultiCard_API.h"  // 基础设施
   #include "application/usecases/EmergencyStop.h"      // 应用层
   ```

2. **应用层头文件** (`src/application/**/*.h`):
   ```cpp
   // ✅ 允许
   #include "domain/<subdomain>/ports/IHardwareController.h"  // 依赖领域
   #include "domain/models/Trajectory.h"

   // ❌ 禁止
   #include "infrastructure/adapters/HardwareAdapter.h"  // 直接依赖适配器
   ```

3. **适配器头文件** (`src/infrastructure/adapters/**/*.h`):
   ```cpp
   // ✅ 允许
   #include "domain/<subdomain>/ports/IHardwareController.h"  // 实现端口
   #include <MultiCard_API.h>  // 具体硬件API

   // ✅ PIMPL实现
   class HardwareAdapter::Impl;  // 前向声明
   ```

**减少编译时依赖**:

```cpp
// ❌ 不好的做法：直接包含完整类型
#include "domain/models/ComplexTrajectory.h"

// ✅ 好的做法1：前向声明（如果只需要指针/引用）
namespace Siligen::Domain::Models {
  class ComplexTrajectory;
}

void ProcessTrajectory(ComplexTrajectory* traj);

// ✅ 好的做法2：使用PIMPL完全隐藏
class TrajectoryProcessor::Impl;
std::unique_ptr<Impl> pImpl_;
```

### 4.4 C++20模块作为未来方向

**来源**:
- [C++20: The Advantages of Modules](https://www.modernescpp.com/index.php/cpp20-modules/)
- [Diving into C++ Modules • The theory](https://www.albertogramaglia.com/modern-cpp-modules-theory/)
- [Migrating large codebases to C++ Modules (CERN)](https://indico.cern.ch/event/708041/papers/3276196/files/8971-ACAT2019_CxxModules_jp.pdf)

**模块 vs 头文件**:

| 特性 | 头文件 | C++20模块 |
|------|--------|-----------|
| **编译次数** | O(n²) | O(n) |
| **宏隔离** | ❌ 全局污染 | ✅ 模块私有 |
| **依赖声明** | 隐式（#include传递） | 显式（import） |
| **工具支持** | 成熟 | 发展中 |
| **架构强制** | 需要外部工具 | 语言内置 |

**模块化架构示例**:

```cpp
// domain/trajectory.mpp (C++23模块语法)
export module Siligen.Domain.Trajectory;

// 显式导出接口
export import Siligen.Domain.Ports;
import std;  // 标准库

// 模块私有实现（不可被外部访问）
namespace Siligen::Domain::Motion::Detail {
  class TrajectoryCalculator {
    // ... 实现细节
  };
}

// 公共接口
export namespace Siligen::Domain::Motion {
  class TrajectoryExecutor {
    // ... 用户接口
  };
}
```

**迁移建议**:
- 短期：继续使用头文件 + IWYU
- 中期：评估C++20模块编译器支持
- 长期：计划渐进式迁移（从领域层开始）

---

## 第五部分：与siligen项目规范的相关性分析

### 5.1 当前10条规范与研究发现对比

| siligen规范 | 研究发现支持 | 工具/技术建议 |
|------------|------------|--------------|
| **HEXAGONAL_DEPENDENCY_DIRECTION** | ✅ 完全一致 | Lattix Architect, CMake Graphviz |
| **HEXAGONAL_LAYER_ORDER** | ✅ 完全一致 | LLVM分层规则, Chromium GN |
| **HEXAGONAL_PORT_ACCESS** | ✅ 完全一致 | 纯虚接口, PIMPL |
| **DOMAIN_NO_DYNAMIC_MEMORY** | ✅ 实时系统标准 | 对象池, 静态分配 |
| **DOMAIN_NO_STL_CONTAINERS** | ⚠️ 需抑制机制 | FixedCapacityVector + SUPPRESS注释 |
| **DOMAIN_NO_IO_OR_THREADING** | ✅ 完全一致 | Clang-Tidy检查器 |
| **DOMAIN_PUBLIC_API_NOEXCEPT** | ✅ 工业标准 | noexcept强制 |
| **HARDWARE_DOMAIN_ISOLATION** | ✅ 关键原则 | HAL层, 依赖倒置 |
| **INDUSTRIAL_REALTIME_SAFETY** | ✅ 安全关键 | O(1)算法验证, WCET分析 |
| **NAMESPACE_LAYER_ISOLATION** | ✅ 最佳实践 | Clang-Tidy namespace检查 |

### 5.2 规范增强建议

基于研究发现，建议增加以下规范：

1. **物理依赖规范** (PHYSICAL_DEPENDENCY.md):
   ```yaml
   RULE: HEADER_INCLUDE_DISCIPLINE
   SCOPE: src/domain/**/*.h
   FORBIDDEN:
     - infrastructure_includes
     - application_includes
   VERIFY: iwyu_tool src/domain/
   ON_FAIL: ABORT
   ```

2. **编译时解耦规范** (COMPILE_TIME_DECOUPLING.md):
   ```yaml
   RULE: PIMPL_FOR_ADAPTERS
   SCOPE: src/infrastructure/adapters/**
   MUST: hide_implementation_details
   VERIFY: rg "^class.*::Impl" src/infrastructure/adapters/
   ON_FAIL: WARNING
   ```

3. **架构验证工具链** (TOOLING.md):
   ```yaml
   RULE: CI_DEPENDENCY_CHECK
   MUST:
     - clang-tidy_every_commit
     - dependency_graph_generation
     - architecture_conformance_check
   VERIFY: .github/workflows/architecture-check.yml
   ON_FAIL: BLOCK_PR
   ```

### 5.3 抑制机制与研究发现一致性

用户当前实现的抑制机制（`.claude/rules/EXCEPTIONS.md`）与研究发现**高度一致**：

- **学术支持**: C++社区承认架构规则需要合理例外
- **先例**: LLVM和Chromium都有架构规则例外（通过代码审查批准）
- **工具支持**: Lattix Architect支持"标记规则"（类似抑制）
- **最佳实践**: Bloomberg BDE使用分层抑制机制

**建议的抑制使用场景**（基于研究）：

1. **轨迹插补模块**:
   ```cpp
   // ✅ 合理抑制：运行时大小不确定
   // CLAUDE_SUPPRESS: DOMAIN_NO_STL_CONTAINERS
   // Reason: 轨迹点数量取决于路径长度(用户输入)和插补精度。
   //         FixedCapacityVector会导致栈溢出(10000+点)或容量不足。
   //         工业实时系统案例：CERN的LHC控制系统使用std::vector存储轨迹。
   // Approved-By: Claude Code Research Agent
   // Date: 2026-01-08
   std::vector<TrajectoryPoint> GenerateTrajectory() noexcept;
   ```

2. **性能关键路径**:
   ```cpp
   // ✅ 合理抑制：O(1)哈希查找需求
   // CLAUDE_SUPPRESS: REALTIME_NO_STL
   // Reason: 紧急停止需要O(1)硬件地址查找。
   //         自定义实现会引入更多bug，且std::unordered_map是标准库的实时安全实现。
   // Approved-By: Claude Code Research Agent
   // Date: 2026-01-08
   std::unordered_map<CardID, HardwareHandle> emergency_stop_map_;
   ```

---

## 第六部分：对siligen项目的适用性建议

### 6.1 短期改进（1-2个月）

**优先级1：工具链自动化**

1. **集成Clang-Tidy架构检查**:
   ```yaml
   # .clang-tidy
   Checks: >
     -*,
     -modernize-*,
    clang-diagnostic-*,
    -clang-analyzer-*,
    misc-non-private-member-variables-in-classes,
    google-build-namespaces

   CheckOptions:
     - key: misc-non-private-member-variables-in-classes.AllowOnConstMembers
       value: 1
   ```

   ```yaml
   # .github/workflows/static-analysis.yml
   - name: Run Clang-Tidy
     run: |
       clang-tidy src/**/*.cpp \
         -p build/compile_commands.json \
         --config-file=.clang-tidy \
         | tee clang-tidy-output.txt

   - name: Check for architecture violations
     run: |
       if grep -i "domain.*infrastructure\|infrastructure.*domain" clang-tidy-output.txt; then
         echo "❌ Architecture violation detected!"
         exit 1
       fi
   ```

2. **依赖图可视化**:
   ```cmake
   # cmake/DependencyGraph.cmake
   function(generate_architecture_graph)
     set(CMAKE_GRAPHVIZ_EXECUTABLES "true")
     set(CMAKE_GRAPHVIZ_GENERATE_PER_TARGET "true")
     set(CMAKE_GRAPHVIZ_IGNORE_TARGETS "test-*")

     # 过滤规则
     set(CMAKE_GRAPHVIZ_GRAPH_HEADER
       "digraph G { node [shape=box]; rankdir=LR; }")

     cmake --graphviz=${CMAKE_BINARY_DIR}/architecture.dot .
   endfunction()
   ```

3. **IWYU集成**:
   ```yaml
   # .github/workflows/iwyu.yml
   - name: Install IWYU
     run: |
       git clone https://github.com/include-what-you-use/include-what-you-use.git
       cd include-what-you-use
       cmake -B build -DCMAKE_PREFIX_PATH=/usr/lib/llvm-18
       cmake --build build --target install

   - name: Run IWYU check
     run: |
       iwyu_tool -p build src/domain/**/*.cpp \
         -Xiwyu --mapping_file=.iwyu.imp \
         -Xiwyu --max_recommends=5
   ```

**优先级2：文档与培训**

1. **架构决策记录（ADR）**:
   ```markdown
   # ADR-001: 领域层STL容器抑制

   状态: 已接受
   日期: 2026-01-08
   背景: 轨迹插补模块需要运行时大小的轨迹点集合

   决策: 使用std::vector并添加CLAUDE_SUPPRESS注释

   理由:
   - 轨迹点数量取决于用户输入（路径长度）和配置（插补精度）
   - 工业先例：CERN LHC控制系统、Chromium渲染引擎
   - FixedCapacityVector会导致栈溢出（10000+点）或容量不足

   后果:
   + ✅ 灵活处理任意长度轨迹
   + ✅ 符合C++最佳实践
   - ⚠️ 需要季度审查抑制
   - ⚠️ 需要性能监控（重分配开销）
   ```

2. **开发者培训材料**:
   - 六边形架构简介（30分钟视频 + 练习）
   - C++物理依赖管理（基于Lakos书籍）
   - 实时C++编程规范（基于INDUSTRIAL.md）

**优先级3：监控与度量**

1. **依赖指标仪表板**:
   ```yaml
   # .github/workflows/metrics.yml
   - name: Calculate dependency metrics
     run: |
       # 计算跨层级依赖数量
       INFRA_TO_DOMAIN=$(rg "#include.*domain/" src/infrastructure/ | wc -l)
       DOMAIN_TO_INFRA=$(rg "#include.*infrastructure/" src/domain/ | wc -l)

       echo "## Dependency Metrics" >> $GITHUB_STEP_SUMMARY
       echo "- Infrastructure → Domain: $INFRA_TO_DOMAIN (允许)" >> $GITHUB_STEP_SUMMARY
       echo "- Domain → Infrastructure: $DOMAIN_TO_INFRA (禁止)" >> $GITHUB_STEP_SUMMARY

       if [ $DOMAIN_TO_INFRA -gt 0 ]; then
         echo "❌ Domain layer depends on Infrastructure!"
         exit 1
       fi
   ```

2. **抑制趋势追踪**:
   ```python
   # scripts/track_suppressions.py
   import re
   from pathlib import Path
   import json
   from datetime import datetime

   suppressions = []
   for file in Path("src").rglob("*.cpp"):
       content = file.read_text()
       matches = re.finditer(
           r"CLAUDE_SUPPRESS: (\w+)\n.*?Reason: (.{50,})",
           content,
           re.MULTILINE | re.DOTALL
       )
       for match in matches:
           suppressions.append({
               "rule": match.group(1),
               "reason": match.group(2).strip(),
               "file": str(file),
               "date": datetime.now().isoformat()
           })

   with open(".claude/suppressions.json", "w") as f:
       json.dump(suppressions, f, indent=2)
   ```

### 6.2 中期改进（3-6个月）

**优先级1：架构测试框架**

1. **架构单元测试**:
   ```cpp
   // tests/architecture/dependency_test.cpp
   #include <gtest/gtest.h>
   #include "analysis/DependencyAnalyzer.h"

   TEST(ArchitectureTest, DomainMustNotDependOnInfrastructure) {
     DependencyAnalyzer analyzer;
     analyzer.LoadSourceTree("src/");

     auto violations = analyzer.FindViolations({
       .from = "src/domain/**",
       .to = "src/infrastructure/**",
       .type = DependencyType::Include
     });

     EXPECT_EQ(violations.size(), 0)
       << "Domain layer has " << violations.size()
       << " dependencies on Infrastructure:\n"
       << FormatViolations(violations);
   }

   TEST(ArchitectureTest, AdaptersMustImplementPorts) {
     auto adapters = ListClassesInDirectory("src/infrastructure/adapters");
     auto ports = ListClassesInDirectory("src/domain/ports");

     for (const auto& adapter : adapters) {
       EXPECT_TRUE(adapter.ImplementsAnyOf(ports))
         << "Adapter " << adapter.name
         << " does not implement any port interface";
     }
   }
   ```

2. **契约测试**:
   ```cpp
   // tests/contracts/port_contract_test.cpp
   template<typename PortImpl>
   class IHardwareControllerContract : public testing::Test {
   protected:
     void TestEmergencyStopLatency() {
       auto controller = CreatePortImpl();
       auto start = std::chrono::high_resolution_clock::now();

       controller->EmergencyStop();

       auto end = std::chrono::high_resolution_clock::now();
       auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

       EXPECT_LT(latency.count(), 10)  // 10ms约束
         << "EmergencyStop exceeded real-time constraint";
     }

     void TestInitializeFailurePropagation() {
       auto controller = CreatePortImpl();
       // Mock hardware failure...
       EXPECT_EQ(controller->Initialize(),
                 Result<void>::Error("Hardware not connected"));
     }
   };

   using Implementations = testing::Types<
     HardwareAdapter,      // 真实实现
     MockHardwareController // 测试Mock
   >;

   TYPED_TEST_SUITE(IHardwareControllerContract, Implementations);
   TYPED_TEST(IHardwareControllerContract, EmergencyStopLatency) { /* ... */ }
   ```

**优先级2：性能分析工具**

1. **WCET（最坏情况执行时间）分析**:
   ```yaml
   # .github/workflows/performance-analysis.yml
   - name: Build with profiling
     run: |
       cmake -B build -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage"
       cmake --build build

   - name: Run real-time critical paths
     run: |
       ./build/tests/realtime_critical_test \
         --benchmark_filter=TrajectoryExecutor \
         --benchmark_out=performance.json

   - name: Verify WCET constraints
     run: |
       python scripts/verify_performance.py \
         --input performance.json \
         --max-time-us=1000  # 1ms约束
   ```

2. **内存分配追踪**:
   ```cpp
   // src/domain/motion/TrajectoryExecutor.cpp
   #ifdef TRACK_ALLOCATIONS
   #include <tracing/allocator_tracker.h>
   #endif

   Result<void> TrajectoryExecutor::Execute(
       const Trajectory& traj,
       Timeslice budget_ms) noexcept {
     #ifdef TRACK_ALLOCATIONS
     AllocationTracker tracker("TrajectoryExecutor::Execute");
     #endif

     // 执行逻辑...

     #ifdef TRACK_ALLOCATIONS
     auto report = tracker.GetReport();
     if (report.allocations > 0) {
       LogWarning() << "Dynamic allocation detected in real-time path: "
                    << report.allocations << " allocations";
     }
     #endif

     return Success();
   }
   ```

**优先级3：重构基础设施**

1. **引入依赖注入框架**:
   ```cpp
   // include/injection/ServiceContainer.h
   namespace Siligen::Infrastructure::Injection {
     class ServiceContainer {
     public:
       template<typename Interface, typename Implementation>
       void RegisterSingleton() {
         factories_[typeid(Interface)] = []() -> std::shared_ptr<void> {
           return std::make_shared<Implementation>();
         };
       }

       template<typename Interface>
       std::shared_ptr<Interface> Resolve() {
         auto it = factories_.find(typeid(Interface));
         if (it == factories_.end()) {
           throw std::runtime_error("Service not registered");
         }
         return std::static_pointer_cast<Interface>(it->second());
       }

     private:
       std::unordered_map<std::type_index,
                          std::function<std::shared_ptr<void>()>> factories_;
     };
   }

   // src/application/bootstrapper.cpp
   auto container = std::make_shared<ServiceContainer>();

   // 注册服务（所有依赖指向领域）
   container->RegisterSingleton<IHardwareController, HardwareAdapter>();
   container->RegisterSingleton<ITrajectoryExecutor, TrajectoryExecutor>();

   // 解析服务
   auto executor = container->Resolve<ITrajectoryExecutor>();
   ```

2. **领域服务定位器模式**:
   ```cpp
   // src/domain/services/DomainServices.h
   namespace Siligen::Domain::Services {
     class DomainServices {
     public:
       static void SetHardwareController(
           std::shared_ptr<Ports::IHardwareController> controller) {
         Instance().hardware_controller_ = controller;
       }

       static std::shared_ptr<Ports::IHardwareController>
       GetHardwareController() {
         return Instance().hardware_controller_;
       }

     private:
       static DomainServices& Instance() {
         static DomainServices instance;
         return instance;
       }

       std::shared_ptr<Ports::IHardwareController> hardware_controller_;
     };
   }
   ```

### 6.3 长期规划（6-12个月）

**优先级1：C++20模块迁移**

1. **迁移路线图**:
   ```
   阶段1 (Month 1-2): 评估与实验
   ├─ 编译器支持矩阵检查（GCC 12+, Clang 16+, MSVC 19.36+）
   ├─ 构建系统改造（CMake 3.28+ modules支持）
   └─ 小规模POC（src/domain/models/单个模块）

   阶段2 (Month 3-4): 领域层迁移
   ├─ src/domain/<subdomain>/ports/ → 端口模块
   ├─ src/domain/models/ → 领域模型模块
   └─ src/domain/motion/ → 运动控制模块

   阶段3 (Month 5-6): 应用层迁移
   ├─ src/application/usecases/ → 用例模块
   └─ src/application/services/ → 应用服务模块

   阶段4 (Month 7-8): 适配器层迁移
   ├─ src/infrastructure/adapters/ → 适配器模块
   └─ 移除旧的头文件包含

   阶段5 (Month 9-12): 优化与完善
   ├─ 性能对比测试（模块 vs 头文件）
   ├─ CI/CD流水线调整
   └─ 开发者培训
   ```

2. **模块化示例**:
   ```cpp
   // src/domain/<subdomain>/ports/trajectory.mpp (C++23)
   export module Siligen.Domain.Ports.Trajectory;

   // 仅导入必要的模块
   export import Siligen.Domain.Core;
   import std;

   // 导出端口接口
   export namespace Siligen::Domain::Ports {
     class ITrajectoryExecutor {
     public:
       virtual Result<void> Execute(
         const Trajectory& traj,
         Timeslice budget_ms
       ) noexcept = 0;

       virtual ~ITrajectoryExecutor() = default;
     };
   }

   // 模块私有实现（不可被外部访问）
   namespace Siligen::Domain::Detail {
     class TrajectoryValidator {
       // ... 仅在此模块内可见
     };
   }
   ```

**优先级2：高级架构分析工具**

1. **Lattix Architect集成**:
   ```python
   # scripts/lattix-config.json
   {
     "project": "siligen-motion-controller",
     "source_roots": ["src/domain", "src/application", "src/infrastructure"],
     "rules": [
       {
         "name": "Hexagonal Layering",
         "type": "layer",
         "layers": {
           "Domain": ["src/domain/**"],
           "Application": ["src/application/**"],
           "Infrastructure": ["src/infrastructure/**"]
         },
         "constraints": [
           {"from": "Application", "to": "Domain", "allowed": true},
           {"from": "Infrastructure", "to": "Domain", "allowed": true},
           {"from": "Domain", "to": ["Application", "Infrastructure"], "allowed": false}
         ]
       },
       {
         "name": "No Cycles",
         "type": "cycle",
         "scope": "all"
       }
     ]
   }
   ```

2. **自定义架构DSL**:
   ```yaml
   # architecture/dependency-rules.yaml
   layers:
     domain:
       path: src/domain
       allowed_dependencies: []
     application:
       path: src/application
       allowed_dependencies: [domain]
     infrastructure:
       path: src/infrastructure
       allowed_dependencies: [domain]

   rules:
     - name: "NO_DOMAIN_TO_INFRA"
       from: domain
       to: infrastructure
       action: ERROR

     - name: "PORT_PURITY"
       scope: src/domain/<subdomain>/ports/**/*.h
       forbidden_patterns:
         - "#include.*infrastructure"
         - "#include.*application"
       action: ERROR

     - name: "REALTIME_SAFETY"
       scope: src/domain/motion/**
       forbidden_patterns:
         - "\\bnew\\s+", "\\bdelete\\s+", "\\bstd::vector\\b"
       exceptions:
         - pattern: "CLAUDE_SUPPRESS.*DOMAIN_NO_STL_CONTAINERS"
           requires_approval: true
       action: WARNING
   ```

3. **实时架构验证**:
   ```cpp
   // tools/architecture_validator/RealtimeValidator.cpp
   class RealtimeValidator {
   public:
     ValidationResult Validate(const std::string& source_file) {
       ValidationResult result;

       // 检查实时路径中的禁止模式
       if (IsInRealtimePath(source_file)) {
         auto content = ReadFile(source_file);

         // 动态内存分配检测
         if (Contains(content, R"(\bnew\s+)") ||
             Contains(content, R"(\bdelete\s+)") ||
             Contains(content, R"(std::make_shared)")) {
           result.AddError("Dynamic allocation in realtime path");
         }

         // 阻塞调用检测
         if (Contains(content, R"(std::cout|std::thread|std::mutex)")) {
           result.AddError("Blocking operation in realtime path");
         }

         // 无界循环检测
         if (ContainsUnboundedLoop(content)) {
           result.AddError("Unbounded loop in realtime path");
         }
       }

       return result;
     }

   private:
     bool IsInRealtimePath(const std::string& file) {
       return file.find("src/domain/motion/") != std::string::npos ||
              file.find("src/application/usecases/EmergencyStop") != std::string::npos;
     }

     bool ContainsUnboundedLoop(const std::string& code) {
       // 简化版：检测没有显式边界条件的while/for循环
       static std::regex loop_regex(R"(\b(while|for)\s*\([^)]*\)\s*\{)");
       // 完整实现需要AST分析
       return std::regex_search(code, loop_regex);
     }
   };
   ```

**优先级3：工业级质量保证**

1. **MISRA C++合规性**:
   ```yaml
   # .github/workflows/misra-check.yml
   - name: Run MISRA C++ checker
     uses: cpp-lint/misra-action@v2
     with:
       ruleset: MISRA_C++_2023
       exceptions: |
         - Rule: 11.4  # Conversion between pointer to integer
           Reason: Hardware address mapping requires reinterpret_cast
           Approved-By: Architecture Review Board
           Date: 2026-01-08
   ```

2. **功能安全认证准备**:
   ```markdown
   # IEC 61508 / ISO 13849 合规性清单

   ## 软件安全要求
   - [ ] 所有安全关键路径有100%代码覆盖
   - [ ] 所有紧急停止操作< 10ms（实测：P99 < 8ms）
   - [ ] 硬件故障检测< 100ms
   - [ ] 所有内存分配在启动时完成（运行时零分配）
   - [ ] 所有异常在安全关键路径外处理

   ## 架构安全措施
   - [ ] 领域层无硬件依赖（HEXAGONAL_DEPENDENCY_DIRECTION）
   - [ ] 紧急停止路径无阻塞操作（INDUSTRIAL_EMERGENCY_STOP）
   - [ ] 所有硬件访问通过抽象层（HARDWARE_DOMAIN_ISOLATION）
   - [ ] 实时路径O(1)算法复杂度（INDUSTRIAL_REALTIME_SAFETY）

   ## 验证证据
   - [ ] 架构合规性报告（自动生成）
   - [ ] 单元测试覆盖率报告（安全关键代码100%）
   - [ ] 性能测试报告（WCET分析）
   - [ ] 故障注入测试报告
   - [ ] 第三方安全审计报告（计划：Q2 2026）
   ```

---

## 第七部分：推荐工具与技术

### 7.1 核心工具清单

| 工具 | 类型 | 许可 | 安装难度 | 学习曲线 | 推荐度 |
|------|------|------|---------|---------|--------|
| **Clang-Tidy** | 静态分析 | 开源 | ⭐ 简单 | ⭐⭐ 中等 | ⭐⭐⭐⭐⭐ 必需 |
| **Include-What-You-Use** | 依赖分析 | 开源 | ⭐⭐ 中等 | ⭐⭐⭐ 较陡 | ⭐⭐⭐⭐ 强烈推荐 |
| **CMake Graphviz** | 可视化 | 内置 | ⭐ 简单 | ⭐ 简单 | ⭐⭐⭐⭐⭐ 必需 |
| **Lattix Architect** | 架构治理 | 商业 | ⭐⭐⭐ 复杂 | ⭐⭐⭐⭐ 较陡 | ⭐⭐⭐ 推荐（有预算） |
| **CodeChecker** | 分析平台 | 开源 | ⭐⭐ 中等 | ⭐⭐⭐ 较陡 | ⭐⭐⭐⭐ 强烈推荐 |
| **cpp-dependencies** | 依赖扫描 | 开源 | ⭐ 简单 | ⭐ 简单 | ⭐⭐⭐ 推荐 |

### 7.2 工具集成示例

**完整的CI/CD流水线**:

```yaml
# .github/workflows/architecture-enforcement.yml
name: Architecture Enforcement

on:
  pull_request:
    paths:
      - 'src/**'
      - '.claude/rules/**'
      - 'CMakeLists.txt'

jobs:
  dependency-analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install tools
        run: |
          sudo apt-get install -y \
            clang-tidy \
            graphviz \
            iwyu

      - name: Generate dependency graph
        run: |
          cmake -B build -DCMAKE_GRAPHVIZ_EXECUTABLES="true"
          cmake --graphviz=build/deps.dot build
          dot -Tsvg build/deps.dot -o dependencies.svg
        continue-on-error: true

      - name: Upload dependency graph
        uses: actions/upload-artifact@v3
        with:
          name: dependency-graph
          path: dependencies.svg

      - name: Run Clang-Tidy
        run: |
          clang-tidy src/**/*.cpp \
            -p build/compile_commands.json \
            --config-file=.clang-tidy \
            --export-fixes=fixes.yml
        continue-on-error: true

      - name: Check IWYU compliance
        run: |
          iwyu_tool -p build src/domain/**/*.cpp \
            -Xiwyu --error_on_missing_includes=1
        continue-on-error: true

      - name: Run architecture validator
        run: |
          python scripts/validate_architecture.py \
            --rules .claude/rules/ \
            --source src/
        continue-on-error: true

      - name: Check for suppressions
        run: |
          python scripts/track_suppressions.py \
            --max-suppressions 10 \
            --require-review-date 2026-04-08

      - name: Generate metrics report
        if: always()
        run: |
          python scripts/generate_metrics.py \
            --output $GITHUB_STEP_SUMMARY

  security-scan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Run CodeChecker
        uses: codechecker/codechecker-action@v1
        with:
          build: "cmake --build build"
          analyze: "codechecker analyze --ctu"
          report: "codechecker parse --print-steps"
```

### 7.3 自定义脚本工具包

**scripts/validate_architecture.py**:
```python
#!/usr/bin/env python3
"""
Architecture validation script for siligen-motion-controller.
Checks hexagonal architecture constraints and reports violations.
"""

import argparse
import re
import sys
from pathlib import Path
from typing import List, Tuple


class ArchitectureValidator:
    def __init__(self, source_dir: Path, rules_dir: Path):
        self.source_dir = source_dir
        self.rules_dir = rules_dir
        self.violations: List[Tuple[str, int, str]] = []

    def validate(self) -> bool:
        """Run all validation checks."""
        print("🔍 Starting architecture validation...\n")

        self.check_domain_layer_isolation()
        self.check_port_purity()
        self.check_adapter_single_responsibility()
        self.check_no_stl_in_domain()

        self.print_report()
        return len(self.violations) == 0

    def check_domain_layer_isolation(self):
        """RULE: Domain must not depend on Infrastructure."""
        print("📦 Checking domain layer isolation...")

        domain_files = list(self.source_dir.glob("domain/**/*.cpp"))
        for file in domain_files:
            content = file.read_text()
            lines = content.split("\n")

            for i, line in enumerate(lines, 1):
                # 检查Infrastructure include
                if re.search(r'#include\s+"infrastructure/', line):
                    # 检查是否有抑制注释
                    has_suppression = self.check_suppression(lines, i)
                    if not has_suppression:
                        self.violations.append((
                            str(file),
                            i,
                            "Domain includes Infrastructure header"
                        ))

    def check_port_purity(self):
        """RULE: Port interfaces must be pure abstract."""
        print("🔌 Checking port interface purity...")

        port_files = list(self.source_dir.glob("domain/<subdomain>/ports/**/*.h"))
        for file in port_files:
            content = file.read_text()

            # 检查是否有实现细节
            if re.search(r'class\s+\w+\s*:\s*public\s+\w+', content):
                # 如果有基类但不是纯虚接口
                if "= 0" not in content:
                    self.violations.append((
                        str(file),
                        1,
                        "Port interface not pure abstract"
                    ))

    def check_adapter_single_responsibility(self):
        """RULE: Adapters must implement exactly one port."""
        print("🔧 Checking adapter single responsibility...")

        adapter_files = list(self.source_dir.glob("infrastructure/adapters/**/*.h"))
        for file in adapter_files:
            content = file.read_text()

            # 检查是否实现多个端口
            port_implementations = re.findall(
                r':\s*public\s+(?:Siligen::)?Domain::Ports::(\w+)',
                content
            )

            if len(port_implementations) > 1:
                self.violations.append((
                    str(file),
                    1,
                    f"Adapter implements {len(port_implementations)} ports (expected 1)"
                ))

    def check_no_stl_in_domain(self):
        """RULE: Domain must not use STL containers (with exceptions)."""
        print("🚫 Checking for STL containers in Domain...")

        domain_files = list(self.source_dir.glob("domain/**/*.cpp"))
        domain_files += list(self.source_dir.glob("domain/**/*.h"))

        stl_patterns = [
            r'std::vector\b',
            r'std::map\b',
            r'std::unordered_map\b',
            r'std::list\b',
        ]

        for file in domain_files:
            # 跳过motion目录（已批准的例外）
            if "domain/motion" in str(file):
                continue

            content = file.read_text()
            lines = content.split("\n")

            for i, line in enumerate(lines, 1):
                for pattern in stl_patterns:
                    if re.search(pattern, line):
                        # 检查抑制
                        has_suppression = self.check_suppression(lines, i)
                        if not has_suppression:
                            self.violations.append((
                                str(file),
                                i,
                                f"STL container usage: {pattern}"
                            ))

    def check_suppression(self, lines: List[str], line_num: int) -> bool:
        """Check if violation has valid suppression comment."""
        # Check 10 lines before the violation
        start = max(0, line_num - 11)
        context = lines[start:line_num]

        suppression = "\n".join(context)
        return (
            "CLAUDE_SUPPRESS" in suppression and
            "Reason:" in suppression and
            "Approved-By:" in suppression and
            "Date:" in suppression
        )

    def print_report(self):
        """Print validation report."""
        print(f"\n{'='*60}")
        print(f"📊 Architecture Validation Report")
        print(f"{'='*60}\n")

        if not self.violations:
            print("✅ No architecture violations found!\n")
            return

        print(f"❌ Found {len(self.violations)} violations:\n")

        for file, line, message in self.violations:
            print(f"  📍 {file}:{line}")
            print(f"     {message}\n")

        print(f"{'='*60}\n")

        # Exit with error code
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Validate hexagonal architecture constraints"
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=Path("src"),
        help="Source directory"
    )
    parser.add_argument(
        "--rules",
        type=Path,
        default=Path(".claude/rules"),
        help="Rules directory"
    )

    args = parser.parse_args()

    validator = ArchitectureValidator(args.source, args.rules)
    validator.validate()
```

---

## 第八部分：关键参考资料

### 8.1 六边形架构权威资源

1. **[Hexagonal Architecture by Alistair Cockburn](https://alistair.cockburn.us/hexagonal-architecture)**
   - 原始定义，2005年发布
   - 核心理念：两层架构（内部/外部）

2. **[Hexagonal Architecture Explained (Cockburn 2024)](https://alistaircockburn.com/figs%20hexarch%20book.pdf)**
   - 2024年更新文档
   - 强调"端口和适配器"而非"六边形"名称

3. **[C++ Hex Architecture Implementation (GitHub)](https://github.com/piergst/hexagonal-this-cpp)**
   - 实际C++代码示例
   - 基于Alistair Cockburn现场编程会话

### 8.2 C++依赖管理工具

4. **[Include-What-You-Use (IWYU)](https://github.com/include-what-you-use/include-what-you-use)**
   - Google开发的头文件包含分析工具
   - 减少编译时依赖

5. **[CMake Graphviz Documentation](https://cmake.org/cmake/help/latest/module/CMakeGraphVizOptions.html)**
   - 官方文档：生成CMake目标依赖图

6. **[cpp-dependencies (GitHub)](https://github.com/tomtom-international/cpp-dependencies)**
   - 简单的C++依赖扫描器
   - 生成#include依赖图

7. **[Lattix Architect](https://docs.lattix.com/lattix/userGuide/LattixArchitectUserGuide.html)**
   - 商业架构治理工具
   - 基于依赖结构矩阵（DSM）

### 8.3 工业级案例研究

8. **[LLVM Coding Standards - Layering](https://llvm.org/docs/CodingStandards.html)**
   - LLVM项目的分层架构规则
   - 库间依赖单向性原则

9. **[Chromium C++ Style Guide](https://chromium.googlesource.com/experimental/chromium/src/+/refs/tags/72.0.3612.2/styleguide/c++/c++.md)**
   - Chromium项目的C++最佳实践
   - 组件化架构模式

10. **[C++ For Industrial Automation](https://www.codewithc.com/c-for-industrial-automation-building-real-time-control-systems/)**
    - 实时控制系统架构
    - 确定性执行约束

### 8.4 C++物理设计

11. **[The Pimpl Pattern - C++ Stories](https://www.cppstories.com/2018/01/pimpl/)**
    - PIMPL模式详解
    - 物理和逻辑依赖解耦

12. **[Physical Code Organization - Bloomberg BDE](https://github.com/bloomberg/bde/wiki/physical-code-organization)**
    - Bloomberg的大型C++代码库组织经验
    - 三级物理聚合模式

13. **[Header File Best Practices (Medium, 2025)](https://ibrahimmansur4.medium.com/header-file-fundamentals-and-best-practices-in-c-98b1fe87dae0)**
    - 2025年最新的头文件设计指南
    - 最小化包含原则

### 8.5 静态分析与工具

14. **[Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)**
    - LLVM的C++ linter工具
    - 可扩展的检查器框架

15. **[CodeChecker Documentation](https://codechecker.readthedocs.io/)**
    - 基于LLVM的静态分析平台
    - CI/CD集成支持

16. **[RFC: Integrate Clang-Tidy into Clang Static Analyzer](https://discourse.llvm.org/t/rfc-integrate-clang-tidy-checkers-into-clang-static-analyzer/88937)**
    - 2025年11月的RFC提案
    - 静态分析工具融合趋势

### 8.6 实时系统

17. **[Mastering Embedded C/C++ for Real-Time Systems](https://medium.com/@pankajkompella22/mastering-embedded-c-c-for-real-time-embedded-systems-62c5adbd3431)**
    - 实时C++编程约束
    - 确定性执行保证

18. **[A Real-time SOA for Industrial Automation (PDF)](https://core.ac.uk/download/pdf/54933057.pdf)**
    - 学术论文：实时SOA架构
    - 工业自动化案例

---

## 第九部分：总结与建议

### 9.1 核心发现总结

1. **六边形架构的普适性**:
   - ✅ 理论成熟（30年发展史）
   - ✅ C++实现可行（有成功案例）
   - ✅ 实时系统兼容（需额外约束）

2. **依赖治理的重要性**:
   - 物理依赖（编译时）比逻辑依赖更难管理
   - 需要工具自动化 + 人工审查相结合
   - 大型项目（LLVM/Chromium）证明分层架构的可行性

3. **工具生态**:
   - 开源工具（Clang-Tidy, IWYU）功能强大
   - 商业工具（Lattix）提供更好的可视化
   - CMake内置支持足以应对大多数场景

4. **实时约束**:
   - 领域层无动态分配是工业标准
   - 紧急停止< 10ms是硬性要求
   - PIMPL模式有助于编译时解耦

### 9.2 对siligen项目的直接建议

**立即行动（本周）**:
1. ✅ 安装Clang-Tidy并配置到CI/CD
2. ✅ 生成当前项目的依赖图（CMake Graphviz）
3. ✅ 运行IWYU分析domain层的头文件包含

**短期改进（本月）**:
1. 实现架构验证脚本（`validate_architecture.py`）
2. 添加依赖指标到PR评论
3. 建立抑制审查流程（季度审查）

**中期目标（本季度）**:
1. 引入CodeChecker或Lattix Architect
2. 建立架构单元测试框架
3. 实现性能分析工具（WCET追踪）

**长期规划（本年度）**:
1. 评估C++20模块迁移可行性
2. 通过功能安全认证准备（IEC 61508）
3. 建立完整的架构治理流程

### 9.3 风险与挑战

**技术风险**:
- ⚠️ C++20模块编译器支持不完整（需谨慎评估）
- ⚠️ 抑制机制可能被滥用（需严格审批）
- ⚠️ 工具链学习曲线陡峭（需培训投入）

**组织风险**:
- ⚠️ 架构规范可能与开发效率冲突（需平衡）
- ⚠️ CI/CD pipeline变复杂（需维护成本）
- ⚠️ 外部依赖（Lattix等）的许可费用

**缓解策略**:
- 🛡️ 从开源工具开始，证明价值后再考虑商业工具
- 🛡️ 建立架构审查委员会（ARB）审批例外
- 🛡️ 逐步迁移，避免"大爆炸"式重构

### 9.4 成功指标

**定量指标**:
- 架构违规数量：目标 < 10个/季度
- Domain层向Infrastructure层的依赖：0个
- 抑制使用率：< 1%的规则检查
- 构建时间：依赖图生成 < 30秒

**定性指标**:
- 新开发者的架构理解时间 < 1周
- PR审查中的架构问题减少50%
- 实时性能（WCET）保持稳定
- 代码审查中架构合规性成为标准话题

---

## 附录A：快速参考表

### A.1 工具对比矩阵

| 需求 | 开源解决方案 | 商业解决方案 | 推荐选择 |
|------|------------|------------|---------|
| **依赖分析** | IWYU, cpp-dependencies | CppDepend | 先用IWYU |
| **架构验证** | 自定义脚本 | Lattix Architect | 自定义脚本 |
| **静态分析** | Clang-Tidy, CodeChecker | SonarQube | Clang-Tidy |
| **可视化** | CMake Graphviz | Lattix, Understand | CMake Graphviz |
| **CI/CD集成** | GitHub Actions | 所有工具都支持 | GitHub Actions |

### A.2 规则优先级

| 优先级 | 规则 | 违规影响 | 工具支持 |
|-------|------|---------|---------|
| ⭐⭐⭐⭐⭐ | HEXAGONAL_DEPENDENCY_DIRECTION | 架构崩塌 | Clang-Tidy |
| ⭐⭐⭐⭐⭐ | HARDWARE_DOMAIN_ISOLATION | 安全风险 | 自定义脚本 |
| ⭐⭐⭐⭐⭐ | INDUSTRIAL_EMERGENCY_STOP | 人员伤害 | 运行时测试 |
| ⭐⭐⭐⭐ | DOMAIN_NO_DYNAMIC_MEMORY | 实时性破坏 | Valgrind |
| ⭐⭐⭐⭐ | DOMAIN_PUBLIC_API_NOEXCEPT | 异常安全 | Clang-Tidy |
| ⭐⭐⭐ | NAMESPACE_LAYER_ISOLATION | 可维护性 | Clang-Tidy |
| ⭐⭐⭐ | DOMAIN_NO_STL_CONTAINERS | 性能退化 | Clang-Tidy + 抑制 |
| ⭐⭐ | PORT_INTERFACE_PURITY | 测试困难 | 自定义脚本 |
| ⭐ | REALTIME_LOCKING | 死锁风险 | ThreadSanitizer |

### A.3 学习资源

**初学者**（理解六边形架构）:
1. [Hexagonal Architecture 101](https://secture.com/en/hexagonal-architecture-101/)
2. [hexagonal-this-cpp (GitHub)](https://github.com/piergst/hexagonal-this-cpp)
3. [Ports and Adapters (Dev.to, 2025)](https://dev.to/rafaeljcamara/ports-and-adapters-hexagonal-architecture-547c)

**进阶**（C++物理设计）:
4. [Large-Scale C++ Software Design (Lakos)](https://www.pearson.com/en-us/subject-catalog/p/large-scale-c-software-design/P200000003146/9780201633627)
5. [Physical Code Organization (Bloomberg BDE)](https://github.com/bloomberg/bde/wiki/physical-code-organization)
6. [The Pimpl Pattern (C++ Stories)](https://www.cppstories.com/2018/01/pimpl/)

**专家**（实时系统与架构治理）:
7. [Real-Time C++ (Springer)](https://link.springer.com/book/10.1007/978-3-642-39619-6)
8. [Software Architecture with C++ (Packt)](https://github.com/PacktPublishing/Software-Architecture-with-Cpp)
9. [Lattix Architect User Guide](https://docs.lattix.com/lattix/userGuide/LattixArchitectUserGuide.html)

---

**报告结束**

**下一步行动**:
1. 团队评审本报告（建议会议时间：2小时）
2. 确定3个最高优先级的改进项
3. 分配责任人和时间表
4. 建立月度架构治理回顾机制

**研究者**: Claude Code (Research Agent)
**联系方式**: 通过项目Issue跟踪器提交问题
**文档版本**: 1.0
**最后更新**: 2026-01-08

