/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this
  distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

/* zos-le-stub.c -- minimal LE stubs so the charset-streaming unit test links
 * standalone on z/OS without pulling in le.c -> zos.c (the CAA / LE foundation).
 *
 * logging.c references getCAA() and abortIfUnsupportedCAA() to attach a
 * thread-local logging context, and charsets.c pulls logging.o in for its
 * zowelog() calls. But convertCharsetStreaming -- the code under test -- never
 * logs, so on the test's execution path these are never called; they only need
 * to satisfy the linker. getCAA returns a zeroed static buffer (readable,
 * non-NULL) as belt-and-braces in case it is ever reached.
 *
 * This is test scaffolding for run-zos.sh only; the real getCAA/abortIfUnsupportedCAA
 * live in le.c and are used by the actual server. */
char *getCAA(void){ static char caa[4096]; return caa; }
void abortIfUnsupportedCAA(void){ /* intentional no-op link stub -- never reached on the test path; see file header */ }
