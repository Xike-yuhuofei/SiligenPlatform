# LoopMaster插件全面自动化测试方案

## 一、项目背景

### 1.1 LoopMaster插件概述

LoopMaster是一个Claude Code插件，使用Stop Hook实现自动循环迭代任务执行。

**核心功能**：
- `/start-loop` - 启动循环任务
- `/cancel-loop` - 取消循环
- `/loop-status` - 查询状态

**工作原理**：
1. 创建状态文件 `.claude/loop-master.local.md`
2. Stop Hook在Claude退出时检查状态
3. 未达到迭代限制时自动继续
4. 达到限制或手动取消时停止

### 1.2 现有测试基础设施

**测试框架**：Google Test + Google Mock
**测试目录**：`tests/`
**测试工具**：
- TestFixture体系（MockServiceFixture, ServiceContainerTestFixture等）
- 丰富的断言宏（EXPECT_RESULT_SUCCESS等）
- Mock系统（MockHardwareController等）
- 集成测试框架（IntegrationTestFixture）

**架构验证**：
- PowerShell验证脚本（arch-validate.ps1）
- Arch-Guard插件系统
- 多层次防御机制

## 二、测试需求分析

### 2.1 需要测试的功能点

#### 命令测试
1. `/start-loop` 命令
   - 状态文件冲突检测
   - 用户输入验证
   - YAML格式正确性
   - 时间戳生成

2. `/cancel-loop` 命令
   - 无循环状态处理
   - 已停止状态处理
   - 运行时长计算
   - 状态更新

3. `/loop-status` 命令
   - 三种场景输出（运行中/已停止/空闲）
   - 进度计算
   - 时间计算

#### Hook脚本测试
1. 状态文件读取
   - 文件不存在处理
   - 文件不可读处理
   - 空文件处理
   - 格式损坏处理

2. YAML解析
   - Frontmatter提取
   - 字段解析（active, iteration, max_iterations等）
   - 默认值处理

3. 迭代控制
   - 激活状态检查
   - 迭代限制检查
   - 计数增加
   - 任务提示词提取

4. 决策输出
   - 允许退出的JSON格式
   - 阻止退出的JSON格式
   - 系统消息构建

#### 错误处理测试
1. 边界条件
   - iteration = max_iterations - 1（应继续）
   - iteration = max_iterations（应停止）
   - max_iterations = 0（无限循环）

2. 异常场景
   - 并发访问状态文件
   - sed命令失败
   - jq命令缺失
   - 磁盘空间不足

### 2.2 架构合规性测试

根据 `.claude/rules/` 的规则要求：

1. **六边形架构规则**
   - 插件不应破坏核心架构
   - 命令和Hook的依赖方向

2. **DOMAIN规则**
   - 插件代码不在domain层，不适用

3. **HARDWARE规则**
   - 插件不应直接访问硬件

4. **NAMESPACE规则**
   - 插件命名空间规范

5. **BUILD规则**
   - 测试构建使用compile agent

### 2.3 自动化要求

用户要求：**自动发现问题、解决问题、验证**

这意味着测试方案需要：
1. 自动检测LoopMaster的功能问题
2. 自动修复发现的问题
3. 自动验证修复效果

## 三、用户需求确认

✅ **已确认需求**：
- **测试平台**：仅Windows平台（Git Bash + WSL）
- **自动化程度**：完全自动化（检测→修复→验证→commit）
- **架构合规**：插件豁免六边形架构规则
- **测试覆盖率**：100%（包括边界、并发、性能、模糊测试）

## 四、详细实施方案

### 4.1 测试目录结构

