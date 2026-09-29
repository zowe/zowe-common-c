/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this
  distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

/* json-conversion-test.c -- tests for the jsonPrinter error-flag split.
 *
 * Before the fix: if one value could not be converted to UTF-8, json.c called
 * jsonSetIOErrorFlag(). jsonShouldStopWriting() honours that flag, so every
 * later jsonAddX/jsonEnd did nothing and the client got a truncated document
 * with no closing braces -- because of a single bad byte.
 *
 * After the fix: a conversion failure sets dataConversionErrorFlag, writes a
 * blank placeholder, and printing continues, so the document stays valid.
 * Only a real write/stream failure sets ioErrorFlag and hard-stops.
 *
 * Three cases only -- the regression itself, the hard stop it must not have
 * broken, and the healthy path. Worth adding later: jsonPrinterReset clearing
 * both flags, soft-flag stickiness across a good write, multipart strings with
 * a failing chunk, and a custom printer whose write callback really fails.
 *
 * How we force a conversion failure: give the printer an inputCCSID that
 * charsets.c has no converter for. Every caller-supplied string then fails in
 * convertToUtf8() while the printer's own structural characters
 * ({ } [ ] , : ") are written from SOURCE_CODE_CHARSET as usual -- exactly the
 * situation the fix is about, with no need to mock convertCharset().
 *
 * Note on encodings: the printer's output is UTF-8, while this file's string
 * literals and stdout are in the compiler's charset (EBCDIC on z/OS), so the
 * document is converted back with toNative() before it is compared or printed,
 * and the healthy-path printers are built with LITERAL_CCSID -- the charset of
 * the strings they are actually given.
 */
#include <stdio.h>
#include <string.h>
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "xlate.h"
#include "json.h"

/* ------------------------------------------------------------------ */
/* test harness: count checks, print one line each, exit with #failures */
/* ------------------------------------------------------------------ */

static int fails  = 0;
static int checks = 0;

/* pass the document as "actual" to have it shown when the check fails,
 * or NULL when there is nothing useful to print */
static void check(int cond, const char *what, const char *actual){
  checks++;
  if (cond){
    printf("  ok   %s\n", what);
  } else {
    if (actual){
      printf("  FAIL %s  [got: %s]\n", what, actual);
    } else {
      printf("  FAIL %s\n", what);
    }
    fails++;
  }
}

/* ------------------------------------------------------------------ */
/* the printer emits UTF-8, the test's own string literals and stdout  */
/* are in the compiler's charset (EBCDIC on z/OS) -- convert before    */
/* comparing or printing, otherwise every comparison fails and the     */
/* failure text comes out as mojibake.                                 */
/*                                                                     */
/* Done with xlate.c's a2e() table rather than convertCharset(): the   */
/* documents under test are pure ASCII, where the table is exact, and  */
/* the c89/CUNLCNV build of convertCharset() fails the UTF-8 -> 1047   */
/* direction (rc 16 / Return_Code 4) -- convertCharset() only has a    */
/* fast path for 1047 -> UTF-8. The conversion under test happens      */
/* inside json.c; this helper only has to render the result.           */
/* ------------------------------------------------------------------ */

#define NATIVE_BUFFER_SIZE 4096

/* The charset this file's own string literals are in, and therefore the
 * inputCCSID any printer must be given when it is fed those literals: json.c
 * converts caller data from inputCCSID to UTF-8, so claiming UTF-8 on z/OS
 * makes that conversion a no-op and drops raw EBCDIC into the document. */
#if defined(__ZOWE_OS_ZOS)
#  define LITERAL_CCSID CCSID_IBM1047
#else
#  define LITERAL_CCSID CCSID_UTF_8
#endif

/* returns a NUL-terminated copy of the document in the native charset;
 * valid until the next call (one static buffer, single-threaded test).
 *
 * JsonBuffer is a counted buffer, not a C string: writeToBuffer() only appends
 * bytes and bumps len, and makeJsonBuffer() uses safeMalloc, so the bytes past
 * len are uninitialized. strlen(buf->data) would scan past the document into
 * that garbage. buf->len is the bound; strlenSafe() stops at it, and also stops
 * early at the NUL that jsonBufferTerminateString() counts inside len. */
static const char *toNative(JsonBuffer *buf){
  static char native[NATIVE_BUFFER_SIZE];
  int len = strlenSafe(buf->data, buf->len);

  if (strncpySafe(native, sizeof(native), buf->data, len) < 0){
    /* document longer than the render buffer: strncpySafe truncated and
     * terminated it; keep the length in step so a2e() converts what is there */
    len = sizeof(native) - 1;
  }
#if defined(__ZOWE_OS_ZOS)
  a2e(native, len);
#endif
  return native;
}

