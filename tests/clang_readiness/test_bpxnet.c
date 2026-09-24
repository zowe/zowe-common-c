/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this
  distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

/*
 * test_bpxnet.c -- clang-readiness smoke test for the bpxskt.c socket
 * layer. Performs a plain HTTP/1.0 GET to example.com:80 and verifies the
 * response begins with an HTTP status line.
 *
 * Exercises:
 *   - socketInit
 *   - getAddressByName (DNS resolution path -> BPX[14]GAI / BPX[14]GHN)
 *   - makeSocketAddr
 *   - tcpClient (BPX[14]SOC + BPX[14]CON)
 *   - socketWrite (BPX[14]WRT)
 *   - socketRead  (BPX[14]RED)
 *   - socketClose (BPX[14]CLO)
 *
 * Charset note: this build runs in EBCDIC compile mode (IBM-1047), so
 * source-level string literals like "HTTP/1." would be EBCDIC bytes, but
 * the response off the wire is plain ASCII. Rather than running the
 * response through convertCharset, the test compares against an explicit
 * ASCII byte sequence and hexdumps the raw response so the human reading
 * the output can verify the bytes are real ASCII HTTP regardless of how
 * the terminal renders them.
 */

#include <stdio.h>
#include <string.h>

#include "zowetypes.h"
#include "bpxnet.h"

#define HOST   "example.com"
#define PORT   80
#define READ_BUFFER_SIZE 4096
#define HEX_DUMP_BYTES   128

/* "HTTP/1." in ASCII / ISO-8859-1, byte-explicit so it works regardless of
 * the source-file -fexec-charset. */
static const unsigned char ASCII_HTTP_1_PREFIX[7] = {
  0x48, 0x54, 0x54, 0x50, 0x2F, 0x31, 0x2E
};

static void hexdump(const unsigned char *p, int n) {
  int i;
  for (i = 0; i < n; i++) {
    printf("%02x", p[i]);
    if ((i & 15) == 15) {
      printf("\n");
    } else if ((i & 1) == 1) {
      printf(" ");
    }
  }
  if ((n & 15) != 0) {
    printf("\n");
  }
}

int main(int argc, char **argv) {
  int rc = 0;
  int reasonCode = 0;

  socketInit("clang_readiness");

  InetAddr *addr = getAddressByName(HOST);
  if (addr == NULL) {
    printf("FAIL: getAddressByName('%s') returned NULL "
           "(no DNS / no network?)\n", HOST);
    return 1;
  }

  SocketAddress *sa = makeSocketAddr(addr, PORT);
  if (sa == NULL) {
    printf("FAIL: makeSocketAddr returned NULL\n");
    return 1;
  }

  Socket *s = tcpClient(sa, &rc, &reasonCode);
  if (s == NULL) {
    printf("FAIL: tcpClient -> rc=%d reasonCode=%d\n", rc, reasonCode);
    return 1;
  }

  /* HTTP/1.0 request with the line breaks expressed in raw ASCII bytes
   * (0x0D 0x0A == CR LF). EBCDIC compile mode would otherwise translate
   * any "\r\n" embedded in a string literal to the IBM-1047 control
   * codes, which the example.com server would not parse as a request. */
  static const unsigned char request[] = {
    0x47, 0x45, 0x54, 0x20, 0x2F, 0x20,                 /* "GET / "    */
    0x48, 0x54, 0x54, 0x50, 0x2F, 0x31, 0x2E, 0x30,     /* "HTTP/1.0"  */
    0x0D, 0x0A,                                         /*  CRLF       */
    0x48, 0x6F, 0x73, 0x74, 0x3A, 0x20,                 /* "Host: "    */
    0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D,  /* "example.com" */
    0x0D, 0x0A,
    0x43, 0x6F, 0x6E, 0x6E, 0x65, 0x63, 0x74, 0x69,     /* "Connecti"  */
    0x6F, 0x6E, 0x3A, 0x20, 0x63, 0x6C, 0x6F, 0x73, 0x65, /* "on: close" */
    0x0D, 0x0A,
    0x0D, 0x0A
  };
  int reqLen = (int)sizeof(request);
  int written = socketWrite(s, (const char *)request, reqLen,
                            &rc, &reasonCode);
  if (written != reqLen) {
    printf("FAIL: socketWrite wrote %d / %d bytes, rc=%d reasonCode=%d\n",
           written, reqLen, rc, reasonCode);
    socketClose(s, &rc, &reasonCode);
    return 1;
  }

  unsigned char buf[READ_BUFFER_SIZE];
  int total = 0;
  for (;;) {
    if (total >= (int)sizeof(buf)) break;
    int got = socketRead(s,
                         (char *)buf + total,
                         (int)sizeof(buf) - total,
                         &rc, &reasonCode);
    if (got <= 0) break;
    total += got;
  }

  socketClose(s, &rc, &reasonCode);

  printf("Read %d bytes from %s:%d\n", total, HOST, PORT);
  if (total > 0) {
    int show = total < HEX_DUMP_BYTES ? total : HEX_DUMP_BYTES;
    printf("--- first %d bytes (hex) ---\n", show);
    hexdump(buf, show);
    printf("--- end ---\n");
  }

  if (total < (int)sizeof(ASCII_HTTP_1_PREFIX) ||
      memcmp(buf, ASCII_HTTP_1_PREFIX, sizeof(ASCII_HTTP_1_PREFIX)) != 0) {
    printf("FAIL: response does not begin with ASCII 'HTTP/1.' "
           "(48 54 54 50 2f 31 2e)\n");
    return 1;
  }

  /* Status digits: bytes [9],[10],[11] of the response are the three
   * ASCII status digits. e.g. ASCII '2','0','0' = 0x32 0x30 0x30. */
  if (total >= 12) {
    printf("PASS: HTTP %02x %02x %02x status from %s "
           "(ASCII; 32 30 30 == 200)\n",
           buf[9], buf[10], buf[11], HOST);
  } else {
    printf("PASS: HTTP/1.x prefix matched but response too short "
           "for a full status code\n");
  }
  return 0;
}
