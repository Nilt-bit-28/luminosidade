#!/usr/bin/env bash
# Carregar no Ubuntu: source ./ambiente.sh
export STM32_SDK="${STM32_SDK:-$HOME/stm32/sdk}"
export PATH="$STM32_SDK/bin:$PATH"
export LD_LIBRARY_PATH="$STM32_SDK/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
