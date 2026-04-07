import { ConfigManager } from "Configuration";

/*
  Verify that the 'zos' module is NOT exposed on macOS/Linux.

  On z/OS: globalThis.zos is set up automatically and the 'zos' module is
  registered.  On macOS the stubs return NULL without registering the module,
  and the globalThis.zos assignment is inside #ifdef __ZOWE_OS_ZOS, so neither
  the global nor the module should exist.

  Usage (from zowe-common-c/):
    ./bin/configmgr -script tests/js/test_no_zos_module.js \
      -s tests/schemadata/zoweappserver.json \
      -p "FILE(tests/schemadata/bundle1.json)"
*/

let passed = 0;
let failed = 0;

function assert(cond, msg) {
    if (cond) {
        console.log("PASS: " + msg);
        passed++;
    } else {
        console.log("FAIL: " + msg);
        failed++;
    }
}

/* ---- 1. globalThis.zos must not exist ---- */
assert(typeof globalThis.zos === 'undefined',
    "globalThis.zos is undefined (not injected on macOS)");

/* ---- 2. The standard cross-platform globals are still present ---- */
assert(typeof globalThis.std !== 'undefined',
    "globalThis.std IS defined (cross-platform module still works)");
assert(typeof globalThis.os !== 'undefined',
    "globalThis.os IS defined (cross-platform module still works)");

/* ---- 3. ConfigManager (the core purpose) still works ---- */
let cmgr = new ConfigManager();
assert(typeof cmgr === 'object' && cmgr !== null,
    "ConfigManager object is constructible");

console.log("\nResults: " + passed + " passed, " + failed + " failed");
if (failed > 0) {
    throw new Error(failed + " check(s) failed");
}
