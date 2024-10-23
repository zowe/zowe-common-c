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

// ========================
// zowe-common-c/c/qjszos.c
// ========================

import * as zos from 'zos';
import * as print from '../lib/print';
import * as test from '../lib/test';


export function test_changeTag() {
    let errs;
    const FAILS = [ ...test.CLASSIC_FAILS,
        './file, which does not exit', ['./file, which does not exit', 250000 ],
    ]
    const FINES = [
        [ './testLib/hello.txt', 0 ],
        [ './testLib/hello.txt', 859 ],
        [ './testLib/hello.txt', 1047 ],
    ];

    errs = test.process(zos.changeTag, FAILS, -1, 'zos.changeTag');
    errs += test.process(zos.changeTag, FINES, 0, 'zos.changeTag');

    return { errors: errs, total: FAILS.length + FINES.length }
}


export function test_changeExtAttr() {
    const FINES = [
        ['./testLib/hello.txt', zos.EXTATTR_PROGCTL, true ],
        ['./testLib/hello.txt', zos.EXTATTR_PROGCTL, false ],
     ]
    let errs = test.process(zos.changeExtAttr, test.CLASSIC_FAILS, -1, 'zos.changeExtAttr');
    errs += test.process(zos.changeExtAttr, FINES, 0, 'zos.changeExtAttr');
    return { errors: errs, total: test.CLASSIC_FAILS.length + FINES.length }
}


export function test_changeStreamCCSID() {
    const FAILS = [ -2, -1, 999999999, 1024*1024 ];
    const errs = test.process(zos.changeStreamCCSID, FAILS, -1, 'zos.changeStreamCCSID');
    return { errors: errs, total: FAILS.length }
}


export function test_zstat() {
    const FAILS = [ ...test.CLASSIC_FAILS ];
    const FINES = [ './testLib/hello.txt', './', './run_test.sh' ];
    let errs = 0;
    for (let f in FAILS) {
        const result = zos.zstat(FAILS[f]);
        errs += print.conditionally(result[1] == 129, 'zos.zstat', FAILS[f], result);
    }
    for (let f in FINES) {
        const result = zos.zstat(FINES[f]);
        errs += print.conditionally(result[1] == 0, 'zos.zstat', FINES[f], result);
    }
    return { errors: errs, total: FAILS.length + FINES.length }
}


export function test_getZosVersion() {
    const result = zos.getZosVersion();
    print.conditionally(true, 'zos.getZosVersion', 'no parameter', result, `${result > 0 ? `in hex 0x${result.toString(16)}` : ``}`);
    return { errors: result ? 0 : 1, total : 1 };
}


export function test_getEsm() {
    const EXPECTED = [ 'ACF2', 'RACF', 'TSS', 'NONE' ];
    const result = zos.getEsm();

    if (result == null || !EXPECTED.includes(result)) {
        print.conditionally(false, 'zos.getEsms', 'no parameter', result);
        return { errors: 1, total: 1 };
    }
    print.conditionally(true, 'zos.getEsms', 'no parameter', result);
    return { errors: 0, total: 1 };
}


export function test_dslist() {
    const FAILS = [ ...test.CLASSIC_FAILS, 'sys1.maclib' ];
    const FINES = [ 'SYS1.PARMLIB', 'SYS1.MACLIB' ];

    let errs = test.process(zos.dslist, FAILS, null, 'zos.dslist');
    for (let f in FINES) {
        const result = zos.dslist(FINES[f]);
        errs += print.conditionally(result.datasets[0].dsn == FINES[f], 'zos.dslist', FINES[f], result);
    }
    return { errors: errs, total: FAILS.length + FINES.length }
}


export function test_resolveSymbol() {
    const result = zos.resolveSymbol('&YYMMDD');
    const date = new Date();
    const yymmdd = (date.getFullYear() - 2000) * 10000 + (date.getMonth() + 1) * 100 + date.getDate() + '';
    let errs = 0;

    errs += print.conditionally(result == yymmdd, 'zos.resolveSymbol', '&YYMMDD', result, `javascript date -> ${yymmdd}`);

    const ERR_SYMBOLS = [ ...test.CLASSIC_FAILS, 'YYMMDD', ' &', ['a', 'b'], '& UNDEFINED SYMBOL !@#$%^&*()' ];
    errs += test.process(zos.resolveSymbol, ERR_SYMBOLS, '', 'zos.resolveSymbol');

    return { errors: errs, total : ERR_SYMBOLS.length + 1 };;
}


export function test_getStatvfs() {
    const PATHS_OK = [ './', '/', '/bin/' ];
    const PATHS_ERR = [ ...test.CLASSIC_FAILS, '/aaaaaaaaaaaaaaaaaaaaaaaaaaaaa/bbbbbbbbbbbbbbbbbbbbbb/ccccccccccccccccccccccc' ];
    let errs = 0;

    for (let p in PATHS_OK) {
        const result = zos.getStatvfs(PATHS_OK[p]);
        errs += print.conditionally(result[1] == 0, 'zos.getStatvfs', PATHS_OK[p], result);
    }

    for (let p in PATHS_ERR) {
        const result = zos.getStatvfs(PATHS_ERR[p]);
        errs += print.conditionally(result[1] != 0, 'zos.getStatvfs', PATHS_ERR[p], result[1]);
    }

    return { errors: errs, total: PATHS_ERR.length + PATHS_OK.length };
}
