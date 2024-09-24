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


// int status = tagFile(pathname, ccsid);
export function test_changeTag() {
    const result = zos.changeTag('./');
    print.purple(`DUMMY TEST: zos.changeTag(./)=${result}`);
    return { errors: 0, total: 0 }
}


// int status = changeExtendedAttributes(pathname, extattr, onOffInt ? true : false);
export function test_changeExtAttr() {
    const result = zos.changeExtAttr('./');
    print.purple(`DUMMY TEST: zos.changeExtAttr(./)=${result}`);
    return { errors: 0, total: 0 }
}


// int status = convertOpenStream(fd, ccsid);
export function test_changeStreamCCSID() {
    const result = zos.changeStreamCCSID('./');
    print.purple(`DUMMY TEST: zos.changeStreamCCSID(./)=${result}`);
    return { errors: 0, total: 0 }
}


// res = stat(pathNative, &st);
export function test_zstat() {
    const result = zos.zstat('./testLib/testZos.js');
    print.purple(`DUMMY TEST: zos.zstat(./testLib/testZos.js)=${JSON.stringify(result)}`);
    return { errors: 0, total: 0 }
}


export function test_getZosVersion() {
    const result = zos.getZosVersion();
    print.conditionally(true, `zos.getZosVersion()=${result}${result > 0 ? `=hex(0x${result.toString(16)}` : ``})`);
    return { errors: result ? 0 : 1, total : 1 };
}


export function test_getEsm() {
    const EXPECTED = [ 'ACF2', 'RACF', 'TSS', 'NONE' ];
    const result = zos.getEsm();

    if (result == null || !EXPECTED.includes(result)) {
        print.conditionally(false, `zos.getEsms()=${result}`);
        return { errors: 1, total: 1 };
    }
    print.conditionally(true, `zos.getEsms()=${result}`);
    return { errors: 0, total: 1 };
}



export function test_dslist() {
    const result = zos.dslist('SYS1.MACLIB');
    print.purple(`DUMMY TEST: zos.zstat(SYS1.MACLIB)=${JSON.stringify(result)}`);
    return { errors: 0, total: 0 }
}


export function test_resolveSymbol() {
    const result = zos.resolveSymbol('&YYMMDD');
    const date = new Date();
    const yymmdd = (date.getFullYear() - 2000) * 10000 + (date.getMonth() + 1) * 100 + date.getDate() + '';
    let errs = 0;

    print.conditionally(result == yymmdd, `zos.resolveSymbol('&YYMMDD')=${result} -> ${yymmdd}`);
    if (result != yymmdd) {
        errs ++
    }

    const SYMBOLS_ERR = [ undefined, null, '', 'YYMMDD', ' &', ['a', 'b'], '& UNDEFINED SYMBOL !@#$%^&*()' ];
    for (let s in SYMBOLS_ERR) {
        const result = zos.resolveSymbol(SYMBOLS_ERR[s]);
        print.conditionally(result.length == 0, `zos.resolveSymbol(${SYMBOLS_ERR[s]})=${result}`);
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
        print.conditionally(result[1] == 0, `zos.getStatvfs(${PATHS_OK[p]})=${result[1]}`);
        if (result[1] != 0) {
            errs++;
        }
        if (result[0]) {
            console.log(`Stats=${JSON.stringify(result[0])}`);
        }
    }

    for (let p in PATHS_ERR) {
        const result = zos.getStatvfs(PATHS_ERR[p]);
        print.conditionally(result[1] != 0, `zos.getStatvfs(${PATHS_ERR[p]})=${result[1]}`);
        if (result[1] == 0) {
            errs++;
        }
    }
    return { errors: errs, total: PATHS_ERR.length + PATHS_OK.length };
}
