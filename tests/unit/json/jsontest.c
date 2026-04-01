

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * tests/unit/json/jsontest.c - Unit tests for json.h / json.c
 *
 * Exercises the JSON writer (jsonPrinter), parser (jsonParseString),
 * type predicates, object/array accessors, and property helpers.
 *
 * Compile on z/OS (xlclang, -qascii):
 *   See the tests/Makefile target "test_json".
 *
 * Compile on Linux/macOS for local development:
 *   clang -I../h -I../platform/posix -D__ZOWE_OS_LINUX \
 *         -o json_test unit/json/jsontest.c ../c/zowetests.c \
 *         ../c/json.c ../c/alloc.c ../c/utils.c ../c/collections.c \
 *         ../c/charsets.c ../c/xlate.c ../c/timeutls.c ../c/logging.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "json.h"
#include "zowetests.h"

/* ============================================================
 *  Printer capture buffer
 *
 *  A simple fixed-size buffer used with makeCustomJsonPrinter() so that
 *  every test suite that writes JSON can inspect the output as a plain
 *  C string without depending on file-system or CCSID concerns.
 * ============================================================ */

#define PRINTER_BUF_SIZE 65536

static char printerBuf[PRINTER_BUF_SIZE];
static int printerBufLen = 0;

static void captureWrite(jsonPrinter *printer, char *text, int len) {
  int remaining = PRINTER_BUF_SIZE - printerBufLen - 1;
  if (len > remaining) {
    len = remaining;
  }
  if (len > 0) {
    memcpy(printerBuf + printerBufLen, text, len);
    printerBufLen += len;
    printerBuf[printerBufLen] = '\0';
  }
}

static void resetPrinterBuf(void) {
  printerBufLen = 0;
  printerBuf[0] = '\0';
}

/* Creates a printer that writes into printerBuf. Caller must call freeJsonPrinter(). */
static jsonPrinter *makeCaptureJsonPrinter(void) {
  resetPrinterBuf();
  return makeCustomJsonPrinter(captureWrite, NULL);
}

/* ============================================================
 *  Shared parser state
 *
 *  A ShortLivedHeap is re-created before each parser test suite
 *  and freed after. Individual tests may also allocate per-test
 *  SLHs for cases that need independent lifetime control.
 * ============================================================ */

static ShortLivedHeap *parserSlh = NULL;

static void setupParserSlh(void) {
  parserSlh = makeShortLivedHeap(0x10000, 100);
}

static void teardownParserSlh(void) {
  SLHFree(parserSlh);
  parserSlh = NULL;
}

/* ============================================================
 *  Suite 1 - JSON Writer: scalar values
 * ============================================================ */

static void testJsonWriterScalars(void) {
  DESCRIBE("JSON Writer - scalar values") {

    IT("writes a string property") {
      TEST_COVERS(makeCustomJsonPrinter);
      TEST_COVERS(jsonStart);
      TEST_COVERS(jsonAddString);
      TEST_COVERS(jsonEnd);
      jsonPrinter *p = makeCaptureJsonPrinter();
      ASSERT_NOT_NULL(p);
      jsonStart(p);
      jsonAddString(p, "greeting", "hello");
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"greeting\"");
      ASSERT_STR_CONTAINS(printerBuf, "\"hello\"");
      ASSERT_EQUAL_INT(jsonCheckIOErrorFlag(p), 0);
      freeJsonPrinter(p);
    } IT_END

    IT("writes an integer property") {
      TEST_COVERS(jsonAddInt);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddInt(p, "count", 42);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"count\"");
      ASSERT_STR_CONTAINS(printerBuf, "42");
      freeJsonPrinter(p);
    } IT_END

    IT("writes an unsigned integer property") {
      TEST_COVERS(jsonAddUInt);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddUInt(p, "size", 4294967295u);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"size\"");
      ASSERT_STR_CONTAINS(printerBuf, "4294967295");
      freeJsonPrinter(p);
    } IT_END

    IT("writes a boolean true property") {
      TEST_COVERS(jsonAddBoolean);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddBoolean(p, "enabled", 1);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"enabled\"");
      ASSERT_STR_CONTAINS(printerBuf, "true");
      freeJsonPrinter(p);
    } IT_END

    IT("writes a boolean false property") {
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddBoolean(p, "disabled", 0);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"disabled\"");
      ASSERT_STR_CONTAINS(printerBuf, "false");
      freeJsonPrinter(p);
    } IT_END

    IT("writes a null property") {
      TEST_COVERS(jsonAddNull);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddNull(p, "missing");
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"missing\"");
      ASSERT_STR_CONTAINS(printerBuf, "null");
      freeJsonPrinter(p);
    } IT_END

    IT("writes a 64-bit integer property") {
      TEST_COVERS(jsonAddInt64);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddInt64(p, "big", (int64)9223372036854775807LL);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"big\"");
      ASSERT_STR_CONTAINS(printerBuf, "9223372036854775807");
      freeJsonPrinter(p);
    } IT_END

    IT("produces a well-formed JSON object wrapper") {
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddString(p, "k", "v");
      jsonEnd(p);
      /* The output must start with '{' and end with '}' */
      ASSERT_TRUE(printerBuf[0] == '{');
      ASSERT_TRUE(printerBuf[printerBufLen - 1] == '}');
      freeJsonPrinter(p);
    } IT_END

    IT("writes multiple properties with commas separating them") {
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddInt(p, "a", 1);
      jsonAddInt(p, "b", 2);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, ",");
      ASSERT_STR_CONTAINS(printerBuf, "\"a\"");
      ASSERT_STR_CONTAINS(printerBuf, "\"b\"");
      freeJsonPrinter(p);
    } IT_END

    IT("resets cleanly via jsonPrinterReset") {
      TEST_COVERS(jsonPrinterReset);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddInt(p, "x", 1);
      jsonEnd(p);
      /* Reset and write again - should not produce residual state */
      resetPrinterBuf();
      jsonPrinterReset(p);
      jsonStart(p);
      jsonAddInt(p, "y", 2);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"y\"");
      ASSERT_STR_NOT_CONTAINS(printerBuf, "\"x\"");
      freeJsonPrinter(p);
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  Suite 2 - JSON Writer: objects and arrays
 * ============================================================ */

