/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this
  distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

/*
 * test_troo.c -- clang-readiness smoke test for the __troo helper used in
 * the IBM-1047 -> ISO-8859-1 fast path of c/charsets.c.
 *
 * Under xlclang the __troo helper was a builtin (#pragma linkage(__troo,
 * builtin)) that lowered directly to the HLASM TROO instruction. Under
 * Open XL / ibm-clang64 there is no such builtin; we replaced it with a
 * static inline-asm TROO emitter (see project_clang_readiness_helpers.md
 * for the engineering rationale).
 *
 * This test exercises the fast path by asking convertCharset() to convert
 * a known EBCDIC string to ASCII and verifying the bytes. If __troo is
 * lowered correctly under either compiler, the bytes round-trip; if it is
 * mis-lowered (wrong register pinning, missing retry loop, or wrong M3
 * mask), the output is silently corrupted and the memcmp catches it.
 */

#include <stdio.h>
#include <string.h>

#include "zowetypes.h"
#include "charsets.h"

int main(int argc, char **argv) {
  /* "HELLO" in IBM-1047 EBCDIC -- 0xC8 0xC5 0xD3 0xD3 0xD6 */
  const char src_ebcdic[5] = { (char)0xC8, (char)0xC5, (char)0xD3,
                               (char)0xD3, (char)0xD6 };
  /* "HELLO" in ASCII / ISO-8859-1 -- 0x48 0x45 0x4C 0x4C 0x4F.
   * Hex bytes (NOT 'H','E','L','L','O') because source-level char literals
   * compile to whatever -fexec-charset is in effect; on z/OS this build
   * uses IBM-1047, so 'H' would expand to 0xC8 -- the wrong byte for the
   * comparison against the converted output. */
  const char expected_ascii[5] = { (char)0x48, (char)0x45, (char)0x4C,
                                   (char)0x4C, (char)0x4F };

  char buf[16];
  char *out = buf;
  int  outLen = 0;
  int  reasonCode = 0;
  memset(buf, 0, sizeof(buf));

  /* Target CCSID is UTF-8 specifically: that is the ONE input/output
   * pair where convertCharset() takes the __troo fast path under
   * ibm-clang64 (see c/charsets.c:289). For the basic-ASCII string
   * "HELLO", UTF-8 and ISO-8859-1 produce the same bytes, so this
   * choice doesn't change the expected output -- only the code path
   * exercised. Under xlclang the fast path is not even compiled in;
   * convertCharset routes to iconv there, which is a different test
   * (and may or may not work depending on installed iconv tables). */
  int rc = convertCharset((char *)src_ebcdic,
                          (int)sizeof(src_ebcdic),
                          CCSID_IBM1047,
                          CHARSET_OUTPUT_USE_BUFFER,
                          &out,
                          (int)sizeof(buf),
                          CCSID_UTF_8,
                          NULL,            /* slh -- not used in BUFFER mode */
                          &outLen,
                          &reasonCode);

  if (rc != 0) {
    printf("FAIL: convertCharset rc=%d reasonCode=%d\n", rc, reasonCode);
    return 1;
  }
  if (outLen != (int)sizeof(expected_ascii)) {
    printf("FAIL: outLen=%d expected=%d\n",
           outLen, (int)sizeof(expected_ascii));
    return 1;
  }
  if (memcmp(out, expected_ascii, outLen) != 0) {
    printf("FAIL: bytes mismatch -- got");
    for (int i = 0; i < outLen; i++) printf(" %02x", (unsigned char)out[i]);
    printf("\n  expected");
    for (int i = 0; i < (int)sizeof(expected_ascii); i++) {
      printf(" %02x", (unsigned char)expected_ascii[i]);
    }
    printf("\n");
    return 1;
  }

  printf("PASS: EBCDIC 'HELLO' -> ASCII 'HELLO' (%d bytes via __troo)\n",
         outLen);
  return 0;
}
