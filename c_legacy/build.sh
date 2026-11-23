#!/usr/bin/env sh
# 编译 C 版（GCC / MinGW）
set -e
cd "$(dirname "$0")"
CC="${CC:-gcc}"
"$CC" -std=c89 -Wall -Wextra -O2 -o notebook notebook.c
echo "已生成 ./notebook"
