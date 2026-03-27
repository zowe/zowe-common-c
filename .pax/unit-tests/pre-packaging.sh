#!/bin/sh -e
set -xe

################################################################################
# This program and the accompanying materials are made available under the terms of the
# Eclipse Public License v2.0 which accompanies this distribution, and is available at
# https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.
################################################################################

# contants
SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR=$(pwd)

# run unit tests
echo "$SCRIPT_NAME running unit tests on z/OS ..."
STEPLIB=CBC.SCCNCMP "$SCRIPT_DIR/content/tests/unit/build_and_test.sh"

# The build_and_test.sh script exits non-zero if any tests fail,
# which will fail this script (set -e) and fail the workflow.

# clean up content folder - make-pax expects content/ to exist
echo "$SCRIPT_NAME cleaning up pax folder ..."
cd "$SCRIPT_DIR"
mv content bak && mkdir -p content

# write test results marker so pax has something to package
echo "unit tests passed" > "$SCRIPT_DIR/content/test_results.txt"
