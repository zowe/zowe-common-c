#!/bin/sh
# Pin: configmgr.c:1895 hardcodes jqt.ccsid = 1208 (UTF-8) for the jq
# tokenizer. On z/OS USS, argv is IBM-1047 (EBCDIC), so
# parsetools.c::testCharProp reads its character-property lookups from
# the wrong row of cp1047to1208 and rejects (or loops on) valid jq
# expressions like ".port" or ".". On WSL/Linux, argv IS UTF-8, so the
# hardcoded 1208 matches the input and jq works.
#
# The bug has been latent since 2022-03-10 (when the line was first
# committed) and is not surfaced anywhere except this pin.
#
# When configmgr.c:1895 gets a platform-conditional ccsid (1047 on
# __ZOWE_OS_ZOS, 1208 elsewhere), the z/OS branch below will FAIL
# because configmgr will start returning exit 0 instead of 15. That
# is the signal to flip the z/OS branch to the "works" form (or just
# delete this file).

cli_dir="$(cd "$(dirname "$0")/.." && pwd)"
# lib.sh auto-discovers configmgr relative to $0, which from this
# subdirectory resolves to the wrong path. Set it explicitly first.
: "${CONFIGMGR:=$cli_dir/../../../bin/configmgr}"
export CONFIGMGR
. "$cli_dir/lib.sh"

start_suite "configmgr jq (platform-conditional pin: z/OS ccsid bug)"

FX="$cli_dir/fixtures"

case "$(uname -s)" in
  OS/390|z/OS)
    # Pin the current broken z/OS behavior.
    run_case "jq .port (latent bug, configmgr.c:1895)" "$CONFIGMGR" \
      -s "$FX/port_range_schema.json" -p "FILE($FX/port_good.yaml)" jq ".port"
    assert_exit "jq exits with parse error (ZCFG_JQ_PARSE_ERROR)" 15
    assert_contains "stdout reports parse failure" \
      "Failed to parse jq expression" "$LAST_STDOUT"
    ;;
  *)
    # Linux / WSL / other UTF-8-argv platforms: jq works correctly.
    run_case "jq .port" "$CONFIGMGR" \
      -s "$FX/port_range_schema.json" -p "FILE($FX/port_good.yaml)" jq ".port"
    assert_exit "jq exits 0" 0
    assert_contains "jq returns port value" "8080" "$LAST_STDOUT"
    ;;
esac

end_suite
