#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${WASI_SDK:?Set WASI_SDK to the official WASI SDK directory}"
"$WASI_SDK/bin/clang" -DCAD_WASM -std=c11 -O2 -fno-builtin -ffp-contract=off -nostdlib -Ibuild/gen -Ifirmware/src/dsp firmware/src/dsp/cad.c host/wasm.c -Wl,--no-entry -Wl,--export-memory -Wl,--initial-memory=2097152 -Wl,--max-memory=2097152 -Wl,-z,stack-size=131072 -Wl,--export=cad_init -Wl,--export=cad_command -Wl,--export=web_render -Wl,--export=web_pattern -Wl,--export=web_status -Wl,--export=web_project -Wl,--export=web_project_size -Wl,--export=web_export -Wl,--export=web_import -Wl,--export=web_bank -Wl,--export=web_bank_load -o dist/engine.wasm
