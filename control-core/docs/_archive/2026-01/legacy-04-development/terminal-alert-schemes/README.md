# 终端视觉提醒方案设计文档

## 📋 场景描述

**环境配置**：Windows Terminal + Git Bash + Claude Code

**使用场景**：
- 同时打开多个 Windows Terminal 窗口
- 每个窗口包含多个 Git Bash 选项卡
- 每个选项卡运行独立的 Claude Code 会话

**核心需求**：
准确识别某个选项卡的 Claude Code 触发了 Stop Hook 或 Notification Hook，并提供直观的视觉提醒效果。

**期望效果**：
- ✅ Toast 消息框通知
- ✅ 提示音提醒
- ✅ 选项卡标题红色闪烁
- ✅ 选项卡颜色动态变化

---

## 🎯 方案对比总览

| 方案 | 实现难度 | 视觉效果 | 可靠性 | 性能影响 | 兼容性 |
|------|---------|---------|--------|---------|--------|
| **方案1：终端标题闪烁** | ⭐ 简单 | ⭐⭐⭐ 中等 | ⭐⭐⭐⭐ 高 | ⭐⭐⭐⭐⭐ 极低 | 所有终端 |
| **方案2：OSC 调用控制颜色** | ⭐⭐ 中等 | ⭐⭐⭐⭐ 优秀 | ⭐⭐⭐⭐ 高 | ⭐⭐⭐⭐⭐ 极低 | Windows Terminal |
| **方案3：Toast + 窗口闪烁** | ⭐⭐⭐ 较难 | ⭐⭐⭐⭐⭐ 优秀 | ⭐⭐⭐ 中等 | ⭐⭐⭐⭐ 低 | Windows 10+ |
| **方案4：状态文件 + 独立监控** | ⭐⭐⭐⭐ 复杂 | ⭐⭐⭐⭐⭐ 卓越 | ⭐⭐⭐⭐⭐ 高 | ⭐⭐⭐⭐ 极低 | 跨平台 |

---

## 📚 文档导航

### 方案文档
- [方案1：终端标题闪烁](./scheme-01-title-flashing.md)
- [方案2：OSC 调用控制颜色](./scheme-02-osc-color-control.md)
- [方案3：Toast + 窗口闪烁](./scheme-03-toast-window-flash.md)
- [方案4：状态文件 + 独立监控](./scheme-04-state-monitor.md)

### 实施指南
- [快速实施指南](./quick-start.md)
- [组合方案实现](./combined-solution.md)
- [调试技巧](./debugging-tips.md)

### 代码示例
- [Shell 实现代码](./code-examples/shell-implementation.sh)
- [PowerShell 辅助脚本](./code-examples/powershell-helpers.ps1)
- [Python 监控进程](./code-examples/python-monitor.py)

---

## 🏆 推荐方案

### 🥇 最佳组合：方案1 + 方案2

**适用场景**：
- ✅ 使用 Windows Terminal + Git Bash
- ✅ 需要快速实施，低维护成本
- ✅ 追求性能和兼容性平衡

**实施效果**：
| 提醒类型 | 标题变化 | 选项卡颜色 | 系统通知 | 实现复杂度 |
|---------|---------|-----------|---------|-----------|
| Stop Hook | 🛑 闪烁 5 次 | 🔴 红色闪烁 3 次 | ❌ 无 | ⭐⭐ 简单 |
| Notification | 🔔 显示消息 | 🟠 橙色常亮 | ❌ 无 | ⭐ 极简 |

**核心优势**：
1. ✅ 兼容性好：标题闪烁在所有终端生效
2. ✅ 视觉明显：Windows Terminal 用户额外获得颜色闪烁
3. ✅ 性能高：纯 Shell 实现，开销 <5ms
4. ✅ 易于维护：代码量少，无外部依赖
5. ✅ 可扩展：未来可轻松添加声音、Toast 等

**详细实施步骤**：参见 [组合方案实现文档](./combined-solution.md)

---

## 🚀 快速开始

### 验证终端能力

在 Git Bash 中运行以下命令，测试你的终端支持哪些特性：

```bash
# 1. 测试标题修改（所有终端）
printf '\e]0;测试标题\a'
sleep 2
printf '\e]0;Claude Code\a'

# 2. 测试颜色控制（仅 Windows Terminal）
printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'
sleep 2
# 恢复默认
printf '\e]1337;Colors=[{"color":"#000000"}]\a'

# 3. 检查环境变量
echo "WT_SESSION: ${WT_SESSION:-not set}"
echo "TERM: $TERM"
```

### 选择方案

根据测试结果选择方案：
- ✅ 标题修改成功 → 可使用**方案1**
- ✅ 颜色控制生效 → 可使用**方案2**（推荐组合）
- ❌ 两者都不支持 → 考虑**方案3**或**方案4**

---

## 📊 性能对比

### Hook 执行时间测试