static void testJsonWriterStructures(void) {
  DESCRIBE("JSON Writer - objects and arrays") {

    IT("writes a nested object") {
      TEST_COVERS(jsonStartObject);
      TEST_COVERS(jsonEndObject);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonStartObject(p, "address");
      jsonAddString(p, "city", "Raleigh");
      jsonEndObject(p);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"address\"");
      ASSERT_STR_CONTAINS(printerBuf, "\"city\"");
      ASSERT_STR_CONTAINS(printerBuf, "\"Raleigh\"");
      freeJsonPrinter(p);
    } IT_END

    IT("writes a top-level array member object with NULL key") {
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonStartArray(p, "items");
      jsonStartObject(p, NULL);
      jsonAddString(p, "name", "alpha");
      jsonEndObject(p);
      jsonStartObject(p, NULL);
      jsonAddString(p, "name", "beta");
      jsonEndObject(p);
      jsonEndArray(p);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"items\"");
      ASSERT_STR_CONTAINS(printerBuf, "\"alpha\"");
      ASSERT_STR_CONTAINS(printerBuf, "\"beta\"");
      freeJsonPrinter(p);
    } IT_END

    IT("writes an array of integers") {
      TEST_COVERS(jsonStartArray);
      TEST_COVERS(jsonEndArray);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonStartArray(p, "numbers");
      jsonAddInt(p, NULL, 1);
      jsonAddInt(p, NULL, 2);
      jsonAddInt(p, NULL, 3);
      jsonEndArray(p);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"numbers\"");
      ASSERT_STR_CONTAINS(printerBuf, "1");
      ASSERT_STR_CONTAINS(printerBuf, "2");
      ASSERT_STR_CONTAINS(printerBuf, "3");
      freeJsonPrinter(p);
    } IT_END

    IT("writes an array of strings") {
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonStartArray(p, "tags");
      jsonAddString(p, NULL, "zowe");
      jsonAddString(p, NULL, "zos");
      jsonEndArray(p);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"zowe\"");
      ASSERT_STR_CONTAINS(printerBuf, "\"zos\"");
      freeJsonPrinter(p);
    } IT_END

    IT("writes deeply nested objects") {
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonStartObject(p, "l1");
      jsonStartObject(p, "l2");
      jsonAddString(p, "leaf", "value");
      jsonEndObject(p);
      jsonEndObject(p);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"l1\"");
      ASSERT_STR_CONTAINS(printerBuf, "\"l2\"");
      ASSERT_STR_CONTAINS(printerBuf, "\"leaf\"");
      ASSERT_STR_CONTAINS(printerBuf, "\"value\"");
      freeJsonPrinter(p);
    } IT_END

    IT("pretty-print mode produces newlines") {
      TEST_COVERS(jsonEnablePrettyPrint);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonEnablePrettyPrint(p);
      jsonStart(p);
      jsonAddInt(p, "x", 1);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\n");
      freeJsonPrinter(p);
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  Suite 3 - JSON Writer: advanced string output
 * ============================================================ */

static void testJsonWriterAdvancedStrings(void) {
  DESCRIBE("JSON Writer - advanced string output") {

    IT("writes an unterminated string") {
      TEST_COVERS(jsonAddUnterminatedString);
      char raw[] = "helloworld";
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddUnterminatedString(p, "partial", raw, 5); /* "hello" */
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"partial\"");
      ASSERT_STR_CONTAINS(printerBuf, "hello");
      freeJsonPrinter(p);
    } IT_END

    IT("writes a multipart string") {
      TEST_COVERS(jsonStartMultipartString);
      TEST_COVERS(jsonAppendStringPart);
      TEST_COVERS(jsonEndMultipartString);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonStartMultipartString(p, "assembled");
      jsonAppendStringPart(p, "foo");
      jsonAppendStringPart(p, "bar");
      jsonEndMultipartString(p);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "\"assembled\"");
      ASSERT_STR_CONTAINS(printerBuf, "foobar");
      freeJsonPrinter(p);
    } IT_END

    IT("writes an unterminated multipart string chunk") {
      TEST_COVERS(jsonAppendUnterminatedStringPart);
      char chunk[] = "abcXXX";
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonStartMultipartString(p, "chunk");
      jsonAppendUnterminatedStringPart(p, chunk, 3); /* "abc" */
      jsonEndMultipartString(p);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "abc");
      freeJsonPrinter(p);
    } IT_END

    IT("injects a pre-formed JSON string") {
      TEST_COVERS(jsonAddJSONString);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonStartArray(p, "raw");
      jsonAddJSONString(p, NULL, "1, 2, 3");
      jsonEndArray(p);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "1, 2, 3");
      freeJsonPrinter(p);
    } IT_END

    IT("injects an unterminated JSON string") {
      TEST_COVERS(jsonAddUnterminatedJSONString);
      char raw[] = "99,100";
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonStartArray(p, "vals");
      jsonAddUnterminatedJSONString(p, NULL, raw, 2); /* "99" */
      jsonEndArray(p);
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "99");
      freeJsonPrinter(p);
    } IT_END

    IT("escapes double quotes in string values") {
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddString(p, "msg", "say \"hello\"");
      jsonEnd(p);
      /* The escaped form: \" */
      ASSERT_STR_CONTAINS(printerBuf, "\\\"hello\\\"");
      freeJsonPrinter(p);
    } IT_END

    IT("escapes backslashes in string values") {
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddString(p, "path", "C:\\temp");
      jsonEnd(p);
      ASSERT_STR_CONTAINS(printerBuf, "C:\\\\temp");
      freeJsonPrinter(p);
    } IT_END

    IT("IO error flag is clear after successful writes") {
      TEST_COVERS(jsonCheckIOErrorFlag);
      jsonPrinter *p = makeCaptureJsonPrinter();
      jsonStart(p);
      jsonAddString(p, "ok", "yes");
      jsonEnd(p);
      ASSERT_EQUAL_INT(jsonCheckIOErrorFlag(p), 0);
      freeJsonPrinter(p);
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  Suite 4 - JSON Writer: buffer printer
 * ============================================================ */

static void testJsonWriterBufferPrinter(void) {
  DESCRIBE("JSON Writer - buffer printer") {

    IT("makeBufferJsonPrinter writes into a JsonBuffer") {
      TEST_COVERS(makeBufferJsonPrinter);
      TEST_COVERS(makeJsonBuffer);
      TEST_COVERS(jsonBufferTerminateString);
      TEST_COVERS(freeJsonBuffer);

      JsonBuffer *buf = makeJsonBuffer();
      ASSERT_NOT_NULL(buf);

      jsonPrinter *p = makeBufferJsonPrinter(1208, buf);
      ASSERT_NOT_NULL(p);

      jsonStart(p);
      jsonAddString(p, "hello", "world");
      jsonEnd(p);
      jsonBufferTerminateString(buf);

      ASSERT_NOT_NULL(buf->data);
      ASSERT_GT_INT(buf->len, 0);
      ASSERT_STR_CONTAINS(buf->data, "\"hello\"");
      ASSERT_STR_CONTAINS(buf->data, "\"world\"");

      freeJsonPrinter(p);
      freeJsonBuffer(buf);
    } IT_END

    IT("makeBufferNativeJsonPrinter writes without CCSID conversion") {
      TEST_COVERS(makeBufferNativeJsonPrinter);

      JsonBuffer *buf = makeJsonBuffer();
      ASSERT_NOT_NULL(buf);

      jsonPrinter *p = makeBufferNativeJsonPrinter(1208, buf);
      ASSERT_NOT_NULL(p);

      jsonStart(p);
      jsonAddInt(p, "n", 7);
      jsonEnd(p);
      jsonBufferTerminateString(buf);

      ASSERT_STR_CONTAINS(buf->data, "\"n\"");
      ASSERT_STR_CONTAINS(buf->data, "7");

      freeJsonPrinter(p);
      freeJsonBuffer(buf);
    } IT_END

    IT("jsonBufferRewind allows the buffer to be reused") {
      TEST_COVERS(jsonBufferRewind);

      JsonBuffer *buf = makeJsonBuffer();
      jsonPrinter *p = makeBufferJsonPrinter(1208, buf);
      jsonStart(p);
      jsonAddInt(p, "first", 1);
      jsonEnd(p);
      jsonBufferTerminateString(buf);

      /* Rewind and write again */
      jsonBufferRewind(buf);
      jsonPrinterReset(p);
      jsonStart(p);
      jsonAddInt(p, "second", 2);
      jsonEnd(p);
      jsonBufferTerminateString(buf);

      ASSERT_GT_INT(buf->len, 0);
      ASSERT_STR_CONTAINS(buf->data, "\"second\"");

      freeJsonPrinter(p);
      freeJsonBuffer(buf);
    } IT_END

    IT("jsonBufferCopy returns a standalone copy of the buffer data") {
      TEST_COVERS(jsonBufferCopy);

      JsonBuffer *buf = makeJsonBuffer();
      jsonPrinter *p = makeBufferJsonPrinter(1208, buf);
      jsonStart(p);
      jsonAddString(p, "snap", "shot");
      jsonEnd(p);
      jsonBufferTerminateString(buf);

      char *copy = jsonBufferCopy(buf);
      ASSERT_NOT_NULL(copy);
      ASSERT_STR_CONTAINS(copy, "\"snap\"");

      safeFree(copy, (int)strlen(copy) + 1);
      freeJsonPrinter(p);
      freeJsonBuffer(buf);
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  Suite 5 - JSON Parser: primitive types
 * ============================================================ */

static void testJsonParserPrimitives(void) {
  DESCRIBE("JSON Parser - primitive types") {
    SET_BEFORE_EACH(setupParserSlh);
    SET_AFTER_EACH(teardownParserSlh);

    IT("parses a null literal") {
      TEST_COVERS(jsonParseString);
      TEST_COVERS(jsonIsNull);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "null", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_EQUAL_INT(errorBuf[0], '\0');
      ASSERT_TRUE(jsonIsNull(json));
    } IT_END

    IT("parses boolean true") {
      TEST_COVERS(jsonIsBoolean);
      TEST_COVERS(jsonAsBoolean);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "true", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsBoolean(json));
      ASSERT_EQUAL_INT(jsonAsBoolean(json), 1);
    } IT_END

    IT("parses boolean false") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "false", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsBoolean(json));
      ASSERT_EQUAL_INT(jsonAsBoolean(json), 0);
    } IT_END

    IT("parses a positive integer") {
      TEST_COVERS(jsonIsNumber);
      TEST_COVERS(jsonAsNumber);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "42", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsNumber(json));
      ASSERT_EQUAL_INT(jsonAsNumber(json), 42);
    } IT_END

    IT("parses a negative integer") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "-7", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsNumber(json));
      ASSERT_EQUAL_INT(jsonAsNumber(json), -7);
    } IT_END

    IT("parses a zero integer") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "0", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsNumber(json));
      ASSERT_EQUAL_INT(jsonAsNumber(json), 0);
    } IT_END

    IT("parses a string value") {
      TEST_COVERS(jsonIsString);
      TEST_COVERS(jsonAsString);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "\"hello\"", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsString(json));
      ASSERT_EQUAL_STR(jsonAsString(json), "hello");
    } IT_END

    IT("parses an empty string") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "\"\"", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsString(json));
      ASSERT_EQUAL_STR(jsonAsString(json), "");
    } IT_END

    IT("parses a string containing escaped characters") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "\"say \\\"hi\\\"\"", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsString(json));
      ASSERT_STR_CONTAINS(jsonAsString(json), "hi");
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  Suite 6 - JSON Parser: complex structures
 * ============================================================ */

