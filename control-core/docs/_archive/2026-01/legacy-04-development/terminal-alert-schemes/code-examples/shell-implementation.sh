#!/bin/bash
# ============================================================================
# 终端视觉提醒模块 - 完整 Shell 实现
# ============================================================================
# 版本：1.0.0
# 作者：Claude Code
# 描述：提供终端标题闪烁、选项卡颜色控制、声音提醒功能
#
# 功能：
#   1. 终端标题闪烁（所有终端）
#   2. 选项卡颜色控制（Windows Terminal 专属）
#   3. 声音提醒（可选）
#
# 依赖：
#   - 无外部依赖
#   - Windows Terminal 1.16+ (用于颜色控制)
# ============================================================================

# ----------------------------------------------------------------------------
# 环境检测
# ----------------------------------------------------------------------------

is_windows_terminal() {
    [[ -n "${WT_SESSION:-}" ]] || [[ "${TERM_PROGRAM:-}" == "vscode" ]]
}

# ----------------------------------------------------------------------------
# 基础控制函数
# ----------------------------------------------------------------------------

set_terminal_title() {
    local title="$1"
    printf '\e]0;%s\a' "$title"
}

set_tab_color() {
    local color="$1"

    if is_windows_terminal; then
        printf '\e]1337;Colors=[{"color":"%s"}]\a' "$color"
    fi
}

# ----------------------------------------------------------------------------
# 颜色和标题定义
# ----------------------------------------------------------------------------

declare -A ALERT_COLORS=(
    [stop]="#FF0000"
    [circuit_breaker]="#8B0000"
    [notification]="#FFA500"
    [rate_limit]="#FFD700"
    [success]="#00FF00"
    [default]="#000000"
)

declare -A ALERT_TITLES=(
    [stop]="🛑 STOP-HOOK"
    [circuit_breaker]="🚨 熔断器触发"
    [notification]="🔔 通知"
    [rate_limit]="⏸️ 速率限制"
    [success]="✅ 成功"
)

# ----------------------------------------------------------------------------
# 闪烁函数
# ----------------------------------------------------------------------------

flash_terminal_title() {
    local alert_type="$1"
    local session_id="${2:-unknown}"
    local flash_count=${3:-5}
    local delay=${4:-0.2}

    local title_pattern="${ALERT_TITLES[$alert_type]:-⚠️ 通知}"

    for ((i = 1; i <= flash_count; i++)); do
        if [[ $((i % 2)) -eq 1 ]]; then
            set_terminal_title "$title_pattern [$session_id]"
        else
            set_terminal_title "⚠️ 等待确认 [$session_id]"
        fi
        sleep "$delay"
    done
}

flash_tab_color() {
    local alert_type="$1"
    local flash_count=${2:-3}
    local delay=${3:-0.3}

    if ! is_windows_terminal; then
        return 0
    fi

    local color="${ALERT_COLORS[$alert_type]:-${ALERT_COLORS[default]}}"

    for ((i = 1; i <= flash_count; i++)); do
        set_tab_color "$color"
        sleep "$delay"
        set_tab_color "${ALERT_COLORS[default]}"
        sleep "$delay"
    done
}

# ----------------------------------------------------------------------------
# 声音提醒
# ----------------------------------------------------------------------------

play_alert_sound() {
    local alert_type="$1"

    case "$alert_type" in
        stop|circuit_breaker)
            for i in {1..3}; do
                printf '\a'
                sleep 0.1
            done
            ;;
        notification|rate_limit)
            printf '\a'
            ;;
        *)
            :
            ;;
    esac
}

# ----------------------------------------------------------------------------
# 综合提醒函数
# ----------------------------------------------------------------------------

trigger_alert() {
    local alert_type="$1"
    local session_id="${2:-unknown}"
    local message="${3:-}"

    case "$alert_type" in
        stop)
            flash_terminal_title "stop" "$session_id" 5 0.2
            flash_tab_color "stop" 3 0.3
            play_alert_sound "stop"
            set_terminal_title "Claude Code [$session_id] - 已停止"
            ;;

        circuit_breaker)
            flash_terminal_title "circuit_breaker" "$session_id" 7 0.15
            set_tab_color "circuit_breaker"
            play_alert_sound "circuit_breaker"
            set_terminal_title "Claude Code [$session_id] - 熔断器触发"
            ;;

        notification)
            if [[ -n "$message" ]]; then
                set_terminal_title "🔔 $message [$session_id]"
            else
                set_terminal_title "🔔 通知 [$session_id]"
            fi
            set_tab_color "notification"
            play_alert_sound "notification"

            (
                sleep 5
                set_terminal_title "Claude Code [$session_id]"
                set_tab_color "default"
            ) &
            ;;

        rate_limit)
            set_terminal_title "⏸️ 速率限制 [$session_id]"
            set_tab_color "rate_limit"
            play_alert_sound "rate_limit"

            (
                sleep 5
                set_terminal_title "Claude Code [$session_id]"
                set_tab_color "default"
            ) &
            ;;

        success)
            set_terminal_title "✅ 成功 [$session_id]"
            set_tab_color "success"

            (
                sleep 3
                set_terminal_title "Claude Code [$session_id]"
                set_tab_color "default"
            ) &
            ;;

        *)
            flash_terminal_title "notification" "$session_id" 3 0.3
            play_alert_sound "notification"
            ;;
    esac
}

# ----------------------------------------------------------------------------
# 导出函数
# ----------------------------------------------------------------------------
export -f is_windows_terminal
export -f set_terminal_title
export -f set_tab_color
export -f flash_terminal_title
export -f flash_tab_color
export -f play_alert_sound
export -f trigger_alert
