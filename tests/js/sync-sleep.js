/*
 * sync-sleep.js -- NEGATIVE control for configmgr signal handling.
 *
 * Demonstrates the JavaScript single-threaded anti-pattern: a SYNCHRONOUS
 * os.sleep() wait loop can never observe a signal. os.sleep() blocks the one
 * JS thread and returns nothing to the event loop; os.signal() callbacks are
 * dispatched only by the event loop (js_os_poll). A blocked thread starves the
 * loop, so the handler cannot run during the wait -- on ANY host (Node/Deno/Bun
 * behave identically). This holds even WITH configmgr's post-eval js_std_loop
 * fix, because the synchronous loop runs entirely inside eval and leaves nothing
 * pending. Included to show that NO handler fires here.
 *
 *   configmgr -script tests/js/sync-sleep.js     # then: kill -TERM <pid>
 *   EXPECTED: FAIL (signal not delivered to JS).
 */
import * as os from 'cm_os';

const SIGTERM = (typeof os.SIGTERM === 'number') ? os.SIGTERM : 15;
let stop = false;

os.signal(SIGTERM, function () {
  stop = true;
  console.log('*** SIGTERM handler fired ***');
});

console.log('sync-sleep: pid=' + os.getpid() + '  (send SIGTERM within ~10s)');

let waited = 0;
while (waited < 10000 && !stop) {
  os.sleep(1000);                 /* SYNCHRONOUS: blocks the thread, starves the loop */
  waited += 1000;
  console.log('  sync tick waited=' + waited + ' stop=' + stop);
}

if (stop) {
  console.log('PASS (unexpected): handler fired');
} else {
  console.log('FAIL (expected): synchronous os.sleep never yields; signal never reached JS');
}
