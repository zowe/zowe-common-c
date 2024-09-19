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

import * as std from 'cm_std';
import * as testZos from './testLib/testZos';

const TEST_ZOS = [
    testZos.test_changeTag,
    testZos.test_changeExtAttr,
    testZos.test_changeStreamCCSID,
    testZos.test_zstat,
    testZos.test_getZosVersion,
    testZos.test_getEsm,
    testZos.test_dslist,
    testZos.test_resolveSymbol,
    testZos.test_getStatvfs,
]

const TESTS = [
    ...TEST_ZOS
]

let result = {};
let errors = 0;
let total = 0;

for (let testFunction in TESTS) {
    result = TESTS[testFunction]();
    errors += result.errors;
    total += result.total;
}

if (errors) {
    console.log(`\n${errors} error${errors == 1 ? '' : 's'} detected in ${total} tests, review the test output.\n`);
    std.exit(8);
} else {
    console.log(`\n${total} tests succesfull.\n`);
    std.exit(0);
}
