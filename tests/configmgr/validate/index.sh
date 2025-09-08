#!/bin/sh

#######################################################################
# This program and the accompanying materials are made available
# under the terms of the Eclipse Public License v2.0 which
# accompanies this distribution, and is available at
# https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.
#######################################################################

# Set anything for more tracing information
TRACE=
errors=0
SCHEMAS="./schema/zowe-yaml-schema.json:./schema/server-common.json"

if [ `uname` != "OS/390" ]; then
    echo "Error: this test must run on a z/OS system."
    exit 1
fi

if [ "${1}" = "--help" ]; then
    echo "Test the configmgr with 'validate' option"
    echo "  no parm: tries to run configmgr from current 'zowe-common-c/bin'"
    echo "  path: path to configmgr"
    echo "  --help: this help"
    exit 0
fi

configmgr_path="${1}"
if [ -z "${configmgr_path}" ]; then
    configmgr_path="../../../bin/configmgr"
fi

if [ ! -f "${configmgr_path}" ]; then
    echo "Error: configmgr not found in '${configmgr_path}'"
    exit 4
fi

printResults() {
    expected="${1}"
    real="${2}"
    res="${3}"
    if [ "${expected}" = "${real}" ]; then
        echo "  OK, got and expected ${expected}\n"
        if [ -n "${TRACE}" ]; then
            echo "${res}" | sed "s/^/  /"
        fi
    else
        echo "  Error, got ${real}, expected ${expected}"
        echo "${res}" | sed "s/^/  /"
        echo "\n"
        errors=`expr $errors + 1`
    fi
}


# Make sure no abend for missing parameters
# 4 => ZCFG_BAD_CONFIG_PATH
# 5 => ZCFG_BAD_JSON_SCHEMA

IFS='|'
while read expectedRC s p; do    
    echo "\n*** configmgr -s(${s}) -p(${p}) validate"
    result=$(_CEE_RUNOPTS="XPLINK(ON)" "${configmgr_path}" -s "${s}" -p "${p}" validate 2>&1 > /dev/null)
    realRC=$?
    printResults "${expectedRC}" "${realRC}" "${result}"
done <<EOF
5||
5||Config
4|Schema|
EOF

# Test the basic behavior
for zoweYaml in ./yaml/*; do
    expectedRC=$(echo "${zoweYaml}" | awk -F. '{ print $3}')
    echo "\n*** configmgr -s(${SCHEMAS}) -p(${zoweYaml}) validate"
    result=$(_CEE_RUNOPTS="XPLINK(ON)" "${configmgr_path}" -s "${SCHEMAS}" -p "FILE(${zoweYaml})" validate 2>&1 > /dev/null)
    realRC=$?
    printResults "${expectedRC}" "${realRC}" "${result}"
done

exit $errors
