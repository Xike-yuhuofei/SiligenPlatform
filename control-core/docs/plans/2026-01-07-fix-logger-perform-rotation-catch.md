# Fix Logger PerformRotation Empty Catch Block

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task.

**Goal:** 修复 `Logger::PerformRotation()` 函数中的空 catch 块，确保异常信息被记录到 stderr

**Architecture:** 单一任务修复，遵循语义审查报告 H4 节推荐模式

**Tech Stack:** C++17, std::fprintf(stderr, ...)

---

## Context

**问题来源:** 语义审查报告 (`docs/04-development/semantic-audit-report.md`) H4 节识别出空 catch 块吞掉异常的问题

**已修复位置:** Task 2 已修复 `InitializeLogFile`、`WriteToFile(LogEntry)`、`WriteToFile(string)` 中的 3 个空 catch 块

**遗漏位置:** 规范审查发现 `PerformRotation()` 函数第 255 行存在空 catch 块，但原始规范未包含此位置

**当前代码:**
```cpp
// src/infrastructure/logging/Logger.cpp:255
} catch (...) { return false; }
```

**推荐修复模式:**
```cpp
} catch (const std::exception& e) {
    std::fprintf(stderr, "[Logger] Exception in PerformRotation: %s\n", e.what());
} catch (...) {
    std::fprintf(stderr, "[Logger] Unknown exception in PerformRotation\n");
}
return false;
```

---

## Task 1: 修复 PerformRotation 空 catch 块

**Files:**
- Modify: `src/infrastructure/logging/Logger.cpp:255`

**Step 1: 读取当前代码确认位置**

读取 `src/infrastructure/logging/Logger.cpp` 第 241-256 行确认 `PerformRotation` 函数的完整结构。

**Step 2: 修改 catch 块**

将第 255 行的空 catch 块:
```cpp
} catch (...) { return false; }
```

替换为标准异常处理模式:
```cpp
} catch (const std::exception& e) {
    std::fprintf(stderr, "[Logger] Exception in PerformRotation: %s\n", e.what());
} catch (...) {
    std::fprintf(stderr, "[Logger] Unknown exception in PerformRotation\n");
}
return false;
```

**Step 3: 验证编译**

运行编译确认语法正确:

```bash
# 使用 compile agent 验证
cmake --build build\stage4-all-modules-check --config Debug --target siligen_spdlog_adapter
```

Expected: 编译成功，无错误无警告

**Step 4: 检查一致性**

确认修复模式与已修复的其他 catch 块风格一致:
- `InitializeLogFile` catch 块 (约第 170-181 行)
- `WriteToFile(LogEntry)` catch 块 (约第 198 行)
- `WriteToFile(string)` catch 块 (约第 210-220 行)

**Step 5: 更新技术债务记录**

编辑 `docs/04-development/semantic-audit-report.md`，在相关章节记录此修复已完成。

**Step 6: 提交**

```bash
git add src/infrastructure/logging/Logger.cpp
git add docs/04-development/semantic-audit-report.md
git commit -m "fix(Logger): 修复 PerformRotation 空 catch 块

- 将空 catch (...) 替换为标准异常处理
- 捕获 std::exception 并记录 what() 到 stderr
- 捕获未知异常并记录通用消息
- 与 Task 2 已修复的其他 catch 块保持一致风格

修复 H4: 空 catch 块吞掉异常 (遗漏位置)

参考: docs/04-development/semantic-audit-report.md H4 节"
```

---

## Completion Criteria

- [ ] 第 255 行 catch 块捕获 `const std::exception&` 并记录 `e.what()`
- [ ] 第 255 行 catch 块捕获未知异常 `...` 并记录通用消息
- [ ] 修复风格与已修复的其他 catch 块一致
- [ ] 编译通过无警告
- [ ] 技术债务报告已更新
- [ ] 提交消息包含 H4 引用

---

## Self-Review Checklist

- [ ] 所有异常路径都记录到 stderr
- [ ] 不再有空的 catch 块
- [ ] return false 语句在两个 catch 块之外
- [ ] 代码格式与周围代码一致
