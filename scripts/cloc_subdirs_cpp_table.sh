#!/usr/bin/env bash
set -euo pipefail

TARGET_DIR="${1:-.}"

if ! command -v cloc >/dev/null 2>&1; then
    echo "错误：未检测到 cloc，请先安装 cloc。"
    exit 1
fi

if [[ ! -d "$TARGET_DIR" ]]; then
    echo "错误：目录不存在 -> $TARGET_DIR"
    exit 1
fi

tmp_file="$(mktemp)"
trap 'rm -f "$tmp_file"' EXIT

found_subdir=0
max_name_len=6
total_code=0

while IFS= read -r -d '' dir; do
    found_subdir=1
    dir_name="$(basename "$dir")"

    cpp_code="$(
        cloc --include-lang="C++" --csv --quiet "$dir" 2>/dev/null \
        | awk -F, '$2=="C++" {print $5}'
    )"

    cpp_code="${cpp_code:-0}"

    printf '%s\t%s\n' "$dir_name" "$cpp_code" >> "$tmp_file"

    if (( ${#dir_name} > max_name_len )); then
        max_name_len=${#dir_name}
    fi

    total_code=$((total_code + cpp_code))
done < <(find "$TARGET_DIR" -mindepth 1 -maxdepth 1 -type d -print0 | sort -z)

if [[ "$found_subdir" -eq 0 ]]; then
    echo "提示：目录 $TARGET_DIR 下未发现一级子文件夹。"
    exit 0
fi

printf "%-${max_name_len}s | %12s\n" "子目录" "C++代码行数"
printf "%-${max_name_len}s-+-%12s\n" "$(printf '%*s' "$max_name_len" '' | tr ' ' '-')" "------------"

while IFS=$'\t' read -r name code; do
    printf "%-${max_name_len}s | %12s\n" "$name" "$code"
done < "$tmp_file"

printf "%-${max_name_len}s-+-%12s\n" "$(printf '%*s' "$max_name_len" '' | tr ' ' '-')" "------------"
printf "%-${max_name_len}s | %12s\n" "TOTAL" "$total_code"