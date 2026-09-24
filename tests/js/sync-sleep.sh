#!/bin/sh

# Simple test for sync-sleep.js
# Starts the script and send the TERM signal
# Possible parameter is configmgr binary

CONFIGMGR="../../bin/configmgr"

if [ -n "${1}" ]; then
  CONFIGMGR="${1}"
fi

if [ ! -f "${CONFIGMGR}" ]; then
  echo "configmgr binary not found: '${CONFIGMGR}'"
  exit 1
fi

"$CONFIGMGR" -script ./sync-sleep.js &
PID=$!
sleep 3
kill -TERM "${PID}"
wait "${PID}"
