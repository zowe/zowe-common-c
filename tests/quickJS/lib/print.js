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

export const RED = "\u001b[31m";
export const GREEN = "\u001b[32m";
export const YELLOW = "\u001b[33m";
export const PURPLE = "\u001b[35m";
export const CYAN = "\u001b[36m";
export const RESET = "\u001b[0m";

export function color(color, msg) {
    console.log(`${color}${msg}${RESET}`);
}

export function red(msg) {
    color(RED, msg);
}

export function green(msg) {
    color(GREEN, msg);
}

export function yellow(msg) {
    color(YELLOW, msg);
}

export function purple(msg) {
    color(PURPLE, msg);
}

export function cyan(msg) {
    color(CYAN, msg);
}

export function conditionally(condition, functionTested, parm, result, additionalInfo) {
    if (typeof parm == 'object') {
        parm = JSON.stringify(parm);
    }
    if (typeof result == 'object') {
        result = JSON.stringify(result);
    }
    if (!additionalInfo) {
        additionalInfo = '';
    } else {
        additionalInfo = `[${additionalInfo}]`;
    }
    console.log(`${condition ? GREEN : RED}` + `${functionTested}("${parm}")=${result} ${additionalInfo}` + RESET);
    return + !condition;
}

export function lines(clr, msg) {
    color(clr, `\n${'-'.repeat(msg.length)}`);
    color(clr, `${msg}`);
    color(clr, `${'-'.repeat(msg.length)}\n`);
}
