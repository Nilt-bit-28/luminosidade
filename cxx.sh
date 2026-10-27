#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/ambiente.sh"
exec "$STM32_SDK/bin/arm-buildroot-linux-gnueabihf-g++" "$@"