/* ------------------------------------------------------------------ */
/* the injected failure                                                */
/* ------------------------------------------------------------------ */

/* A CCSID that has no converter anywhere in charsets.c, so every string
 * tagged with it fails to convert. 9999 is not a registered CCSID:
 *   - iconv builds: getCharsetName() has no name for it, so convertCharset()
 *     returns CHARSET_UNKNOWN_CCSID before iconv is even called;
 *   - metal/CUNLCNV build: CUNLCNV rejects it with a non-zero return code.
 * Hardcoded rather than probed -- the failure is by construction, not by
 * platform luck. isUnconvertible() below keeps that claim honest. */
#define UNCONVERTIBLE_CCSID 9999

static int isUnconvertible(int ccsid){
  char outBuf[64];
  char *out = outBuf;
  int outLen = 0;
  int reason = 0;
  int rc = convertCharset("abc", 3, ccsid, CHARSET_OUTPUT_USE_BUFFER,
                          &out, sizeof(outBuf), CCSID_UTF_8, NULL, &outLen, &reason);
  return rc != CHARSET_CONVERSION_SUCCESS;
}

/* ------------------------------------------------------------------ */

int main(void){

  printf("\n== an unmappable value must not truncate the document ==\n");
  {
    JsonBuffer *buf = makeJsonBuffer();
    jsonPrinter *p = makeBufferJsonPrinter(UNCONVERTIBLE_CCSID, buf);
    const char *doc = NULL;
    int docLen = 0;

    /* precondition: if this ever passes, the rest of the case proves nothing */
    check(isUnconvertible(UNCONVERTIBLE_CCSID),
          "the injected CCSID really is unconvertible", NULL);

    jsonStart(p);
    jsonAddString(p, "bad", "unmappable");   /* key and value both fail */
    jsonAddInt(p, "good", 7);                /* ints do not use inputCCSID */
    jsonEnd(p);
    jsonBufferTerminateString(buf);

    doc = toNative(buf);
    docLen = strlenSafe(doc, NATIVE_BUFFER_SIZE);

    check(jsonCheckDataConversionErrorFlag(p),
          "conversion failure recorded on the soft flag", NULL);
    check(jsonCheckIOErrorFlag(p) == FALSE,
          "conversion failure did NOT latch ioErrorFlag", NULL);
    check(indexOfString((const char *)doc, docLen, "7", 0) >= 0,
          "printing continued past the bad value", doc);
    check(docLen > 0 && doc[docLen - 1] == '}',
          "document is closed, not truncated", doc);

    freeJsonPrinter(p);
    freeJsonBuffer(buf);
  }

  printf("\n== ioErrorFlag must still stop output ==\n");
  {
    JsonBuffer *buf = makeJsonBuffer();
    jsonPrinter *p = makeBufferJsonPrinter(LITERAL_CCSID, buf);
    int lenAtFailure;

    jsonStart(p);
    jsonAddInt(p, "before", 1);
    lenAtFailure = buf->len;

    jsonSetIOErrorFlag(p);
    jsonAddInt(p, "after", 2);
    jsonEnd(p);   /* not even the closing brace may be written */

    check(buf->len == lenAtFailure,
          "nothing written after ioErrorFlag", NULL);

    freeJsonPrinter(p);
    freeJsonBuffer(buf);
  }

  printf("\n== the healthy path is untouched ==\n");
  {
    static const char expected[] = "{\"name\":\"zowe\",\"n\":5}";
    JsonBuffer *buf = makeJsonBuffer();
    jsonPrinter *p = makeBufferJsonPrinter(LITERAL_CCSID, buf);
    const char *doc = NULL;

    jsonStart(p);
    jsonAddString(p, "name", "zowe");
    jsonAddInt(p, "n", 5);
    jsonEnd(p);
    jsonBufferTerminateString(buf);

    /* one call only: toNative() returns its static buffer, so calling it twice
     * in the same expression would let the second call clobber the first */
    doc = toNative(buf);
    check(strlenSafe(doc, NATIVE_BUFFER_SIZE) == sizeof(expected) - 1 &&
          memcmp(doc, expected, sizeof(expected) - 1) == 0,
          "normal output unchanged", doc);
    check(!jsonCheckIOErrorFlag(p) && !jsonCheckDataConversionErrorFlag(p),
          "no flags raised on the happy path", NULL);

    freeJsonPrinter(p);
    freeJsonBuffer(buf);
  }

  printf("\n%s  (%d/%d checks passed)\n",
         fails == 0 ? "ALL PASS" : "FAILURES", checks - fails, checks);
  return fails;
}