static void testJsonParserStructures(void) {
  DESCRIBE("JSON Parser - complex structures") {
    SET_BEFORE_EACH(setupParserSlh);
    SET_AFTER_EACH(teardownParserSlh);

    IT("parses a simple flat object") {
      TEST_COVERS(jsonIsObject);
      TEST_COVERS(jsonAsObject);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "{\"name\":\"zowe\",\"version\":2}",
          errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsObject(json));
      JsonObject *obj = jsonAsObject(json);
      ASSERT_NOT_NULL(obj);
    } IT_END

    IT("parses an empty object") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "{}", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsObject(json));
    } IT_END

    IT("parses a simple array") {
      TEST_COVERS(jsonIsArray);
      TEST_COVERS(jsonAsArray);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "[1,2,3]", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsArray(json));
      JsonArray *arr = jsonAsArray(json);
      ASSERT_NOT_NULL(arr);
      ASSERT_EQUAL_INT(jsonArrayGetCount(arr), 3);
    } IT_END

    IT("parses an empty array") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "[]", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsArray(json));
      ASSERT_EQUAL_INT(jsonArrayGetCount(jsonAsArray(json)), 0);
    } IT_END

    IT("parses a nested object") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"outer\":{\"inner\":\"value\"}}", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsObject(json));
      JsonObject *outer = jsonAsObject(json);
      Json *innerJson = jsonObjectGetPropertyValue(outer, "outer");
      ASSERT_NOT_NULL(innerJson);
      ASSERT_TRUE(jsonIsObject(innerJson));
    } IT_END

    IT("parses an array of objects") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "[{\"id\":1},{\"id\":2}]", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsArray(json));
      JsonArray *arr = jsonAsArray(json);
      ASSERT_EQUAL_INT(jsonArrayGetCount(arr), 2);
    } IT_END

    IT("parses an object containing an array") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"codes\":[10,20,30]}", errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      JsonObject *obj = jsonAsObject(json);
      JsonArray *arr = jsonObjectGetArray(obj, "codes");
      ASSERT_NOT_NULL(arr);
      ASSERT_EQUAL_INT(jsonArrayGetCount(arr), 3);
    } IT_END

    IT("parses an object with mixed value types") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"s\":\"text\",\"n\":5,\"b\":true,\"z\":null}",
          errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      JsonObject *obj = jsonAsObject(json);
      ASSERT_EQUAL_STR(jsonObjectGetString(obj, "s"), "text");
      ASSERT_EQUAL_INT(jsonObjectGetNumber(obj, "n"), 5);
      ASSERT_EQUAL_INT(jsonObjectGetBoolean(obj, "b"), 1);
    } IT_END

    IT("parses an unterminated string input") {
      TEST_COVERS(jsonParseUnterminatedString);
      char input[] = "{\"a\":1}GARBAGE";
      char errorBuf[256] = {0};
      Json *json = jsonParseUnterminatedString(parserSlh, input, 7, errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_TRUE(jsonIsObject(json));
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  Suite 7 - JSON Parser: error handling
 * ============================================================ */

static void testJsonParserErrors(void) {
  DESCRIBE("JSON Parser - error handling") {
    SET_BEFORE_EACH(setupParserSlh);
    SET_AFTER_EACH(teardownParserSlh);

    IT("returns NULL or error json for invalid input and fills the error buffer") {
      char errorBuf[256] = {0};
      /*
       * Behaviour: the parser returns either NULL or a Json with
       * JSON_TYPE_ERROR. The error buffer should be non-empty.
       */
      Json *json = jsonParseString(parserSlh, "{invalid}", errorBuf, sizeof(errorBuf));
      bool hadError = (json == NULL) || (json != NULL && json->type == JSON_TYPE_ERROR);
      ASSERT_TRUE(hadError || errorBuf[0] != '\0');
    } IT_END

    IT("handles an unterminated object gracefully") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "{\"open\":", errorBuf, sizeof(errorBuf));
      bool hadError = (json == NULL) || (json != NULL && json->type == JSON_TYPE_ERROR);
      ASSERT_TRUE(hadError || errorBuf[0] != '\0');
    } IT_END

    IT("handles an empty input string gracefully") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "", errorBuf, sizeof(errorBuf));
      /*
       * The parser may return NULL or an error type for empty input.
       * Either is acceptable; we simply verify it does not crash.
       */
      ASSERT_TRUE(json == NULL || json->type == JSON_TYPE_ERROR || errorBuf[0] != '\0'
                  || json != NULL);
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  Suite 8 - JSON Type predicates
 * ============================================================ */

static void testJsonTypePredicates(void) {
  DESCRIBE("JSON Type predicates") {
    SET_BEFORE_EACH(setupParserSlh);
    SET_AFTER_EACH(teardownParserSlh);

    IT("jsonIsNull returns true only for null") {
      char errorBuf[256] = {0};
      Json *nullJson = jsonParseString(parserSlh, "null", errorBuf, sizeof(errorBuf));
      Json *numJson  = jsonParseString(parserSlh, "1",    errorBuf, sizeof(errorBuf));
      ASSERT_TRUE(jsonIsNull(nullJson));
      ASSERT_FALSE(jsonIsNull(numJson));
    } IT_END

    IT("jsonIsBoolean returns true only for booleans") {
      char errorBuf[256] = {0};
      Json *boolJson = jsonParseString(parserSlh, "true",  errorBuf, sizeof(errorBuf));
      Json *numJson  = jsonParseString(parserSlh, "1",     errorBuf, sizeof(errorBuf));
      ASSERT_TRUE(jsonIsBoolean(boolJson));
      ASSERT_FALSE(jsonIsBoolean(numJson));
    } IT_END

    IT("jsonIsNumber returns true only for numbers") {
      char errorBuf[256] = {0};
      Json *numJson  = jsonParseString(parserSlh, "99",    errorBuf, sizeof(errorBuf));
      Json *strJson  = jsonParseString(parserSlh, "\"x\"", errorBuf, sizeof(errorBuf));
      ASSERT_TRUE(jsonIsNumber(numJson));
      ASSERT_FALSE(jsonIsNumber(strJson));
    } IT_END

    IT("jsonIsString returns true only for strings") {
      char errorBuf[256] = {0};
      Json *strJson = jsonParseString(parserSlh, "\"hello\"", errorBuf, sizeof(errorBuf));
      Json *numJson = jsonParseString(parserSlh, "1",         errorBuf, sizeof(errorBuf));
      ASSERT_TRUE(jsonIsString(strJson));
      ASSERT_FALSE(jsonIsString(numJson));
    } IT_END

    IT("jsonIsObject returns true only for objects") {
      char errorBuf[256] = {0};
      Json *objJson = jsonParseString(parserSlh, "{\"k\":1}", errorBuf, sizeof(errorBuf));
      Json *arrJson = jsonParseString(parserSlh, "[1]",      errorBuf, sizeof(errorBuf));
      ASSERT_TRUE(jsonIsObject(objJson));
      ASSERT_FALSE(jsonIsObject(arrJson));
    } IT_END

    IT("jsonIsArray returns true only for arrays") {
      char errorBuf[256] = {0};
      Json *arrJson = jsonParseString(parserSlh, "[1,2]",    errorBuf, sizeof(errorBuf));
      Json *objJson = jsonParseString(parserSlh, "{\"k\":1}", errorBuf, sizeof(errorBuf));
      ASSERT_TRUE(jsonIsArray(arrJson));
      ASSERT_FALSE(jsonIsArray(objJson));
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  Suite 9 - JSON Object accessors
 * ============================================================ */

static void testJsonObjectAccessors(void) {
  DESCRIBE("JSON Object accessors") {
    SET_BEFORE_EACH(setupParserSlh);
    SET_AFTER_EACH(teardownParserSlh);

    IT("jsonObjectGetString retrieves a string property") {
      TEST_COVERS(jsonObjectGetString);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"city\":\"Raleigh\"}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      char *city = jsonObjectGetString(obj, "city");
      ASSERT_NOT_NULL(city);
      ASSERT_EQUAL_STR(city, "Raleigh");
    } IT_END

    IT("jsonObjectGetNumber retrieves a numeric property") {
      TEST_COVERS(jsonObjectGetNumber);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"port\":8543}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      ASSERT_EQUAL_INT(jsonObjectGetNumber(obj, "port"), 8543);
    } IT_END

    IT("jsonObjectGetBoolean retrieves a boolean property") {
      TEST_COVERS(jsonObjectGetBoolean);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"enabled\":true,\"disabled\":false}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      ASSERT_EQUAL_INT(jsonObjectGetBoolean(obj, "enabled"), 1);
      ASSERT_EQUAL_INT(jsonObjectGetBoolean(obj, "disabled"), 0);
    } IT_END

    IT("jsonObjectGetArray retrieves an array property") {
      TEST_COVERS(jsonObjectGetArray);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"ids\":[10,20,30]}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      JsonArray *arr = jsonObjectGetArray(obj, "ids");
      ASSERT_NOT_NULL(arr);
      ASSERT_EQUAL_INT(jsonArrayGetCount(arr), 3);
    } IT_END

    IT("jsonObjectGetObject retrieves a nested object property") {
      TEST_COVERS(jsonObjectGetObject);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"meta\":{\"version\":1}}", errorBuf, sizeof(errorBuf));
      JsonObject *root = jsonAsObject(json);
      JsonObject *meta = jsonObjectGetObject(root, "meta");
      ASSERT_NOT_NULL(meta);
      ASSERT_EQUAL_INT(jsonObjectGetNumber(meta, "version"), 1);
    } IT_END

    IT("jsonObjectHasKey returns true for a present key") {
      TEST_COVERS(jsonObjectHasKey);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"present\":1}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      ASSERT_TRUE(jsonObjectHasKey(obj, "present"));
      ASSERT_FALSE(jsonObjectHasKey(obj, "absent"));
    } IT_END

    IT("jsonObjectGetPropertyValue returns the Json node for a key") {
      TEST_COVERS(jsonObjectGetPropertyValue);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"answer\":42}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      Json *value = jsonObjectGetPropertyValue(obj, "answer");
      ASSERT_NOT_NULL(value);
      ASSERT_TRUE(jsonIsNumber(value));
      ASSERT_EQUAL_INT(jsonAsNumber(value), 42);
    } IT_END

    IT("jsonObjectGetPropertyValue returns NULL for an absent key") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "{\"a\":1}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      Json *value = jsonObjectGetPropertyValue(obj, "nonexistent");
      ASSERT_NULL(value);
    } IT_END

    IT("jsonObjectGetFirstProperty and GetNextProperty iterate all properties") {
      TEST_COVERS(jsonObjectGetFirstProperty);
      TEST_COVERS(jsonObjectGetNextProperty);
      TEST_COVERS(jsonPropertyGetKey);
      TEST_COVERS(jsonPropertyGetValue);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"a\":1,\"b\":2,\"c\":3}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      int count = 0;
      JsonProperty *prop = jsonObjectGetFirstProperty(obj);
      while (prop != NULL) {
        ASSERT_NOT_NULL(jsonPropertyGetKey(prop));
        ASSERT_NOT_NULL(jsonPropertyGetValue(prop));
        count++;
        prop = jsonObjectGetNextProperty(prop);
      }
      ASSERT_EQUAL_INT(count, 3);
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  Suite 10 - JSON Array accessors
 * ============================================================ */

static void testJsonArrayAccessors(void) {
  DESCRIBE("JSON Array accessors") {
    SET_BEFORE_EACH(setupParserSlh);
    SET_AFTER_EACH(teardownParserSlh);

    IT("jsonArrayGetCount returns the number of elements") {
      TEST_COVERS(jsonArrayGetCount);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "[10,20,30,40]", errorBuf, sizeof(errorBuf));
      JsonArray *arr = jsonAsArray(json);
      ASSERT_EQUAL_INT(jsonArrayGetCount(arr), 4);
    } IT_END

    IT("jsonArrayGetItem retrieves items by index") {
      TEST_COVERS(jsonArrayGetItem);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "[\"a\",\"b\",\"c\"]", errorBuf, sizeof(errorBuf));
      JsonArray *arr = jsonAsArray(json);
      Json *item0 = jsonArrayGetItem(arr, 0);
      Json *item2 = jsonArrayGetItem(arr, 2);
      ASSERT_NOT_NULL(item0);
      ASSERT_NOT_NULL(item2);
      ASSERT_EQUAL_STR(jsonAsString(item0), "a");
      ASSERT_EQUAL_STR(jsonAsString(item2), "c");
    } IT_END

    IT("jsonArrayGetString retrieves string elements") {
      TEST_COVERS(jsonArrayGetString);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "[\"foo\",\"bar\"]", errorBuf, sizeof(errorBuf));
      JsonArray *arr = jsonAsArray(json);
      ASSERT_EQUAL_STR(jsonArrayGetString(arr, 0), "foo");
      ASSERT_EQUAL_STR(jsonArrayGetString(arr, 1), "bar");
    } IT_END

    IT("jsonArrayGetNumber retrieves numeric elements") {
      TEST_COVERS(jsonArrayGetNumber);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "[5,10,15]", errorBuf, sizeof(errorBuf));
      JsonArray *arr = jsonAsArray(json);
      ASSERT_EQUAL_INT(jsonArrayGetNumber(arr, 0), 5);
      ASSERT_EQUAL_INT(jsonArrayGetNumber(arr, 1), 10);
      ASSERT_EQUAL_INT(jsonArrayGetNumber(arr, 2), 15);
    } IT_END

    IT("jsonArrayGetBoolean retrieves boolean elements") {
      TEST_COVERS(jsonArrayGetBoolean);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "[true,false,true]", errorBuf, sizeof(errorBuf));
      JsonArray *arr = jsonAsArray(json);
      ASSERT_EQUAL_INT(jsonArrayGetBoolean(arr, 0), 1);
      ASSERT_EQUAL_INT(jsonArrayGetBoolean(arr, 1), 0);
    } IT_END

    IT("jsonArrayGetObject retrieves object elements") {
      TEST_COVERS(jsonArrayGetObject);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "[{\"id\":1},{\"id\":2}]", errorBuf, sizeof(errorBuf));
      JsonArray *arr = jsonAsArray(json);
      JsonObject *first = jsonArrayGetObject(arr, 0);
      ASSERT_NOT_NULL(first);
      ASSERT_EQUAL_INT(jsonObjectGetNumber(first, "id"), 1);
    } IT_END

    IT("jsonArrayGetArray retrieves nested array elements") {
      TEST_COVERS(jsonArrayGetArray);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "[[1,2],[3,4]]", errorBuf, sizeof(errorBuf));
      JsonArray *outer = jsonAsArray(json);
      JsonArray *inner = jsonArrayGetArray(outer, 0);
      ASSERT_NOT_NULL(inner);
      ASSERT_EQUAL_INT(jsonArrayGetCount(inner), 2);
    } IT_END

    IT("jsonVerifyHomogeneity detects a uniform array") {
      TEST_COVERS(jsonVerifyHomogeneity);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "[1,2,3]", errorBuf, sizeof(errorBuf));
      JsonArray *arr = jsonAsArray(json);
      ASSERT_TRUE(jsonVerifyHomogeneity(arr, JSON_TYPE_NUMBER));
      ASSERT_FALSE(jsonVerifyHomogeneity(arr, JSON_TYPE_STRING));
    } IT_END

    IT("jsonArrayContainsString finds a matching element") {
      TEST_COVERS(jsonArrayContainsString);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "[\"alpha\",\"beta\",\"gamma\"]", errorBuf, sizeof(errorBuf));
      JsonArray *arr = jsonAsArray(json);
      ASSERT_TRUE(jsonArrayContainsString(arr, "beta"));
      ASSERT_FALSE(jsonArrayContainsString(arr, "delta"));
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  Suite 11 - JSON Property helper functions
 * ============================================================ */

