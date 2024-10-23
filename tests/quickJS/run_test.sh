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
    echo "Run the Quick JS tests"
    echo "  no parm: tries to run configmgr from current 'zowe-common-c/bin'"
    echo "  path: path to configmgr"
    echo "  --help: this help"
    exit 0
fi

configmgr_path="${1}"
if [ -z "${configmgr_path}" ]; then
    configmgr_path="../../bin/configmgr"
fi

if [ -f "${configmgr_path}" ]; then
    "${configmgr_path}" -script ./quickJS.js
else
    echo "Error: configmgr not found in '${configmgr_path}'"
    exit 4
fi
