#!/usr/bin/env bash
set -euo pipefail

# 用法：
#   ./cloc_subdirs_cpp.sh /path/to/target_dir
#
# 作用：
#   对目标目录下的每个一级子目录分别执行 cloc 统计
#   仅统计：
#     - C++
#     - C/C++ Header

TARGET_DIR="${1:-.}"

if ! command -v cloc >/dev/null 2>&1; then
    echo "错误：未检测到 cloc，请先安装 cloc。"
    exit 1
fi

if [[ ! -d "$TARGET_DIR" ]]; then
    echo "错误：目录不存在 -> $TARGET_DIR"
    exit 1
fi

echo "目标目录：$TARGET_DIR"
echo "统计范围：一级子文件夹"
echo "统计语言：C++, C/C++ Header"
echo

found_subdir=0

for dir in "$TARGET_DIR"/*/; do
    [[ -d "$dir" ]] || continue
    found_subdir=1

    echo "============================================================"
    echo "子目录：${dir%/}"
    echo "============================================================"

    cloc \
        --include-lang="C++,C/C++ Header" \
        "$dir"

    echo
done

if [[ "$found_subdir" -eq 0 ]]; then
    echo "提示：目录 $TARGET_DIR 下未发现一级子文件夹。"
fi