/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * Tests how sxUpdateTLSInfo() turns SIOCTTLSCTL results into socket extension state.
 * No AT-TLS policy is needed: this file supplies its own tcpIOControl() instead of
 * linking bpxskt.c, and answers with canned TTLS_IOCTL data.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "tls.h"
#include "ezbztlsc.h"

#ifndef USE_ZOWE_TLS
#error This test exercises the USE_ZOWE_TLS code path
#endif

#define MAX_FAKE_CERT 80000

typedef struct FakeIoctl_tag {
  int sts;            /* what tcpIOControl returns */
  int errnoValue;     /* returned in *returnCode when sts is nonzero */
  int statConn;
  int secType;
  int certLength;     /* 0 = the partner sent no certificate */
  unsigned char cert[MAX_FAKE_CERT];
  int callCount;
} FakeIoctl;

static FakeIoctl fake;

static void resetFake(void) {
  memset(&fake, 0, sizeof(fake));
  fake.statConn = TTLS_CONN_SECURE;
  fake.secType = TTLS_SEC_SRV_CA_FULL;
}

static void setFakeCert(int length) {
  fake.certLength = length;
  for (int i = 0; i < length; i++) {
    fake.cert[i] = (unsigned char)(i * 7 + 3);
  }
}

int tcpIOControl(Socket *socket, int command, int argumentLength, char *argument,
                 int *returnCode, int *reasonCode) {
  struct TTLS_IOCTL *ioc = (struct TTLS_IOCTL*)argument;
  *returnCode = 0;
  *reasonCode = 0;
  fake.callCount++;

  if (command != SIOCTTLSCTL || argumentLength != sizeof(struct TTLS_IOCTL)) {
    *returnCode = EINVAL;
    return -1;
  }
  if (fake.sts != 0) {
    *returnCode = fake.errnoValue;
    return fake.sts;
  }

  ioc->TTLSi_Stat_Policy = TTLS_POL_APPLCNTRL;
  ioc->TTLSi_Stat_Conn = fake.statConn;
  ioc->TTLSi_Sec_Type = fake.secType;

  if (ioc->TTLSi_Req_Type == TTLS_RETURN_CERTIFICATE && fake.certLength > 0) {
    ioc->TTLSi_Cert_Len = fake.certLength;
    if (ioc->TTLSi_BufferLen < fake.certLength) {
      *returnCode = ENOBUFS;
      return -1;
    }
    memcpy(ioc->TTLSi_BufferPtr, fake.cert, fake.certLength);
  }
  return 0;
}

static int failures = 0;

#define CHECK(cond, name) do { \
  if (cond) { \
    printf("[PASSED] %s\n", name); \
  } else { \
    printf("[FAILED] %s (line %d)\n", name, __LINE__); \
    failures++; \
  } \
} while (0)

typedef struct Fixture_tag {
  ShortLivedHeap *slh;
  Socket socket;
  SocketExtension *sext;
} Fixture;

static void setUp(Fixture *f, int isServer) {
  resetFake();
  memset(&f->socket, 0, sizeof(f->socket));
  f->socket.isServer = isServer;
  f->socket.tlsFlags = RS_TLS_WANT_TLS | RS_TLS_WANT_PEERCERT;
  f->slh = makeShortLivedHeap(65536, 8);
  f->sext = makeSocketExtension(&f->socket, f->slh, FALSE, NULL, 1024);
}

static void tearDown(Fixture *f) {
  safeFree31((char*)f->sext, sizeof(SocketExtension));
  SLHFree(f->slh);
}

static int hasFlag(Fixture *f, int flag) {
  return (f->sext->tlsFlags & flag) != 0;
}

static int certMatches(Fixture *f) {
  return f->sext->peerCertificateLength == fake.certLength &&
         0 == memcmp(f->sext->peerCertificate, fake.cert, fake.certLength);
}

static void testSecureWithCertificate(void) {
  Fixture f;
  setUp(&f, FALSE);
  setFakeCert(900);
  int sts = sxUpdateTLSInfo(f.sext, 1);
  CHECK(sts == 0, "secure connection: returns 0");
  CHECK(hasFlag(&f, RS_TLS_HAVE_TLS), "secure connection: HAVE_TLS set");
  CHECK(hasFlag(&f, RS_TLS_HAVE_PEERCERT), "secure connection: HAVE_PEERCERT set");
  CHECK(hasFlag(&f, RS_TTLS_IOCTL_DONE), "secure connection: IOCTL_DONE set");
  CHECK(certMatches(&f), "secure connection: certificate bytes match");
  CHECK(fake.callCount == 1, "secure connection: one ioctl");
  tearDown(&f);
}

