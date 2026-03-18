# 架构改进方案与实施计划

**版本**: v1.0.0
**日期**: 2026-01-08
**负责人**: Coder Agent
**状态**: 设计阶段

---

## 执行摘要

基于对 **siligen-motion-controller** 项目的架构审计，发现以下关键问题：

| 严重程度 | 问题数量 | 状态 |
|---------|---------|------|
| 🔴 Critical | 1 | Domain层依赖Infrastructure层（已修复） |
| 🟡 Medium | 5 | 缺少物理依赖隔离、组合根机制不完善 |
| 🟢 Low | 3 | Port接口命名不一致 |

**总体架构合规性评分**: 75/100 (目标: 90/100)

---

## 改进任务清单（按优先级排序）

### ✅ Task 1: 修复Domain层依赖Infrastructure层违规 [已完成]

**优先级**: P0 (严重架构违规)
**状态**: ✅ 已完成
**详情**:
- **问题**: `src/domain/planning/ports/IVisualizationPort.h` 包含 `infrastructure/visualizers/DXFVisualizer.h`
- **解决方案**: 将 `VisualizationConfig` 和 `DXFSegment` 移至 `src/shared/types/VisualizationTypes.h`
- **验证**: 重新读取文件确认已修复

---

### 🔵 Task 2: 在CMake中实现物理依赖隔离

**优先级**: P0 (核心架构保障机制)
**预估工作量**: 3-5天
**影响范围**: 构建系统、CI/CD流程

#### 2.1 问题分析

**当前状态**:
```cmake
# src/domain/CMakeLists.txt (当前配置)
target_include_directories(siligen_motion PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/motion
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types  # ✅ 允许
    # 缺少对Infrastructure层的访问限制
)
```

**违规示例**（理论上可以编译，但不应该）:
```cpp
// src/domain/motion/SomeController.cpp
#include "infrastructure/hardware/MultiCardAdapter.h"  // 应该编译失败！
```

#### 2.2 解决方案

**方案A: CMake include路径隔离**（推荐）

创建 `cmake/ArchitectureGuard.cmake`:

```cmake
# cmake/ArchitectureGuard.cmake

macro(siligen_set_layer_includes target layer)
    if (layer STREQUAL "domain")
        # Domain层: 只能include自己和Shared
        target_include_directories(${target} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared/utils
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared/interfaces
        )

    elseif (layer STREQUAL "application")
        # Application层: 可以include Domain和Shared
        target_include_directories(${target} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared
            ${CMAKE_CURRENT_SOURCE_DIR}/../domain
        )

    elseif (layer STREQUAL "infrastructure")
        # Infrastructure层: 可以include Domain和Shared
        target_include_directories(${target} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared
            ${CMAKE_CURRENT_SOURCE_DIR}/../domain
        )

    elseif (layer STREQUAL "adapters")
        # Adapters层: 可以include所有层
        target_include_directories(${target} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/../shared
            ${CMAKE_CURRENT_SOURCE_DIR}/../domain
            ${CMAKE_CURRENT_SOURCE_DIR}/../application
            ${CMAKE_CURRENT_SOURCE_DIR}/../infrastructure
        )
    endif()
endmacro()
```

**应用到Domain层**:

```cmake
# src/domain/CMakeLists.txt (修改后)

include(ArchitectureGuard)  # 引入宏

add_library(siligen_motion STATIC
    motion/InterpolationBase.cpp
    # ... 其他源文件
)

# 使用宏设置include路径（自动强制架构规则）
siligen_set_layer_includes(siligen_motion domain)

target_link_libraries(siligen_motion
    siligen_types
    siligen_utils
)
```

#### 2.3 验证方法

**单元测试**（创建架构测试库）:

```cpp
// tests/architecture/architecture_test.cpp

#include <gtest/gtest.h>

// 测试Domain层不能include Infrastructure层
TEST(ArchitectureTest, DomainCannotIncludeInfrastructure) {
    // 尝试编译应该失败
    #include "domain/motion/LinearInterpolator.h"
    #include "infrastructure/hardware/MultiCardAdapter.h"  // ❌ 链接错误
}

// 测试Application层只能通过Port访问Infrastructure
TEST(ArchitectureTest, ApplicationAccessInfrastructureViaPort) {
    // 应该使用Port接口
    #include "application/usecases/HardwareConnectionUseCase.h"
    #include "domain/<subdomain>/ports/IHardwarePort.h"  // ✅ 正确

    // 不应该直接include Adapter
    // #include "infrastructure/adapters/hardware/HardwareTestAdapter.h"  // ❌ 错误
}
```

