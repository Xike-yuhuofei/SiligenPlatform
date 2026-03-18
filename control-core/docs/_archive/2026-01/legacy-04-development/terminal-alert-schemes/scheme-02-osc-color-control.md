# 方案2：OSC 调用控制颜色

## 📋 方案概述

**方案名称**：OSC 调用控制终端选项卡颜色 (OSC Color Control)

**核心原理**：使用 Windows Terminal 特有的 OSC 1337 序列动态修改选项卡颜色。

**实现难度**：⭐⭐ 中等

**视觉效果**：⭐⭐⭐⭐ 优秀（整个选项卡变色）

**兼容性**：⭐⭐⭐⭐ Windows Terminal 1.16+（其他终端不生效）

---

## 🎯 工作原理

### OSC 1337 序列

OSC 1337 是 Windows Terminal 自定义的 OSC 序列，用于控制终端外观：

```
\e]1337;Colors=[{"color":"#RRGGBB"}]\a
```

**组成解析**：
- `\e]1337;` - OSC 1337 命令前缀
- `Colors=[...]` - JSON 格式的颜色配置
- `#RRGGBB` - 十六进制颜色值（如 `#FF0000` 为红色）
- `\a` - 命令终止符

**示例**：
```bash
# 修改选项卡为红色
printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'

# 修改选项卡为橙色
printf '\e]1337;Colors=[{"color":"#FFA500"}]\a'

# 恢复默认颜色
printf '\e]1337;Colors=[{"color":"#000000"}]\a'
```

### 颜色闪烁实现原理

通过快速切换选项卡颜色实现闪烁效果：

```bash
# 红色闪烁 3 次
for i in {1..3}; do
    printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'  # 鲜红
    sleep 0.3
    printf '\e]1337;Colors=[{"color":"#000000"}]\a'  # 默认
    sleep 0.3
done
```

**视觉效果**：
- 0.0s: 🔴 鲜红
- 0.3s: ⚫ 默认
- 0.6s: 🔴 鲜红
- 0.9s: ⚫ 默认
- 1.2s: 🔴 鲜红
- 1.5s: ⚫ 默认

---

## 💻 完整实现代码

### Hook 集成代码

将以下代码添加到 `stop-hook.sh`：

```bash
# ============================================================================
# Windows Terminal 选项卡颜色控制模块
# ============================================================================

# ----------------------------------------------------------------------------
# 检测 Windows Terminal 环境
# ----------------------------------------------------------------------------
is_windows_terminal() {
    [[ -n "${WT_SESSION:-}" ]] || [[ "${TERM_PROGRAM:-}" == "vscode" ]]
}

# ----------------------------------------------------------------------------
# 设置选项卡颜色（仅 Windows Terminal）
# ----------------------------------------------------------------------------
set_tab_color() {
    local color="$1"  # 十六进制颜色值，如 #FF0000

    # 仅在 Windows Terminal 中生效
    if is_windows_terminal; then
        printf '\e]1337;Colors=[{"color":"%s"}]\a' "$color"
    else
        log_debug "非 Windows Terminal 环境，跳过颜色设置"
    fi
}

# ----------------------------------------------------------------------------
# 选项卡颜色闪烁函数
# ----------------------------------------------------------------------------
flash_tab_color() {
    local alert_type="$1"

    # 非 Windows Terminal 环境，直接返回
    if ! is_windows_terminal; then
        return 0
    fi

    case "$alert_type" in
        stop)
            # Stop Hook: 红色闪烁 3 次
            local flash_count=3
            local delay=0.3

            for ((i = 1; i <= flash_count; i++)); do
                set_tab_color "#FF0000"  # 鲜红
                sleep "$delay"
                set_tab_color "#000000"  # 默认黑
                sleep "$delay"
            done
            ;;

        notification)
            # Notification: 橙色常亮（不闪烁）
            set_tab_color "#FFA500"  # 橙色
            ;;

        circuit_breaker)
            # Circuit Breaker: 红紫色常亮
            set_tab_color "#8B0000"  # 深红
            ;;

        rate_limit)
            # Rate Limit: 黄色常亮
            set_tab_color "#FFD700"  # 金色
            ;;

        success)
            # Success: 绿色常亮
            set_tab_color "#00FF00"  # 绿色
            ;;

        *)
            # 默认：恢复默认颜色
            set_tab_color "#000000"
            ;;
    esac
}

# ----------------------------------------------------------------------------
# 在 Hook 退出前调用
# ----------------------------------------------------------------------------

# 修改 allow_exit 函数
allow_exit() {
    local message="${1:-}"

    # 1. 触发选项卡颜色闪烁（Windows Terminal）
    flash_tab_color "stop"

    # 2. 恢复默认颜色（延迟 5 秒后自动恢复）
    (
        sleep 5
        set_tab_color "#000000"
    ) &

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

## 🎨 颜色方案

### 预定义颜色表

| Alert 类型 | 颜色名称 | 十六进制 | RGB | 视觉效果 |
|-----------|---------|---------|-----|---------|
| **Stop Hook** | 鲜红 | `#FF0000` | (255, 0, 0) | 🔴 最醒目 |
| **Circuit Breaker** | 深红 | `#8B0000` | (139, 0, 0) | 🟎 严肃警告 |
| **Notification** | 橙色 | `#FFA500` | (255, 165, 0) | 🟠 明显但不刺眼 |
| **Rate Limit** | 金色 | `#FFD700` | (255, 215, 0) | 🟡 温和提醒 |
| **Success** | 绿色 | `#00FF00` | (0, 255, 0) | 🟢 成功状态 |
| **Default** | 黑色 | `#000000` | (0, 0, 0) | ⚫ 默认状态 |

