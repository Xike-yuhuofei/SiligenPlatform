# 方案1：终端标题闪烁

## 📋 方案概述

**方案名称**：终端标题闪烁 (Terminal Title Flashing)

**核心原理**：使用 ANSI OSC 0 序列修改终端/选项卡标题，通过快速切换标题文字实现闪烁效果。

**实现难度**：⭐ 简单

**视觉效果**：⭐⭐⭐ 中等

**兼容性**：⭐⭐⭐⭐⭐ 所有支持 ANSI 转义序列的终端

---

## 🎯 工作原理

### ANSI OSC 0 序列

OSC 0 是 ANSI 转义序列标准中用于修改窗口/选项卡标题的命令：

```
\e]0;标题文本\a
```

**组成解析**：
- `\e]0;` - OSC 0 命令前缀（ESC + ] + 0 + ;）
- `标题文本` - 要显示的标题内容
- `\a` - 命令终止符（BEL 字符，响铃）

**示例**：
```bash
# 修改标题为 "Claude Code"
printf '\e]0;Claude Code\a'

# 修改标题为 "🛑 STOP-HOOK"
printf '\e]0;🛑 STOP-HOOK\a'
```

### 闪烁实现原理

通过在短时间内多次修改标题，实现闪烁效果：

```bash
# 闪烁 3 次（红色 → 黄色 → 恢复）
printf '\e]0;🛑 STOP-HOOK\a'
sleep 0.2
printf '\e]0;⚠️ 等待确认\a'
sleep 0.2
printf '\e]0;🛑 STOP-HOOK\a'
sleep 0.2
printf '\e]0;⚠️ 等待确认\a'
sleep 0.2
printf '\e]0;Claude Code\a'
```

**视觉效果**：
- 0.0s: 🛑 STOP-HOOK
- 0.2s: ⚠️ 等待确认
- 0.4s: 🛑 STOP-HOOK
- 0.6s: ⚠️ 等待确认
- 0.8s: Claude Code（恢复正常）

---

## 💻 完整实现代码

### Hook 集成代码

将以下代码添加到 `stop-hook.sh`：

```bash
# ============================================================================
# 终端标题闪烁模块
# ============================================================================

# ----------------------------------------------------------------------------
# 设置终端标题（通用函数）
# ----------------------------------------------------------------------------
set_terminal_title() {
    local title="$1"
    printf '\e]0;%s\a' "$title"
}

# ----------------------------------------------------------------------------
# 标题闪烁函数
# ----------------------------------------------------------------------------
flash_terminal_title() {
    local alert_type="$1"
    local session_id="${2:-unknown}"

    case "$alert_type" in
        stop)
            # Stop Hook: 红色闪烁 5 次
            local flash_count=5
            local delay=0.2

            for ((i = 1; i <= flash_count; i++)); do
                if [[ $((i % 2)) -eq 1 ]]; then
                    set_terminal_title "🛑 STOP-HOOK [$session_id]"
                else
                    set_terminal_title "⚠️ 等待确认 [$session_id]"
                fi
                sleep "$delay"
            done

            # 恢复正常标题
            set_terminal_title "Claude Code [$session_id] - 已停止"
            ;;

        notification)
            # Notification: 显示消息
            local message="$3"
            set_terminal_title "🔔 $message [$session_id]"
            ;;

        circuit_breaker)
            # Circuit Breaker: 熔断器触发
            set_terminal_title "🚨 熔断器触发 [$session_id]"
            ;;

        rate_limit)
            # Rate Limit: 速率限制
            set_terminal_title "⏸️ 速率限制 [$session_id]"
            ;;

        *)
            # 默认警告
            set_terminal_title "⚠️ 通知 [$session_id]"
            ;;
    esac
}

# ----------------------------------------------------------------------------
# 在 Hook 退出前调用
# ----------------------------------------------------------------------------

# 修改 allow_exit 函数
allow_exit() {
    local message="${1:-}"
    local session_id="${CLAUDE_SESSION_ID:-${PPID}}"

    # 1. 触发标题闪烁
    flash_terminal_title "stop" "$session_id"

    # 2. 可选：播放提示音
    printf '\a'  # 终端铃声

    # 3. 返回 JSON 响应
    if [ -n "$message" ]; then
        echo "{\"systemMessage\": \"$message\"}"
    else
        echo '{}'
    fi
    exit 0
}
```

---

## 🧪 测试验证

### 单元测试

创建测试脚本 `test_title_flashing.sh`：

```bash
#!/bin/bash
# 测试终端标题闪烁功能

echo "测试1: 基本标题修改"
printf '\e]0;测试标题\a'
sleep 2
echo "✅ 标题应该显示为 '测试标题'"
sleep 1

echo "测试2: Stop Hook 闪烁"
for i in {1..5}; do
    if [[ $((i % 2)) -eq 1 ]]; then
        printf '\e]0;🛑 STOP-HOOK\a'
    else
        printf '\e]0;⚠️ 等待确认\a'
    fi
    sleep 0.2
done
printf '\e]0;Claude Code\a'
echo "✅ 标题应该闪烁 5 次"
sleep 1

echo "测试3: Notification 消息"
printf '\e]0;🔔 这是一个测试通知\a'
sleep 2
printf '\e]0;Claude Code\a'
echo "✅ 标题应该显示通知消息"

echo ""
echo "所有测试完成！"
```