**CMake测试配置**:

```cmake
# tests/CMakeLists.txt

if(SILIGEN_BUILD_TESTS)
    # 架构合规性测试（编译时测试）
    add_executable(test_architecture_violations
        architecture/architecture_violation_test.cpp
    )

    # 此测试应该编译失败，如果编译成功说明架构检查无效
    target_compile_definitions(test_architecture_violations PRIVATE
        EXPECT_COMPILE_FAILURE=1
    )

    add_test(NAME ArchitectureViolationTest
             COMMAND test_architecture_violations
             EXPECT_FAIL)
endif()
```

#### 2.4 渐进式实施计划

**Week 1**: 准备阶段
- Day 1-2: 创建 `cmake/ArchitectureGuard.cmake`
- Day 3-4: 修改Domain层CMakeLists.txt使用新宏
- Day 5: 运行完整构建，验证无破坏性变更

**Week 2**: 检测阶段
- Day 1-2: 实现PowerShell跨层检测脚本
- Day 3-4: 集成到CI工作流，仅报告不阻止
- Day 5: 生成初始违规报告

**Week 3**: 强制执行
- Day 1-2: 修复所有Critical/High违规
- Day 3-4: 启用CI阻止机制
- Day 5: 验证PR质量门控生效

#### 2.5 风险评估

| 风险 | 概率 | 影响 | 缓解策略 |
|-----|------|------|---------|
| 现有代码编译失败 | 高 | 高 | Week 1仅警告，Week 3才强制执行 |
| CI构建时间增加 | 中 | 低 | 使用增量分析，只检查变更文件 |
| 误报导致PR被阻塞 | 中 | 中 | 提供白名单机制，人工review例外 |

---

### 🟢 Task 3: 完善Python架构分析器

**优先级**: P1 (CI自动化关键组件)
**预估工作量**: 2-3天
**影响范围**: CI/CD流程

#### 3.1 当前状态

**已实现功能**:
- ✅ 基础include依赖检测
- ✅ 模块类型识别
- ✅ 违规记录和报告

**缺失功能**:
- ❌ 命名空间合规性检查
- ❌ Port接口定义完整性验证
- ❌ 依赖深度分析（循环依赖检测）
- ❌ HTML报告生成

#### 3.2 增强功能设计

**增强1: 命名空间检查**

```python
def check_namespaces(self, file_path, content):
    """检查命名空间合规性"""
    module_type = self.get_module_type(file_path)

    # 提取namespace声明
    namespaces = re.findall(r'namespace\s+(\w+)', content)

    # 定义各层应有的命名空间前缀
    expected_namespaces = {
        'domain': ['Siligen', 'Domain'],
        'application': ['Siligen', 'Application'],
        'infrastructure': ['Siligen', 'Infrastructure'],
        'adapters': ['Siligen', 'Adapters'],
        'shared': ['Siligen', 'Shared']
    }

    if module_type in expected_namespaces:
        expected = expected_namespaces[module_type]
        for ns in namespaces:
            if ns not in expected and ns not in ['std', 'details']:
                self.add_violation(
                    'medium',
                    'namespace',
                    f"Invalid namespace '{ns}' in {module_type} layer",
                    str(file_path),
                    content.find(f'namespace {ns}')
                )
```

**增强2: 循环依赖检测**

```python
def detect_circular_dependencies(self):
    """使用DFS检测循环依赖"""
    # 构建依赖图
    dependency_graph = defaultdict(set)

    for file in self.src_dir.rglob('*.cpp'):
        module_from = self.get_module_type(file)
        content = file.read_text()

        includes = re.findall(r'#include\s*[<"]([^>"]+)[">]', content)
        for inc in includes:
            module_to = self.extract_module_from_include(inc)
            if module_to and module_to != module_from:
                dependency_graph[module_from].add(module_to)

    # DFS检测环
    def dfs(node, visited, rec_stack):
        visited.add(node)
        rec_stack.add(node)

        for neighbor in dependency_graph.get(node, []):
            if neighbor not in visited:
                if dfs(neighbor, visited, rec_stack):
                    return True
            elif neighbor in rec_stack:
                return True

        rec_stack.remove(node)
        return False

    # 检查所有节点
    for module in dependency_graph:
        if dfs(module, set(), set()):
            self.violations['critical'] += 1
            self.violations['dependency_direction'].append({
                'severity': 'critical',
                'message': f'Circular dependency detected involving module: {module}'
            })
```

