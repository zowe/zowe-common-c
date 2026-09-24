/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this
  distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

/*
 * test_charconv.c -- clang-readiness BEHAVIORAL test for c/charsets.c.
 * Replaces the old test_troo.c, which baked in a wrong premise.
 *
 * Compiler -> branch routing (zowetypes.h + charsets.c). This is the thing that
 * was mis-wired and that this test now guards:
 *   z/OS xlclang     -> defines __IBMC__ AND __clang__ (AND __ibmxl__)
 *                       -> __ZOWE_COMP_XLCLANG -> ICONV branch
 *   z/OS ibm-clang64 -> defines __clang__ but NOT __IBMC__ (Open XL / LE clang)
 *                       -> __ZOWE_COMP_CLANG   -> ICONV branch   <-- must match xlclang
 *   z/OS metal / old xlc -> __IBMC__ without __clang__
 *                       -> neither             -> CUNLCNV branch (the __troo path)
 *
 * ibm-clang64 used to fall into the CUNLCNV branch via !__ZOWE_COMP_XLCLANG. It
 * compiled (a __troo shim papered over the missing builtin) and passed an
 * ASCII-only smoke test, while silently breaking multibyte conversion and
 * convertCharsetStreaming (CUNLCNV consumes incomplete sequences, drops
 * unmappable chars, returns ROUTINE_FAILURE). This test exercises exactly the
 * cases that differ between iconv and CUNLCNV, under WHATEVER compiler it is
 * built with, so that class of "compiles + ASCII-passes but wrong branch" bug
 * cannot hide: it fails here.
 *
 * All conversion DATA is numeric bytes on purpose. Source-level char literals
 * compile to whatever -fexec-charset is in effect (IBM-1047 on the z/OS build),
 * so 'H' would be 0xC8, not the 0x48 we compare converted output against.
 */

#include <stdio.h>
#include <string.h>

#include "zowetypes.h"
#include "charsets.h"

static int fails = 0;
#define CHECK(cond, ...) do {                    \
    if (cond) { printf("  ok  : "); }            \
    else      { printf("  FAIL: "); fails++; }   \
    printf(__VA_ARGS__); printf("\n");           \
  } while (0)

/* The stand-in byte for a character unmappable in the target: z/OS iconv
 * substitutes SUB (0x1A); glibc //TRANSLIT and our manual path use '?' (0x3F).
 * Accept either so the same test is meaningful on z/OS and on Linux. */
static int isSub(unsigned char b) { return b == 0x1A || b == 0x3F; }

int main(void) {
  unsigned char obuf[64];
  char *out;
  int outLen;
  int reason;
  int o;
  int c;
  int r;

  printf("== convertCharset ==\n");

  /* (1) IBM-1047 "HELLO" -> UTF-8: basic single-byte round trip. Under a z/OS
   *     clang compiler this is the iconv path; under metal it is the __troo
   *     fast path. Either way the bytes must round-trip (the old test_troo
   *     only checked this much -- necessary but not sufficient). */
  {
    unsigned char in[]  = { 0xC8, 0xC5, 0xD3, 0xD3, 0xD6 }; /* HELLO EBCDIC */
    unsigned char exp[] = { 0x48, 0x45, 0x4C, 0x4C, 0x4F }; /* HELLO ASCII  */
    out = (char *)obuf;
    outLen = 0;
    reason = 0;
    int rc = convertCharset((char *)in, 5, CCSID_IBM1047,
                            CHARSET_OUTPUT_USE_BUFFER, &out, sizeof(obuf),
                            CCSID_UTF_8, NULL, &outLen, &reason);
    CHECK(rc == 0 && outLen == 5 && memcmp(out, exp, 5) == 0,
          "IBM-1047 HELLO -> UTF-8 (rc=%d len=%d)", rc, outLen);
  }

  /* (2) UTF-8 e-acute (C3 A9) -> ISO-8859-1: a MULTIBYTE mappable char. iconv
   *     yields 0xE9; the CUNLCNV/metal path mishandles it. This is the check
   *     the ASCII-only smoke test lacked. */
  {
    unsigned char in[] = { 0xC3, 0xA9 };
    out = (char *)obuf;
    outLen = 0;
    reason = 0;
    int rc = convertCharset((char *)in, 2, CCSID_UTF_8,
                            CHARSET_OUTPUT_USE_BUFFER, &out, sizeof(obuf),
                            CCSID_ISO_8859_1, NULL, &outLen, &reason);
    CHECK(rc == 0 && outLen == 1 && (unsigned char)out[0] == 0xE9,
          "UTF-8 e-acute -> 819 0xE9 (rc=%d len=%d b0=0x%02x)",
          rc, outLen, outLen ? (unsigned char)out[0] : 0);
  }

  printf("== convertCharsetStreaming ==\n");

  /* (3) straddle: "aaa" + a lone lead byte 0xC3 must HOLD the partial (POSIX
   *     iconv EINVAL). CUNLCNV consumes it -> consumed=4, breaking carry-forward. */
  {
    unsigned char in[] = { 0x61, 0x61, 0x61, 0xC3 };
    o = 0;
    c = 0;
    r = 0;
    convertCharsetStreaming((char *)in, 4, CCSID_UTF_8,
                            (char *)obuf, sizeof(obuf), CCSID_ISO_8859_1,
                            &o, &c, &r);
    CHECK(c == 3 && o == 3,
          "streaming straddle holds partial C3 (consumed=%d out=%d)", c, o);
  }

  /* (4) resume: the held C3 completed by A9 -> real 0xE9 (reassembled). */
  {
    unsigned char in[] = { 0xC3, 0xA9 };
    o = 0;
    c = 0;
    r = 0;
    convertCharsetStreaming((char *)in, 2, CCSID_UTF_8,
                            (char *)obuf, sizeof(obuf), CCSID_ISO_8859_1,
                            &o, &c, &r);
    CHECK(o == 1 && obuf[0] == 0xE9,
          "streaming resume C3A9 -> 0xE9 (out=%d b0=0x%02x)",
          o, o ? obuf[0] : 0);
  }

  /* (5) unmappable coffee (E2 98 95) -> substitute, NON-empty (the #828 core:
   *     never a silent empty body). CUNLCNV drops it -> out=0. */
  {
    unsigned char in[] = { 0xE2, 0x98, 0x95 };
    o = 0;
    c = 0;
    r = 0;
    convertCharsetStreaming((char *)in, 3, CCSID_UTF_8,
                            (char *)obuf, sizeof(obuf), CCSID_ISO_8859_1,
                            &o, &c, &r);
    CHECK(o == 1 && isSub(obuf[0]),
          "streaming unmappable coffee -> substitute (out=%d b0=0x%02x)",
          o, o ? obuf[0] : 0);
  }

  printf("\n%s (%d failure%s)\n",
         fails == 0 ? "ALL PASS" : "FAILURES", fails, fails == 1 ? "" : "s");
  return fails;
}
