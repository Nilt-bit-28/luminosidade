#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
mkdir -p build
# Gerar uma versao para o notebook. A versao ARM da placa fica separada.
env -u LD_LIBRARY_PATH /usr/bin/g++ -std=c++11 -O2 -Wall -Wextra -pthread luminosidade.cpp -o build/luminosidade_ubuntu
# Arquivo de demonstracao: substitui somente a origem da leitura do ADC.
printf '1234\n' > build/adc_simulado.txt
printf 'Teste no Ubuntu: a classe deve ler o valor 1234 tres vezes.\n\n'
env -u LD_LIBRARY_PATH ./build/luminosidade_ubuntu build/adc_simulado.txt 3