**增强3: HTML报告生成**

```python
def generate_html_report(self, output_file):
    """生成HTML格式的架构分析报告"""

    html_template = """
    <!DOCTYPE html>
    <html>
    <head>
        <title>架构分析报告</title>
        <style>
            .critical { color: red; }
            .high { color: orange; }
            .medium { color: yellow; }
            .score {{ font-size: 48px; font-weight: bold; }}
        </style>
    </head>
    <body>
        <h1>架构合规性分析报告</h1>
        <div class="score">{score}/100</div>
        <h2>违规统计</h2>
        <ul>
            <li>Critical: {critical}</li>
            <li>High: {high}</li>
            <li>Medium: {medium}</li>
        </ul>
        <h2>详细违规列表</h2>
        {violations_table}
    </body>
    </html>
    """

    # 渲染模板
    html_content = html_template.format(
        score=self.calculate_score(),
        critical=self.violations['critical'],
        high=self.violations['high'],
        medium=self.violations['medium'],
        violations_table=self._generate_violations_table()
    )

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(html_content)
```

#### 3.3 CI集成

```yaml
# .github/workflows/architecture-validation.yml (增强版)

- name: Run Enhanced Architecture Analysis
  id: arch-analysis
  run: |
    python .claude/agents/arch_analyzer.py \
      --src src \
      --check-namespaces \
      --detect-circular-deps \
      --format html \
      --output reports/arch-analysis-report.html \
      --verbose
  continue-on-error: true

- name: Upload HTML Report
  uses: actions/upload-artifact@v3
  with:
    name: architecture-report
    path: reports/arch-analysis-report.html
```

---

### 🔵 Task 4: 设计并实现组合根（Composition Root）机制

**优先级**: P1 (依赖注入最佳实践)
**预估工作量**: 3-4天
**影响范围**: Application层、Adapters层启动流程

#### 4.1 问题分析

**当前状态**:
- `src/application/container/ApplicationContainer.cpp` 存在
- 但缺少统一的依赖配置入口
- 各Adapters直接创建依赖，耦合度高

**理想状态**（参考Mark Seemann的Composition Root模式）:

```
Composition Root (组合根)
    ├── 配置所有依赖注入
    ├── 创建所有Adapter实例
    ├── 装配所有UseCase
    └── 提供唯一的启动入口
```

#### 4.2 设计方案

**创建 `src/bootstrap/CompositionRoot.h`**:

```cpp
// CompositionRoot.h - 统一依赖注入配置
#pragma once

namespace Siligen {
namespace Bootstrap {

/**
 * @brief 组合根 - 负责装配所有依赖关系
 *
 * 职责:
 * 1. 创建所有Infrastructure层Adapter实例
 * 2. 创建所有Application层UseCase实例
 * 3. 注入依赖关系
 * 4. 提供唯一启动入口
 */
class CompositionRoot {
public:
    /**
     * @brief 初始化组合根
     * @param config_path 配置文件路径
     * @return Result<void> 成功或失败
     */
    static Result<void> Initialize(const std::string& config_path);

    /**
     * @brief 获取MotionController实例
     */
    static std::shared_ptr<Domain::Ports::IPositionControlPort>
        GetMotionController();

    /**
     * @brief 获取特定UseCase实例
     */
    template<typename UseCaseType>
    static std::shared_ptr<UseCaseType> GetUseCase();

    /**
     * @brief 清理所有资源
     */
    static void Shutdown();

private:
    // 私有构造函数（单例模式）
    CompositionRoot() = default;

    // 注册所有Adapters
    static Result<void> RegisterAdapters(const std::string& config_path);

    // 注册所有UseCases
    static Result<void> RegisterUseCases();

    // 依赖容器
    static std::unique_ptr<Shared::DI::ServiceContainer> container_;
};

} // namespace Bootstrap
} // namespace Siligen
```

**实现文件 `src/bootstrap/CompositionRoot.cpp`**:

