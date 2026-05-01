#!/usr/bin/env sh
# Zephyr + Python venv for this project (macOS zsh / WSL bash).
# Must be SOURCED so PATH and venv apply to your current shell:
#   . ./.zephyr-env.sh
#   source ./.zephyr-env.sh
# Do NOT run: ./.zephyr-env.sh  (that starts a subshell; west will not stay on PATH.)

case "$0" in
*/.zephyr-env.sh|.zephyr-env.sh|./.zephyr-env.sh)
	printf '%s\n' "Run this file with source, not execute:" >&2
	printf '  . %s\n' "./.zephyr-env.sh" >&2
	printf '  source ./.zephyr-env.sh\n' >&2
	return 1 2>/dev/null || exit 1
	;;
esac

ZEPHYR_BASE="${HOME}/zephyrproject/zephyr"
export ZEPHYR_BASE

if [ -f "${ZEPHYR_BASE}/zephyr-env.sh" ]; then
	. "${ZEPHYR_BASE}/zephyr-env.sh"
fi

ZEPHYR_VENV_BIN="${HOME}/zephyrproject/.venv/bin"
if [ -d "${ZEPHYR_VENV_BIN}" ]; then
	PATH="${ZEPHYR_VENV_BIN}${PATH:+:${PATH}}"
	export PATH
fi

VENV_ACTIVATE="${HOME}/zephyrproject/.venv/bin/activate"
if [ -f "${VENV_ACTIVATE}" ]; then
	. "${VENV_ACTIVATE}"
fi

printf '%s\n' "Zephyr env ready: ZEPHYR_BASE=${ZEPHYR_BASE}"
