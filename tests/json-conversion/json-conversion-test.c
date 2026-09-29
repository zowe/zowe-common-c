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
 */
#include <stdio.h>
#include <string.h>
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
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
    printf("  FAIL %s%s%s\n", what,
           actual ? "  [got: " : "", actual ? actual : "");
    fails++;
  }
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

    /* precondition: if this ever passes, the rest of the case proves nothing */
    check(isUnconvertible(UNCONVERTIBLE_CCSID),
          "the injected CCSID really is unconvertible", NULL);

    jsonStart(p);
    jsonAddString(p, "bad", "unmappable");   /* key and value both fail */
    jsonAddInt(p, "good", 7);                /* ints do not use inputCCSID */
    jsonEnd(p);
    jsonBufferTerminateString(buf);

    check(jsonCheckDataConversionErrorFlag(p),
          "conversion failure recorded on the soft flag", NULL);
    check(jsonCheckIOErrorFlag(p) == FALSE,
          "conversion failure did NOT latch ioErrorFlag", NULL);
    check(strstr(buf->data, "7") != NULL,
          "printing continued past the bad value", buf->data);
    /* len - 2: jsonBufferTerminateString() appends a NUL and bumps len */
    check(buf->data[buf->len - 2] == '}',
          "document is closed, not truncated", buf->data);

    freeJsonPrinter(p);
    freeJsonBuffer(buf);
  }

  printf("\n== ioErrorFlag must still stop output ==\n");
  {
    JsonBuffer *buf = makeJsonBuffer();
    jsonPrinter *p = makeBufferJsonPrinter(CCSID_UTF_8, buf);
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
    JsonBuffer *buf = makeJsonBuffer();
    jsonPrinter *p = makeBufferJsonPrinter(CCSID_UTF_8, buf);

    jsonStart(p);
    jsonAddString(p, "name", "zowe");
    jsonAddInt(p, "n", 5);
    jsonEnd(p);
    jsonBufferTerminateString(buf);

    check(strcmp(buf->data, "{\"name\":\"zowe\",\"n\":5}") == 0,
          "normal output unchanged", buf->data);
    check(!jsonCheckIOErrorFlag(p) && !jsonCheckDataConversionErrorFlag(p),
          "no flags raised on the happy path", NULL);

    freeJsonPrinter(p);
    freeJsonBuffer(buf);
  }

  printf("\n%s  (%d/%d checks passed)\n",
         fails == 0 ? "ALL PASS" : "FAILURES", checks - fails, checks);
  return fails;
}
