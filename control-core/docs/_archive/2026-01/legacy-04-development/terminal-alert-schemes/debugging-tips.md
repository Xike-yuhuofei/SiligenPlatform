# 调试技巧

## 🔍 诊断工具

### 环境检测脚本

```bash
#!/bin/bash
# diagnose_environment.sh - 终端环境诊断工具

echo "========================================="
echo "终端环境诊断报告"
echo "========================================="
echo ""

# 1. 基本环境信息
echo "1. 基本环境信息"
echo "-----------------------------------------"
echo "Shell 类型: $SHELL"
echo "终端类型: $TERM"
echo "操作系统: $(uname -a)"
echo "当前用户: $USER"
echo ""

# 2. Windows Terminal 检测
echo "2. Windows Terminal 检测"
echo "-----------------------------------------"
if [[ -n "${WT_SESSION:-}" ]]; then
    echo "✅ 检测到 Windows Terminal 环境"
    echo "   WT_SESSION: $WT_SESSION"
else
    echo "❌ 非 Windows Terminal 环境"
    echo "   颜色控制功能将不可用"
fi
echo ""

# 3. 终端能力测试
echo "3. 终端能力测试"
echo "-----------------------------------------"

# 测试标题修改
echo "测试1: 终端标题修改..."
printf '\e]0;🧪 测试标题\a'
sleep 2
echo "   如果看到 '🧪 测试标题'，说明支持 OSC 0"
printf '\e]0;Claude Code\a'
sleep 1
echo ""

# 测试颜色控制
echo "测试2: 选项卡颜色控制..."
if [[ -n "${WT_SESSION:-}" ]]; then
    printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'
    echo "   如果选项卡变为红色，说明支持 OSC 1337"
    sleep 2
    printf '\e]1337;Colors=[{"color":"#000000"}]\a'
    echo "   恢复默认颜色"
else
    echo "   跳过（非 Windows Terminal 环境）"
fi
echo ""

# 4. 转义序列测试
echo "4. 转义序列支持测试"
echo "-----------------------------------------"

# 测试 BEL 字符
echo "测试1: 终端响铃（BEL 字符）..."
read -p "   准备好后按回车..."
printf '\a'
echo "   是否听到提示音？"
echo ""

# 测试 OSC 序列
echo "测试2: OSC 序列解析..."
if printf '\e]0;OSC测试\a' 2>/dev/null; then
    echo "   ✅ OSC 序列支持正常"
else
    echo "   ❌ OSC 序列可能不支持"
fi
echo ""

# 5. 性能测试
echo "5. 性能测试"
echo "-----------------------------------------"

echo "测试1: 标题修改性能..."
start_time=$(date +%s%N)
for i in {1..100}; do
    printf '\e]0;性能测试\a' >/dev/null
done
end_time=$(date +%s%N)
elapsed=$(( (end_time - start_time) / 1000000 ))
avg_time=$(( elapsed / 100 ))
echo "   100 次标题修改总耗时: ${elapsed}ms"
echo "   平均单次耗时: ${avg_time}ms"
echo ""

if [[ -n "${WT_SESSION:-}" ]]; then
    echo "测试2: 颜色修改性能..."
    start_time=$(date +%s%N)
    for i in {1..100}; do
        printf '\e]1337;Colors=[{"color":"#FF0000"}]\a' >/dev/null
    done
    end_time=$(date +%s%N)
    elapsed=$(( (end_time - start_time) / 1000000 ))
    avg_time=$(( elapsed / 100 ))
    echo "   100 次颜色修改总耗时: ${elapsed}ms"
    echo "   平均单次耗时: ${avg_time}ms"
    echo ""

    # 恢复默认颜色
    printf '\e]1337;Colors=[{"color":"#000000"}]\a'
fi
echo ""

# 6. Hook 脚本检查
echo "6. Hook 脚本检查"
echo "-----------------------------------------"

HOOK_SCRIPT=".claude/plugins/loop-master/hooks/scripts/stop-hook.sh"
if [[ -f "$HOOK_SCRIPT" ]]; then
    echo "✅ 找到 Hook 脚本: $HOOK_SCRIPT"

    # 检查是否加载 Alert 模块
    if grep -q "alert_handler.sh" "$HOOK_SCRIPT"; then
        echo "✅ Hook 脚本已加载 Alert 模块"
    else
        echo "⚠️  Hook 脚本未加载 Alert 模块"
        echo "   建议添加: source \"\${CLAUDE_PLUGIN_ROOT}/lib/alert_handler.sh\""
    fi

    # 检查是否调用 trigger_alert
    if grep -q "trigger_alert" "$HOOK_SCRIPT"; then
        echo "✅ Hook 脚本已调用 trigger_alert 函数"
    else
        echo "⚠️  Hook 脚本未调用 trigger_alert 函数"
        echo "   建议在 allow_exit 中添加: trigger_alert \"stop\" \"\$session_id\""
    fi
else
    echo "❌ 未找到 Hook 脚本: $HOOK_SCRIPT"
fi
echo ""

# 7. 总结
echo "========================================="
echo "诊断总结"
echo "========================================="

# 计算得分
score=0
max_score=5

[[ -n "${WT_SESSION:-}" ]] && ((score++))
printf '\e]0;测试\a' 2>/dev/null && ((score++))
[[ -f "$HOOK_SCRIPT" ]] && ((score++))
grep -q "alert_handler.sh" "$HOOK_SCRIPT" 2>/dev/null && ((score++))
grep -q "trigger_alert" "$HOOK_SCRIPT" 2>/dev/null && ((score++))

echo "得分: $score / $max_score"
echo ""

if [[ $score -eq $max_score ]]; then
    echo "✅ 所有检查通过！视觉提醒功能应可正常工作。"
elif [[ $score -ge 3 ]]; then
    echo "⚠️  部分功能可用，请参考上述提示进行修复。"
else
    echo "❌ 多项检查失败，建议重新检查配置。"
fi

echo ""
echo "诊断完成！"
```