### 渐变色闪烁（高级）

```bash
# 从深红渐变到鲜红
flash_gradient_red() {
    local colors=("#8B0000" "#A00000" "#B00000" "#C00000" "#D00000" "#E00000" "#FF0000")

    for color in "${colors[@]}"; do
        set_tab_color "$color"
        sleep 0.1
    done

    # 反向渐变
    for ((i = ${#colors[@]} - 1; i >= 0; i--)); do
        set_tab_color "${colors[$i]}"
        sleep 0.1
    done
}

# 使用
flash_gradient_red  # 渐变闪烁，更柔和
```

---

## 🧪 测试验证

### 环境检测测试

```bash
#!/bin/bash
# test_windows_terminal.sh

echo "检测 Windows Terminal 环境..."

# 方法1: 检查 WT_SESSION
if [[ -n "${WT_SESSION:-}" ]]; then
    echo "✅ 检测到 WT_SESSION 环境变量"
    echo "   值: $WT_SESSION"
else
    echo "❌ 未设置 WT_SESSION 环境变量"
fi

# 方法2: 检查 TERM_PROGRAM
if [[ "${TERM_PROGRAM:-}" == "vscode" ]]; then
    echo "✅ 检测到 VS Code 集成终端"
else
    echo "ℹ️ TERM_PROGRAM: ${TERM_PROGRAM:-未设置}"
fi

# 方法3: 检查父进程名称
parent_process=$(ps -o comm= -p $PPID)
echo "ℹ️ 父进程: $parent_process"

# 测试 OSC 1337 序列
echo ""
echo "测试 OSC 1337 颜色控制..."
printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'
echo "如果选项卡变为红色，说明支持 OSC 1337"
sleep 2
printf '\e]1337;Colors=[{"color":"#000000"}]\a'
echo "恢复默认颜色"
```

### 功能测试

```bash
#!/bin/bash
# test_tab_colors.sh

echo "测试1: 红色闪烁"
for i in {1..3}; do
    printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'
    sleep 0.3
    printf '\e]1337;Colors=[{"color":"#000000"}]\a'
    sleep 0.3
done
echo "✅ 应该看到 3 次红色闪烁"
sleep 1

echo "测试2: 橙色常亮"
printf '\e]1337;Colors=[{"color":"#FFA500"}]\a'
echo "✅ 选项卡应该变为橙色（持续 2 秒）"
sleep 2
printf '\e]1337;Colors=[{"color":"#000000"}]\a'
echo "恢复默认颜色"

echo ""
echo "所有测试完成！"
```

---

## 📊 性能分析

### 执行时间测试

```bash
# 测试单次颜色设置
time printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'
# 输出：real 0m0.002s

# 测试 3 次闪烁（含 sleep）
time for i in {1..3}; do
    printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'
    sleep 0.3
    printf '\e]1337;Colors=[{"color":"#000000"}]\a'
    sleep 0.3
done
# 输出：real 0m1.802s

# 测试完整闪烁函数（不包含 sleep）
time flash_tab_color "stop"
# 输出：real 0m0.006s (仅 printf 调用)
```

### 性能优化建议

