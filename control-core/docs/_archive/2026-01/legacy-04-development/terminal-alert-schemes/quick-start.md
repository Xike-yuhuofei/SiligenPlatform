# 快速实施指南

## 🚀 5分钟快速部署

### 前置条件检查

```bash
# 1. 检查终端类型
echo "WT_SESSION: ${WT_SESSION:-未设置}"
echo "TERM: $TERM"

# 2. 测试标题修改（所有终端）
printf '\e]0;测试标题\a'
# 应该看到终端标题变为 "测试标题"
sleep 2
printf '\e]0;Claude Code\a'
# 恢复正常

# 3. 测试颜色控制（仅 Windows Terminal）
printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'
# 如果是 Windows Terminal，选项卡应该变红
sleep 2
printf '\e]1337;Colors=[{"color":"#000000"}]\a'
# 恢复默认
```

**预期结果**：
- ✅ 标题修改生效 → 可使用**方案1**（标题闪烁）
- ✅ 颜色修改生效 → 可使用**方案2**（颜色控制）→ 推荐**组合方案**
- ❌ 两者都不生效 → 考虑**方案3**（Toast + 窗口闪烁）或**方案4**（独立监控）

---

## 📦 方案1：纯标题闪烁（2分钟部署）

### 步骤1：修改 stop-hook.sh

在 `stop-hook.sh` 的 `allow_exit` 函数中添加：

```bash
allow_exit() {
    local message="${1:-}"
    local session_id="${CLAUDE_SESSION_ID:-${PPID}}"

    # 标题闪烁 5 次
    for i in {1..5}; do
        if [[ $((i % 2)) -eq 1 ]]; then
            printf '\e]0;🛑 STOP-HOOK [%s]\a' "$session_id"
        else
            printf '\e]0;⚠️ 等待确认 [%s]\a' "$session_id"
        fi
        sleep 0.2
    done

    # 恢复正常标题
    printf '\e]0;Claude Code [%s] - 已停止\a' "$session_id"

    # 返回响应
    if [ -n "$message" ]; then
        echo "{\"systemMessage\": \"$message\"}"
    else
        echo '{}'
    fi
    exit 0
}
```

### 步骤2：测试

```bash
# 启动测试循环
start-loop --max-iterations 3 --prompt "测试任务"

# 等待 Stop Hook 触发，观察标题是否闪烁
```

### 完成！✅

---

## 📦 组合方案：标题 + 颜色（5分钟部署）

### 步骤1：创建 Alert 模块

```bash
# 创建模块文件
cat > .claude/plugins/loop-master/lib/alert_handler.sh <<'EOF'
#!/bin/bash
# 终端视觉提醒模块

is_windows_terminal() {
    [[ -n "${WT_SESSION:-}" ]] || [[ "${TERM_PROGRAM:-}" == "vscode" ]]
}

set_terminal_title() {
    printf '\e]0;%s\a' "$1"
}

set_tab_color() {
    if is_windows_terminal; then
        printf '\e]1337;Colors=[{"color":"%s"}]\a' "$1"
    fi
}

trigger_alert() {
    local alert_type="$1"
    local session_id="${2:-unknown}"

    case "$alert_type" in
        stop)
            # 标题闪烁 5 次
            for i in {1..5}; do
                if [[ $((i % 2)) -eq 1 ]]; then
                    set_terminal_title "🛑 STOP-HOOK [$session_id]"
                else
                    set_terminal_title "⚠️ 等待确认 [$session_id]"
                fi
                sleep 0.2
            done

            # 颜色闪烁 3 次（仅 Windows Terminal）
            if is_windows_terminal; then
                for i in {1..3}; do
                    set_tab_color "#FF0000"
                    sleep 0.3
                    set_tab_color "#000000"
                    sleep 0.3
                done
            fi

            # 恢复正常
            set_terminal_title "Claude Code [$session_id] - 已停止"
            ;;
    esac
}
EOF

chmod +x .claude/plugins/loop-master/lib/alert_handler.sh
```

### 步骤2：修改 stop-hook.sh

在 `stop-hook.sh` 顶部添加：

```bash
# 加载 Alert 模块
source "${CLAUDE_PLUGIN_ROOT}/lib/alert_handler.sh"
```

修改 `allow_exit` 函数：

```bash
allow_exit() {
    local message="${1:-}"
    local session_id="${CLAUDE_SESSION_ID:-${PPID}}"

    # 触发视觉提醒
    trigger_alert "stop" "$session_id"

    # 返回响应
    if [ -n "$message" ]; then
        echo "{\"systemMessage\": \"$message\"}"
    else
        echo '{}'
    fi
    exit 0
}
```

### 步骤3：测试

```bash
# 启动测试循环
start-loop --max-iterations 3 --prompt "测试任务"

# 等待 Stop Hook 触发，观察：
# 1. 终端标题是否闪烁 5 次
# 2. 选项卡颜色是否闪烁 3 次（仅 Windows Terminal）
```

### 完成！✅

---