---

## 🐛 常见问题排查

### 问题1：标题不闪烁

**症状**：Stop Hook 触发时，终端标题没有任何变化。

**诊断步骤**：

```bash
# 步骤1: 验证终端支持 OSC 0
printf '\e]0;测试标题\a'
sleep 2
# 如果标题变为 "测试标题"，继续下一步
printf '\e]0;Claude Code\a'

# 步骤2: 检查 Hook 脚本是否调用 set_terminal_title
grep "set_terminal_title" .claude/plugins/loop-master/hooks/scripts/stop-hook.sh

# 步骤3: 检查是否有语法错误
bash -n .claude/plugins/loop-master/hooks/scripts/stop-hook.sh
```

**可能原因和解决方案**：

1. **终端不支持 OSC 0**
   - 解决：更换支持 ANSI 转义序列的终端（Windows Terminal, iTerm2, GNOME Terminal）

2. **Hook 脚本语法错误**
   - 解决：运行 `bash -n stop-hook.sh` 检查语法

3. **函数未调用**
   - 解决：在 `allow_exit` 中添加 `flash_terminal_title "stop" "$session_id"`

---

### 问题2：选项卡颜色不变

**症状**：Windows Terminal 中，Stop Hook 触发时选项卡颜色没有变化。

**诊断步骤**：

```bash
# 步骤1: 验证 Windows Terminal 环境
echo "WT_SESSION: ${WT_SESSION:-未设置}"

# 步骤2: 测试 OSC 1337 序列
printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'
# 如果选项卡变为红色，继续下一步
sleep 2
printf '\e]1337;Colors=[{"color":"#000000"}]\a'

# 步骤3: 检查 Windows Terminal 版本
# 打开 Windows Terminal 设置 → 关于 → 查看版本号
# 需要版本 1.16 或更高
```

**可能原因和解决方案**：

1. **非 Windows Terminal 环境**
   - 解决：颜色控制仅支持 Windows Terminal，其他终端会自动跳过

2. **Windows Terminal 版本过低**
   - 解决：升级 Windows Terminal 到 1.16+

3. **OSC 1337 序列格式错误**
   - 解决：确保 JSON 格式正确：`\e]1337;Colors=[{"color":"#RRGGBB"}]\a`