```
tests/loop-master/
├── CMakeLists.txt                           # 测试构建配置
│
├── fixtures/                                # 测试夹具
│   ├── LoopMasterTestFixture.h             # 主测试夹具
│   ├── HookScriptTester.h                  # Hook脚本测试器
│   ├── StateFileSimulator.h                # 状态文件模拟器
│   └── ConcurrentAccessTester.h            # 并发测试工具
│
├── unit/                                    # 单元测试
│   ├── test_yaml_parser.cpp                # YAML解析测试
│   ├── test_iteration_logic.cpp            # 迭代逻辑测试
│   ├── test_hook_decision.cpp              # Hook决策测试
│   └── test_time_calculations.cpp          # 时间计算测试
│
├── integration/                             # 集成测试
│   ├── test_start_loop_command.cpp         # start-loop命令测试
│   ├── test_cancel_loop_command.cpp        # cancel-loop命令测试
│   ├── test_loop_status_command.cpp        # loop-status命令测试
│   ├── test_full_workflow.cpp              # 完整工作流测试
│   └── test_error_scenarios.cpp            # 错误场景测试
│
├── concurrent/                              # 并发测试
│   ├── test_concurrent_file_access.cpp     # 并发文件访问
│   └── test_race_conditions.cpp            # 竞态条件测试
│
├── performance/                             # 性能测试
│   ├── test_hook_execution_time.cpp        # Hook执行时间
│   └── test_large_file_handling.cpp        # 大文件处理
│
├── fuzz/                                    # 模糊测试
│   ├── test_malformed_yaml.cpp             # 损坏的YAML
│   ├── test_special_characters.cpp         # 特殊字符注入
│   └── test_extreme_inputs.cpp             # 极端输入测试
│
└── automation/                              # 自动化工具
    ├── IssueDetector.h/cpp                 # 问题检测器
    ├── AutoFixer.h/cpp                     # 自动修复器
    ├── TestRunner.h/cpp                    # 测试运行器
    └── ReportGenerator.h/cpp               # 报告生成器
```

### 4.2 核心测试组件设计

#### 4.2.1 LoopMasterTestFixture（主测试夹具）

**职责**：
- 临时目录管理
- 状态文件创建/删除/读取
- Hook脚本执行封装
- Mock Bash环境设置

#### 4.2.2 HookScriptTester（Hook脚本测试器）

**职责**：
- 封装Hook脚本执行
- 验证输出JSON格式
- 测试跨平台兼容性（Git Bash vs WSL）
- 测量执行时间

#### 4.2.3 StateFileSimulator（状态文件模拟器）

**职责**：
- 生成各种格式的YAML文件
- 模拟文件损坏场景
- 模拟特殊字符注入
- 支持并发写入模拟

#### 4.2.4 ConcurrentAccessTester（并发测试工具）

**职责**：
- 创建多线程并发访问场景
- 检测文件竞争条件
- 验证文件锁定机制
- 统计并发错误

### 4.3 自动化组件设计

#### 4.3.1 IssueDetector（问题检测器）

**检测能力**：
1. **静态分析**
   - Bash语法错误
   - sed/grep/awk命令兼容性
   - 未处理的错误分支
   - 缺失的引号/转义

2. **YAML问题**
   - 字段缺失
   - 格式错误
   - 类型不匹配

3. **兼容性**
   - Git Bash vs WSL差异
   - 路径分隔符
   - 换行符处理

4. **并发安全**
   - 文件锁缺失
   - 竞态条件
   - 原子性违反

5. **性能**
   - 超时风险（>10秒）
   - 低效命令
   - 资源泄漏

#### 4.3.2 AutoFixer（自动修复器）

**修复能力**：
1. **Hook脚本修复**
   - 修正sed命令语法（`-i` vs `-i ''`）
   - 添加缺失的引号
   - 修复路径分隔符
   - 添加错误处理

2. **命令文档修复**
   - 修正YAML格式
   - 更新示例
   - 修复拼写错误

3. **测试用例更新**
   - 添加缺失的测试
   - 更新断言
   - 添加边界条件

4. **自动提交**
   - 生成规范的commit message
   - 遵循项目的commit规范
   - 包含修复说明

#### 4.3.3 TestRunner（测试运行器）

**功能**：
- 并行执行测试（提升速度）
- 实时进度显示
- 覆盖率统计
- 失败测试重试
- 生成HTML报告

#### 4.3.4 ReportGenerator（报告生成器）

**报告内容**：
1. **执行摘要**
   - 测试覆盖率
   - 问题统计
   - 修复统计
   - 性能指标

2. **详细问题列表**
   - 按严重程度分类
   - 包含文件位置和行号
   - 修复建议

3. **测试结果**
   - 通过/失败/跳过
   - 执行时间
   - 错误消息

4. **覆盖率分析**
   - 行覆盖率
   - 分支覆盖率
   - 未覆盖代码

### 4.4 测试用例设计

#### 4.4.1 单元测试用例

**test_yaml_parser.cpp**
- YAML解析测试
- 字段缺失测试
- 格式错误测试
- 任务提示词提取测试
- 空内容处理测试

**test_iteration_logic.cpp**
- 激活状态检查测试
- 迭代限制测试（边界值）
- 无限循环模式测试
- 计数增加测试

**test_hook_decision.cpp**
- 允许退出JSON格式测试
- 阻止退出JSON格式测试
- 系统消息生成测试
- 任务提示词重新注入测试

#### 4.4.2 集成测试用例

**test_full_workflow.cpp**
- 完整工作流测试（启动→执行→迭代→停止）
- 手动取消测试
- 状态查询测试
- 完成条件检测测试

