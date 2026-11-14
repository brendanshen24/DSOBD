#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_bin=$(mktemp /tmp/dsobd-test.XXXXXX)
trap 'rm -f "$test_bin"' EXIT
${HOST_CC:-cc} -std=c11 -Wall -Wextra -Werror -Wno-sign-compare \
    -fsanitize=address,undefined -Itests/stubs -Iinclude \
    tests/dtec_test.c source/dtec_obd.c source/dtec_top.c source/dtec_bottom.c \
    source/dtec_audio.c source/dtec_settings.c source/dtec_logger.c source/dtec_assets.c \
    -lm -o "$test_bin"
"$test_bin"
${HOST_CC:-cc} -std=c11 -Wall -Wextra -Werror -Wno-sign-compare \
    -fsanitize=address,undefined -Itests/stubs -Iinclude \
    tests/render_test.c source/dtec_obd.c source/dtec_top.c source/dtec_bottom.c \
    source/dtec_gfx.c source/dtec_assets.c -lm -o "$test_bin"
"$test_bin" "$@"
${HOST_CC:-cc} -std=c11 -D_DARWIN_C_SOURCE -Wall -Wextra -Werror \
    -fsanitize=address,undefined -Icore/include tests/ports_test.c \
    core/source/elm_core.c core/source/obd_diag.c -lm -o "$test_bin"
"$test_bin"
