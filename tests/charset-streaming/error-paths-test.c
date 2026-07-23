/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this
  distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

/* error-paths-test.c -- regression tests for the streaming-conversion ERROR
 * paths raised in the zowe-common-c#630 review:
 *
 *   issue 1: a caller-forced CCSID that getCharsetCode accepts but the
 *            converter cannot open used to be masked by streamTextForFile2,
 *            producing HTTP 200 with an EMPTY body (the #828 symptom on the
 *            new forced-encoding path). Fixed by validating the pair up front
 *            (isCharsetStreamingPairSupported -> 400) plus a hard-error abort
 *            in the streaming loop.
 *   issue 2: a conversion that expands past the output buffer returns
 *            CHARSET_SHORT_BUFFER with partial progress; the caller used to
 *            drop the unconsumed remainder (silent truncation). Fixed by
 *            drain-and-recall: emit, then convert the rest.
 *   issue 3: getCharsetCode understands ~66 names but getCharsetName can open
 *            converters for far fewer; that asymmetry is what makes issue 1
 *            reachable, and isCharsetStreamingPairSupported is the guard that
 *            makes it harmless.
 *
 * streamTextForFile2 itself lives in httpserver.c (not yet compilable
 * off-platform), so modelServerLoop below is a faithful copy of its FIXED
 * read/convert/emit loop. Keep them in sync: a change to one belongs in the
 * other. All cases here assert the fixed behavior and run under ASan (run.sh)
 * and on z/OS (run-zos.sh).
 */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "zowetypes.h"
#include "utils.h"
#include "charsets.h"

static int fails = 0;
static int checks = 0;
#define OK(cond, ...) do{ checks++; if(!(cond)){ printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } }while(0)

/* Single-shot conversion into a large buffer: the expected-output oracle. */
static int oracle(const unsigned char *in, int n, int src, int tgt,
                  unsigned char *out, int cap){
  int outLen = 0;
  int consumed = 0;
  int reason = 0;
  convertCharsetStreaming((char*)in, n, src, (char*)out, cap, tgt, &outLen, &consumed, &reason);
  return outLen;
}

/* Faithful copy of streamTextForFile2's FIXED conversion loop (httpserver.c):
 * per read, drain-and-recall on SHORT_BUFFER (emitting per converter call),
 * carry a trailing partial character (< sizeof pending) to the next read, and
 * abort with a failure status on a hard converter error instead of masking it.
 * readChunk models the file read size; convCap models the translation buffer. */
static int modelServerLoop(const unsigned char *in, int n, int readChunk, int convCap,
                           int src, int tgt, unsigned char *out, int outCap,
                           int *emitted){
  unsigned char pending[8];
  int pendingLen = 0;
  unsigned char buf[8192];
  unsigned char conv[8192];
  int pos = 0;
  int outTotal = 0;
  *emitted = 0;
  while (pos < n || pendingLen > 0){
    int take = readChunk - pendingLen;
    if (take > n - pos) take = n - pos;
    if (take < 0) take = 0;
    memcpy(buf, pending, pendingLen);
    memcpy(buf + pendingLen, in + pos, take);
    int inTotal = pendingLen + take;
    pos += take;
    pendingLen = 0;
    if (inTotal == 0) break;

    int off = 0;
    while (off < inTotal){
      int outLen = 0;
      int consumed = 0;
      int reason = 0;
      int rc = convertCharsetStreaming((char*)buf + off, inTotal - off, src,
                                       (char*)conv, convCap, tgt,
                                       &outLen, &consumed, &reason);
      if (outLen > 0 && outTotal + outLen <= outCap){
        memcpy(out + outTotal, conv, outLen);
        outTotal += outLen;
      }
      off += consumed;
      if (rc == CHARSET_CONVERSION_SUCCESS){
        break;
      } else if (rc == CHARSET_SHORT_BUFFER && consumed > 0){
        continue;             /* drain-and-recall */
      } else {
        *emitted = outTotal;
        return rc;            /* hard error: surface it, do not mask */
      }
    }
    int leftover = inTotal - off;
    if (leftover > 0){
      if (pos < n && leftover <= (int)sizeof(pending)){
        memcpy(pending, buf + off, leftover);
        pendingLen = leftover;
      } else if (pos >= n){
        break;                /* true EOF with an incomplete tail: drop it */
      } else {
        *emitted = outTotal;
        return CHARSET_INTERNAL_ERROR;  /* large unconsumed tail: never drop silently */
      }
    }
  }
  *emitted = outTotal;
  return CHARSET_CONVERSION_SUCCESS;
}