static void testLargeCertificateRetries(void) {
  Fixture f;
  setUp(&f, FALSE);
  setFakeCert(3000);
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(fake.callCount == 2, "certificate over 2048 bytes: retried once with a bigger buffer");
  CHECK(hasFlag(&f, RS_TLS_HAVE_PEERCERT) && certMatches(&f), "certificate over 2048 bytes: stored in full");
  tearDown(&f);
}

static void testCertificateOverLimit(void) {
  Fixture f;
  setUp(&f, FALSE);
  setFakeCert(70000);
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(fake.callCount == 1, "certificate over the limit: no retry");
  CHECK(!hasFlag(&f, RS_TLS_HAVE_PEERCERT) && f.sext->peerCertificate == NULL, "certificate over the limit: not stored");
  CHECK(hasFlag(&f, RS_TTLS_IOCTL_DONE), "certificate over the limit: not asked again");
  tearDown(&f);
}

static void testHandshakeInProgress(void) {
  Fixture f;
  setUp(&f, FALSE);
  setFakeCert(500);
  fake.statConn = TTLS_CONN_HS_INPROGRESS;
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(!hasFlag(&f, RS_TTLS_IOCTL_DONE), "handshake in progress: IOCTL_DONE stays clear");
  CHECK(!hasFlag(&f, RS_TLS_HAVE_TLS) && !hasFlag(&f, RS_TLS_HAVE_PEERCERT), "handshake in progress: no TLS state yet");

  fake.statConn = TTLS_CONN_SECURE;
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(fake.callCount == 2, "handshake finished: asked again");
  CHECK(hasFlag(&f, RS_TLS_HAVE_TLS) && hasFlag(&f, RS_TLS_HAVE_PEERCERT) && certMatches(&f),
        "handshake finished: certificate picked up");
  tearDown(&f);
}

static void testNotSecure(void) {
  Fixture f;
  setUp(&f, FALSE);
  setFakeCert(500);
  fake.statConn = TTLS_CONN_NOTSECURE;
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(!hasFlag(&f, RS_TLS_HAVE_TLS) && !hasFlag(&f, RS_TLS_HAVE_PEERCERT), "not secure: no TLS state");
  CHECK(hasFlag(&f, RS_TTLS_IOCTL_DONE), "not secure: IOCTL_DONE set");
  tearDown(&f);
}

static void testSecureWithoutCertificate(void) {
  Fixture f;
  setUp(&f, FALSE);
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(hasFlag(&f, RS_TLS_HAVE_TLS), "no client certificate: HAVE_TLS set");
  CHECK(!hasFlag(&f, RS_TLS_HAVE_PEERCERT) && f.sext->peerCertificate == NULL, "no client certificate: none stored");
  tearDown(&f);
}

static void testIoctlFailure(void) {
  Fixture f;
  setUp(&f, FALSE);
  fake.sts = -1;
  fake.errnoValue = EINVAL;
  int sts = sxUpdateTLSInfo(f.sext, 1);
  CHECK(sts == 0, "ioctl failure (not AT-TLS): not reported as an error");
  CHECK(!hasFlag(&f, RS_TLS_HAVE_TLS) && !hasFlag(&f, RS_TLS_HAVE_PEERCERT), "ioctl failure: no TLS state");
  CHECK(hasFlag(&f, RS_TTLS_IOCTL_DONE), "ioctl failure: IOCTL_DONE set");
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(fake.callCount == 1, "ioctl failure: not retried on the next read");
  tearDown(&f);
}

static void testPassThruCertificateRejected(void) {
  Fixture f;
  setUp(&f, FALSE);
  setFakeCert(500);
  fake.secType = TTLS_SEC_SRV_CA_PASS;
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(hasFlag(&f, RS_TLS_HAVE_TLS), "PassThru: connection still counts as TLS");
  CHECK(!hasFlag(&f, RS_TLS_HAVE_PEERCERT) && f.sext->peerCertificate == NULL, "PassThru: unvalidated certificate not kept");
  tearDown(&f);
}

