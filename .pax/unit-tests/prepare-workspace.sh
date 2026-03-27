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

################################################################################
# Prepare folders/files will be uploaded to Build/PAX server
################################################################################

# contants
SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR=$(dirname "$0")
PAX_WORKSPACE_DIR=.pax/unit-tests

# make sure in project root folder
cd $SCRIPT_DIR/../..

# prepare pax workspace
echo "[${SCRIPT_NAME}] preparing folders ..."
rm -fr "${PAX_WORKSPACE_DIR}/ascii" && mkdir -p "${PAX_WORKSPACE_DIR}/ascii"
rm -fr "${PAX_WORKSPACE_DIR}/content" && mkdir -p "${PAX_WORKSPACE_DIR}/content"

echo "[${SCRIPT_NAME}] copying files ..."
# Only copy what the tests need: c/, h/, platform/, tests/unit/, build/
mkdir -p "${PAX_WORKSPACE_DIR}/ascii/c"
mkdir -p "${PAX_WORKSPACE_DIR}/ascii/h"
mkdir -p "${PAX_WORKSPACE_DIR}/ascii/platform"
mkdir -p "${PAX_WORKSPACE_DIR}/ascii/tests/unit"

cp c/alloc.c c/utils.c c/collections.c c/timeutls.c "${PAX_WORKSPACE_DIR}/ascii/c/"
cp h/*.h "${PAX_WORKSPACE_DIR}/ascii/h/"
cp -R platform/* "${PAX_WORKSPACE_DIR}/ascii/platform/"
cp tests/unit/*.c tests/unit/*.h tests/unit/build_and_test.sh "${PAX_WORKSPACE_DIR}/ascii/tests/unit/"
