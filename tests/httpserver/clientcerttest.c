/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * Tests how httpGetClientCertificate() finds a client certificate for a response whose socket
 * has no GSK session, which is what an AT-TLS connection looks like to ZSS. The certificate is
 * injected into the socket extension, so no AT-TLS policy or network is needed.
 *
 *   _C89_L6SYSLIB="CEE.SCEEBND2:SYS1.CSSLIB:CSF.SCSFMOD0" make clientcerttest
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "httpserver.h"

static int failures = 0;

#define CHECK(cond, name) do { \
  if (cond) { \
    printf("[PASSED] %s\n", name); \
  } else { \
    printf("[FAILED] %s (line %d)\n", name, __LINE__); \
    failures++; \
  } \
} while (0)

#define CERT_LENGTH 700

typedef struct Fixture_tag {
  ShortLivedHeap *slh;
  Socket socket;
  SocketExtension *sext;
  HttpConversation conversation;
  HttpResponse *response;
  unsigned char cert[CERT_LENGTH];
} Fixture;

static void setUp(Fixture *f) {
  memset(f, 0, sizeof(*f));
  for (int i = 0; i < CERT_LENGTH; i++) {
    f->cert[i] = (unsigned char)(i * 5 + 1);
  }
  f->slh = makeShortLivedHeap(65536, 4);
  f->socket.tlsFlags = RS_TLS_WANT_TLS | RS_TLS_WANT_PEERCERT;
  f->sext = makeSocketExtension(&f->socket, f->slh, FALSE, NULL, 1024);
  f->conversation.socketExtension = f->sext;
  f->response = makeHttpResponse(NULL, f->slh, &f->socket);
  f->response->conversation = &f->conversation;
}

static void injectCertificate(Fixture *f) {
  f->sext->peerCertificate = (unsigned char*)SLHAlloc(f->slh, CERT_LENGTH);
  memcpy(f->sext->peerCertificate, f->cert, CERT_LENGTH);
  f->sext->peerCertificateLength = CERT_LENGTH;
  f->sext->tlsFlags |= RS_TLS_HAVE_PEERCERT;
}

static void tearDown(Fixture *f) {
  SLHFree(f->response->slh);
  safeFree31((char*)f->sext, sizeof(SocketExtension));
  SLHFree(f->slh);
}

static void testNoCertificateOnPlainSocket(void) {
  Fixture f;
  char buffer[4096];
  unsigned int length = 99;
  setUp(&f);
  /* This used to dereference socket->tlsSocket, which is NULL here. */
  int rc = httpGetClientCertificate(f.response, buffer, sizeof(buffer), &length);
  CHECK(rc != 0 && length == 0, "no certificate: reported as not found");
  tearDown(&f);
}

static void testCertificateFromAttls(void) {
  Fixture f;
  char buffer[4096];
  unsigned int length = 0;
  setUp(&f);
  injectCertificate(&f);
  int rc = httpGetClientCertificate(f.response, buffer, sizeof(buffer), &length);
  CHECK(rc == 0 && length == CERT_LENGTH, "certificate present: found with the right length");
  CHECK(0 == memcmp(buffer, f.cert, CERT_LENGTH), "certificate present: bytes match");
  tearDown(&f);
}

static void testBufferTooSmall(void) {
  Fixture f;
  char buffer[CERT_LENGTH - 1];
  unsigned int length = 99;
  setUp(&f);
  injectCertificate(&f);
  int rc = httpGetClientCertificate(f.response, buffer, sizeof(buffer), &length);
  CHECK(rc != 0 && length == 0, "buffer too small: fails without a partial copy");
  tearDown(&f);
}

static void testStaleCertificateIgnored(void) {
  Fixture f;
  char buffer[4096];
  unsigned int length = 99;
  setUp(&f);
  injectCertificate(&f);
  f.sext->tlsFlags &= ~RS_TLS_HAVE_PEERCERT;
  int rc = httpGetClientCertificate(f.response, buffer, sizeof(buffer), &length);
  CHECK(rc != 0 && length == 0, "HAVE_PEERCERT clear: stored bytes ignored");
  tearDown(&f);
}

static void testResponseWithoutConversation(void) {
  Fixture f;
  char buffer[4096];
  unsigned int length = 99;
  setUp(&f);
  injectCertificate(&f);
  f.response->conversation = NULL; /* as in a pseudoRespond() response */
  int rc = httpGetClientCertificate(f.response, buffer, sizeof(buffer), &length);
  CHECK(rc != 0 && length == 0, "no conversation: not found, no crash");
  tearDown(&f);
}

static void testConversationWithoutExtension(void) {
  Fixture f;
  char buffer[4096];
  unsigned int length = 99;
  setUp(&f);
  f.conversation.socketExtension = NULL;
  int rc = httpGetClientCertificate(f.response, buffer, sizeof(buffer), &length);
  CHECK(rc != 0 && length == 0, "no socket extension: not found, no crash");
  tearDown(&f);
}

static void testBadArguments(void) {
  Fixture f;
  char buffer[16];
  unsigned int length = 99;
  setUp(&f);
  CHECK(httpGetClientCertificate(NULL, buffer, sizeof(buffer), &length) != 0, "NULL response rejected");
  CHECK(httpGetClientCertificate(f.response, buffer, sizeof(buffer), NULL) != 0, "NULL length rejected");
  f.response->socket = NULL;
  CHECK(httpGetClientCertificate(f.response, buffer, sizeof(buffer), &length) != 0, "NULL socket rejected");
  tearDown(&f);
}

int main(int argc, char **argv) {
  testNoCertificateOnPlainSocket();
  testCertificateFromAttls();
  testBufferTooSmall();
  testStaleCertificateIgnored();
  testResponseWithoutConversation();
  testConversationWithoutExtension();
  testBadArguments();

  if (failures > 0) {
    printf("%d check(s) failed\n", failures);
    return 1;
  }
  puts("all checks passed");
  return 0;
}