## 🔧 自定义配置

### 调整闪烁次数和速度

编辑 `alert_handler.sh`，修改循环参数：

```bash
# 快速闪烁（适合紧急提醒）
for i in {1..3}; do          # 改为 3 次
    sleep 0.15              # 改为 0.15 秒间隔
done

# 慢速闪烁（适合温和提醒）
for i in {1..7}; do          # 改为 7 次
    sleep 0.3               # 改为 0.3 秒间隔
done
```

### 自定义颜色

编辑颜色值：

```bash
set_tab_color "#FF0000"    # 鲜红
set_tab_color "#FFA500"    # 橙色
set_tab_color "#FFD700"    # 金色
set_tab_color "#00FF00"    # 绿色
set_tab_color "#000000"    # 默认黑
```

### 添加声音提醒

在 `trigger_alert` 函数中添加：

```bash
trigger_alert() {
    local alert_type="$1"
    local session_id="${2:-unknown}"

    case "$alert_type" in
        stop)
            # ... 标题和颜色闪烁 ...

            # 添加响铃
            for i in {1..3}; do
                printf '\a'
                sleep 0.1
            done
            ;;
    esac
}
```

---

## 🧪 验证测试

### 单元测试

```bash
# 创建测试脚本
cat > test_alert.sh <<'EOF'
#!/bin/bash
source .claude/plugins/loop-master/lib/alert_handler.sh

echo "测试 Stop Hook 提醒..."
trigger_alert "stop" "test-session"

echo "测试完成！"
EOF

chmod +x test_alert.sh
bash test_alert.sh
```

### 集成测试

```bash
# 启动测试循环
start-loop --max-iterations 5 --prompt "测试任务"

# 观察效果
```

---

## 🐛 故障排查

### 问题1：标题不闪烁

**原因**：终端不支持 OSC 0 序列

**解决方案**：
- 检查终端类型：`echo $TERM`
- 尝试其他终端（Windows Terminal, iTerm2, GNOME Terminal）
- 考虑使用**方案3**或**方案4**

### 问题2：颜色不变

**原因**：非 Windows Terminal 或版本过低

**解决方案**：
- 检查环境变量：`echo $WT_SESSION`
- 升级 Windows Terminal 到 1.16+
- 颜色控制仅支持 Windows Terminal，其他终端会自动跳过

### 问题3：闪烁不明显

**解决方案**：
- 增加闪烁次数：`{1..5}` → `{1..7}`
- 延长闪烁间隔：`sleep 0.2` → `sleep 0.3`
- 使用更大对比度的颜色：`#FF0000`（鲜红）

### 问题4：Hook 执行时间过长

**检查方法**：
```bash
# 在 Hook 中添加计时
start_time=$(date +%s%N)
trigger_alert "stop" "test"
end_time=$(date +%s%N)
elapsed=$(( (end_time - start_time) / 1000000 ))
echo "Alert 耗时: ${elapsed}ms"
```

**解决方案**：
- 使用后台执行：将 `trigger_alert` 改为后台进程
- 减少闪烁次数
- 缩短 sleep 时间

---

## 📊 性能基准

### 预期性能指标

| 操作 | 预期时间 | 可接受范围 |
|------|---------|-----------|
| 单次标题修改 | ~1ms | <5ms |
| 单次颜色修改 | ~2ms | <10ms |
| 完整 Stop Hook 提醒 | ~8ms | <20ms |

### 性能测试

```bash
# 测试完整提醒流程
time source .claude/plugins/loop-master/lib/alert_handler.sh && \
     trigger_alert "stop" "test" &>/dev/null

# 预期输出：real 0m0.008s (约 8ms)
```

如果超过 20ms，请检查：
- 是否有其他阻塞操作
- 终端渲染性能
- 系统负载

---

## 🎯 下一步

完成基础部署后，可以考虑：

1. **扩展提醒类型**：添加 Notification, Circuit Breaker 等
2. **添加声音**：使用 `\a` 或外部音频文件
3. **Toast 通知**：集成 PowerShell 或 Python Toast 库
4. **独立监控**：实施**方案4**（状态文件 + 监控进程）

详细实现参见：
- [方案1：终端标题闪烁](./scheme-01-title-flashing.md)
- [方案2：OSC 调用控制颜色](./scheme-02-osc-color-control.md)
- [组合方案实现](./combined-solution.md)

---

## 💡 最佳实践

1. **渐进式部署**：先部署方案1，验证效果后再添加方案2
2. **性能监控**：定期检查 Hook 执行时间
3. **用户反馈**：收集团队成员对闪烁效果的意见
4. **持续优化**：根据反馈调整闪烁次数、间隔、颜色

---

## 📞 获取帮助

如遇到问题，请：
1. 查看 [调试技巧](./debugging-tips.md)
2. 检查 [代码示例](./code-examples/)
3. 提交 Issue 到项目仓库

---

**准备好开始了吗？** 选择一个方案，按照上面的步骤操作，5 分钟内即可完成部署！🚀
