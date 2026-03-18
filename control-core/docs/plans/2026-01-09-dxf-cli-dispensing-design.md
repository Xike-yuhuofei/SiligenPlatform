# DXF CLI点胶功能设计文档

**日期**: 2026-01-09
**目标**: 为明天的领导演示实现CLI方式的DXF文件驱动电机和电磁阀联动功能

## 需求概述

绕过WebUI，通过CLI直接读取DXF文件并执行点胶操作，实现电机和电磁阀的联动控制。

## 设计决策

### 1. 交互方式
- **选择**: 命令行参数式（一次性执行）
- **理由**: 实现最快，适合演示

### 2. 文件访问
- **选择**: 本地文件路径
- **理由**: 完全独立于WebUI系统

### 3. 配置方式
- **选择**: 使用默认配置
- **理由**: 命令最简洁

### 4. 执行模式
- **选择**: 支持真实执行和模拟执行
- **理由**: 演示可靠性和开发便利性

### 5. 触发策略
- **选择**: CMP 位置触发基于规划位置（Profile Position），不使用编码器反馈
- **理由**: 受硬件条件限制，编码器无法参与点胶规划

## 架构设计

```
用户命令行输入
    ↓
CommandLineParser 解析参数
    ↓
CLIAdapter::executeDXFDispensing()
    ↓
ApplicationContainer 解析依赖
    ↓
DXFDispensingExecutionUseCase::Execute()
    ↓
[真实硬件] 或 [模拟模式]
    ↓
输出执行结果到控制台
```

## 实现方案

**方案**: 最小化实现（在现有main.cpp中扩展）

**优点**:
- 实现最快（~70分钟）
- 代码量最少
- 足够满足演示需求
- 利用现有的依赖注入容器

## 需要修改的文件

1. `src/application/cli/CommandLineParser.h/cpp`
   - 添加 `RunMode::DXF_DISPENSE`
   - 添加 `dxf_file_path` 和 `dry_run` 字段

2. `apps/control-cli/main.cpp`
   - 添加 `executeDXFDispensing()` 方法
   - 更新 `executeCommand()` switch
   - 更新 `printHelp()`

3. `src/adapters/cli/CLICompositionRoot.cpp`（可能）
   - 确保 `DXFDispensingExecutionUseCase` 可解析

## 命令格式

```bash
# 真实执行
siligen_cli dxf-dispense --file=D:/test/pattern.dxf

# 模拟执行
siligen_cli dxf-dispense --file=D:/test/pattern.dxf --dry-run
```

## 核心实现逻辑

```cpp
int executeDXFDispensing(const CommandLineConfig& config) {
    // 1. 验证文件路径
    if (config.dxf_file_path.empty()) {
        LogError("未指定DXF文件路径");
        return 1;
    }

    // 2. 从容器解析用例
    auto usecase = container_->Resolve<DXFDispensingExecutionUseCase>();
    if (!usecase) {
        LogError("无法解析DXFDispensingExecutionUseCase");
        return 1;
    }

    // 3. 构建请求
    DXFDispensingMVPRequest request;
    request.dxf_filepath = config.dxf_file_path;

    // 4. 执行（同步）
    auto result = usecase->Execute(request);

    // 5. 输出结果
    if (result.IsSuccess()) {
        auto response = result.Value();
        // 输出成功信息和统计数据
        std::cout << "点胶完成！" << std::endl;
        std::cout << "统计信息:" << std::endl;
        std::cout << "  - 总段数: " << response.total_segments << std::endl;
        std::cout << "  - 成功段数: " << response.executed_segments << std::endl;
        std::cout << "  - 失败段数: " << response.failed_segments << std::endl;
        std::cout << "  - 总距离: " << response.total_distance << " mm" << std::endl;
        std::cout << "  - 执行时间: " << response.execution_time_seconds << " 秒" << std::endl;
        return 0;
    } else {
        std::cout << "点胶失败: " << result.GetError().GetMessage() << std::endl;
        return 1;
    }
}
```

## 模拟模式处理

**方案**: 自动降级
- 如果硬件连接失败，显示警告并继续执行文件解析
- 无需修改用例代码
- 依赖错误检测机制

## 错误处理

**分层错误处理**:
1. 参数验证层（CLI层）- 文件路径、用例解析
2. 用例执行层（应用层）- 硬件连接、DXF解析、运动执行
3. 异常捕获 - 所有操作包裹在try-catch中

## 控制台输出格式

### 成功执行
```
[INFO] CLI: Siligen CLI启动
[INFO] CLI: 执行DXF点胶
正在加载DXF文件: D:/test/pattern.dxf
DXF解析成功: 找到 15 个LINE段
正在配置2轴坐标系...
正在打开供胶阀...
开始执行点胶路径...
  [1/15] 执行段 (0.0, 0.0) -> (10.0, 10.0) ✓
  ...
正在关闭供胶阀...
点胶完成！
统计信息:
  - 总段数: 15
  - 成功段数: 15
  - 失败段数: 0
  - 总距离: 120.5 mm
  - 执行时间: 45.2 秒
```

### 模拟模式
```
[WARN] CLI: 硬件连接失败，进入模拟模式
正在加载DXF文件: D:/test/pattern.dxf
DXF解析成功: 找到 15 个LINE段
[模拟] 配置2轴坐标系
[模拟] 打开供胶阀
[模拟] 执行点胶路径...
模拟执行完成！
```

## 实现步骤

1. **扩展CommandLineParser**（15分钟）
   - 添加枚举和字段
   - 实现参数解析

2. **实现executeDXFDispensing**（30分钟）
   - 验证、解析、执行、输出

3. **更新executeCommand**（5分钟）
   - 添加switch分支

4. **更新帮助信息**（5分钟）
   - 添加命令说明

5. **测试**（15分钟）
   - 编译、错误处理、模拟模式、真实执行

**预计总时间**: ~70分钟

## 成功标准

✅ 命令可以成功解析DXF文件
✅ 可以显示将要执行的路径段信息
✅ 硬件连接失败时自动进入模拟模式
✅ 硬件连接成功时可以真实执行点胶
✅ 错误信息清晰易懂

## 风险和缓解

**风险**: 依赖注入容器配置问题
**缓解**: 先检查容器配置，确保用例可解析

**风险**: 硬件连接问题影响演示
**缓解**: 支持模拟模式作为备选方案

## 后续优化（演示后）

- 重构为独立的Command类
- 添加更多命令行选项（速度、加速度等）
- 支持进度条显示
- 添加暂停/恢复功能