**test_error_scenarios.cpp**
- 状态文件不存在测试
- 状态文件不可读测试
- 状态文件为空测试
- Frontmatter损坏测试
- 任务提示词缺失测试
- 迭代值无效测试

#### 4.4.3 并发测试用例

**test_concurrent_file_access.cpp**
- 多线程同时读取测试
- 多线程同时写入测试
- 混合读写场景测试
- Hook同时执行测试

#### 4.4.4 性能测试用例

**test_hook_execution_time.cpp**
- Hook执行时间<10秒测试
- 大文件解析性能测试（10000+行）
- 高频迭代场景测试（100次/秒）
- 内存占用测试

#### 4.4.5 模糊测试用例

**test_malformed_yaml.cpp**
- 随机YAML损坏测试
- 无效UTF-8字符测试
- 二进制内容注入测试

**test_special_characters.cpp**
- SQL注入字符测试
- XSS尝试测试
- 路径遍历字符测试
- Shell转义字符测试

### 4.5 自动化流程设计

```
┌─────────────────────────────────────┐
│ 1. 自动发现问题                      │
│    - 运行完整测试套件                │
│    - 静态分析Hook脚本                │
│    - 架构合规性检查                  │
│    - 生成问题报告                    │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 2. 自动分类问题                      │
│    - Critical: 阻塞性问题            │
│    - High: 功能缺陷                  │
│    - Medium: 优化建议                │
│    - Low: 代码风格                   │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 3. 自动修复                          │
│    - Critical/High: 自动修复         │
│    - Medium: 生成修复建议            │
│    - Low: 记录到待办                │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 4. 自动验证                          │
│    - 重新运行测试                    │
│    - 验证修复效果                    │
│    - 生成验证报告                    │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ 5. 生成报告                          │
│    - 测试覆盖率                      │
│    - 问题修复统计                    │
│    - 架构合规性评分                  │
└─────────────────────────────────────┘
```

### 4.6 CI/CD集成

创建 `.github/workflows/loop-master-test.yml`：

```yaml
name: LoopMaster Automated Tests

on:
  push:
    paths:
      - '.claude/plugins/loop-master/**'
      - 'tests/loop-master/**'
  pull_request:
    paths:
      - '.claude/plugins/loop-master/**'
      - 'tests/loop-master/**'
  schedule:
    - cron: '0 0 * * *'  # 每天运行

jobs:
  test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3

      - name: Setup Dependencies
        run: |
          choco install git -y
          # 确保Bash可用

      - name: Build Tests
        run: |
          cmake -B build -S . -DSILIGEN_BUILD_TESTS=ON
          cmake --build build --target test_loop_master

      - name: Run Automated Tests
        run: |
          ./build/bin/test_loop_master_automation.exe

      - name: Upload Reports
        uses: actions/upload-artifact@v3
        with:
          name: test-reports
          path: loop-master-test-report.html
```

### 4.7 CMake集成

参考完整的CMakeLists.txt配置（见5.6节），包括：
- 测试目标定义
- Google Test集成
- 测试发现配置
- 覆盖率编译选项

### 4.8 实施步骤

#### Phase 1: 基础测试框架（2-3天）
- [ ] 创建测试目录结构
- [ ] 实现LoopMasterTestFixture
- [ ] 实现HookScriptTester
- [ ] 实现StateFileSimulator
- [ ] 配置CMake构建

**交付物**：
- 可运行的测试框架
- 基础测试夹具可用

#### Phase 2: 核心功能测试（3-4天）
- [ ] YAML解析测试
- [ ] 迭代逻辑测试
- [ ] Hook决策测试
- [ ] 命令测试（start/cancel/status）
- [ ] 错误场景测试

**交付物**：
- 核心功能测试覆盖率>80%
- 所有基本场景通过

#### Phase 3: 高级测试（3-4天）
- [ ] 并发访问测试
- [ ] 性能测试
- [ ] 模糊测试
- [ ] 边界条件测试

**交付物**：
- 测试覆盖率>95%
- 并发安全验证
- 性能基准建立

#### Phase 4: 自动化工具（4-5天）
- [ ] IssueDetector实现
- [ ] AutoFixer实现
- [ ] TestRunner实现
- [ ] ReportGenerator实现
- [ ] 主自动化脚本

**交付物**：
- 完整的自动化工具链
- 可运行的auto_test_loop_master.exe

#### Phase 5: 完善和优化（2-3天）
- [ ] 达到100%覆盖率
- [ ] CI/CD集成
- [ ] 文档完善
- [ ] 性能优化

