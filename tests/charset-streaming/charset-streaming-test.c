/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this
  distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

/* charset-streaming-test.c
 *
 * Regression tests for convertCharsetStreaming() -- the streaming charset
 * converter behind the #828 fix. Covers the two behaviours that were broken:
 *
 *   1. CARRY-FORWARD: a multibyte character split across a read-buffer boundary
 *      must be reassembled, never lost or corrupted. The centrepiece is an
 *      *exhaustive* sweep: a mixed 1/2/3/4-byte string is streamed at EVERY
 *      chunk size and the concatenated output must equal the single-shot
 *      conversion -- i.e. the result is independent of where chunks split.
 *
 *   2. SUBSTITUTION: characters the target encoding cannot represent become a
 *      replacement ('?'/TRANSLIT) instead of failing the whole response (the
 *      #828 silent empty body); representable characters are preserved.
 *
 * Runs under AddressSanitizer. See run.sh.
 */
#include <stdio.h>
#include <string.h>
#include "zowetypes.h"
#include "charsets.h"

static int fails = 0;
static int checks = 0;
#define OK(cond, ...) do{ checks++; if(!(cond)){ printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } }while(0)

static void hx(const char *label, const unsigned char *b, int n){
  printf("    %-8s(%2d): ", label, n);
  for (int i = 0; i < n; i++) printf("%02x ", b[i]);
  printf("\n");
}

/* A byte that stands in for an unconvertible character. z/OS iconv substitutes
 * an unmappable char with SUB (0x1A); glibc //TRANSLIT and our manual EILSEQ
 * path use '?' (0x3F). Accept either so this test is meaningful on both WSL and
 * z/OS. (Compile the driver with an ASCII exec-charset so its char literals are
 * ASCII/819 bytes matching the converted output -- see run-zos.sh.) */
static int isSub(unsigned char b){ return b == 0x1A || b == 0x3F; }

/* memcmp variant where a 0x3F in `expect` means "any substitute byte" -- this
 * test only places 0x3F where a character is expected to be substituted. */
static int eqSub(const unsigned char *got, const unsigned char *expect, int n){
  for (int i = 0; i < n; i++){
    if (expect[i] == 0x3F){ if (!isSub(got[i])) return 0; }
    else if (got[i] != expect[i]) return 0;
  }
  return 1;
}

/* Single-shot conversion -- the oracle the chunked path must match. */
static int oracle(const unsigned char *in, int n, int src, int tgt,
                  unsigned char *out, int cap){
  int outLen = 0;
  int consumed = 0;
  int reason = 0;
  convertCharsetStreaming((char*)in, n, src, (char*)out, cap, tgt, &outLen, &consumed, &reason);
  return outLen;
}

/* Replicate streamTextForFile2's carry-forward loop: feed [in] through
 * convertCharsetStreaming in chunks of chunkSize, holding an incomplete
 * trailing multibyte sequence and prepending it to the next chunk. A leftover
 * at true EOF is dropped (matching the server's EOF handling). */
static int streamConv(const unsigned char *in, int n, int chunkSize, int src, int tgt,
                      unsigned char *out, int cap){
  unsigned char pending[8];
  int pendingLen = 0;
  unsigned char buf[8192];
  int pos = 0;
  int outTotal = 0;
  while (pos < n || pendingLen > 0){
    int take = chunkSize;               /* read chunkSize NEW bytes each pass (fixed-size read) */
    if (take > n - pos) take = n - pos;
    memcpy(buf, pending, pendingLen);
    memcpy(buf + pendingLen, in + pos, take);
    int inTotal = pendingLen + take;
    pos += take;
    pendingLen = 0;
    if (inTotal == 0) break;

    int outLen = 0;
    int consumed = 0;
    int reason = 0;
    convertCharsetStreaming((char*)buf, inTotal, src, (char*)(out + outTotal), cap - outTotal,
                            tgt, &outLen, &consumed, &reason);
    outTotal += outLen;

    int leftover = inTotal - consumed;
    if (leftover > 0){
      if (pos < n && leftover <= (int)sizeof(pending)){
        memcpy(pending, buf + consumed, leftover);
        pendingLen = leftover;
      } else {
        /* true EOF with an incomplete trailing sequence: drop it, matching
           streamTextForFile2 -- a well-formed source never reaches here (its
           completing bytes exist), so only truncated/mistagged input loses a
           tail, exactly as the pre-fix server did (no base64-unsafe substitute). */
        break;
      }
    }
  }
  return outTotal;
}