---

### 问题3：Hook 执行时间过长

**症状**：Stop Hook 触发时，有明显延迟（>1 秒）。

**诊断步骤**：

```bash
# 在 Hook 脚本中添加性能计时
# 在 allow_exit 函数开头添加：
start_time=$(date +%s%N)

# 在 allow_exit 函数结尾添加：
end_time=$(date +%s%N)
elapsed=$(( (end_time - start_time) / 1000000 ))
echo "Hook 耗时: ${elapsed}ms" >&2
```

**可能原因和解决方案**：

1. **闪烁次数过多**
   - 解决：减少闪烁次数：`{1..5}` → `{1..3}`

2. **sleep 时间过长**
   - 解决：缩短间隔：`sleep 0.3` → `sleep 0.15`

3. **未使用后台执行**
   - 解决：将闪烁逻辑放到后台：`(flash_terminal_title ...) &`

---

### 问题4：声音不播放

**症状**：Stop Hook 触发时，没有听到提示音。

**诊断步骤**：

```bash
# 测试终端响铃
printf '\a'

# 检查终端声音设置
# Windows Terminal: 设置 → 默认值 → 贝铃
# Git Bash: Options → Sounds → Silence bell (取消勾选)
```

**可能原因和解决方案**：

1. **终端声音被禁用**
   - 解决：启用终端声音设置

2. **系统音量为 0**
   - 解决：调高系统音量

3. **printf '\a' 不支持**
   - 解决：使用外部音频工具（如 `apkg`, `paplay`）

---

## 🔧 调试模式

### 启用详细日志

在 Hook 脚本中添加调试输出：

```bash
# 在 stop-hook.sh 开头添加
set -x  # 启用命令追踪

# 或者添加自定义调试日志
log_debug() {
    if [[ "${LOOP_MASTER_DEBUG:-false}" == "true" ]]; then
        echo "[DEBUG] $*" >&2
    fi
}

# 使用
log_debug "触发 Stop Hook 提醒"
log_debug "会话 ID: $session_id"
log_debug "提醒类型: $alert_type"
```

### 测试单个函数

```bash
# 加载 Alert 模块
source .claude/plugins/loop-master/lib/alert_handler.sh

# 测试环境检测
if is_windows_terminal; then
    echo "✅ Windows Terminal 环境"
else
    echo "❌ 非 Windows Terminal 环境"
fi

# 测试标题修改
set_terminal_title "测试标题"
sleep 2
set_terminal_title "Claude Code"

# 测试颜色修改（仅 Windows Terminal）
set_tab_color "#FF0000"
sleep 2
set_tab_color "#000000"

# 测试完整提醒
trigger_alert "stop" "test-session"
```

---

## 📊 性能分析

### Hook 执行时间分析

```bash
#!/bin/bash
# benchmark_hook.sh

source .claude/plugins/loop-master/lib/alert_handler.sh

echo "性能基准测试"
echo "================"

# 测试1: 标题修改性能
echo "测试1: 标题修改 (1000 次)"
start=$(date +%s%N)
for i in {1..1000}; do
    set_terminal_title "性能测试" >/dev/null
done
end=$(date +%s%N)
elapsed=$(( (end - start) / 1000000 ))
avg=$(( elapsed / 1000 ))
echo "  总耗时: ${elapsed}ms"
echo "  平均: ${avg}μs/次"
echo ""

# 测试2: 颜色修改性能（仅 Windows Terminal）
if is_windows_terminal; then
    echo "测试2: 颜色修改 (1000 次)"
    start=$(date +%s%N)
    for i in {1..1000}; do
        set_tab_color "#FF0000" >/dev/null
    done
    end=$(date +%s%N)
    elapsed=$(( (end - start) / 1000000 ))
    avg=$(( elapsed / 1000 ))
    echo "  总耗时: ${elapsed}ms"
    echo "  平均: ${avg}μs/次"
    echo ""

    # 恢复默认颜色
    set_tab_color "#000000"
fi

# 测试3: 完整提醒流程
echo "测试3: 完整 Stop Hook 提醒 (100 次)"
start=$(date +%s%N)
for i in {1..100}; do
    trigger_alert "stop" "test" >/dev/null 2>&1
done
end=$(date +%s%N)
elapsed=$(( (end - start) / 1000000 ))
avg=$(( elapsed / 100 ))
echo "  总耗时: ${elapsed}ms"
echo "  平均: ${avg}ms/次"
echo ""

echo "基准测试完成！"
```

