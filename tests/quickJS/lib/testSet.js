/*
// This program and the accompanying materials are made available
// under the terms of the Eclipse Public License v2.0 which
// accompanies this distribution, and is available at
// https://www.eclipse.org/legal/epl-v20.html
//
// SPDX-License-Identifier: EPL-2.0
//
// Copyright Contributors to the Zowe Project.
*/

import * as testZos from '../testLib/testZos'

export const TEST_ZOS = [
    testZos.test_changeTag,
    testZos.test_changeExtAttr,
    testZos.test_changeStreamCCSID,
    testZos.test_zstat,
    testZos.test_getZosVersion,
    testZos.test_getEsm,
    testZos.test_dslist,
    testZos.test_resolveSymbol,
    testZos.test_getStatvfs,
];

export const TESTS = [
    ...TEST_ZOS
];
