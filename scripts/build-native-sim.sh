#!/usr/bin/env sh
# Build this app for native_sim. Zephyr only supports POSIX/native_sim on Linux
# (including WSL), not on macOS.

set -e

case "$(uname -s)" in
Darwin)
	printf '%s\n' "native_sim is not supported on macOS — Zephyr POSIX is Linux-only." >&2
	printf '%s\n' "Use WSL Ubuntu or Linux, then:" >&2
	printf '  cd "%s"\n' "$(dirname "$0")/.." >&2
	printf '  west build -p always -b native_sim\n' >&2
	printf '  west build -t run\n' >&2
	exit 1
	;;
esac

cd "$(dirname "$0")/.."
exec west build -p always -b native_sim "$@"