**预期结果**：
- 标题修改：<5μs/次
- 颜色修改：<10μs/次
- 完整提醒：<20ms/次

---

## 🎯 验收测试

### 完整验收清单

```bash
#!/bin/bash
# acceptance_test.sh

source .claude/plugins/loop-master/lib/alert_handler.sh

echo "验收测试清单"
echo "================"
echo ""

pass=0
total=0

# 测试1: 环境检测
((total++))
if is_windows_terminal; then
    echo "✅ 测试1: Windows Terminal 环境检测"
    ((pass++))
else
    echo "⚠️  测试1: 非 Windows Terminal 环境（颜色控制将跳过）"
    ((pass++))
fi
echo ""

# 测试2: 标题修改
((total++))
printf '\e]0;验收测试\a'
sleep 1
if [[ $? -eq 0 ]]; then
    echo "✅ 测试2: 标题修改功能正常"
    ((pass++))
else
    echo "❌ 测试2: 标题修改功能异常"
fi
printf '\e]0;Claude Code\a'
echo ""

# 测试3: 颜色修改
((total++))
if is_windows_terminal; then
    printf '\e]1337;Colors=[{"color":"#FF0000"}]\a'
    sleep 1
    if [[ $? -eq 0 ]]; then
        echo "✅ 测试3: 颜色修改功能正常"
        ((pass++))
    else
        echo "❌ 测试3: 颜色修改功能异常"
    fi
    printf '\e]1337;Colors=[{"color":"#000000"}]\a'
else
    echo "⏭️  测试3: 跳过（非 Windows Terminal 环境）"
    ((pass++))
fi
echo ""

# 测试4: Stop Hook 提醒
((total++))
echo "测试4: Stop Hook 提醒（请观察效果）..."
trigger_alert "stop" "acceptance-test"
if [[ $? -eq 0 ]]; then
    echo "✅ 测试4: Stop Hook 提醒功能正常"
    ((pass++))
else
    echo "❌ 测试4: Stop Hook 提醒功能异常"
fi
echo ""

# 测试5: Notification 提醒
((total++))
echo "测试5: Notification 提醒（请观察效果）..."
trigger_alert "notification" "acceptance-test" "验收测试通知"
if [[ $? -eq 0 ]]; then
    echo "✅ 测试5: Notification 提醒功能正常"
    ((pass++))
else
    echo "❌ 测试5: Notification 提醒功能异常"
fi
echo ""

# 总结
echo "================"
echo "测试结果: $pass / $total 通过"
echo ""

if [[ $pass -eq $total ]]; then
    echo "✅ 所有测试通过！验收成功。"
    exit 0
elif [[ $pass -ge $((total * 80 / 100)) ]]; then
    echo "⚠️  大部分测试通过，建议修复失败项。"
    exit 1
else
    echo "❌ 多项测试失败，请重新检查配置。"
    exit 1
fi
```

---

## 📞 获取帮助

如果以上调试技巧无法解决问题，请：

1. 运行诊断脚本并保存输出：
   ```bash
   bash diagnose_environment.sh > diagnosis.txt 2>&1
   ```

2. 检查 Hook 日志：
   ```bash
   # 查看 Loop Master 日志
   cat .claude/plugins/loop-master/logs/*.log
   ```

3. 提交 Issue，包含：
   - 诊断脚本输出
   - Hook 日志
   - 终端类型和版本
   - 预期行为和实际行为

---

## 🔗 相关资源

- [快速实施指南](./quick-start.md)
- [方案1：终端标题闪烁](./scheme-01-title-flashing.md)
- [方案2：OSC 调用控制颜色](./scheme-02-osc-color-control.md)
- [代码示例](./code-examples/)
