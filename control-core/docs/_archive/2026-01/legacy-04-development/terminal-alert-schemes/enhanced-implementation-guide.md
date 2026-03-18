# 终端视觉提醒增强版 - 部署指南

## 📋 概述

本文档提供了完整的"终极组合方案"部署指南，该方案集成了三个 OSC 控制序列：

1. **OSC 0** - 终端标题闪烁（所有终端）
2. **OSC 6;1;bg** - 标签页背景色控制（Windows Terminal）
3. **OSC 9;4** - 进度环/任务栏状态（Windows Terminal）

## 🎯 效果预览

当 Claude Code 触发 Stop Hook 或 Notification Hook 时：

- ✅ **标签页背景**变为红色（或其他警示色）
- ✅ **标题闪烁**，显示 🛑 或 🚨 图标
- ✅ **进度环**显示为红色 Error 状态
- ✅ **任务栏图标**下方出现红色进度条
- ✅ 可选的**声音提醒**

## 📦 文件清单

已创建的文件：

```
.claude/plugins/loop-master/lib/
  └── alert_handler_enhanced.sh          # 增强版 Alert 模块（核心）

.claude/hooks/
  ├── wt-attn.sh                          # Hook 集成脚本
  └── wt-attn-auto-recovery.sh           # 自动恢复配置
```

## 🚀 快速部署（3分钟）

### 步骤 1：验证环境（30秒）

```bash
# 1. 检查终端类型
echo "WT_SESSION: ${WT_SESSION:-未设置}"

# 2. 测试 OSC 序列支持
printf '\e]0;测试标题\a'
sleep 2
printf '\e]0;Claude Code\a'

# 3. 测试颜色控制（仅 Windows Terminal）
if [[ -n "${WT_SESSION:-}" ]]; then
    printf '\e]6;1;bg;red;brightness;255\a'
    printf '\e]6;1;bg;green;brightness;0\a'
    printf '\e]6;1;bg;blue;brightness;0\a'
    sleep 2
    printf '\e]6;1;bg;red;brightness;default\a'
    printf '\e]6;1;bg;green;brightness;default\a'
    printf '\e]6;1;bg;blue;brightness;default\a'
fi
```

**预期结果**：
- 标题变为 "测试标题" → 支持 OSC 0 ✅
- 选项卡背景变红（仅 Windows Terminal）→ 支持 OSC 6;1;bg ✅

### 步骤 2：添加可执行权限（10秒）

```bash
chmod +x .claude/hooks/wt-attn.sh
chmod +x .claude/hooks/wt-attn-auto-recovery.sh
chmod +x .claude/plugins/loop-master/lib/alert_handler_enhanced.sh
```

### 步骤 3：配置 Claude Code Hook（1分钟）

编辑 `.claude/settings.json`，添加 Hook 配置：

```json
{
  "hooks": {
    "Stop": [
      {
        "type": "command",
        "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/wt-attn.sh"
      }
    ],
    "Notification": [
      {
        "type": "command",
        "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/wt-attn.sh"
      }
    ]
  }
}
```

**注意**：如果已有其他 Hook，将 `wt-attn.sh` 添加到 hooks 数组中。

### 步骤 4：启用自动恢复（1分钟）

在 `~/.bashrc` 中添加以下内容：

```bash
# Claude Code 终端提醒自动恢复
if [[ -f "$HOME/.claude/hooks/wt-attn-auto-recovery.sh" ]]; then
    source "$HOME/.claude/hooks/wt-attn-auto-recovery.sh"
fi
```

或者如果配置在项目级别：

```bash
# Claude Code 终端提醒自动恢复
if [[ -f "$CLAUDE_PROJECT_DIR/.claude/hooks/wt-attn-auto-recovery.sh" ]]; then
    source "$CLAUDE_PROJECT_DIR/.claude/hooks/wt-attn-auto-recovery.sh"
fi
```

重新加载配置：

```bash
source ~/.bashrc
```