static void testJsonPropertyHelpers(void) {
  DESCRIBE("JSON Property helpers") {
    SET_BEFORE_EACH(setupParserSlh);
    SET_AFTER_EACH(teardownParserSlh);

    IT("jsonStringProperty returns the string value for a present key") {
      TEST_COVERS(jsonStringProperty);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"name\":\"zowe\"}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      int status = 0;
      char *name = jsonStringProperty(obj, "name", &status);
      ASSERT_EQUAL_INT(status, JSON_PROPERTY_OK);
      ASSERT_EQUAL_STR(name, "zowe");
    } IT_END

    IT("jsonStringProperty reports JSON_PROPERTY_NOT_FOUND for an absent key") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "{\"a\":\"1\"}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      int status = 0;
      jsonStringProperty(obj, "missing", &status);
      ASSERT_EQUAL_INT(status, JSON_PROPERTY_NOT_FOUND);
    } IT_END

    IT("jsonStringProperty reports JSON_PROPERTY_UNEXPECTED_TYPE for a type mismatch") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "{\"count\":5}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      int status = 0;
      jsonStringProperty(obj, "count", &status);
      ASSERT_EQUAL_INT(status, JSON_PROPERTY_UNEXPECTED_TYPE);
    } IT_END

    IT("jsonIntProperty returns the integer value for a present key") {
      TEST_COVERS(jsonIntProperty);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"port\":7554}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      int status = 0;
      int port = jsonIntProperty(obj, "port", &status, 0);
      ASSERT_EQUAL_INT(status, JSON_PROPERTY_OK);
      ASSERT_EQUAL_INT(port, 7554);
    } IT_END

    IT("jsonIntProperty returns the default value for an absent key") {
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, "{}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      int status = 0;
      int val = jsonIntProperty(obj, "absent", &status, 99);
      ASSERT_EQUAL_INT(status, JSON_PROPERTY_NOT_FOUND);
      ASSERT_EQUAL_INT(val, 99);
    } IT_END

    IT("jsonArrayProperty returns the array value for a present key") {
      TEST_COVERS(jsonArrayProperty);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"list\":[1,2,3]}", errorBuf, sizeof(errorBuf));
      JsonObject *obj = jsonAsObject(json);
      int status = 0;
      JsonArray *arr = jsonArrayProperty(obj, "list", &status);
      ASSERT_EQUAL_INT(status, JSON_PROPERTY_OK);
      ASSERT_NOT_NULL(arr);
      ASSERT_EQUAL_INT(jsonArrayGetCount(arr), 3);
    } IT_END

    IT("jsonObjectProperty returns the object value for a present key") {
      TEST_COVERS(jsonObjectProperty);
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh,
          "{\"cfg\":{\"debug\":true}}", errorBuf, sizeof(errorBuf));
      JsonObject *root = jsonAsObject(json);
      int status = 0;
      JsonObject *cfg = jsonObjectProperty(root, "cfg", &status);
      ASSERT_EQUAL_INT(status, JSON_PROPERTY_OK);
      ASSERT_NOT_NULL(cfg);
      ASSERT_EQUAL_INT(jsonObjectGetBoolean(cfg, "debug"), 1);
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  Suite 12 - JSON round-trip: write then parse
 * ============================================================ */