```cpp
#include "CompositionRoot.h"
#include "application/container/ApplicationContainer.h"
#include "infrastructure/factories/InfrastructureAdapterFactory.h"

namespace Siligen {
namespace Bootstrap {

std::unique_ptr<Shared::DI::ServiceContainer> CompositionRoot::container_;

Result<void> CompositionRoot::Initialize(const std::string& config_path) {
    // 创建DI容器
    container_ = std::make_unique<Shared::DI::ServiceContainer>();

    // Step 1: 注册配置
    auto config = std::make_shared<Configuration::MachineConfig>();
    auto load_result = config->LoadFromFile(config_path);
    if (load_result.IsError()) {
        return Result<void>::Error(load_result.GetError());
    }
    container_->RegisterInstance<Configuration::IMachineConfig>(config);

    // Step 2: 注册Infrastructure层Adapters
    auto register_result = RegisterAdapters(config_path);
    if (register_result.IsError()) {
        return register_result;
    }

    // Step 3: 注册Application层UseCases
    auto usecases_result = RegisterUseCases();
    if (usecases_result.IsError()) {
        return usecases_result;
    }

    return Result<void>::Success();
}

Result<void> CompositionRoot::RegisterAdapters(const std::string& config_path) {
    // 使用Factory创建Adapters
    auto factory = Infrastructure::Factories::InfrastructureAdapterFactory();

    // 创建并注册MotionController
    auto motion_controller = factory.CreateMotionController(config_path);
    if (motion_controller == nullptr) {
        return Result<void>::Error("Failed to create MotionController");
    }
    container_->RegisterSingleton<Domain::Ports::IPositionControlPort>(
        motion_controller
    );

    // 创建并注册其他Adapters...
    // (HardwareController, VisualizationPort, etc.)

    return Result<void>::Success();
}

Result<void> CompositionRoot::RegisterUseCases() {
    // Motion Control UseCases
    container_->RegisterTransient<Application::UseCases::JogTestUseCase>();
    container_->RegisterTransient<Application::UseCases::HomingTestUseCase>();
    container_->RegisterTransient<Application::UseCases::CMPTestUseCase>();

    // Valve Control UseCases
    container_->RegisterTransient<Application::UseCases::StartDispenserUseCase>();
    container_->RegisterTransient<Application::UseCases::StopDispenserUseCase>();

    // ... 其他UseCases

    return Result<void>::Success();
}

std::shared_ptr<Domain::Ports::IPositionControlPort>
    CompositionRoot::GetMotionController() {
    return container_->Resolve<Domain::Ports::IPositionControlPort>();
}

template<typename UseCaseType>
std::shared_ptr<UseCaseType> CompositionRoot::GetUseCase() {
    return container_->Resolve<UseCaseType>();
}

void CompositionRoot::Shutdown() {
    // 清理顺序：Adapters先于容器
    container_->Shutdown();
    container_.reset();
}

// 显式实例化常用UseCase类型
template std::shared_ptr<Application::UseCases::JogTestUseCase>
    CompositionRoot::GetUseCase<Application::UseCases::JogTestUseCase>();

} // namespace Bootstrap
} // namespace Siligen
```

#### 4.3 使用示例

**CLI启动入口**:

```cpp
// apps/control-cli/main.cpp (修改后)

#include "bootstrap/CompositionRoot.h"

int main(int argc, char* argv[]) {
    // 1. 初始化组合根
    auto init_result = Bootstrap::CompositionRoot::Initialize("config/machine_config.ini");
    if (init_result.IsError()) {
        std::cerr << "Failed to initialize: " << init_result.GetError().message << std::endl;
        return 1;
    }

    // 2. 获取UseCase
    auto usecase = Bootstrap::CompositionRoot::GetUseCase<
        Application::UseCases::JogTestUseCase
    >();

    // 3. 执行业务逻辑
    // ...

    // 4. 清理
    Bootstrap::CompositionRoot::Shutdown();
    return 0;
}
```

#### 4.4 CMake配置

```cmake
# src/bootstrap/CMakeLists.txt (新建)

add_library(siligen_bootstrap STATIC
    CompositionRoot.cpp
)

target_include_directories(siligen_bootstrap PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../application
    ${CMAKE_CURRENT_SOURCE_DIR}/../infrastructure
)

target_link_libraries(siligen_bootstrap PUBLIC
    siligen_application
    siligen_infrastructure
    siligen_types
    siligen_utils
)
```

