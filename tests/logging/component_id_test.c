/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/* component_id_test.c -- the logging component-ID walk must not depend on
 * the host's byte order (zowe/zowe-common-c#668).
 *
 * A component ID is four big-endian shorts: vendor, then up to three
 * component levels. The walk used to alias the 64-bit ID as an array of
 * shorts, which reads the levels in reverse on a little-endian host, while
 * the vendor check next to it used a shift. The visible effect: configuring
 * a level-1 component landed on the ROOT component instead, so every zowe
 * component started tracing, siblings included.
 *
 * Expectations hold on z/OS today and on Linux only with the fix:
 *   configure vendor 8F / level-1 component 1 at DEBUG
 *   -> component 1 itself traces at DEBUG
 *   -> its child (1, 9) traces at DEBUG (inherits)
 *   -> sibling component 2 does NOT trace at DEBUG
 *   -> component 1 does NOT trace above DEBUG
 */
#include <stdio.h>
#include <stdlib.h>

#include "zowetypes.h"
#include "logging.h"

#define PARENT   0x008F000100000000LLU  /* vendor 8F, level-1 component 1 */
#define CHILD    0x008F000100090000LLU  /* ... its child 9 (= LOG_COMP_HTTPSERVER) */
#define SIBLING  0x008F000200000000LLU  /* vendor 8F, level-1 component 2 */

static int failures = 0;

static void check(bool ok, const char *what) {
  printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) {
    failures++;
  }
}

int main(void) {
  LoggingContext *context = makeLoggingContext();
  if (context == NULL) {
    printf("  FAIL  makeLoggingContext returned NULL\n");
    return 1;
  }
  logConfigureDestination(context, LOG_DEST_PRINTF_STDOUT, "printf(stdout)", NULL, NULL);
  logConfigureComponent(context, PARENT, "parent", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_DEBUG);

  check(logShouldTraceInternal(context, PARENT, ZOWE_LOG_DEBUG),
        "configured component traces at its own level");
  check(logShouldTraceInternal(context, CHILD, ZOWE_LOG_DEBUG),
        "child of the configured component inherits the level");
  check(!logShouldTraceInternal(context, SIBLING, ZOWE_LOG_DEBUG),
        "sibling component does not trace (the configuration did not land on the root)");
  check(!logShouldTraceInternal(context, PARENT, ZOWE_LOG_DEBUG + 1),
        "configured component does not trace above its level");

  printf("%d failure%s\n", failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