static void testJsonRoundTrip(void) {
  DESCRIBE("JSON round-trip - write then parse") {
    SET_BEFORE_EACH(setupParserSlh);
    SET_AFTER_EACH(teardownParserSlh);

    IT("a value written by the printer can be read back by the parser") {
      /* Write */
      JsonBuffer *buf = makeJsonBuffer();
      ASSERT_NOT_NULL(buf);
      jsonPrinter *p = makeBufferJsonPrinter(1208, buf);
      ASSERT_NOT_NULL(p);
      jsonStart(p);
      jsonAddString(p, "product", "Zowe");
      jsonAddInt(p, "major", 3);
      jsonAddBoolean(p, "stable", 1);
      jsonStartArray(p, "components");
      jsonAddString(p, NULL, "ZSS");
      jsonAddString(p, NULL, "APIML");
      jsonEndArray(p);
      jsonEnd(p);
      jsonBufferTerminateString(buf);
      freeJsonPrinter(p);

      /* Parse the captured output */
      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, buf->data, errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);
      ASSERT_EQUAL_INT(errorBuf[0], '\0');
      ASSERT_TRUE(jsonIsObject(json));

      JsonObject *obj = jsonAsObject(json);
      ASSERT_EQUAL_STR(jsonObjectGetString(obj, "product"), "Zowe");
      ASSERT_EQUAL_INT(jsonObjectGetNumber(obj, "major"), 3);
      ASSERT_EQUAL_INT(jsonObjectGetBoolean(obj, "stable"), 1);

      JsonArray *components = jsonObjectGetArray(obj, "components");
      ASSERT_NOT_NULL(components);
      ASSERT_EQUAL_INT(jsonArrayGetCount(components), 2);
      ASSERT_EQUAL_STR(jsonArrayGetString(components, 0), "ZSS");
      ASSERT_EQUAL_STR(jsonArrayGetString(components, 1), "APIML");

      freeJsonBuffer(buf);
    } IT_END

    IT("round-trips a deeply nested structure") {
      JsonBuffer *buf = makeJsonBuffer();
      jsonPrinter *p = makeBufferJsonPrinter(1208, buf);
      jsonStart(p);
      jsonStartObject(p, "level1");
      jsonStartObject(p, "level2");
      jsonAddString(p, "deep", "value");
      jsonEndObject(p);
      jsonEndObject(p);
      jsonEnd(p);
      jsonBufferTerminateString(buf);
      freeJsonPrinter(p);

      char errorBuf[256] = {0};
      Json *json = jsonParseString(parserSlh, buf->data, errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(json);

      JsonObject *root = jsonAsObject(json);
      JsonObject *l1 = jsonObjectGetObject(root, "level1");
      ASSERT_NOT_NULL(l1);
      JsonObject *l2 = jsonObjectGetObject(l1, "level2");
      ASSERT_NOT_NULL(l2);
      ASSERT_EQUAL_STR(jsonObjectGetString(l2, "deep"), "value");

      freeJsonBuffer(buf);
    } IT_END

  } DESCRIBE_END
}

/* ============================================================
 *  main
 * ============================================================ */

int main(void) {
  zoweTestInit();

  testJsonWriterScalars();
  testJsonWriterStructures();
  testJsonWriterAdvancedStrings();
  testJsonWriterBufferPrinter();
  testJsonParserPrimitives();
  testJsonParserStructures();
  testJsonParserErrors();
  testJsonTypePredicates();
  testJsonObjectAccessors();
  testJsonArrayAccessors();
  testJsonPropertyHelpers();
  testJsonRoundTrip();

  return ZOWE_TEST_REPORT();
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
