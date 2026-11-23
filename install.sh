#!/usr/bin/env bash
# 墨记 MoJi 一键安装（Linux / macOS）
# 用法：
#   bash install.sh
#   bash install.sh --no-gui     # 只装终端版
#   bash install.sh --no-tui     # 只装图形版
set -euo pipefail

cd "$(dirname "$0")"

NO_GUI=0
NO_TUI=0
for arg in "$@"; do
    case "$arg" in
        --no-gui) NO_GUI=1 ;;
        --no-tui) NO_TUI=1 ;;
        *) echo "未知参数：$arg"; exit 2 ;;
    esac
done

PYTHON="${PYTHON:-python3}"
if ! command -v "$PYTHON" >/dev/null 2>&1; then
    echo "未找到 $PYTHON，请先安装 Python 3.9+。"
    exit 1
fi

if [ "$NO_GUI" = "1" ]; then
    EXTRAS="tui"
elif [ "$NO_TUI" = "1" ]; then
    EXTRAS="gui"
else
    EXTRAS="all"
fi

echo "使用 Python：$("$PYTHON" --version)"
"$PYTHON" -m pip install --upgrade pip setuptools wheel

PIP_USER=""
if [ -z "${VIRTUAL_ENV:-}" ]; then
    PIP_USER="--user"
fi

echo "安装 moji-note[$EXTRAS] ..."
# shellcheck disable=SC2086
"$PYTHON" -m pip install $PIP_USER ".[$EXTRAS]"

if [ -n "${VIRTUAL_ENV:-}" ]; then
    SCRIPTS="$VIRTUAL_ENV/bin"
    echo "已安装到虚拟环境：$SCRIPTS"
else
    SCRIPTS="$("$PYTHON" -c 'import sysconfig; print(sysconfig.get_path("scripts", "posix_user"))')"
    case ":$PATH:" in
        *":$SCRIPTS:"*) ;;
        *)
            PROFILE="$HOME/.profile"
            if [ -n "${BASH_VERSION:-}" ]; then
                PROFILE="$HOME/.bashrc"
            elif [ -n "${ZSH_VERSION:-}" ] || [ "$(basename "${SHELL:-}")" = "zsh" ]; then
                PROFILE="$HOME/.zshrc"
            fi
            if ! grep -qs "moji-note PATH" "$PROFILE" 2>/dev/null; then
                {
                    echo ""
                    echo "# moji-note PATH"
                    echo "export PATH=\"$SCRIPTS:\$PATH\""
                } >> "$PROFILE"
                echo "已把 $SCRIPTS 写入 $PROFILE，重新打开终端后生效。"
            fi
            export PATH="$SCRIPTS:$PATH"
            ;;
    esac
fi

if command -v moji >/dev/null 2>&1; then
    moji --version
    echo ""
    echo "安装完成！输入 moji 进入 TUI，输入 moji gui 打开图形界面。"
else
    echo "安装完成，但 moji 暂不在 PATH 中；可先执行：$SCRIPTS/moji --version"
fi
