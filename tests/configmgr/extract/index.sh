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

if [ `uname` != "OS/390" ]; then
    echo "Error: this test must run on a z/OS system."
    exit 1
fi

if [ "${1}" = "--help" ]; then
    echo "Test the configmgr with 'extract' option"
    echo "  no parm: tries to run configmgr from current 'zowe-common-c/bin'"
    echo "  path: path to configmgr"
    echo "  --help: this help"
    exit 0
fi

configmgr_path="${1}"
if [ -z "${configmgr_path}" ]; then
    configmgr_path="../../../bin/configmgr"
fi

if [ -f "${configmgr_path}" ]; then
    "${configmgr_path}" -script ./quickJS.js
else
    echo "Error: configmgr not found in '${configmgr_path}'"
    exit 4
fi

schema="./extract.json"
yaml="./extract.yaml"
fileYaml="FILE(${yaml})"

# To simplify this test, any property starting with lowercase will be tested, the rest is ignored
test_set=$(cat "${yaml}" | grep -E '^[\ ]+[a-z]+' | awk -F: '{ print $1 }');

for item in $test_set; do

    result=$(_CEE_RUNOPTS="XPLINK(ON)" "${configmgr_path}" -s "${schema}" -p "${fileYaml}" extract "/test/${item}");
    echo "rc=${?}: Item ${item} -> $result"

done
