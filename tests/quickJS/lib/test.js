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

import * as print from './print';

export const CLASSIC_FAILS = [ '', null, undefined, true, false, -16, 0, 256, 3.141592, 999999999, 
    ['banana', 'apple', 'kiwi'], 
    { success: "guaranteed" }
];

export function process(testFunction, testSet, compare, msg) {
    let errors = 0;
    let result;
    if (typeof compare == 'object') {
        compare = JSON.stringify(compare);
    }
    for (let t in testSet) {
        if (Array.isArray(testSet[t])) {
            result = testFunction(...testSet[t]);
        } else {
            result = testFunction(testSet[t]);
        }
        if (typeof result == 'object') {
            result = JSON.stringify(result);
        }
        errors += print.conditionally(result == compare, msg, testSet[t], result);
    }
    return errors;
}