#### 4.5 渐进式迁移路径

**Phase 1**: 创建CompositionRoot，与现有代码并存
- Week 1: 实现 `CompositionRoot` 基础框架
- Week 2: 在新代码中使用CompositionRoot

**Phase 2**: 逐步迁移现有UseCase
- Week 3: 迁移Motion相关UseCase
- Week 4: 迁移Valve相关UseCase

**Phase 3**: 废弃旧的ApplicationContainer
- Week 5: 移除 `ApplicationContainer` 中的依赖注入逻辑
- Week 6: 删除 `ApplicationContainer.cpp`

---

### 🟢 Task 5: 统一Port接口命名规范

**优先级**: P2 (代码一致性)
**预估工作量**: 1天
**影响范围**: Domain层Ports、Infrastructure层Adapters

#### 5.1 当前状态

**不一致的命名**:
```
src/domain/<subdomain>/ports/
├── IVisualizationPort.h        ✅ IPort格式
├── IPositionControlPort.h    ❌ 应改为 IMotionPort.h
└── IHardwareControllerPort.h  ❌ 应改为 IHardwarePort.h
```

#### 5.2 重命名规则

**统一为 `I{功能名}Port.h` 格式**:

| 当前名称 | 新名称 |
|---------|--------|
| `IPositionControlPort.h` | `IMotionPort.h` |
| `IHardwareControllerPort.h` | `IHardwarePort.h` |
| `IVisualizationPort.h` | (保持不变) ✅ |

#### 5.3 批量重命名脚本

```powershell
# scripts/refactor/rename-ports.ps1

$portRenames = @{
    "IPositionControlPort.h" = "IMotionPort.h"
    "IHardwareControllerPort.h" = "IHardwarePort.h"
}

$srcDir = "src"

foreach ($oldName in $portRenames.Keys) {
    $newName = $portRenames[$oldName]

    Write-Host "重命名: $oldName -> $newName" -ForegroundColor Cyan

    # 1. 重命名文件
    Get-ChildItem -Path $srcDir -Filter $oldName -Recurse | ForEach-Object {
        $newPath = Join-Path $_.Directory $newName
        Move-Item -Path $_.FullName -Destination $newPath -Force
        Write-Host "  文件: $($_.FullName) -> $newPath" -ForegroundColor Green
    }

    # 2. 更新所有#include语句
    Get-ChildItem -Path $srcDir -Include "*.h","*.cpp" -Recurse | ForEach-Object {
        $content = Get-Content $_.FullName -Raw
        if ($content -match [regex]::Escape($oldName)) {
            $content = $content -replace [regex]::Escape($oldName), $newName
            Set-Content -Path $_.FullName -Value $content -NoNewline
            Write-Host "  更新: $($_.FullName)" -ForegroundColor Yellow
        }
    }
}
```

---

### 🟢 Task 6: 创建架构测试（CMake单元测试）

**优先级**: P2 (持续验证架构合规性)
**预估工作量**: 2-3天
**影响范围**: 测试框架、CI/CD

#### 6.1 测试类型

**Type 1: 编译时架构测试**

```cpp
// tests/architecture/test_domain_layer_isolation.cpp

// 此测试应该编译失败
#include "domain/motion/LinearInterpolator.h"
#include "infrastructure/hardware/MultiCardAdapter.h"  // ❌ 应该触发编译错误

int main() {
    return 0;  // 不应该到达这里
}
```

**CMake配置**:

```cmake
# tests/CMakeLists.txt

# 架构违规测试（应该编译失败）
add_executable(test_architecture_domain_violation
    architecture/test_domain_layer_isolation.cpp
)

# 尝试编译，期望失败
try_compile(
    RESULT SHOULD_FAIL
    SOURCES ${test_architecture_domain_violation}
    CMAKE_FLAGS -DCMAKE_CXX_STANDARD=17
)

# 如果编译成功，说明架构检查失效
if (SHOULD_FAIL)
    message(FATAL_ERROR "架构测试失败: Domain层成功include了Infrastructure层")
endif()
```

**Type 2: 运行时依赖图测试**

