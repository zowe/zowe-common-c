/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this
  distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

/* tests/wsl-stubs.c -- shared link stubs for building zowe-common-c unit tests
 * on Linux/WSL (tests/build.sh). A handful of symbols live in z/OS-only TUs
 * (e.g. bpxnet.c) that don't compile off-platform, yet are referenced from
 * otherwise-portable code the tests DO link -- most commonly logging.c, whose
 * socket-log destination reaches the networking layer. These functions are
 * never called on a unit test's execution path; they exist only to satisfy the
 * linker. Each returns a benign failure. If a test genuinely needs one of these
 * (i.e. it is actually a network test), it belongs in a z/OS-run test instead,
 * or needs a real implementation here.
 */
#include "zowetypes.h"
#include "bpxnet.h"

int SocketAddress_toString(const SocketAddress *in_socketAddress,
                           char *inout_strbuf, int *inout_len) {
  if (inout_len) { *inout_len = 0; }
  if (inout_strbuf) { inout_strbuf[0] = '\0'; }
  return -1;
}

InetAddr *getAddressByName2(char *addressString, int ipv4only) {
  return NULL;
}
