#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
if [[ -r /proc/sys/kernel/osrelease ]] && grep -qi microsoft /proc/sys/kernel/osrelease; then
    # A Ethernet pertence ao Windows. Usar Python Windows evita o NAT UDP do WSL 2.
    if command -v py.exe >/dev/null 2>&1; then
        python_windows=(py.exe -3)
    elif command -v python.exe >/dev/null 2>&1; then
        python_windows=(python.exe)
    else
        printf 'Instale Python 3 no Windows para receber pela Ethernet a partir do WSL.\n' >&2
        exit 1
    fi
    mkdir -p logs
    csv="$PWD/logs/leituras_$(date -u +%Y%m%d_%H%M%S)_$$.csv"
    exec "${python_windows[@]}" "$(wslpath -w "$PWD/servidor_udp.py")" --csv "$(wslpath -w "$csv")" "$@"
else
    exec env -u LD_LIBRARY_PATH python3 servidor_udp.py "$@"
fi
