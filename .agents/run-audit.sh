#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

to_windows_path() {
    local input="$1"

    if command -v cygpath >/dev/null 2>&1; then
        cygpath -aw "$input"
        return
    fi

    case "$input" in
        /*)
            printf '%s\n' "$input"
            ;;
        *)
            printf '%s\\%s\n' "$(pwd -W)" "${input#./}" | sed 's#/#\\#g'
            ;;
    esac
}

ps_script="$(to_windows_path "$script_dir/run-audit.ps1")"

args=()
expect_module_path=0

for arg in "$@"; do
    if [[ "$expect_module_path" -eq 1 ]]; then
        args+=("$(to_windows_path "$arg")")
        expect_module_path=0
        continue
    fi

    args+=("$arg")
    if [[ "$arg" == "-ModulePath" ]]; then
        expect_module_path=1
    fi
done

exec pwsh -NoProfile -File "$ps_script" "${args[@]}"