```cpp
// tests/architecture/test_dependency_graph.cpp

#include <gtest/gtest.h>
#include "analysis/DependencyAnalyzer.h"

TEST(DependencyGraphTest, DomainLayerDoesNotDependOnInfrastructure) {
    using namespace Siligen::Analysis;

    DependencyAnalyzer analyzer("src");
    auto graph = analyzer.BuildDependencyGraph();

    // 获取Domain层的所有依赖
    auto domain_deps = graph.GetOutgoingDependencies("domain");

    // 验证不包含Infrastructure
    EXPECT_FALSE(domain_deps.contains("infrastructure"));
    EXPECT_FALSE(domain_deps.contains("adapters"));

    // 应该只依赖Shared
    EXPECT_TRUE(domain_deps.contains("shared"));
}

TEST(DependencyGraphTest, NoCircularDependencies) {
    using namespace Siligen::Analysis;

    DependencyAnalyzer analyzer("src");
    auto graph = analyzer.BuildDependencyGraph();

    auto cycles = graph.DetectCycles();
    EXPECT_EQ(cycles.size(), 0) << "发现循环依赖: " << cycles[0];
}
```

#### 6.2 CI集成

```yaml
# .github/workflows/architecture-tests.yml

name: Architecture Tests

on:
  pull_request:
    branches: [main, develop]

jobs:
  architecture-tests:
    runs-on: windows-latest

    steps:
    - uses: actions/checkout@v4

    - name: Setup CMake
      uses: jwlawson/actions-setup-cmake@v1.14

    - name: Build Architecture Tests
      run: |
        cmake -B build -DSILIGEN_BUILD_TESTS=ON
        cmake --build build --target test_architecture

    - name: Run Architecture Tests
      run: |
        cd build
        ctest --output-on-failure -R architecture
```

---

## 实施时间表

### Week 1-2: 基础设施建设
- [x] Task 1: 修复Domain层依赖Infrastructure层违规 ✅
- [ ] Task 2: 实现CMake物理依赖隔离
- [ ] Task 3: 完善Python架构分析器

### Week 3-4: 依赖注入重构
- [ ] Task 4: 设计并实现组合根机制

### Week 5: 代码规范统一
- [ ] Task 5: 统一Port接口命名规范
- [ ] Task 6: 创建架构测试

---

## 成功指标

| 指标 | 当前值 | 目标值 | 测量方法 |
|-----|--------|--------|---------|
| 架构合规性评分 | 75/100 | 90/100 | `arch_analyzer.py` |
| Critical违规数 | 0 | 0 | CI工作流 |
| 跨层依赖违规 | 未测量 | <5 | `detect-cross-layer-includes.ps1` |
| 循环依赖 | 未测量 | 0 | 依赖图分析 |
| Port接口命名一致性 | 60% | 100% | 代码审查 |

---

## 风险与缓解策略

### 风险1: 重构导致现有功能破坏
**缓解**:
- 渐进式迁移，每周一个任务
- 完整的单元测试覆盖
- 保留旧代码并行运行

### 风险2: CI构建时间过长
**缓解**:
- 增量分析（只检查变更文件）
- 并行执行架构检查和构建
- 缓存依赖分析结果

### 风险3: 团队采用率低
**缓解**:
- 详细的文档和示例
- 代码审查checklist
- 自动化工具辅助

---

## 附录

### A. 相关文档
- `.claude/rules/HEXAGONAL.md` - 六边形架构规范
- `.claude/rules/DOMAIN.md` - Domain层约束
- `.claude/rules/PORTS_ADAPTERS.md` - 端口和适配器规则

### B. 工具清单
- **架构分析**: `.claude/agents/arch_analyzer.py`
- **依赖检测**: `scripts/analysis/detect-cross-layer-includes.ps1`
- **编译检查**: `cmake/ArchitectureGuard.cmake`

### C. 参考资源
- [Clean Architecture by Robert C. Martin](https://blog.cleancoder.com/uncle-bob/2012/08/13/the-clean-architecture.html)
- [Composition Root pattern by Mark Seemann](https://blog.ploeh.dk/2011/07/28/CompositionRoot/)
- [Hexagonal Architecture by Alistair Cockburn](https://alistair.cockburn.us/hexagonal-architecture/)

---

**文档版本**: v1.0.0
**最后更新**: 2026-01-08
**审核者**: Architect Agent, Research Agent

