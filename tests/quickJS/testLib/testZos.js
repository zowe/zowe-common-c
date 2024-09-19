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

import * as zos from 'zos';
import * as print from '../lib/print';


export function test_changeTag() {
    return { errors: 0, total: 0 }
}


export function test_changeExtAttr() {
    return { errors: 0, total: 0 }
}


export function test_changeStreamCCSID() {
    return { errors: 0, total: 0 }
}


export function test_zstat() {
    return { errors: 0, total: 0 }
}


export function test_getZosVersion() {
    const result = zos.getZosVersion();
    print.clog(true, `zos.getZosVersion()=${result}${result > 0 ? ` --hex--> 0x${result.toString(16)}` : ``}`);
    return { errors: result ? 0 : 1, total : 1 };
}


export function test_getEsm() {
    const EXPECTED = [ 'ACF2', 'RACF', 'TSS', 'NONE' ];
    const result = zos.getEsm();

    if (result == null || !EXPECTED.includes(result)) {
        print.clog(false, `zos.getEsms()=${result}`);
        return { errors: 1, total: 1 };
    }
    print.clog(true, `zos.getEsms()=${result}`);
    return { errors: 0, total: 1 };
}



export function test_dslist() {
    return { errors: 0, total: 0 }
}


export function test_resolveSymbol() {
    const result = zos.resolveSymbol('&YYMMDD');
    const date = new Date();
    const yymmdd = (date.getFullYear() - 2000) * 10000 + (date.getMonth() + 1) * 100 + date.getDate() + '';
    let errs = 0;

    print.clog(result == yymmdd, `zos.resolveSymbol('&YYMMDD')=${result} -> ${yymmdd}`);
    if (result != yymmdd) {
        errs ++
    }

    const SYMBOLS_ERR = [ undefined, null, '', 'YYMMDD', ' &', ['a', 'b'], '& UNDEFINED SYMBOL !@#$%^&*()' ];
    for (let s in SYMBOLS_ERR) {
        const result = zos.resolveSymbol(SYMBOLS_ERR[s]);
        print.clog(result.length == 0, `zos.resolveSymbol(${SYMBOLS_ERR[s]})=${result}`);
        if (result.length) {
            errs++
        }
    }

    return { errors: errs, total : SYMBOLS_ERR.length + 1 };;
}


export function test_getStatvfs() {
    const PATHS_OK = [ './', '/', '/bin/' ];
    const PATHS_ERR = [ '', '/aaaaaaaaaaaaaaaaaaaaaaaaaaaaa', [ 'a', 3.14 ], { path: '/dev/null' }, null, true, false, undefined, 0, -1, 800 ];
    let errs = 0;

    for (let p in PATHS_OK) {
        const result = zos.getStatvfs(PATHS_OK[p]);
        print.clog(result[1] == 0, `zos.getStatvfs(${PATHS_OK[p]})=${result[1]}`);
        if (result[1] != 0) {
            errs++;
        }
        if (result[0]) {
            console.log(`  stats=${JSON.stringify(result[0])}`);
        }
    }

    for (let p in PATHS_ERR) {
        const result = zos.getStatvfs(PATHS_ERR[p]);
        print.clog(result[1] != 0, `zos.getStatvfs(${PATHS_ERR[p]})=${result[1]}`);
        if (result[1] == 0) {
            errs++;
        }
    }
    return { errors: errs, total: PATHS_ERR.length + PATHS_OK.length };
}