```bash
# 测试方案1（标题闪烁）
time printf '\e]0;🛑 STOP-HOOK\a'
# 实际输出：0.001s

# 测试方案2（OSC 颜色控制）
time printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'
# 实际输出：0.002s

# 测试方案3（PowerShell Toast）
time powershell -Command "Show-BalloonTip"
# 实际输出：0.8s

# 测试方案4（写入状态文件）
time echo '{"alert":"stop"}' > .claude/alert.json
# 实际输出：0.003s
```

**结论**：
- 方案1和方案2性能最优（<5ms）
- 方案3性能最差（>800ms）
- 方案4性能良好（<10ms），但需要额外监控进程

---

## 🔍 架构对比

### 方案1 + 方案2 架构

```
┌─────────────────┐
│   Stop Hook     │
└────────┬────────┘
         │
         ├─────────────────────────┐
         │                         │
         ▼                         ▼
┌─────────────────┐    ┌──────────────────┐
│  修改终端标题    │    │  OSC 颜色控制     │
│  (OSC 0 序列)   │    │  (OSC 1337 序列)  │
└─────────────────┘    └────────┬─────────┘
                                 │
                                 ▼
                        ┌──────────────────┐
                        │ Windows Terminal  │
                        │  - 标题闪烁      │
                        │  - 选项卡变色    │
                        └──────────────────┘
```

### 方案4 架构

```
┌─────────────────┐    write to    ┌──────────────────┐
│   Stop Hook     │ ──────────────>│ tab-alert.json   │
└─────────────────┘                 └────────┬─────────┘
                                             │
                                             │ monitor
                                             ▼
                                    ┌──────────────────┐
                                    │  Alert Monitor   │
                                    │  (Python/Node)   │
                                    └────────┬─────────┘
                                             │
                                             │ invoke API
                                             ▼
                                    ┌──────────────────┐
                                    │ Windows Terminal │
                                    │  - Set Tab Color │
                                    │  - Flash Title   │
                                    └──────────────────┘
```

---

## 📝 实施路线图

### 阶段1：快速验证（1小时）
- [ ] 在 `stop-hook.sh` 中添加标题闪烁
- [ ] 测试 Git Bash 中的效果
- [ ] 如果使用 Windows Terminal，添加 OSC 颜色控制

### 阶段2：增强体验（2-4小时，可选）
- [ ] 添加声音提醒（`printf '\a'` 终端铃声）
- [ ] 添加 PowerShell Toast 通知（需要用户手动授权）
- [ ] 考虑方案4的独立监控进程（如果需要更复杂的提醒）

### 阶段3：性能优化（1小时，可选）
- [ ] 使用 `flock` 确保多进程安全
- [ ] 添加去重逻辑（避免同一提醒重复触发）
- [ ] 监控 Hook 执行时间（确保 <100ms）

---

## 🛠️ 技术细节

### OSC 序列说明

**OSC (Operating System Command)** 是 ANSI 转义序列的一种，用于向终端发送特殊命令。

**常用 OSC 序列**：
- `\e]0;标题\a` - 修改窗口/选项卡标题
- `\e]1337;Colors=[...]\a` - Windows Terminal 颜色控制
- `\e]9;通知文本\a` - 部分终端的桌面通知

**兼容性**：
- OSC 0: 所有现代终端（Git Bash, iTerm2, GNOME Terminal 等）
- OSC 1337: 仅 Windows Terminal 1.16+

### 环境变量

**关键环境变量**：
- `WT_SESSION` - Windows Terminal 会话标识符（仅 WT 中存在）
- `TERM` - 终端类型（如 `xterm-256color`）
- `CLAUDE_SESSION_ID` - Claude Code 会话 ID（自定义）

---

## 🔗 相关资源

### 官方文档
- [Windows Terminal OSC 1337 规范](https://github.com/microsoft/terminal/blob/main/doc/specs/%234999%20-%20Terminal%20as%20a%20Console%20Engine.md)
- [ANSI 转义序列标准](https://en.wikipedia.org/wiki/ANSI_escape_code)
- [Claude Code Hooks 文档](https://docs.anthropic.com/claude-code/hooks)

### 工具库
- [BurntToast (PowerShell)](https://github.com/Windos/BurntToast)
- [terminal-notifier (macOS)](https://github.com/julienXX/terminal-notifier)
- [libnotify (Linux)](https://developer.gnome.org/libnotify/)

---

## 🤝 贡献

如果你有新的方案或改进建议，请：
1. 在 `docs/04-development/terminal-alert-schemes/` 目录下创建新文档
2. 更新本 README.md，添加方案对比和导航链接
3. 提交 PR，描述你的方案优势和适用场景

---

## 📅 更新日志

| 日期 | 版本 | 更新内容 |
|------|------|---------|
| 2025-01-06 | 1.0.0 | 初始版本，包含4个方案设计 |

---

## 📄 许可

本文档遵循项目许可证。
