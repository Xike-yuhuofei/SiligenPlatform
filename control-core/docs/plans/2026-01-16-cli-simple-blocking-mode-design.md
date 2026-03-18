# CLI点胶功能 - 简单阻塞模式设计

**日期**: 2026-01-16
**状态**: completed
**版本**: v2.0 (重构)

---

## 一、设计决策

### 1.1 问题背景

原实现采用多线程异步模式,存在以下问题:
- `WaitForTaskCompletion()` 使用 `cv_.wait()` 但无 `cv_.notify()` 调用
- 主线程永久阻塞,程序无法正常退出
- 线程清理使用 `detach()` 导致资源泄漏风险

### 1.2 解决方案

采用**简单阻塞模式**重构:
- 移除所有多线程代码
- 使用同步 `Execute()` 替代 `ExecuteAsync()`
- 执行期间用户可通过 Ctrl+C 强制退出

### 1.3 设计权衡

| 特性 | 原设计 | 新设计 |
|------|--------|--------|
| 代码行数 | ~600行 | ~265行 |
| 线程数 | 3个 (主+键盘+进度) | 1个 |
| 进度显示 | 实时进度条 | 无 |
| 中断方式 | ESC菜单 | Ctrl+C |
| 复杂度 | 高 | 低 |
| 可维护性 | 差 | 好 |

---

## 二、架构

```
CLI客户端 (siligen_cli.exe)
    |
    v
CLIAdapter (简单阻塞模式)
    |
    v
ApplicationContainer (依赖注入)
    |
    v
DXFDispensingExecutionUseCase::Execute() (同步)
    |
    v
[硬件层] MultiCard + 电磁阀
```

---

## 三、代码结构

### 3.1 CLIAdapter 类

```cpp
class CLIAdapter {
public:
    CLIAdapter();   // 初始化容器,自动连接硬件
    ~CLIAdapter();  // 静默stdout
    int run();      // 主循环

private:
    std::shared_ptr<ApplicationContainer> container_;
    Shared::Types::Result<void> connection_result_;

    void executeDXFDispensing();  // 核心点胶功能
    void ShowResult(...);         // 显示执行结果
    // ... 其他辅助方法
};
```

### 3.2 执行流程

```
1. 显示菜单
2. 用户选择 "1. DXF文件点胶"
3. 输入文件路径 (或使用默认)
4. 验证文件存在
5. 调用 usecase->Execute() (同步阻塞)
6. 显示执行结果
7. 返回主菜单
```

---

## 四、移除的功能

- `InterruptAction` 枚举
- `KeyboardListenerThread()` 键盘监听线程
- `ProgressDisplayThread()` 进度显示线程
- `UpdateProgressBar()` 进度条绘制
- `ShowInterruptMenu()` 中断菜单
- `HandleInterruptRequest()` 中断处理
- `WaitForTaskCompletion()` 任务等待
- `CleanupAfterExecution()` 资源清理
- `ShowFinalResultFromTask()` 异步结果显示
- 所有 `std::thread`, `std::atomic`, `std::mutex`, `std::condition_variable`
- `<conio.h>` Windows键盘检测

---

## 五、用户界面

### 5.1 主菜单

```
========================================
   Siligen 点胶机控制系统
   交互式菜单模式
========================================

请选择操作:
  1. DXF文件点胶
  0. 退出

请输入选项:
```

### 5.2 执行点胶

```
--- DXF文件点胶 ---
请输入DXF文件路径 (直接按Enter使用默认路径):
[默认]: D:\Projects\SiligenSuite\control-core\uploads\dxf\demo_100x100_rect_diag.dxf

路径>

正在加载DXF文件: D:\Projects\SiligenSuite\control-core\uploads\dxf\demo_100x100_rect_diag.dxf

正在执行点胶... (按 Ctrl+C 可强制退出)
```

### 5.3 执行结果

```
========================================
         执行结果
========================================

状态: 成功
消息: 点胶执行成功

统计信息:
  - 总段数: 8
  - 已执行: 8
  - 失败段数: 0
  - 总距离: 565.69 mm
  - 执行时间: 56.57 秒
========================================

按Enter键继续...
```

---

## 六、后续扩展

如需恢复进度显示功能,建议采用以下方案:

### 方案A: 回调模式

```cpp
// UseCase 提供进度回调
using ProgressCallback = std::function<void(uint32 executed, uint32 total)>;
Result<...> Execute(const Request& req, ProgressCallback on_progress = nullptr);
```

### 方案B: 轮询模式 (单线程)

```cpp
void executeDXFDispensing() {
    auto task_id = usecase->ExecuteAsync(request);

    while (true) {
        auto status = usecase->GetTaskStatus(task_id);
        if (status.state == "completed") break;

        // 检测 ESC (非阻塞)
        if (_kbhit() && _getch() == 27) {
            usecase->CancelTask(task_id);
            break;
        }

        // 显示进度
        PrintProgress(status.executed, status.total);
        Sleep(100);
    }
}
```

---

## 七、参考文档

- 原设计: `.claude/plans/2026-01-16-cli-dispensing-exit-feature.md`
- UseCase: `src/application/usecases/DXFDispensingExecutionUseCase.h`
- 实现: `apps/control-cli/main.cpp`