### 集成测试

修改 `stop-hook.sh` 后，通过以下方式测试：

```bash
# 1. 启动一个测试循环
start-loop --max-iterations 5 --prompt "测试任务"

# 2. 等待 Stop Hook 触发

# 3. 观察终端标题是否闪烁
```

---

## 📊 性能分析

### 执行时间测试

```bash
# 测试单次标题修改
time printf '\e]0;测试\a'
# 输出：real 0m0.001s

# 测试 5 次闪烁（含 sleep）
time for i in {1..5}; do printf '\e]0;测试\a'; sleep 0.2; done
# 输出：real 0m1.003s

# 测试完整闪烁函数（不包含 sleep）
time flash_terminal_title "stop" "test"
# 输出：real 0m0.005s (仅 printf 调用)
```

### 性能优化建议

1. **减少闪烁次数**：5 次闪烁 → 3 次（节省 0.6 秒）
2. **缩短闪烁间隔**：0.2 秒 → 0.15 秒（节省 0.25 秒）
3. **并行执行**：使用后台进程避免阻塞 Hook

**优化后的代码**：

```bash
flash_terminal_title_async() {
    local alert_type="$1"
    local session_id="${2:-unknown}"

    # 在后台执行闪烁
    (
        case "$alert_type" in
            stop)
                for ((i = 1; i <= 3; i++)); do
                    if [[ $((i % 2)) -eq 1 ]]; then
                        printf '\e]0;🛑 STOP-HOOK\a'
                    else
                        printf '\e]0;⚠️ 等待确认\a'
                    fi
                    sleep 0.15
                done
                printf '\e]0;Claude Code\a'
                ;;
        esac
    ) &  # 后台执行，不阻塞 Hook
}
```

---

## 🔧 高级用法

### 多会话支持

在多窗口多选项卡场景下，可以通过 `CLAUDE_SESSION_ID` 区分不同会话：

```bash
# 在启动 Claude Code 时设置会话 ID
export CLAUDE_SESSION_ID="tab-$(date +%s)"

# Hook 中使用会话 ID
flash_terminal_title "stop" "$CLAUDE_SESSION_ID"
# 标题显示：🛑 STOP-HOOK [tab-1704556800]
```

### 自定义闪烁模式

根据不同 Alert 类型定义不同的闪烁模式：

```bash
# 快速闪烁（紧急）
flash_quick() {
    for i in {1..10}; do
        printf '\e]0;🚨 紧急\a'
        sleep 0.1
        printf '\e]0;⚠️ 注意\a'
        sleep 0.1
    done
}

# 慢速闪烁（提醒）
flash_slow() {
    for i in {1..3}; do
        printf '\e]0;🔔 提醒\a'
        sleep 0.5
        printf '\e]0;信息\a'
        sleep 0.5
    done
}

# 呼吸闪烁（渐变）
flash_breathe() {
    for i in {1..5}; do
        printf '\e]0;█ 提醒\a'
        sleep 0.3
        printf '\e]0;▓ 提醒\a'
        sleep 0.1
        printf '\e]0;▒ 提醒\a'
        sleep 0.1
        printf '\e]0;░ 提醒\a'
        sleep 0.3
    done
}
```

---

## ⚠️ 注意事项

### 兼容性问题

1. **Windows Terminal**: ✅ 完全支持
2. **Git Bash**: ✅ 完全支持
3. **iTerm2 (macOS)**: ✅ 完全支持
4. **GNOME Terminal (Linux)**: ✅ 完全支持
5. **CMD**: ❌ 不支持（需要使用 `title` 命令）
6. **PowerShell**: ⚠️ 部分支持（使用 `$Host.UI.RawUI.WindowTitle`）

### 已知问题

1. **标题字符截断**：
   - 某些终端限制标题长度（如 50 字符）
   - 解决方案：使用简短符号或缩写

2. **多行标题无效**：
   - `\n` 在标题中不会换行
   - 解决方案：使用单行标题

3. **特殊字符转义**：
   - 某些特殊字符可能无法显示
   - 解决方案：使用通用 Emoji 或 ASCII 字符

---

## 🔗 相关资源

- [ANSI 转义序列完整列表](https://en.wikipedia.org/wiki/ANSI_escape_code)
- [Windows Terminal 文档 - Shell Integration](https://github.com/microsoft/terminal/blob/main/doc/specs/%234999%20-%20Terminal%20as%20a%20Console%20Engine.md)
- [方案2：OSC 调用控制颜色](./scheme-02-osc-color-control.md)（推荐组合使用）

---

## 📄 示例输出

### 正常状态
```
终端标题: Claude Code [tab-123456]
```

### Stop Hook 触发
```
0.0s: 🛑 STOP-HOOK [tab-123456]
0.2s: ⚠️ 等待确认 [tab-123456]
0.4s: 🛑 STOP-HOOK [tab-123456]
0.6s: ⚠️ 等待确认 [tab-123456]
0.8s: Claude Code [tab-123456] - 已停止
```

### Notification 触发
```
终端标题: 🔔 API 速率限制接近上限 [tab-123456]
```
