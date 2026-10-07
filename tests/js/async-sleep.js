/*
 * async-sleep.js -- POSITIVE case for configmgr signal handling.
 *
 * The correct pattern: an ASYNC wait that yields to the event loop every tick
 * (await os.sleepAsync). The pending timer keeps js_os_poll alive and its
 * select() interruptible, so a SIGTERM delivered during the wait is dispatched
 * to the JS handler between ticks. This requires configmgr to run the async
 * continuation loop (js_std_loop) after top-level eval -- see the fix in
 * embeddedjs.c ejsEvalFile(). WITHOUT that fix the loop never runs, the
 * sleepAsync timer never fires, and this script exits right after the first
 * line (no ticks) -- which is itself the demonstration that the loop is missing.
 *
 *   configmgr -script tests/js/async-sleep.js    # then: kill -TERM <pid>
 *   EXPECTED (with fix): ticks once/sec, then PASS when SIGTERM interrupts.
 */
import * as os from 'cm_os';

const SIGTERM = (typeof os.SIGTERM === 'number') ? os.SIGTERM : 15;
let stop = false;

os.signal(SIGTERM, function () {
  stop = true;
  console.log('*** SIGTERM handler fired ***');
});

console.log('async-sleep: pid=' + os.getpid() + '  (send SIGTERM within ~10s)');

async function main() {
  let waited = 0;
  while (waited < 10000 && !stop) {
    await os.sleepAsync(1000);    /* ASYNC: yields to the loop; timer keeps it alive + interruptible */
    waited += 1000;
    console.log('  async tick waited=' + waited + ' stop=' + stop);
  }
  if (stop) {
    console.log('PASS: handler fired, async wait interrupted cleanly');
  } else {
    console.log('FAIL: wait completed without a signal');
  }
}

main();
