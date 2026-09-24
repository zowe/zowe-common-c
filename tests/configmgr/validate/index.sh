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
    schema="${1}"
    configPath="${2}"
    expectedRC="${3}"
    realRC="${4}"
    result="${5}"
    echo ""
    echo "*** configmgr -s(${schema}) -p(${configPath}) validate"
    if [ "${expectedRC}" = "${realRC}" ]; then
        echo "  OK, got and expected ${expectedRC}"
        if [ -n "${TRACE}" ]; then
            echo "${result}" | sed "s/^/  /"
        fi
    else
        echo "  Error, got ${realRC}, expected ${expectedRC}"
        echo "${result}" | sed "s/^/  /"
        errors=`expr $errors + 1`
    fi
}

errors=0
SCHEMAS="./schema/zowe-yaml-schema.json:./schema/server-common.json"

# Make sure no abend for missing parameters
# 4 => ZCFG_BAD_CONFIG_PATH
# 5 => ZCFG_BAD_JSON_SCHEMA

IFS='|'
while read expectedRC s p; do    
    result=$(_CEE_RUNOPTS="XPLINK(ON)" "${configmgr_path}" -s "${s}" -p "${p}" validate 2>&1 > /dev/null)
    realRC=$?
    printResults "${s}" "${p}" "${expectedRC}" "${realRC}" "${result}"
done <<EOF
5||
5||Config
4|Schema|
5|./schema/empty.json|./yaml/empty.yaml
5|./schema/invalid.json|./yaml/valid.0.yaml
EOF

# Test the basic behavior
for zoweYaml in ./yaml/*; do
    expectedRC=$(echo "${zoweYaml}" | awk -F. '{ print $3}')
    result=$(_CEE_RUNOPTS="XPLINK(ON)" "${configmgr_path}" -s "${SCHEMAS}" -p "FILE(${zoweYaml})" validate 2>&1 > /dev/null)
    realRC=$?
    printResults "${SCHEMAS}" "${zoweYaml}" "${expectedRC}" "${realRC}" "${result}"
done

exit $errors