**交付物**：
- 100%测试覆盖率
- CI/CD管道运行
- 完整文档

### 4.9 关键技术方案

#### 4.9.1 从C++测试Bash脚本

**挑战**：C++测试需要执行和验证Bash脚本

**方案**：使用Windows API（CreateProcess + pipes）创建子进程执行Bash命令，捕获stdout/stderr，获取退出码。

**关键点**：
- 创建匿名管道重定向输出
- 设置STARTUPINFO重定向标准输入/输出
- 使用CreateProcessA启动bash.exe
- 读取管道数据获取输出
- 获取进程退出码

#### 4.9.2 并发测试实现

**挑战**：模拟多会话并发访问状态文件

**方案**：使用C++11 std::thread创建多线程，每个线程独立读写状态文件，使用std::atomic<int>统计错误。

**关键点**：
- 使用lambda表达式传递线程逻辑
- 线程安全地访问共享文件
- 统计并发错误
- 验证数据一致性

#### 4.9.3 100%覆盖率保证

**策略**：
1. **代码覆盖率工具**：使用`gcov`/`lcov`（GCC）或`OpenCppCoverage`（MSVC）
2. **分支覆盖**：确保每个if/else分支都被测试
3. **异常覆盖**：测试所有异常路径
4. **边界覆盖**：测试所有边界值

**CMake配置**：
```cmake
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_loop_master PRIVATE --coverage)
    target_link_options(test_loop_master PRIVATE --coverage)
elseif(MSVC)
    # 使用OpenCppCoverage
endif()
```

**验证脚本**：
```bash
#!/bin/bash
# 运行测试并生成覆盖率报告

ctest --output-on-failure
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

### 4.10 成功标准

#### 功能性指标
- ✅ 所有核心功能测试通过（100%）
- ✅ 所有错误场景测试通过（100%）
- ✅ 所有边界条件测试通过（100%）

#### 覆盖率指标
- ✅ 代码行覆盖率：100%
- ✅ 分支覆盖率：100%
- ✅ 函数覆盖率：100%

#### 质量指标
- ✅ 无内存泄漏
- ✅ 无竞态条件
- ✅ Hook执行时间<10秒
- ✅ 并发测试通过

#### 自动化指标
- ✅ 问题检测率>95%
- ✅ 自动修复成功率>90%
- ✅ 修复验证通过率100%
- ✅ CI/CD集成正常

---

## 五、附录

### 5.1 关键文件清单

**需要创建的文件**：
- `tests/loop-master/CMakeLists.txt` - 测试构建配置
- `tests/loop-master/fixtures/*.h` - 测试夹具（4个）
- `tests/loop-master/unit/*.cpp` - 单元测试（4个）
- `tests/loop-master/integration/*.cpp` - 集成测试（5个）
- `tests/loop-master/concurrent/*.cpp` - 并发测试（2个）
- `tests/loop-master/performance/*.cpp` - 性能测试（2个）
- `tests/loop-master/fuzz/*.cpp` - 模糊测试（3个）
- `tests/loop-master/automation/*.{h,cpp}` - 自动化工具（4组）

**需要修改的文件**：
- `tests/CMakeLists.txt` - 添加LoopMaster测试子目录

**需要参考的文件**：
- `.claude/plugins/loop-master/hooks/scripts/stop-hook.sh` - Hook脚本
- `.claude/plugins/loop-master/commands/*.md` - 命令定义
- `tests/utils/TestFixture.h` - 现有测试工具
- `tests/integration/IntegrationTestFixture.h` - 集成测试模式

### 5.2 预计总工时

**总工时**：14-19天

**各阶段工时**：
- Phase 1: 2-3天（基础框架）
- Phase 2: 3-4天（核心功能）
- Phase 3: 3-4天（高级测试）
- Phase 4: 4-5天（自动化工具）
- Phase 5: 2-3天（完善优化）

### 5.3 风险与缓解措施

**风险因素**：
1. Bash脚本跨平台兼容性问题（已简化为仅Windows）
2. 并发测试的复杂性
3. 100%覆盖率的难度

**缓解措施**：
1. 使用Mock简化复杂场景
2. 分阶段实施，逐步提高覆盖率
3. 优先自动化简单修复，复杂问题人工处理
4. 充分利用现有测试基础设施

### 5.4 参考文档

- LoopMaster插件README：`.claude/plugins/loop-master/README.md`
- 测试基础设施文档：`docs/04-development/testing-guide/test-infrastructure.md`
- 项目编码规范：`docs/04-development/coding-guidelines.md`
- Google Test文档：https://google.github.io/googletest/
