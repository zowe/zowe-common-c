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
import * as print from './lib/print';
import * as testSet from './testSet';

let result = {};
let errors = 0;
let total = 0;

const loopStart = Date.now();
for (let testFunction in testSet.TESTS) {
    result = testSet.TESTS[testFunction]();
    errors += result.errors;
    total += result.total;
}
const loopEnd = Date.now();

const timeDiff = loopEnd-loopStart;
print.cyan(`\nTime elapsed ${timeDiff} ms.`);

if (errors) {
    print.lines(print.RED, `${errors} error${errors == 1 ? '' : 's'} detected in ${total} tests, review the test output.`);
    std.exit(8);
} else {
    print.lines(print.GREEN, `${total} tests succesfull.`);
    std.exit(0);
}