int main(void){
  unsigned char o1[1024];
  unsigned char o2[1024];
  int streamLen;

  printf("== absolute-correctness anchors (UTF-8 -> ISO-8859-1) ==\n");
  { unsigned char in[] = {0x61, 0x61, 0x61, 0xC3};   /* "aaa" + partial */
    int c = 0;
    int o = 0;
    int r = 0;
    convertCharsetStreaming((char*)in, 4, CCSID_UTF_8, (char*)o1, sizeof(o1), CCSID_ISO_8859_1, &o, &c, &r);
    OK(c == 3 && o == 3, "T1 straddle: hold partial 0xC3 (out=%d consumed=%d)", o, c); }
  { unsigned char in[] = {0xC3, 0xA9, 0x54};         /* e-acute + 'T'(0x54) */
    int c = 0;
    int o = 0;
    int r = 0;
    convertCharsetStreaming((char*)in, 3, CCSID_UTF_8, (char*)o1, sizeof(o1), CCSID_ISO_8859_1, &o, &c, &r);
    OK(o == 2 && o1[0] == 0xE9 && o1[1] == 0x54, "T2 complete: C3A9->0xE9 + T"); }
  { unsigned char in[] = {0xE2, 0x98, 0x95};
    int c = 0;
    int o = 0;
    int r = 0;
    convertCharsetStreaming((char*)in, 3, CCSID_UTF_8, (char*)o1, sizeof(o1), CCSID_ISO_8859_1, &o, &c, &r);
    OK(o == 1 && isSub(o1[0]), "T3 substitute: coffee -> substitute (0x1A z/OS / 0x3F WSL)"); }
  { unsigned char in[] = {0xC3, 0xA9};
    int c = 0;
    int o = 0;
    int r = 0;
    convertCharsetStreaming((char*)in, 2, CCSID_UTF_8, (char*)o1, sizeof(o1), CCSID_ISO_8859_1, &o, &c, &r);
    OK(o == 1 && o1[0] == 0xE9, "T4 representable: e-acute -> real 0xE9 (not substituted)"); }

  printf("\n== EXHAUSTIVE STRADDLE SWEEP (every chunk size) ==\n");
  /* mixed 1/2/3/4-byte UTF-8: ASCII + e-acute + a-grave (mappable) + coffee-cup
     + CJK ni-hon + emoji (unmappable). ALL bytes numeric so no char literal lands
     in EBCDIC when this compiles with -fexec-charset=IBM-1047 on z/OS. */
  unsigned char ref[] = {
    0x48,0x65,0x6C,0x6C,0x6F,0x20,      /* "Hello "                        */
    0xC3,0xA9,0x20,                     /* U+00E9 e-acute  2B -> 819 0xE9  */
    0xC3,0xA0,0x20,                     /* U+00E0 a-grave  2B -> 819 0xE0  */
    0xE2,0x98,0x95,0x20,                /* U+2615 coffee   3B -> subst     */
    0xE6,0x97,0xA5,0xE6,0x9C,0xAC,0x20, /* U+65E5 U+672C   3Bx2 -> subst   */
    0xF0,0x9F,0x98,0x80,0x20,           /* U+1F600 emoji   4B -> subst     */
    0x57,0x6F,0x72,0x6C,0x64            /* "World"                         */
  };
  int reflen = (int)sizeof(ref);
  /* expected 819 output; 0x3F marks substitute positions (eqSub treats a 0x3F in
     expect as "any substitute" -- 0x1A on z/OS iconv, 0x3F on WSL). */
  unsigned char expect[] = {0x48,0x65,0x6C,0x6C,0x6F,0x20, 0xE9,0x20, 0xE0,0x20,
                            0x3F,0x20, 0x3F,0x3F,0x20, 0x3F,0x20, 0x57,0x6F,0x72,0x6C,0x64};
  int explen = (int)sizeof(expect);

  int oraLen = oracle(ref, reflen, CCSID_UTF_8, CCSID_ISO_8859_1, o1, sizeof(o1));
  hx("oracle", o1, oraLen);
  OK(oraLen == explen && eqSub(o1, expect, explen), "oracle matches hand-computed 819 output (substitute-tolerant)");

  int mismatches = 0;
  int firstBad = -1;
  for (int chunk = 1; chunk <= reflen; chunk++){
    streamLen = streamConv(ref, reflen, chunk, CCSID_UTF_8, CCSID_ISO_8859_1, o2, sizeof(o2));
    if (streamLen != oraLen || memcmp(o1, o2, oraLen) != 0){
      mismatches++;
      if (firstBad < 0){
        firstBad = chunk;
        printf("    first mismatch at chunk=%d:\n", chunk);
        hx("got", o2, streamLen);
      }
    }
  }
  OK(mismatches == 0, "chunked == single-shot for ALL %d chunk sizes (%d mismatched, first@%d)",
     reflen, mismatches, firstBad);

  printf("\n== edge cases ==\n");
  { /* file ends mid-character (truncated / mistagged source): the incomplete
       trailing byte is held across the boundary, then dropped at true EOF --
       a clean prefix with no corruption and no crash. */
    unsigned char in[] = {0x61, 0x62, 0x63, 0xC3};   /* "abc" + partial */
    streamLen = streamConv(in, 4, 2, CCSID_UTF_8, CCSID_ISO_8859_1, o2, sizeof(o2));
    OK(streamLen == 3 && o2[0] == 0x61 && o2[1] == 0x62 && o2[2] == 0x63, "EOF truncation: abc, partial dropped (got %d)", streamLen);
    hx("trunc", o2, streamLen); }
  { /* invalid source byte in the middle -> substitute and keep going */
    unsigned char in[] = {0x61, 0xFF, 0x62};   /* 'a' + invalid UTF-8 + 'b' */
    streamLen = streamConv(in, 3, 1, CCSID_UTF_8, CCSID_ISO_8859_1, o2, sizeof(o2));
    OK(streamLen == 3 && o2[0] == 0x61 && isSub(o2[1]) && o2[2] == 0x62, "invalid byte -> substitute + continue (got %d)", streamLen);
    hx("badbyte", o2, streamLen); }
  { /* pure ASCII, mappable, no substitution -- identity through chunking */
    unsigned char in[] = {0x70, 0x6C, 0x61, 0x69, 0x6E};   /* "plain" */
    int mm = 0;
    int ol = oracle(in, 5, CCSID_UTF_8, CCSID_ISO_8859_1, o1, sizeof(o1));
    for (int ch = 1; ch <= 5; ch++){
      streamLen = streamConv(in, 5, ch, CCSID_UTF_8, CCSID_ISO_8859_1, o2, sizeof(o2));
      if (streamLen != ol || memcmp(o1, o2, ol)) mm++;
    }
    OK(mm == 0 && ol == 5, "ascii identity across all chunk sizes"); }

  printf("\n%s  (%d/%d checks passed)\n", fails == 0 ? "ALL PASS" : "FAILURES", checks - fails, checks);
  return fails;
}
