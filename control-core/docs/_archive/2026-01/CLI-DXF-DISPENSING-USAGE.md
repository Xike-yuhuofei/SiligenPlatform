# CLI DXF点胶功能使用说明

**日期**: 2026-01-09
**目标**: 明天领导演示 - 基于DXF文件的电机和电磁阀联动功能

## 快速开始

### 1. 编译（已完成）

CLI可执行文件位置：
```
D:\Projects\Backend_CPP\build\bin\siligen_cli.exe
```

### 2. 基本命令格式

```bash
# 查看帮助
siligen_cli.exe --help

# 执行DXF点胶（真实硬件）
siligen_cli.exe --mode=dxf-dispense --file=<DXF文件路径>

# 模拟执行（无需硬件）
siligen_cli.exe --mode=dxf-dispense --file=<DXF文件路径> --dry-run
```

### 3. 演示示例

#### 示例1：使用真实硬件执行点胶

```bash
cd D:\Projects\Backend_CPP\build\bin
siligen_cli.exe --mode=dxf-dispense --file=D:/test/pattern.dxf
```

**预期输出**：
```
[INFO] CLI: Siligen CLI启动
[INFO] CLI: 执行DXF点胶
正在加载DXF文件: D:/test/pattern.dxf
DXF解析成功: 找到 15 个LINE段
正在配置2轴坐标系...
正在打开供胶阀...
开始执行点胶路径...
  [1/15] 执行段 (0.0, 0.0) -> (10.0, 0.0) ✓
  ...
正在关闭供胶阀...
========================================
点胶完成！
========================================
统计信息:
  - 总段数: 15
  - 成功段数: 15
  - 失败段数: 0
  - 总距离: 120.5 mm
  - 执行时间: 45.2 秒
========================================
```

#### 示例2：模拟执行（演示备选方案）

如果硬件出现问题，可以使用模拟模式演示功能：

```bash
siligen_cli.exe --mode=dxf-dispense --file=D:/test/pattern.dxf --dry-run
```

**注意**：当前实现中，如果硬件连接失败会自动显示警告，但仍会继续解析DXF文件。

## 功能说明

### 核心功能

1. **DXF文件解析**
   - 自动解析DXF文件中的LINE和ARC段
   - 提取路径坐标信息

2. **电机控制**
   - 2轴坐标系（X=轴1, Y=轴2）
   - 固定速度：10 mm/s（点胶）、200 mm/s（空移）
   - 固定加速度：500 mm/s²

3. **电磁阀联动**
   - 自动管理供胶阀生命周期
   - 开始前打开，结束后关闭
   - 错误时自动回滚阀门状态

4. **安全保护**
   - 软限位检查
   - 硬件连接验证
   - 错误自动恢复

### 架构优势

- **六边形架构**：CLI作为适配器层，直接调用应用层用例
- **业务逻辑复用**：与WebUI使用相同的`DXFDispensingExecutionUseCase`
- **依赖注入**：通过`ApplicationContainer`自动管理依赖

## 故障排查

### 问题1：找不到DXF文件

**错误信息**：
```
[ERROR] CLI: 未指定DXF文件路径，请使用 --file=<路径> 参数
```

**解决方案**：
- 确保使用 `--file=` 参数指定文件路径
- 使用绝对路径，例如：`D:/test/pattern.dxf`

### 问题2：硬件连接失败

**错误信息**：
```
[WARNING] 硬件连接失败，进入模拟模式
```

**解决方案**：
- 检查控制卡连接
- 检查IP地址配置（默认：192.168.0.1）
- 或使用 `--dry-run` 参数进行模拟演示

### 问题3：DXF解析失败

**错误信息**：
```
点胶失败: DXF文件解析失败
```

**解决方案**：
- 确保DXF文件格式正确
- 确保文件包含LINE或ARC段
- 检查文件路径是否正确

## 演示建议

### 演示流程

1. **准备阶段**
   - 准备一个简单的DXF测试文件（矩形或圆形）
   - 确认硬件连接正常
   - 测试一次确保功能正常

2. **演示阶段**
   - 打开命令行窗口
   - 切换到CLI目录：`cd D:\Projects\Backend_CPP\build\bin`
   - 执行命令：`siligen_cli.exe --mode=dxf-dispense --file=<测试文件>`
   - 观察输出和实际运动

3. **备选方案**
   - 如果硬件出现问题，立即切换到模拟模式
   - 强调架构优势：CLI和WebUI使用相同的业务逻辑

### 演示要点

1. **跳过WebUI**：直接通过CLI执行，无需启动Web服务器
2. **电机和电磁阀联动**：强调自动管理供胶阀生命周期
3. **架构优势**：展示六边形架构的灵活性
4. **实时反馈**：显示执行进度和统计信息

## 技术细节

### 修改的文件

1. `src/application/cli/CommandLineParser.h/cpp`
   - 添加 `DXF_DISPENSE` 模式
   - 添加 `dxf_file_path` 和 `dry_run` 字段

2. `apps/control-cli/main.cpp`
   - 实现 `executeDXFDispensing()` 方法
   - 添加命令分支和帮助信息

3. `src/adapters/cli/CMakeLists.txt`
   - 启用CLI目标编译

4. `src/application/container/ApplicationContainer.cpp`
   - 注释未实现的UseCase引用

### 实现时间

- 设计：30分钟
- 实现：40分钟
- 调试：20分钟
- **总计：~90分钟**

## 后续优化（演示后）

1. 实现 `InitializeSystemUseCase` 和 `HomeAxesUseCase`
2. 添加更多命令行选项（速度、加速度等）
3. 支持进度条显示
4. 添加暂停/恢复功能
5. 重构为独立的Command类

## 联系方式

如有问题，请参考：
- 设计文档：`docs/plans/2026-01-09-dxf-cli-dispensing-design.md`
- 本使用说明：`docs/CLI-DXF-DISPENSING-USAGE.md`