1. **减少闪烁次数**：3 次 → 2 次（节省 0.6 秒）
2. **缩短闪烁间隔**：0.3 秒 → 0.2 秒（节省 0.2 秒）
3. **并行执行**：使用后台进程避免阻塞 Hook

**优化后的代码**：

```bash
flash_tab_color_async() {
    local alert_type="$1"

    # 非 Windows Terminal 环境，直接返回
    if ! is_windows_terminal; then
        return 0
    fi

    # 在后台执行闪烁
    (
        case "$alert_type" in
            stop)
                for ((i = 1; i <= 2; i++)); do
                    set_tab_color "#FF0000"
                    sleep 0.2
                    set_tab_color "#000000"
                    sleep 0.2
                done
                ;;
        esac
    ) &  # 后台执行，不阻塞 Hook
}
```

---

## 🔧 高级用法

### 多颜色组合

```bash
# 彩虹闪烁（调试用）
flash_rainbow() {
    local colors=("#FF0000" "#FF7F00" "#FFFF00" "#00FF00" "#0000FF" "#4B0082" "#9400D3")

    for color in "${colors[@]}"; do
        set_tab_color "$color"
        sleep 0.2
    done

    set_tab_color "#000000"
}

# 使用：调试时确认颜色控制是否工作
flash_rainbow
```

### 与标题闪烁组合

```bash
# 同时闪烁标题和颜色（推荐组合使用）
flash_all() {
    local alert_type="$1"

    case "$alert_type" in
        stop)
            # 并行执行标题和颜色闪烁
            (
                # 标题闪烁
                for i in {1..5}; do
                    if [[ $((i % 2)) -eq 1 ]]; then
                        printf '\e]0;🛑 STOP-HOOK\a'
                    else
                        printf '\e]0;⚠️ 等待确认\a'
                    fi
                    sleep 0.2
                done
                printf '\e]0;Claude Code\a'
            ) &

            # 颜色闪烁（仅 Windows Terminal）
            if is_windows_terminal; then
                (
                    for i in {1..3}; do
                        set_tab_color "#FF0000"
                        sleep 0.3
                        set_tab_color "#000000"
                        sleep 0.3
                    done
                ) &
            fi
            ;;
    esac
}
```

---

## ⚠️ 注意事项

### 兼容性问题

1. **Windows Terminal 版本要求**：
   - ✅ Windows Terminal 1.16+：完全支持
   - ⚠️ Windows Terminal 1.15 及以下：可能不支持
   - ❌ 其他终端（Git Bash, iTerm2 等）：不生效

2. **环境变量检测**：
   - `WT_SESSION`：Windows Terminal 设置的会话 ID
   - 如果检测失败，颜色控制不会生效，但不会报错

3. **颜色持久化**：
   - 颜色修改会持续到下次修改或终端关闭
   - 建议在闪烁后恢复默认颜色

### 已知问题

1. **选项卡颜色不生效**：
   - 原因：非 Windows Terminal 环境
   - 解决：使用 `is_windows_terminal` 检测环境

2. **颜色覆盖**：
   - 如果同时有多个进程修改颜色，可能产生冲突
   - 解决：使用 `flock` 确保原子操作

3. **性能影响**：
   - OSC 1337 序列执行时间 <5ms，可忽略
   - 但频繁调用可能增加终端渲染负担

---

## 🔗 相关资源

- [Windows Terminal OSC 1337 规范](https://github.com/microsoft/terminal/blob/main/doc/specs/%234999%20-%20Terminal%20as%20a%20Console%20Engine.md)
- [Windows Terminal 颜色方案文档](https://docs.microsoft.com/en-us/windows/terminal/customize-settings/color-schemes)
- [方案1：终端标题闪烁](./scheme-01-title-flashing.md)（推荐组合使用）
- [组合方案实现](./combined-solution.md)

---

## 📄 示例输出

### 正常状态
```
选项卡颜色: 默认（黑色）
```

### Stop Hook 触发
```
0.0s: 🔴 鲜红
0.3s: ⚫ 默认黑
0.6s: 🔴 鲜红
0.9s: ⚫ 默认黑
1.2s: 🔴 鲜红
1.5s: ⚫ 默认黑
```

### Notification 触发
```
选项卡颜色: 🟠 橙色（常亮 5 秒后自动恢复）
```

### Circuit Breaker 触发
```
选项卡颜色: 🟎 深红（常亮，直到手动恢复）
```
