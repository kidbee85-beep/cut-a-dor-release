#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
CC=${CC:-clang}
FLAGS="-DEN_HOST -std=c11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -ffp-contract=off -Ibuild/gen -Ifirmware/src -Ifirmware/src/dsp"
if [ "$(uname -s)" = Darwin ] && [ -d /Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk ]; then
 FLAGS="$FLAGS -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk"
fi
for name in core editor midi step mixer audit save; do
 $CC $FLAGS tests/${name}_test.c firmware/src/dsp/cad.c -o build/$name-test
 build/$name-test
done
for name in storage panel; do
 $CC $FLAGS tests/${name}_test.c -o build/$name-test
 build/$name-test
done
$CC $FLAGS host/display.c firmware/src/dsp/cad.c -o build/display-test
build/display-test build
for name in help settings_ui; do
 $CC $FLAGS tests/${name}_test.c firmware/src/dsp/cad.c -o build/$name-test
 build/$name-test build
done
${NODE:-node} tests/web_test.mjs
${NODE:-node} tests/bounce_test.mjs
${NODE:-node} tests/tone_test.mjs
${NODE:-node} tests/processor_test.mjs
${NODE:-node} tests/punch_test.mjs

${NODE:-node} tests/device_browser_test.mjs
${NODE:-node} tests/installer.test.mjs
