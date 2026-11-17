/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * Runs the real SIOCTTLSCTL ioctl through sxUpdateTLSInfo() on an accepted loopback connection.
 *
 *   attls_live_test --serverHost 127.0.0.1 --serverPort 8555 [--expect-attls]
 *
 * Without --expect-attls the port must NOT be covered by AT-TLS policy; the test passes when the
 * ioctl fails cleanly and no TLS state or certificate is recorded.
 *
 * With --expect-attls the port must be covered by an Inbound ServerWithClientAuth rule and an
 * Outbound rule for the connecting client (this process), so that the loopback handshake completes.
 * The test passes when the connection is reported as TLS. A certificate is only expected when the
 * client rule supplies one, so its presence is reported but not required.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "bpxnet.h"
#include "fdpoll.h"
#include "socketmgmt.h"

#define POLL_TIME_MS 5000

static char *getKeywordArg(const char *key, int argc, char **argv) {
  for (int i = 1; i < argc - 1; i++) {
    if (!strcmp(argv[i], key)) {
      return argv[i + 1];
    }
  }
  return NULL;
}

static int hasFlag(int argc, char **argv, const char *flag) {
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], flag)) {
      return TRUE;
    }
  }
  return FALSE;
}

int main(int argc, char **argv) {
  char *host = getKeywordArg("--serverHost", argc, argv);
  char *portArg = getKeywordArg("--serverPort", argc, argv);
  int expectAttls = hasFlag(argc, argv, "--expect-attls");
  int returnCode = 0;
  int reasonCode = 0;
  int failed = 0;

  if (host == NULL || portArg == NULL) {
    printf("Usage: attls_live_test --serverHost host --serverPort port [--expect-attls]\n");
    return 1;
  }
  int port = atoi(portArg);

  InetAddr *address = getAddressByName(host);
  if (address == NULL) {
    printf("[FAILED] could not resolve %s\n", host);
    return 1;
  }
  Socket *server = tcpServer2(address, port, 0, &returnCode, &reasonCode);
  if (server == NULL) {
    printf("[FAILED] could not listen on %s:%d (rc=%d, rsn=0x%x)\n", host, port, returnCode, reasonCode);
    return 1;
  }

  SocketAddress *serverAddress = makeSocketAddr(address, port);
  Socket *client = tcpClient2(serverAddress, 1000 * 10, &returnCode, &reasonCode);
  if (client == NULL) {
    printf("[FAILED] could not connect to %s:%d (rc=%d, rsn=0x%x)\n", host, port, returnCode, reasonCode);
    return 1;
  }

  PollItem item = {0};
  item.fd = server->sd;
  item.events = POLLRIN;
  fdPoll(&item, 0, 1, POLL_TIME_MS, &returnCode, &reasonCode);
  Socket *peer = (item.revents & POLLRIN) ? socketAccept(server, &returnCode, &reasonCode) : NULL;
  if (peer == NULL) {
    printf("[FAILED] no connection was accepted (rc=%d, rsn=0x%x)\n", returnCode, reasonCode);
    return 1;
  }

  /* An AT-TLS handshake only completes once the client has sent something. */
  char message[] = "attls live test";
  char received[64] = {0};
  socketWrite(client, message, strlen(message), &returnCode, &reasonCode);
  int bytesRead = socketRead(peer, received, sizeof(received), &returnCode, &reasonCode);
  if (bytesRead <= 0) {
    printf("[FAILED] nothing readable on the accepted connection (rc=%d, rsn=0x%x)\n", returnCode, reasonCode);
    return 1;
  }

  ShortLivedHeap *slh = makeShortLivedHeap(65536, 4);
  peer->tlsFlags = RS_TLS_WANT_TLS | RS_TLS_WANT_PEERCERT;
  SocketExtension *sext = makeSocketExtension(peer, slh, FALSE, NULL, 1024);
  int sts = sxUpdateTLSInfo(sext, 1);

  int haveTls = (sext->tlsFlags & RS_TLS_HAVE_TLS) != 0;
  int haveCert = (sext->tlsFlags & RS_TLS_HAVE_PEERCERT) != 0;
  int done = (sext->tlsFlags & RS_TTLS_IOCTL_DONE) != 0;
  printf("sxUpdateTLSInfo=%d haveTls=%d haveCert=%d certLength=%d ioctlDone=%d\n",
         sts, haveTls, haveCert, sext->peerCertificateLength, done);

  if (sts != 0) {
    printf("[FAILED] sxUpdateTLSInfo reported an error\n");
    failed = 1;
  }
  if (!done) {
    printf("[FAILED] ioctl result was not recorded\n");
    failed = 1;
  }
  if (expectAttls) {
    if (!haveTls) {
      printf("[FAILED] AT-TLS expected but the connection was not reported as TLS\n");
      failed = 1;
    }
    printf("[INFO] client certificate was %s\n", haveCert ? "returned" : "not returned");
  } else if (haveTls || haveCert || sext->peerCertificate != NULL) {
    printf("[FAILED] no AT-TLS expected but TLS state was recorded\n");
    failed = 1;
  }

  socketClose(client, &returnCode, &reasonCode);
  socketFree(client);
  socketClose(peer, &returnCode, &reasonCode);
  socketClose(server, &returnCode, &reasonCode);
  socketFree(server);

  if (!failed) {
    puts("[PASSED] live AT-TLS ioctl behaves as expected");
  }
  return failed;
}