### 步骤 5：测试（30秒）

```bash
# 启动测试循环
start-loop --max-iterations 3 --prompt "测试视觉提醒"

# 观察效果：
# 1. 循环结束时，选项卡背景应变为红色
# 2. 标题应显示 "🔴 STOP-HOOK"
# 3. 进度环应显示红色 Error 状态
# 4. 任务栏图标下方应有红色进度条
```

手动清除提醒：

```bash
cclear          # 清除视觉提醒
cclear status   # 查看状态
```

## 🔧 集成到现有的 Loop Master

如果已经在使用 Loop Master 的 `stop-hook.sh`，可以直接集成 Alert Handler：

### 方式 1：直接调用 trigger_alert

编辑 `.claude/plugins/loop-master/hooks/scripts/stop-hook.sh`：

在文件顶部添加加载：

```bash
# 加载增强版 Alert Handler
source "${CLAUDE_PLUGIN_ROOT}/../lib/alert_handler_enhanced.sh"
```

在 `allow_exit` 函数中添加提醒：

```bash
allow_exit() {
    local message="${1:-}"
    local session_id="${CLAUDE_SESSION_ID:-${PPID}}"

    # 根据消息判断提醒类型
    local alert_type="stop"
    if [[ "$message" =~ 熔断器 ]]; then
        alert_type="circuit_breaker"
    elif [[ "$message" =~ 速率限制 ]]; then
        alert_type="rate_limit"
    elif [[ "$message" =~ Promise.*完成|成功 ]]; then
        alert_type="success"
    fi

    # 触发视觉提醒
    trigger_alert "$alert_type" "$session_id" "" "false"

    # 返回响应
    if [ -n "$message" ]; then
        echo "{\"systemMessage\": \"$message\"}"
    else
        echo '{}'
    fi
    exit 0
}
```

### 方式 2：使用 wt-attn.sh 作为独立 Hook

保持 `stop-hook.sh` 不变，额外配置 `.claude/settings.json`：

```json
{
  "hooks": {
    "Stop": [
      {
        "type": "command",
        "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/wt-attn.sh"
      }
    ]
  }
}
```

**优点**：解耦，不影响现有逻辑。

## 🎨 自定义配置

### 调整颜色

编辑 `alert_handler_enhanced.sh` 中的颜色定义：

```bash
declare -A ALERT_COLORS=(
    [stop]="#FF0000"              # 改为其他颜色，如 #FF6600（橙色）
    [circuit_breaker]="#8B0000"   # 深红色
    [notification]="#FFA500"      # 橙色
    # ...
)
```

### 调整闪烁次数和速度

编辑 `trigger_alert` 函数中的参数：

```bash
# Stop Hook: 标题闪烁 5 次，间隔 0.2 秒
flash_terminal_title "stop" "$session_id" 5 0.2

# 标签页背景闪烁 3 次，间隔 0.3 秒
flash_tab_background "stop" 3 0.3
```

### 禁用声音提醒

编辑 `trigger_alert` 函数，注释掉 `play_alert_sound` 调用：

```bash
# play_alert_sound "stop"  # 注释掉此行
```

### 自定义自动恢复时间

编辑 `wt-attn-auto-recovery.sh`，修改超时时间：

```bash
# 从 5 秒改为 10 秒
if [[ $elapsed -gt 10 ]]; then
    reset_claude_terminal
fi
```

## 🐛 故障排查

### 问题 1：标题不闪烁

**诊断**：

```bash
# 测试 OSC 0 支持
printf '\e]0;测试标题\a'
```

**解决方案**：
- 确认终端支持 ANSI 转义序列
- 检查 Windows Terminal 设置中是否禁用了 "suppressApplicationTitle"
- 尝试其他终端（Windows Terminal, iTerm2, GNOME Terminal）

### 问题 2：颜色不变

**诊断**：

```bash
# 检查 Windows Terminal 环境
echo "WT_SESSION: ${WT_SESSION:-未设置}"

# 测试 OSC 6;1;bg
printf '\e]6;1;bg;red;brightness;255\a'
```

