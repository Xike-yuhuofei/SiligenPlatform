#!/bin/bash
# Stop Hook 真实环境验证方案
#
# 此脚本提供了三种验证方法，从简单到真实环境

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$PROJECT_ROOT"

echo "========================================="
echo "  Stop Hook 真实环境验证方案"
echo "========================================="
echo ""

# ============================================================================
# 方法 1: 单元测试（验证脚本功能）
# ============================================================================
echo "【方法 1】单元测试 - 验证脚本功能"
echo "-------------------------------------------"

export CLAUDE_PROJECT_DIR="$PROJECT_ROOT"
export CLAUDE_PLUGIN_ROOT="$PROJECT_ROOT/.claude/plugins/loop-master"
export LOOP_STATE_FILE="/tmp/loop-state-verification-test.txt"

# 创建临时状态文件
cat > "$LOOP_STATE_FILE" <<EOF
active=false
iteration=0
max_iterations=1
completion_promise=测试完成
---
测试任务提示词
EOF

echo "📝 测试场景：达到最大迭代次数"
echo "📄 状态文件: $LOOP_STATE_FILE"

# 激活循环
update_loop_state "active" "true"
update_loop_state "iteration" "1"

echo ""
echo "⏳ 触发 Stop Hook..."
output=$(echo '{"hook_event_name":"Stop","session_id":"test-123"}' | \
    bash .claude/plugins/loop-master/hooks/scripts/stop-hook.sh 2>&1)

exit_code=$?
echo ""
echo "📤 退出码: $exit_code"
echo "📤 输出:"
echo "$output" | jq '.' 2>/dev/null || echo "$output"

if [[ $exit_code -eq 0 ]]; then
    echo ""
    echo "✅ 方法 1 通过：脚本执行成功"
else
    echo ""
    echo "❌ 方法 1 失败：脚本执行错误"
fi

# 清理
rm -f "$LOOP_STATE_FILE"

echo ""
echo "按 Enter 继续方法 2..."
read

# ============================================================================
# 方法 2: Loop Master 真实循环测试
# ============================================================================
echo ""
echo "【方法 2】Loop Master 真实循环测试"
echo "-------------------------------------------"
echo "📝 测试场景：启动 1 次迭代的循环"
echo ""
echo "⚠️  注意：此方法需要你手动执行以下命令："
echo ""
echo "    /start-loop --max-iterations 1 --prompt '测试 Stop Hook'"
echo ""
echo "然后观察 Stop Hook 是否正确触发："
echo "  ✅ 终端标题闪烁：🔴 STOP-HOOK | [session_id]"
echo "  ✅ 标签页背景变红"
echo "  ✅ 进度环显示错误状态"
echo "  ✅ 控制台输出：🛑 Loop Master: 达到最大迭代次数 (1)，停止循环"
echo ""
echo "按 Enter 跳过..."
read

# ============================================================================
# 方法 3: 真实 Claude Code Stop Hook 触发
# ============================================================================
echo ""
echo "【方法 3】真实 Claude Code Stop Hook 触发"
echo "-------------------------------------------"
echo "📝 测试场景：在 Claude Code 中触发 Stop Hook"
echo ""
echo "⚠️  注意：此方法需要在 Claude Code 中执行："
echo ""
echo "    1. 启动一个简单的任务（如文件读取）"
echo "    2. 使用 Ctrl+C 停止任务"
echo "    3. 观察 Stop Hook 是否触发"
echo ""
echo "预期效果："
echo "  ✅ 终端标题闪烁：🔴 STOP-HOOK"
echo "  ✅ 标签页背景变红"
echo "  ✅ 进度环显示错误状态"
echo "  ✅ 没有错误信息"
echo ""
echo "按 Enter 跳过..."
read

# ============================================================================
# 总结
# ============================================================================
echo ""
echo "========================================="
echo "  验证完成"
echo "========================================="
echo ""
echo "📋 检查清单："
echo ""
echo "  ☐ 单元测试通过（方法 1）"
echo "  ☐ Loop Master 循环测试通过（方法 2）"
echo "  ☐ 真实 Stop Hook 触发测试通过（方法 3）"
echo ""
echo "💡 提示："
echo "  - 如果方法 1 失败，检查脚本语法错误"
echo "  - 如果方法 2 失败，检查 Loop Master 配置"
echo "  - 如果方法 3 失败，检查全局 settings.json 是否冲突"
echo ""
echo "📚 相关文档："
echo "  - .claude/hooks/test-stop-hook.sh（单元测试脚本）"
echo "  - docs/04-development/terminal-alert-schemes/stop-hook-fix-summary.md（修复总结）"
echo "  - docs/04-development/terminal-alert-schemes/global-hook-fix.md（全局 Hook 解决方案）"
echo ""
