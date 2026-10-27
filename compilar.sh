#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
source ./ambiente.sh
compilador="$STM32_SDK/bin/arm-buildroot-linux-gnueabihf-g++"
if [[ ! -x "$compilador" ]]; then
    printf 'Erro: compilador ARM nao encontrado em %s\n' "$compilador" >&2
    printf 'Instale o SDK do professor ou informe STM32_SDK antes de executar.\n' >&2
    exit 1
fi
if [[ ! -d "$STM32_SDK/arm-buildroot-linux-gnueabihf/sysroot" ]]; then
    printf 'Erro: SDK incompleto; pasta sysroot nao encontrada.\n' >&2
    exit 1
fi
mkdir -p build
artefato_tmp="$(mktemp build/.luminosidade.XXXXXX)"
trap 'rm -f -- "$artefato_tmp"' EXIT
"$compilador" -std=c++11 -O2 -Wall -Wextra -pthread luminosidade.cpp -o "$artefato_tmp"
chmod +x "$artefato_tmp"
mv -- "$artefato_tmp" build/luminosidade
printf '\nPrograma compilado: build/luminosidade\n'
if command -v file >/dev/null 2>&1; then
    file build/luminosidade
fi
printf 'Copie este executavel para a STM32; ele nao executa no notebook x86.\n'
