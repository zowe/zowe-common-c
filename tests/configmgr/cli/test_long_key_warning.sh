#!/bin/sh
# A YAML key longer than MAX_JSON_KEY is dropped with a warning that shows the
# key (zowe/zowe-common-c#671: on z/OS the key was printed in the wrong
# encoding). Here we check the warning names the key and the rest still loads.

cd "$(dirname "$0")"
. ./lib.sh

start_suite "over-long YAML key warning is readable (#671)"

FX=fixtures
run_case "300-char key" \
  "$CONFIGMGR" -s "$FX/any_object_schema.json" -p "FILE($FX/long_key.yaml)" validate
assert_exit "config validates without the long key" 0
assert_contains "warning shows the key text" "key too long 'kkkkkkkkkkkkkkkk" "$LAST_STDOUT"

end_suite