int main(void){
  printf("== issue 3: code-table vs converter-name asymmetry is guarded ==\n");
  int code37 = getCharsetCode("IBM-37");
  OK(code37 == 37, "getCharsetCode(\"IBM-37\") == 37 (name parsing accepts it), got %d", code37);
  OK(!isCharsetStreamingPairSupported(CCSID_UTF_8, code37),
     "pair (UTF-8 -> 37) reported unsupported: the guard a caller uses for a 400");
  OK(isCharsetStreamingPairSupported(CCSID_UTF_8, CCSID_ISO_8859_1),
     "pair (UTF-8 -> 819) reported supported");
  OK(isCharsetStreamingPairSupported(code37, code37),
     "identity pair (37 -> 37) supported: streamed without opening a converter");

  printf("\n== review follow-up: strict numeric parsing of encoding values ==\n");
  {
    /* sscanf("%d") accepted trailing junk ("1047foo" -> 1047); the strict
       parser and parseEncodingValue must reject anything but a whole int.
       Table-driven abuse of every edge: signs, both exact int boundaries
       (the negative guard depends on C99 truncation-toward-zero division),
       whitespace in every position, doubled signs, leading zeros, non-digit
       high bytes. expectRc 0 rows also pin the parsed value. */
    static const struct { const char *in; int expectRc; int expectVal; } CASES[] = {
      { "1047",        0, 1047        },
      { "0",           0, 0           },
      { "007",         0, 7           },  /* leading zeros: digits, so accepted */
      { "+1047",       0, 1047        },
      { "-1",          0, -1          },
      { "2147483647",  0, INT_MAX     },  /* INT_MAX exactly */
      { "-2147483648", 0, INT_MIN     },  /* INT_MIN exactly */
      { "2147483648",  -1, 0 },  /* INT_MAX + 1 */
      { "-2147483649", -1, 0 },  /* INT_MIN - 1 */
      { "99999999999", -1, 0 },  /* far overflow */
      { "",            -1, 0 },
      { "+",           -1, 0 },  /* bare sign */
      { "-",           -1, 0 },
      { "--1",         -1, 0 },
      { "+-1",         -1, 0 },
      { " 1047",       -1, 0 },  /* leading whitespace (strtol would accept) */
      { "1047 ",       -1, 0 },
      { "10 47",       -1, 0 },
      { "1047foo",     -1, 0 },
      { "foo",         -1, 0 },
      { "12.5",        -1, 0 },
      { "0x10",        -1, 0 },
      { "1-",          -1, 0 },
      { "\xFF\xFE",    -1, 0 },  /* high bytes: & 0xff path, must not misparse */
    };
    int n = (int)(sizeof(CASES) / sizeof(CASES[0]));
    for (int i = 0; i < n; i++){
      int v = 12345;
      int rc = parseIntSafely(CASES[i].in, &v);
      if (CASES[i].expectRc == 0){
        OK(rc == 0 && v == CASES[i].expectVal,
           "parseIntSafely(\"%s\") == %d (rc=%d v=%d)", CASES[i].in, CASES[i].expectVal, rc, v);
      } else {
        OK(rc == -1, "parseIntSafely(\"%s\") rejected (rc=%d v=%d)", CASES[i].in, rc, v);
      }
    }
    int v = 0;
    OK(parseIntSafely(NULL, &v) == -1, "parseIntSafely(NULL, out) rejected");
    OK(parseIntSafely("1", NULL) == -1, "parseIntSafely(str, NULL) rejected");

    /* the user-facing contract in parseEncodingValue (range 1-65535 on top) */
    OK(parseEncodingValue("1047") == 1047, "parseEncodingValue still accepts a plain CCSID");
    OK(parseEncodingValue("1047foo") == -1, "parseEncodingValue rejects '1047foo' (was 1047 via sscanf)");
    OK(parseEncodingValue("65535") == 65535, "parseEncodingValue accepts 65535 (top of range)");
    OK(parseEncodingValue("65536") == -1, "parseEncodingValue rejects 65536 (over range)");
    OK(parseEncodingValue("0") == -1, "parseEncodingValue rejects 0 (under range)");
    OK(parseEncodingValue("-819") == -1, "parseEncodingValue rejects negatives");
  }

  printf("\n== issue 1: hard converter errors are surfaced, not masked ==\n");
  {
    unsigned char in[] = {0x61, 0x62, 0x63};
    unsigned char out[64];
    int outLen = 0;
    int consumed = 0;
    int reason = 0;
    int rc = convertCharsetStreaming((char*)in, 3, CCSID_UTF_8, (char*)out, sizeof(out),
                                     code37, &outLen, &consumed, &reason);
    OK(rc == CHARSET_UNKNOWN_CCSID,
       "converting to a code-valid but un-openable CCSID returns CHARSET_UNKNOWN_CCSID, got %d", rc);
    OK(outLen == 0 && consumed == 0,
       "no partial output on that failure (outLen=%d consumed=%d)", outLen, consumed);
  }
  {
    /* the fixed server loop reports the failure to its caller */
    unsigned char in[] = {0x48,0x65,0x6c,0x6c,0x6f}; /* "Hello" */
    unsigned char out[256];
    int emitted = -1;
    int status = modelServerLoop(in, 5, 64, 128, CCSID_UTF_8, code37, out, sizeof(out), &emitted);
    OK(status != CHARSET_CONVERSION_SUCCESS,
       "fixed server loop returns a failure status (got %d), never a quiet empty success", status);
    OK(emitted == 0, "and emitted nothing for it (emitted=%d)", emitted);
  }

  printf("\n== issue 2: SHORT_BUFFER drains instead of truncating ==\n");
  {
    /* 819 e-acute x3 -> UTF-8 doubles: 3 in -> 6 out. A 3-byte conversion
       buffer forces SHORT_BUFFER mid-read; the fixed loop must still deliver
       every byte the oracle does. */
    unsigned char in[] = {0xE9, 0xE9, 0xE9};
    unsigned char full[64];
    int oraLen = oracle(in, 3, CCSID_ISO_8859_1, CCSID_UTF_8, full, sizeof(full));
    OK(oraLen == 6, "oracle: 3 x 0xE9 (819) -> 6 UTF-8 bytes, got %d", oraLen);

    unsigned char out[64];
    int emitted = -1;
    int status = modelServerLoop(in, 3, 3, 3, CCSID_ISO_8859_1, CCSID_UTF_8,
                                 out, sizeof(out), &emitted);
    OK(status == CHARSET_CONVERSION_SUCCESS, "fixed loop succeeds through SHORT_BUFFER (status=%d)", status);
    OK(emitted == oraLen && memcmp(out, full, oraLen) == 0,
       "drain-and-recall delivered all %d bytes (got %d) -- was 4 of 6 before the fix", oraLen, emitted);

    /* converter-level contract underneath the loop */
    int outLen = 0;
    int consumed = 0;
    int reason = 0;
    int rc = convertCharsetStreaming((char*)in, 3, CCSID_ISO_8859_1, (char*)out, 3,
                                     CCSID_UTF_8, &outLen, &consumed, &reason);
    OK(rc == CHARSET_SHORT_BUFFER && consumed > 0 && consumed < 3,
       "converter signals SHORT_BUFFER with partial progress (rc=%d consumed=%d outLen=%d)",
       rc, consumed, outLen);
  }

  printf("\n== regression: valid multibyte straddle still carries across reads ==\n");
  {
    /* e-acute split across a 4-byte read boundary must survive the fixed loop */
    unsigned char in[] = {0x61,0x62,0x63,0xC3,0xA9,0x7A}; /* abc e-acute z */
    unsigned char full[64];
    int oraLen = oracle(in, 6, CCSID_UTF_8, CCSID_ISO_8859_1, full, sizeof(full));
    unsigned char out[64];
    int emitted = -1;
    int status = modelServerLoop(in, 6, 4, 64, CCSID_UTF_8, CCSID_ISO_8859_1,
                                 out, sizeof(out), &emitted);
    OK(status == CHARSET_CONVERSION_SUCCESS && emitted == oraLen && memcmp(out, full, oraLen) == 0,
       "straddled char intact through drain-capable loop (status=%d got=%d want=%d)",
       status, emitted, oraLen);
  }

  printf("\n%s  (%d/%d checks passed)\n", fails == 0 ? "ALL PASS" : "FAILURES", checks - fails, checks);
  return fails;
}