**解决方案**：
- 确认使用的是 Windows Terminal（非 Git Bash 默认终端）
- 升级 Windows Terminal 到 1.16+
- 颜色控制仅支持 Windows Terminal，其他终端会自动跳过

### 问题 3：进度环不显示

**诊断**：

```bash
# 测试 OSC 9;4
printf '\e]9;4;2;100\a'  # Error 状态
```

**解决方案**：
- 进度环功能仅支持 Windows Terminal
- 确认 Windows Terminal 版本 >= 1.16

### 问题 4：Hook 不执行

**诊断**：

```bash
# 测试 Hook 脚本
echo '{"hook_event_name":"Stop","cwd":"/tmp"}' | .claude/hooks/wt-attn.sh
```

**解决方案**：
- 检查 `.claude/settings.json` 中的路径是否正确
- 确认脚本有可执行权限：`chmod +x .claude/hooks/wt-attn.sh`
- 查看 Claude Code 日志，确认 Hook 是否被调用

### 问题 5：自动恢复不工作

**诊断**：

```bash
# 检查 PROMPT_COMMAND
echo "$PROMPT_COMMAND"

# 手动测试恢复函数
reset_claude_terminal
```

**解决方案**：
- 确认 `wt-attn-auto-recovery.sh` 已被 source
- 检查 `~/.bashrc` 中的配置是否正确
- 重新加载配置：`source ~/.bashrc`

## 📊 性能影响

| 操作 | 预期时间 | 可接受范围 |
|------|---------|-----------|
| 单次标题修改 | ~1ms | <5ms |
| 单次背景色修改 | ~2ms | <10ms |
| 单次进度环修改 | ~1ms | <5ms |
| 完整 Stop Hook 提醒 | ~12ms | <30ms |
| 自动恢复检查 | <1ms | <5ms |

性能测试：

```bash
# 测试完整提醒流程
time source .claude/plugins/loop-master/lib/alert_handler_enhanced.sh && \
     trigger_alert "stop" "test" "" "false" &>/dev/null

# 预期输出：real 0m0.012s (约 12ms)
```

## 🎯 高级用法

### 场景 1：区分不同类型的 Stop

根据不同的停止原因显示不同的颜色和图标：

```bash
# 在 stop-hook.sh 中
allow_exit() {
    local message="${1:-}"
    local alert_type="stop"

    case "$message" in
        *熔断器*)
            alert_type="circuit_breaker"
            ;;
        *速率限制*)
            alert_type="rate_limit"
            ;;
        *Promise.*完成*)
            alert_type="success"
            ;;
        *)
            alert_type="stop"
            ;;
    esac

    trigger_alert "$alert_type" "$session_id" "" "false"
}
```

### 场景 2：自定义标题前缀

根据项目名称显示不同的标题：

```bash
# 在 wt-attn.sh 中
local repo_name="$(get_repo_name "$cwd")"
set_terminal_title "🔴 [$repo_name] Stop Hook | [$session_id]"
```

### 场景 3：多项目管理

如果同时管理多个项目，可以为每个项目设置不同的颜色：

```bash
# 在 alert_handler_enhanced.sh 中添加
declare -A PROJECT_COLORS=(
    ["project-a"]="#FF0000"  # 红色
    ["project-b"]="#00FF00"  # 绿色
    ["project-c"]="#0000FF"  # 蓝色
)

get_project_color() {
    local project_name="$1"
    echo "${PROJECT_COLORS[$project_name]:-#FF0000}"
}
```

## 📞 获取帮助

如果遇到问题：

1. 查看 [调试技巧](./debugging-tips.md)
2. 运行诊断脚本（如果提供）
3. 检查 [快速实施指南](./quick-start.md)
4. 提交 Issue 到项目仓库

---

**祝你使用愉快！🎉**

如有任何问题或建议，欢迎反馈。