static void testValidatingAuthTypesAccepted(void) {
  int types[] = { TTLS_SEC_SRV_CA_FULL, TTLS_SEC_SRV_CA_REQD, TTLS_SEC_SRV_CA_SAFCHK };
  const char *names[] = { "Full", "Required", "SAFCheck" };
  for (int i = 0; i < 3; i++) {
    Fixture f;
    char name[64];
    setUp(&f, FALSE);
    setFakeCert(500);
    fake.secType = types[i];
    sxUpdateTLSInfo(f.sext, 1);
    snprintf(name, sizeof(name), "%s client auth: certificate kept", names[i]);
    CHECK(hasFlag(&f, RS_TLS_HAVE_PEERCERT) && certMatches(&f), name);
    tearDown(&f);
  }
}

static void testOnceOnly(void) {
  Fixture f;
  setUp(&f, FALSE);
  setFakeCert(500);
  sxUpdateTLSInfo(f.sext, 1);
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(fake.callCount == 1, "onceOnly: second call makes no ioctl");
  sxUpdateTLSInfo(f.sext, 0);
  CHECK(fake.callCount == 2, "not onceOnly: second call asks again");
  tearDown(&f);
}

static void testGskSocketSkipsIoctl(void) {
  Fixture f;
  TlsSocket gskSocket;
  setUp(&f, FALSE);
  memset(&gskSocket, 0, sizeof(gskSocket));
  f.socket.tlsSocket = &gskSocket;
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(fake.callCount == 0, "GSK socket: no AT-TLS ioctl");
  CHECK(hasFlag(&f, RS_TLS_HAVE_TLS), "GSK socket: HAVE_TLS set");
  tearDown(&f);
}

static void testServerSocketSkipsIoctl(void) {
  Fixture f;
  setUp(&f, TRUE);
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(fake.callCount == 0, "listener socket: no AT-TLS ioctl");
  tearDown(&f);
}

static void testGetPeerCertificate(void) {
  Fixture f;
  char buffer[4096];
  unsigned int length = 99;
  setUp(&f, FALSE);

  CHECK(sxGetPeerCertificate(f.sext, buffer, sizeof(buffer), &length) == SX_PEERCERT_NONE && length == 0,
        "sxGetPeerCertificate: none before one is stored");

  setFakeCert(600);
  sxUpdateTLSInfo(f.sext, 1);
  CHECK(sxGetPeerCertificate(f.sext, buffer, sizeof(buffer), &length) == SX_PEERCERT_OK &&
        length == 600 && 0 == memcmp(buffer, fake.cert, 600),
        "sxGetPeerCertificate: returns the stored certificate");
  CHECK(sxGetPeerCertificate(f.sext, buffer, 599, &length) == SX_PEERCERT_BUFFER_TOO_SMALL && length == 0,
        "sxGetPeerCertificate: buffer one byte too small");
  CHECK(sxGetPeerCertificate(f.sext, buffer, 600, &length) == SX_PEERCERT_OK,
        "sxGetPeerCertificate: buffer exactly the right size");
  CHECK(sxGetPeerCertificate(NULL, buffer, sizeof(buffer), &length) == SX_PEERCERT_BAD_ARGUMENT,
        "sxGetPeerCertificate: NULL extension");
  CHECK(sxGetPeerCertificate(f.sext, NULL, sizeof(buffer), &length) == SX_PEERCERT_BAD_ARGUMENT,
        "sxGetPeerCertificate: NULL buffer");
  CHECK(sxGetPeerCertificate(f.sext, buffer, sizeof(buffer), NULL) == SX_PEERCERT_BAD_ARGUMENT,
        "sxGetPeerCertificate: NULL length");

  f.sext->tlsFlags &= ~RS_TLS_HAVE_PEERCERT;
  CHECK(sxGetPeerCertificate(f.sext, buffer, sizeof(buffer), &length) == SX_PEERCERT_NONE,
        "sxGetPeerCertificate: stale data ignored without HAVE_PEERCERT");
  tearDown(&f);
}

int main(int argc, char **argv) {
  testSecureWithCertificate();
  testLargeCertificateRetries();
  testCertificateOverLimit();
  testHandshakeInProgress();
  testNotSecure();
  testSecureWithoutCertificate();
  testIoctlFailure();
  testPassThruCertificateRejected();
  testValidatingAuthTypesAccepted();
  testOnceOnly();
  testGskSocketSkipsIoctl();
  testServerSocketSkipsIoctl();
  testGetPeerCertificate();

  if (failures > 0) {
    printf("%d check(s) failed\n", failures);
    return 1;
  }
  puts("all checks passed");
  return 0;
}
