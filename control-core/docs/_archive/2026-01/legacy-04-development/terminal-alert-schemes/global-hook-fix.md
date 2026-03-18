# 全局 Stop Hook 冲突解决方案

## 当前状态

**问题：** 两个 Stop Hook 同时触发

1. **项目级 Hook** (推荐保留):
   - 位置: `.claude/settings.json`
   - 功能: Loop Master Stop Hook + 视觉警报
   - 状态: ✅ 已修复并集成视觉警报

2. **全局 Hook** (建议禁用):
   - 位置: `C:\Users\Xike\.claude\settings.json`
   - 功能: PowerShell 通知
   - 问题: 与项目级 Hook 冲突

## 解决方案

### 方案 A：禁用全局 Stop Hook（推荐）

编辑 `C:\Users\Xike\.claude\settings.json`，注释掉 Stop Hook：

```json
{
  "hooks": {
    "SessionStart": [...],
    "Notification": [...],
    // "Stop": [              ← 注释掉 Stop Hook
    //   {
    //     "hooks": [
    //       {
    //         "type": "command",
    //         "command": "powershell.exe -ExecutionPolicy Bypass -File C:/Users/Xike/.claude/notify-with-sound.ps1 -Title 'Claude Code' -Message '任务完成 ✅' -Sound 'C:/Windows/Media/tada.wav'"
    //       }
    //     ]
    //   }
    // ],
    "SessionEnd": [...]
  }
}
```

### 方案 B：集成到项目级 Hook

如果你希望保留 PowerShell 通知，可以将它集成到项目级 `stop-hook.sh` 的 `allow_exit()` 函数中：

```bash
allow_exit() {
    local message="${1:-}"

    # 触发视觉警报
    local wt_attn_script="${CLAUDE_PROJECT_DIR:-}/.claude/hooks/wt-attn.sh"
    if [[ -f "$wt_attn_script" ]] && [[ -n "${HOOK_INPUT:-}" ]]; then
        echo "$HOOK_INPUT" | bash "$wt_attn_script" >/dev/null 2>&1 || true
    fi

    # 触发 PowerShell 通知（保留原有功能）
    powershell.exe -ExecutionPolicy Bypass -File \
        C:/Users/Xike/.claude/notify-with-sound.ps1 \
        -Title 'Claude Code' \
        -Message '任务完成 ✅' \
        -Sound 'C:/Windows/Media/tada.wav' \
        >/dev/null 2>&1 || true

    if [ -n "$message" ]; then
        echo "{\"systemMessage\": \"$message\"}"
    else
        echo '{}'
    fi
    exit 0
}
```

## 立即验证

在解决冲突后，执行以下命令验证：

```bash
# 手动测试 stop-hook.sh
export CLAUDE_PROJECT_DIR="$PWD"
export CLAUDE_PLUGIN_ROOT="$PWD/.claude/plugins/loop-master"
echo '{"hook_event_name":"Stop","session_id":"test-123"}' | \
    bash .claude/plugins/loop-master/hooks/scripts/stop-hook.sh
```

预期效果：
- ✅ 没有错误信息
- ✅ 终端标题闪烁：🔴 STOP-HOOK
- ✅ 标签页背景变红
- ✅ 进度环显示错误状态
