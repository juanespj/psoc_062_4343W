#!/usr/bin/env sh
# Serial port monitor for the CY8CPROTO-062-4343W (KitProg3 USB-CDC).
#
# Usage:
#   ./scripts/monitor.sh              — auto-detect port, 115200 baud
#   ./scripts/monitor.sh /dev/cu.xxx  — explicit port
#   ./scripts/monitor.sh /dev/cu.xxx 9600
#
# Requires one of: screen (built-in macOS), picocom, or minicom.
# On macOS the KitProg3 UART appears as /dev/cu.usbmodem* or /dev/tty.usbmodem*.
# On Linux/WSL it appears as /dev/ttyACM* or /dev/ttyUSB*.

BAUD="${2:-115200}"

# ── port resolution ──────────────────────────────────────────────────────────
if [ -n "$1" ]; then
    PORT="$1"
else
    case "$(uname -s)" in
    Darwin)
        # Use find to avoid glob-expansion failures when no device is present
        PORT="$(find /dev -maxdepth 1 -name 'cu.usbmodem*' 2>/dev/null | head -n 1)"
        ;;
    Linux)
        PORT="$(find /dev -maxdepth 1 \( -name 'ttyACM*' -o -name 'ttyUSB*' \) 2>/dev/null | head -n 1)"
        ;;
    *)
        printf 'Unsupported OS. Pass the port explicitly: %s /dev/ttyXXX\n' "$0" >&2
        exit 1
        ;;
    esac

    if [ -z "$PORT" ]; then
        printf '\nNo USB serial port found.\n' >&2
        printf 'Available /dev/cu.* devices:\n' >&2
        find /dev -maxdepth 1 -name 'cu.*' 2>/dev/null | sort >&2 || true
        printf '\nMake sure the board is connected and try again.\n' >&2
        exit 1
    fi
fi

if [ ! -e "$PORT" ]; then
    printf 'Port not found: %s\n' "$PORT" >&2
    exit 1
fi

# ── kill any process already holding the port ────────────────────────────────
PORT_HOLDER="$(lsof -t "$PORT" 2>/dev/null)"
if [ -n "$PORT_HOLDER" ]; then
    printf 'Port %s is held by PID(s): %s\n' "$PORT" "$PORT_HOLDER"
    printf 'Killing stale session(s)...\n'
    # shellcheck disable=SC2086
    kill $PORT_HOLDER 2>/dev/null || true
    sleep 0.5
    # If still alive, force-kill
    STILL_ALIVE="$(lsof -t "$PORT" 2>/dev/null)"
    if [ -n "$STILL_ALIVE" ]; then
        # shellcheck disable=SC2086
        kill -9 $STILL_ALIVE 2>/dev/null || true
        sleep 0.3
    fi
    printf 'Done.\n\n'
fi

printf 'Connecting to %s at %s baud...\n' "$PORT" "$BAUD"
printf 'Exit: Ctrl-A then X  (picocom) | Ctrl-A then K  (screen)\n\n'

# ── pick a terminal emulator ─────────────────────────────────────────────────
if ! command -v picocom >/dev/null 2>&1; then
    if command -v screen >/dev/null 2>&1; then
        printf 'picocom not found, falling back to screen (exit: Ctrl-A then K).\n'
        exec screen "$PORT" "$BAUD"
    elif command -v minicom >/dev/null 2>&1; then
        printf 'picocom not found, falling back to minicom (exit: Ctrl-A then X).\n'
        exec minicom -D "$PORT" -b "$BAUD"
    else
        printf 'picocom not found. Install it with:\n' >&2
        printf '  brew install picocom\n' >&2
        exit 1
    fi
fi

exec picocom -b "$BAUD" "$PORT"
