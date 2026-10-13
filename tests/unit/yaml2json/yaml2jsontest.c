
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * tests/unit/yaml2json/yaml2jsontest.c - Unit tests for yaml2json.h / yaml2json.c
 *
 * Tests the readYAML / readYAML2 file readers and the yaml2JSON converter
 * that turns a libyaml yaml_document_t into a Json* tree (json.h API).
 *
 * Strategy: several test suites write small YAML snippets to a temp file,
 * call readYAML(), then yaml2JSON(), and inspect the resulting Json* tree
 * with the standard json.h accessor functions.  Other suites read fixture
 * files that already exist in tests/schemadata/ or tests/configmgr/.  One
 * suite generates a ~1 MiB YAML mapping at runtime to exercise the parser
 * on large inputs.
 *
 * The CWD when the binary is run is tests/ (per the Makefile "test_yaml2json"
 * target), so relative fixture paths like "./schemadata/yamltypes.yaml" are
 * resolved against tests/.
 *
 * On z/OS the source code is in EBCDIC; fputs() writes EBCDIC bytes.
 * readYAML's internal yamlReadHandler converts from native to ASCII before
 * handing the data to libyaml, so no special handling is needed here.
 *
 * Compile on z/OS (xlclang, lp64) -- see tests/Makefile target "test_yaml2json".
 *
 * Compile on Linux/macOS for local development:
 *   clang -I../../h -I../../platform/posix -D__ZOWE_OS_LINUX \
 *         -Ilibyaml/include -DYAML_DECLARE_STATIC=1 \
 *         -o yaml2jsontest \
 *         unit/yaml2json/yaml2jsontest.c ../c/zowetests.c \
 *         ../c/yaml2json.c ../c/json.c ../c/alloc.c ../c/utils.c \
 *         ../c/collections.c ../c/charsets.c ../c/xlate.c \
 *         ../c/timeutls.c ../c/logging.c \
 *         libyaml/src/api.c libyaml/src/reader.c libyaml/src/scanner.c \
 *         libyaml/src/parser.c libyaml/src/loader.c libyaml/src/writer.c \
 *         libyaml/src/emitter.c libyaml/src/dumper.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "zowetypes.h"
#include "alloc.h"
#include "json.h"
#include "yaml2json.h"
#include "zowetests.h"

/* ===================================================================
 *  Shared state
 * =================================================================== */

static char           s_tempPath[256];
static ShortLivedHeap *s_slh = NULL;

/* ===================================================================
 *  Helpers
 * =================================================================== */

/* Write yamlText to a unique temp file; return true on success. */
static bool writeTempYAML(const char *yamlText) {
  snprintf(s_tempPath, sizeof(s_tempPath),
           "/tmp/yaml2jsontest_%d.yaml", (int)getpid());
  FILE *f = fopen(s_tempPath, "w");
  if (!f) return false;
  fputs(yamlText, f);
  fclose(f);
  return true;
}

/* Delete the current temp file (silently ignore errors). */
static void removeTempFile(void) {
  if (s_tempPath[0] != '\0') {
    unlink(s_tempPath);
    s_tempPath[0] = '\0';
  }
}

/*
 * Write YAML, parse it, convert it to JSON, free the document, and return
 * the Json* root allocated in s_slh.  Returns NULL on any failure.
 */
static Json *yamlStrToJSON(const char *yamlText) {
  char errorBuf[512] = {0};
  if (!writeTempYAML(yamlText)) return NULL;
  yaml_document_t *doc = readYAML(s_tempPath, errorBuf, sizeof(errorBuf));
  if (!doc) return NULL;
  Json *root = yaml2JSON(doc, s_slh);
  yaml_document_delete(doc);
  safeFree((char *)doc, sizeof(yaml_document_t));
  return root;
}

/*
 * Parse an existing file (no temp lifecycle) and convert to JSON.
 * The returned tree lives in s_slh.  Returns NULL on any failure.
 */
static Json *yamlFileToJSON(const char *path) {
  char errorBuf[512] = {0};
  yaml_document_t *doc = readYAML(path, errorBuf, sizeof(errorBuf));
  if (!doc) return NULL;
  Json *root = yaml2JSON(doc, s_slh);
  yaml_document_delete(doc);
  safeFree((char *)doc, sizeof(yaml_document_t));
  return root;
}

/* ===================================================================
 *  Before / after hooks
 * =================================================================== */

/* Inline-YAML suites: manage slh + temp file lifecycle. */
static void beforeEach(void)    { s_slh = makeShortLivedHeap(0x10000, 100); }
static void afterEach(void) {
  if (s_slh) { SLHFree(s_slh); s_slh = NULL; }
  removeTempFile();
}

/* Disk-fixture suites: manage slh only; no temp file to remove. */
static void beforeEachFile(void) { s_slh = makeShortLivedHeap(0x10000, 100); }
static void afterEachFile(void) {
  if (s_slh) { SLHFree(s_slh); s_slh = NULL; }
}

/* ===================================================================
 *  Large-file helpers (~1 MiB YAML mapping)
 * =================================================================== */

#define LARGE_YAML_TARGET_BYTES 1048576L

static char s_largeTempPath[256];
static int  s_largeYAMLCount = 0;

/*
 * Write a YAML mapping composed of entries of the form:
 *   item_NNNNNN: "value_NNNNNN"
 * until at least LARGE_YAML_TARGET_BYTES bytes have been written.
 * Records the entry count in s_largeYAMLCount.
 */
static bool writeLargeYAMLFile(void) {
  snprintf(s_largeTempPath, sizeof(s_largeTempPath),
           "/tmp/yaml2jsontest_large_%d.yaml", (int)getpid());
  FILE *f = fopen(s_largeTempPath, "w");
  if (!f) return false;
  long bytes = 0;
  int  count = 0;
  char entry[64];
  while (bytes < LARGE_YAML_TARGET_BYTES) {
    int n = snprintf(entry, sizeof(entry),
                     "item_%06d: \"value_%06d\"\n", count, count);
    fputs(entry, f);
    bytes += n;
    count++;
  }
  fclose(f);
  s_largeYAMLCount = count;
  return true;
}

static void beforeEachLargeYAML(void) {
  /* Use a 2 MiB heap so the converted Json tree fits comfortably. */
  s_slh = makeShortLivedHeap(0x200000, 200);
  writeLargeYAMLFile();
}

static void afterEachLargeYAML(void) {
  if (s_slh) { SLHFree(s_slh); s_slh = NULL; }
  if (s_largeTempPath[0] != '\0') {
    unlink(s_largeTempPath);
    s_largeTempPath[0] = '\0';
  }
}

/* ===================================================================
 *  Test suites
 * =================================================================== */

/* ------------------------------------------------------------------ */
static void testReadYAMLErrors(void) {
  DESCRIBE("readYAML error handling") {
    SET_BEFORE_EACH(beforeEach);
    SET_AFTER_EACH(afterEach);

    IT("returns NULL and sets errorBuf when file does not exist") {
      TEST_COVERS(readYAML);
      char errorBuf[256] = {0};
      yaml_document_t *doc = readYAML("/tmp/no_such_file_yaml2jsontest.yaml",
                                      errorBuf, sizeof(errorBuf));
      ASSERT_NULL(doc);
      ASSERT_TRUE(strlen(errorBuf) > 0);
    } IT_END

    IT("sets wasMissing=true with readYAML2 when file does not exist") {
      TEST_COVERS(readYAML2);
      char errorBuf[256] = {0};
      bool wasMissing = false;
      yaml_document_t *doc = readYAML2("/tmp/no_such_file_yaml2jsontest.yaml",
                                       errorBuf, sizeof(errorBuf), &wasMissing);
      ASSERT_NULL(doc);
      ASSERT_TRUE(wasMissing);
    } IT_END

    IT("sets wasMissing=false with readYAML2 when file exists") {
      TEST_COVERS(readYAML2);
      char errorBuf[256] = {0};
      bool wasMissing = true;
      ASSERT_TRUE(writeTempYAML("key: value\n"));
      yaml_document_t *doc = readYAML2(s_tempPath, errorBuf, sizeof(errorBuf), &wasMissing);
      ASSERT_NOT_NULL(doc);
      ASSERT_FALSE(wasMissing);
      yaml_document_delete(doc);
      safeFree((char *)doc, sizeof(yaml_document_t));
    } IT_END

  } DESCRIBE_END
}

/* ------------------------------------------------------------------ */
static void testYaml2JSONStrings(void) {
  DESCRIBE("yaml2JSON string scalars") {
    SET_BEFORE_EACH(beforeEach);
    SET_AFTER_EACH(afterEach);

    IT("converts a plain string value to a JSON string") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("greeting: hello\n");
      ASSERT_NOT_NULL(root);
      ASSERT_TRUE(jsonIsObject(root));
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "greeting");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsString(val));
      ASSERT_EQUAL_STR("hello", jsonAsString(val));
    } IT_END

    IT("preserves a quoted integer-looking value as a string") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("code: \"42\"\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "code");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsString(val));
      ASSERT_EQUAL_STR("42", jsonAsString(val));
    } IT_END

    IT("handles a multi-word string value") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("message: hello world\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "message");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsString(val));
      ASSERT_EQUAL_STR("hello world", jsonAsString(val));
    } IT_END

  } DESCRIBE_END
}

/* ------------------------------------------------------------------ */
static void testYaml2JSONIntegers(void) {
  DESCRIBE("yaml2JSON integer values") {
    SET_BEFORE_EACH(beforeEach);
    SET_AFTER_EACH(afterEach);

    IT("converts a plain decimal integer to a JSON int64") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("count: 42\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "count");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsInt64(val));
      ASSERT_EQUAL_INT(42, (int)jsonAsInt64(val));
    } IT_END

    IT("converts zero") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("zero: 0\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "zero");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsInt64(val));
      ASSERT_EQUAL_INT(0, (int)jsonAsInt64(val));
    } IT_END

    IT("converts a large integer") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("big: 1000000\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "big");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsInt64(val));
      ASSERT_EQUAL_INT(1000000, (int)jsonAsInt64(val));
    } IT_END

  } DESCRIBE_END
}

/* ------------------------------------------------------------------ */
static void testYaml2JSONBooleans(void) {
  DESCRIBE("yaml2JSON boolean values") {
    SET_BEFORE_EACH(beforeEach);
    SET_AFTER_EACH(afterEach);

    IT("converts lowercase true") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("flag: true\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "flag");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsBoolean(val));
      ASSERT_TRUE(jsonAsBoolean(val));
    } IT_END

    IT("converts title-case True") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("flag: True\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "flag");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsBoolean(val));
      ASSERT_TRUE(jsonAsBoolean(val));
    } IT_END

    IT("converts uppercase TRUE") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("flag: TRUE\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "flag");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsBoolean(val));
      ASSERT_TRUE(jsonAsBoolean(val));
    } IT_END

    IT("converts lowercase false") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("flag: false\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "flag");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsBoolean(val));
      ASSERT_FALSE(jsonAsBoolean(val));
    } IT_END

    IT("converts title-case False") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("flag: False\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "flag");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsBoolean(val));
      ASSERT_FALSE(jsonAsBoolean(val));
    } IT_END

    IT("converts uppercase FALSE") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("flag: FALSE\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "flag");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsBoolean(val));
      ASSERT_FALSE(jsonAsBoolean(val));
    } IT_END

  } DESCRIBE_END
}

/* ------------------------------------------------------------------ */
static void testYaml2JSONNulls(void) {
  DESCRIBE("yaml2JSON null values") {
    SET_BEFORE_EACH(beforeEach);
    SET_AFTER_EACH(afterEach);

    IT("converts the keyword null to JSON null") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("val: null\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "val");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsNull(val));
    } IT_END

    IT("converts the tilde ~ shorthand to JSON null") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("val: ~\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "val");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsNull(val));
    } IT_END

    IT("converts Null (title case) to JSON null") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("val: Null\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "val");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsNull(val));
    } IT_END

    IT("converts NULL (uppercase) to JSON null") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("val: NULL\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "val");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsNull(val));
    } IT_END

    IT("converts an empty plain scalar to JSON null") {
      TEST_COVERS(yaml2JSON);
      /* "val:\n" has an implicit empty plain scalar -- treated as null. */
      Json *root = yamlStrToJSON("val:\n");
      ASSERT_NOT_NULL(root);
      Json *val = jsonObjectGetPropertyValue(jsonAsObject(root), "val");
      ASSERT_NOT_NULL(val);
      ASSERT_TRUE(jsonIsNull(val));
    } IT_END

  } DESCRIBE_END
}

/* ------------------------------------------------------------------ */
static void testYaml2JSONNestedObjects(void) {
  DESCRIBE("yaml2JSON nested objects") {
    SET_BEFORE_EACH(beforeEach);
    SET_AFTER_EACH(afterEach);

    IT("converts a two-level nested mapping") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON(
        "outer:\n"
        "  inner: deep\n"
      );
      ASSERT_NOT_NULL(root);
      ASSERT_TRUE(jsonIsObject(root));
      Json *outerVal = jsonObjectGetPropertyValue(jsonAsObject(root), "outer");
      ASSERT_NOT_NULL(outerVal);
      ASSERT_TRUE(jsonIsObject(outerVal));
      Json *innerVal = jsonObjectGetPropertyValue(jsonAsObject(outerVal), "inner");
      ASSERT_NOT_NULL(innerVal);
      ASSERT_EQUAL_STR("deep", jsonAsString(innerVal));
    } IT_END

    IT("converts three-level nesting") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON(
        "a:\n"
        "  b:\n"
        "    c: leaf\n"
      );
      ASSERT_NOT_NULL(root);
      Json *a = jsonObjectGetPropertyValue(jsonAsObject(root), "a");
      ASSERT_NOT_NULL(a);
      Json *b = jsonObjectGetPropertyValue(jsonAsObject(a), "b");
      ASSERT_NOT_NULL(b);
      Json *c = jsonObjectGetPropertyValue(jsonAsObject(b), "c");
      ASSERT_NOT_NULL(c);
      ASSERT_EQUAL_STR("leaf", jsonAsString(c));
    } IT_END

    IT("preserves sibling keys at the top level") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON(
        "name: Alice\n"
        "age: 30\n"
      );
      ASSERT_NOT_NULL(root);
      Json *name = jsonObjectGetPropertyValue(jsonAsObject(root), "name");
      ASSERT_NOT_NULL(name);
      ASSERT_EQUAL_STR("Alice", jsonAsString(name));
      Json *age = jsonObjectGetPropertyValue(jsonAsObject(root), "age");
      ASSERT_NOT_NULL(age);
      ASSERT_EQUAL_INT(30, (int)jsonAsInt64(age));
    } IT_END

  } DESCRIBE_END
}

/* ------------------------------------------------------------------ */
static void testYaml2JSONArrays(void) {
  DESCRIBE("yaml2JSON array (sequence) values") {
    SET_BEFORE_EACH(beforeEach);
    SET_AFTER_EACH(afterEach);

    IT("converts a block sequence of strings") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON(
        "colors:\n"
        "  - red\n"
        "  - green\n"
        "  - blue\n"
      );
      ASSERT_NOT_NULL(root);
      Json *colors = jsonObjectGetPropertyValue(jsonAsObject(root), "colors");
      ASSERT_NOT_NULL(colors);
      ASSERT_TRUE(jsonIsArray(colors));
      JsonArray *arr = jsonAsArray(colors);
      ASSERT_EQUAL_INT(3, jsonArrayGetCount(arr));
      ASSERT_EQUAL_STR("red",   jsonAsString(jsonArrayGetItem(arr, 0)));
      ASSERT_EQUAL_STR("green", jsonAsString(jsonArrayGetItem(arr, 1)));
      ASSERT_EQUAL_STR("blue",  jsonAsString(jsonArrayGetItem(arr, 2)));
    } IT_END

    IT("converts a block sequence of integers") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON(
        "nums:\n"
        "  - 1\n"
        "  - 2\n"
        "  - 3\n"
      );
      ASSERT_NOT_NULL(root);
      Json *nums = jsonObjectGetPropertyValue(jsonAsObject(root), "nums");
      ASSERT_NOT_NULL(nums);
      ASSERT_TRUE(jsonIsArray(nums));
      JsonArray *arr = jsonAsArray(nums);
      ASSERT_EQUAL_INT(3, jsonArrayGetCount(arr));
      ASSERT_EQUAL_INT(1, (int)jsonAsInt64(jsonArrayGetItem(arr, 0)));
      ASSERT_EQUAL_INT(2, (int)jsonAsInt64(jsonArrayGetItem(arr, 1)));
      ASSERT_EQUAL_INT(3, (int)jsonAsInt64(jsonArrayGetItem(arr, 2)));
    } IT_END

    IT("converts a sequence of objects") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON(
        "people:\n"
        "  - name: Alice\n"
        "    age: 30\n"
        "  - name: Bob\n"
        "    age: 25\n"
      );
      ASSERT_NOT_NULL(root);
      Json *people = jsonObjectGetPropertyValue(jsonAsObject(root), "people");
      ASSERT_NOT_NULL(people);
      ASSERT_TRUE(jsonIsArray(people));
      JsonArray *arr = jsonAsArray(people);
      ASSERT_EQUAL_INT(2, jsonArrayGetCount(arr));
      Json *alice = jsonArrayGetItem(arr, 0);
      ASSERT_NOT_NULL(alice);
      ASSERT_EQUAL_STR("Alice",
        jsonAsString(jsonObjectGetPropertyValue(jsonAsObject(alice), "name")));
      ASSERT_EQUAL_INT(30,
        (int)jsonAsInt64(jsonObjectGetPropertyValue(jsonAsObject(alice), "age")));
      Json *bob = jsonArrayGetItem(arr, 1);
      ASSERT_NOT_NULL(bob);
      ASSERT_EQUAL_STR("Bob",
        jsonAsString(jsonObjectGetPropertyValue(jsonAsObject(bob), "name")));
    } IT_END

    IT("handles an empty flow sequence []") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON("items: []\n");
      ASSERT_NOT_NULL(root);
      Json *items = jsonObjectGetPropertyValue(jsonAsObject(root), "items");
      ASSERT_NOT_NULL(items);
      ASSERT_TRUE(jsonIsArray(items));
      ASSERT_EQUAL_INT(0, jsonArrayGetCount(jsonAsArray(items)));
    } IT_END

  } DESCRIBE_END
}

/* ------------------------------------------------------------------ */
static void testYaml2JSONMixed(void) {
  DESCRIBE("yaml2JSON mixed-type object") {
    SET_BEFORE_EACH(beforeEach);
    SET_AFTER_EACH(afterEach);

    IT("round-trips an object with string, int, bool, and null fields") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlStrToJSON(
        "host: localhost\n"
        "port: 8080\n"
        "secure: false\n"
        "token: null\n"
      );
      ASSERT_NOT_NULL(root);
      ASSERT_TRUE(jsonIsObject(root));

      Json *host = jsonObjectGetPropertyValue(jsonAsObject(root), "host");
      ASSERT_NOT_NULL(host);
      ASSERT_TRUE(jsonIsString(host));
      ASSERT_EQUAL_STR("localhost", jsonAsString(host));

      Json *port = jsonObjectGetPropertyValue(jsonAsObject(root), "port");
      ASSERT_NOT_NULL(port);
      ASSERT_TRUE(jsonIsInt64(port));
      ASSERT_EQUAL_INT(8080, (int)jsonAsInt64(port));

      Json *secure = jsonObjectGetPropertyValue(jsonAsObject(root), "secure");
      ASSERT_NOT_NULL(secure);
      ASSERT_TRUE(jsonIsBoolean(secure));
      ASSERT_FALSE(jsonAsBoolean(secure));

      Json *token = jsonObjectGetPropertyValue(jsonAsObject(root), "token");
      ASSERT_NOT_NULL(token);
      ASSERT_TRUE(jsonIsNull(token));
    } IT_END

  } DESCRIBE_END
}

/* ------------------------------------------------------------------ */
/*
 * schemadata/yamltypes.yaml (500 bytes) -- plain scalar values.
 *
 * Structure (top-level key "zowe"):
 *   zowe.default.port           : 3000           (int)
 *   zowe.default.hlq            : "ZWE.PROD"     (string, double-quoted)
 *   zowe.setup.certificate.pkcs12: ~             (null)
 *   zowe.setup.certificate.type : PKCS12         (string)
 *   zowe.setup.mvs.foo          : 3              (int)
 *   zowe.setup.mvs.goforit      : true           (bool)
 *   zowe.setup.mvs.authLoadlib  : ~              (null)
 *   zowe.setup.mvs.jcllib       : IBMUSER...     (string)
 *   (plus several ${{ }} template keys -- tested as unevaluated objects)
 */
static void testYamlTypesFile(void) {
  DESCRIBE("schemadata/yamltypes.yaml - plain scalar values") {
    SET_BEFORE_EACH(beforeEachFile);
    SET_AFTER_EACH(afterEachFile);

    IT("parses zowe.default.port as integer 3000") {
      TEST_COVERS(readYAML);
      TEST_COVERS(yaml2JSON);
      Json *root  = yamlFileToJSON("./schemadata/yamltypes.yaml");
      ASSERT_NOT_NULL(root);
      Json *zowe  = jsonObjectGetPropertyValue(jsonAsObject(root), "zowe");
      ASSERT_NOT_NULL(zowe);
      Json *def   = jsonObjectGetPropertyValue(jsonAsObject(zowe), "default");
      ASSERT_NOT_NULL(def);
      Json *port  = jsonObjectGetPropertyValue(jsonAsObject(def), "port");
      ASSERT_NOT_NULL(port);
      ASSERT_TRUE(jsonIsInt64(port));
      ASSERT_EQUAL_INT(3000, (int)jsonAsInt64(port));
    } IT_END

    IT("parses zowe.default.hlq as string ZWE.PROD") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlFileToJSON("./schemadata/yamltypes.yaml");
      ASSERT_NOT_NULL(root);
      Json *zowe = jsonObjectGetPropertyValue(jsonAsObject(root), "zowe");
      Json *def  = jsonObjectGetPropertyValue(jsonAsObject(zowe), "default");
      Json *hlq  = jsonObjectGetPropertyValue(jsonAsObject(def), "hlq");
      ASSERT_NOT_NULL(hlq);
      ASSERT_TRUE(jsonIsString(hlq));
      ASSERT_EQUAL_STR("ZWE.PROD", jsonAsString(hlq));
    } IT_END

    IT("parses zowe.setup.certificate.pkcs12 as null (tilde)") {
      TEST_COVERS(yaml2JSON);
      Json *root   = yamlFileToJSON("./schemadata/yamltypes.yaml");
      ASSERT_NOT_NULL(root);
      Json *zowe   = jsonObjectGetPropertyValue(jsonAsObject(root), "zowe");
      Json *setup  = jsonObjectGetPropertyValue(jsonAsObject(zowe), "setup");
      Json *cert   = jsonObjectGetPropertyValue(jsonAsObject(setup), "certificate");
      ASSERT_NOT_NULL(cert);
      Json *pkcs12 = jsonObjectGetPropertyValue(jsonAsObject(cert), "pkcs12");
      ASSERT_NOT_NULL(pkcs12);
      ASSERT_TRUE(jsonIsNull(pkcs12));
    } IT_END

    IT("parses zowe.setup.certificate.type as string PKCS12") {
      TEST_COVERS(yaml2JSON);
      Json *root  = yamlFileToJSON("./schemadata/yamltypes.yaml");
      ASSERT_NOT_NULL(root);
      Json *zowe  = jsonObjectGetPropertyValue(jsonAsObject(root), "zowe");
      Json *setup = jsonObjectGetPropertyValue(jsonAsObject(zowe), "setup");
      Json *cert  = jsonObjectGetPropertyValue(jsonAsObject(setup), "certificate");
      Json *type  = jsonObjectGetPropertyValue(jsonAsObject(cert), "type");
      ASSERT_NOT_NULL(type);
      ASSERT_TRUE(jsonIsString(type));
      ASSERT_EQUAL_STR("PKCS12", jsonAsString(type));
    } IT_END

    IT("parses zowe.setup.mvs.foo as integer 3") {
      TEST_COVERS(yaml2JSON);
      Json *root  = yamlFileToJSON("./schemadata/yamltypes.yaml");
      ASSERT_NOT_NULL(root);
      Json *zowe  = jsonObjectGetPropertyValue(jsonAsObject(root), "zowe");
      Json *setup = jsonObjectGetPropertyValue(jsonAsObject(zowe), "setup");
      Json *mvs   = jsonObjectGetPropertyValue(jsonAsObject(setup), "mvs");
      ASSERT_NOT_NULL(mvs);
      Json *foo   = jsonObjectGetPropertyValue(jsonAsObject(mvs), "foo");
      ASSERT_NOT_NULL(foo);
      ASSERT_TRUE(jsonIsInt64(foo));
      ASSERT_EQUAL_INT(3, (int)jsonAsInt64(foo));
    } IT_END

    IT("parses zowe.setup.mvs.goforit as boolean true") {
      TEST_COVERS(yaml2JSON);
      Json *root    = yamlFileToJSON("./schemadata/yamltypes.yaml");
      ASSERT_NOT_NULL(root);
      Json *zowe    = jsonObjectGetPropertyValue(jsonAsObject(root), "zowe");
      Json *setup   = jsonObjectGetPropertyValue(jsonAsObject(zowe), "setup");
      Json *mvs     = jsonObjectGetPropertyValue(jsonAsObject(setup), "mvs");
      Json *goforit = jsonObjectGetPropertyValue(jsonAsObject(mvs), "goforit");
      ASSERT_NOT_NULL(goforit);
      ASSERT_TRUE(jsonIsBoolean(goforit));
      ASSERT_TRUE(jsonAsBoolean(goforit));
    } IT_END

    IT("parses zowe.setup.mvs.authLoadlib as null (tilde)") {
      TEST_COVERS(yaml2JSON);
      Json *root  = yamlFileToJSON("./schemadata/yamltypes.yaml");
      ASSERT_NOT_NULL(root);
      Json *zowe  = jsonObjectGetPropertyValue(jsonAsObject(root), "zowe");
      Json *setup = jsonObjectGetPropertyValue(jsonAsObject(zowe), "setup");
      Json *mvs   = jsonObjectGetPropertyValue(jsonAsObject(setup), "mvs");
      Json *allib = jsonObjectGetPropertyValue(jsonAsObject(mvs), "authLoadlib");
      ASSERT_NOT_NULL(allib);
      ASSERT_TRUE(jsonIsNull(allib));
    } IT_END

    IT("template expression keys produce unevaluated JSON objects") {
      TEST_COVERS(yaml2JSON);
      /*
       * Keys like hlq2 have ${{ }} template values.  yaml2JSON converts
       * each into { "__zowe_internal_type__": "unevaluated", "source": ... }.
       */
      Json *root  = yamlFileToJSON("./schemadata/yamltypes.yaml");
      ASSERT_NOT_NULL(root);
      Json *zowe  = jsonObjectGetPropertyValue(jsonAsObject(root), "zowe");
      Json *setup = jsonObjectGetPropertyValue(jsonAsObject(zowe), "setup");
      Json *mvs   = jsonObjectGetPropertyValue(jsonAsObject(setup), "mvs");
      /* hlq2: ${{ zowe.default.hlq+".BAZ" }} */
      Json *hlq2  = jsonObjectGetPropertyValue(jsonAsObject(mvs), "hlq2");
      ASSERT_NOT_NULL(hlq2);
      ASSERT_TRUE(jsonIsObject(hlq2));
      Json *itype = jsonObjectGetPropertyValue(jsonAsObject(hlq2), ZOWE_INTERNAL_TYPE);
      ASSERT_NOT_NULL(itype);
      ASSERT_TRUE(jsonIsString(itype));
      ASSERT_EQUAL_STR(ZOWE_UNEVALUATED, jsonAsString(itype));
    } IT_END

  } DESCRIBE_END
}

/* ------------------------------------------------------------------ */
/*
 * configmgr/extract/extract.yaml (596 bytes).
 *
 * Root key "test" holds a rich set of types:
 *   number1: 0            (int)
 *   number2: "${{...}}"   (template -> unevaluated object)
 *   number3: 123456789    (int)
 *   boolean1: false       (bool)
 *   null1: ~              (null)
 *   null2: null           (null)
 *   null3:                (empty plain -> null)
 *   string1: 'Hello, World!' (string)
 *   array1: [apple, banana, kiwi]
 *   array2: [3,1,4,1,5,9,2,6,5,3,5]  (11 items)
 *   array4: []
 *   array5: [~, ~, ~]    (3 null items)
 */
static void testExtractYAMLFile(void) {
  DESCRIBE("configmgr/extract/extract.yaml - diverse types") {
    SET_BEFORE_EACH(beforeEachFile);
    SET_AFTER_EACH(afterEachFile);

    IT("parses test.number1 as integer 0") {
      TEST_COVERS(readYAML);
      TEST_COVERS(yaml2JSON);
      Json *root    = yamlFileToJSON("./configmgr/extract/extract.yaml");
      ASSERT_NOT_NULL(root);
      Json *test    = jsonObjectGetPropertyValue(jsonAsObject(root), "test");
      ASSERT_NOT_NULL(test);
      ASSERT_TRUE(jsonIsObject(test));
      Json *number1 = jsonObjectGetPropertyValue(jsonAsObject(test), "number1");
      ASSERT_NOT_NULL(number1);
      ASSERT_TRUE(jsonIsInt64(number1));
      ASSERT_EQUAL_INT(0, (int)jsonAsInt64(number1));
    } IT_END

    IT("parses test.number3 as integer 123456789") {
      TEST_COVERS(yaml2JSON);
      Json *root    = yamlFileToJSON("./configmgr/extract/extract.yaml");
      Json *test    = jsonObjectGetPropertyValue(jsonAsObject(root), "test");
      Json *number3 = jsonObjectGetPropertyValue(jsonAsObject(test), "number3");
      ASSERT_NOT_NULL(number3);
      ASSERT_TRUE(jsonIsInt64(number3));
      ASSERT_EQUAL_INT(123456789, (int)jsonAsInt64(number3));
    } IT_END

    IT("parses test.boolean1 as boolean false") {
      TEST_COVERS(yaml2JSON);
      Json *root     = yamlFileToJSON("./configmgr/extract/extract.yaml");
      Json *test     = jsonObjectGetPropertyValue(jsonAsObject(root), "test");
      Json *boolean1 = jsonObjectGetPropertyValue(jsonAsObject(test), "boolean1");
      ASSERT_NOT_NULL(boolean1);
      ASSERT_TRUE(jsonIsBoolean(boolean1));
      ASSERT_FALSE(jsonAsBoolean(boolean1));
    } IT_END

    IT("parses test.null1 (tilde) as JSON null") {
      TEST_COVERS(yaml2JSON);
      Json *root  = yamlFileToJSON("./configmgr/extract/extract.yaml");
      Json *test  = jsonObjectGetPropertyValue(jsonAsObject(root), "test");
      Json *null1 = jsonObjectGetPropertyValue(jsonAsObject(test), "null1");
      ASSERT_NOT_NULL(null1);
      ASSERT_TRUE(jsonIsNull(null1));
    } IT_END

    IT("parses test.null2 (null keyword) as JSON null") {
      TEST_COVERS(yaml2JSON);
      Json *root  = yamlFileToJSON("./configmgr/extract/extract.yaml");
      Json *test  = jsonObjectGetPropertyValue(jsonAsObject(root), "test");
      Json *null2 = jsonObjectGetPropertyValue(jsonAsObject(test), "null2");
      ASSERT_NOT_NULL(null2);
      ASSERT_TRUE(jsonIsNull(null2));
    } IT_END

    IT("parses test.null3 (empty plain scalar) as JSON null") {
      TEST_COVERS(yaml2JSON);
      Json *root  = yamlFileToJSON("./configmgr/extract/extract.yaml");
      Json *test  = jsonObjectGetPropertyValue(jsonAsObject(root), "test");
      Json *null3 = jsonObjectGetPropertyValue(jsonAsObject(test), "null3");
      ASSERT_NOT_NULL(null3);
      ASSERT_TRUE(jsonIsNull(null3));
    } IT_END

    IT("parses test.string1 as 'Hello, World!'") {
      TEST_COVERS(yaml2JSON);
      Json *root    = yamlFileToJSON("./configmgr/extract/extract.yaml");
      Json *test    = jsonObjectGetPropertyValue(jsonAsObject(root), "test");
      Json *string1 = jsonObjectGetPropertyValue(jsonAsObject(test), "string1");
      ASSERT_NOT_NULL(string1);
      ASSERT_TRUE(jsonIsString(string1));
      ASSERT_EQUAL_STR("Hello, World!", jsonAsString(string1));
    } IT_END

    IT("parses test.array1 as a 3-element string array") {
      TEST_COVERS(yaml2JSON);
      Json *root   = yamlFileToJSON("./configmgr/extract/extract.yaml");
      Json *test   = jsonObjectGetPropertyValue(jsonAsObject(root), "test");
      Json *array1 = jsonObjectGetPropertyValue(jsonAsObject(test), "array1");
      ASSERT_NOT_NULL(array1);
      ASSERT_TRUE(jsonIsArray(array1));
      JsonArray *arr = jsonAsArray(array1);
      ASSERT_EQUAL_INT(3, jsonArrayGetCount(arr));
      ASSERT_EQUAL_STR("apple",  jsonAsString(jsonArrayGetItem(arr, 0)));
      ASSERT_EQUAL_STR("banana", jsonAsString(jsonArrayGetItem(arr, 1)));
      ASSERT_EQUAL_STR("kiwi",   jsonAsString(jsonArrayGetItem(arr, 2)));
    } IT_END

    IT("parses test.array2 as an 11-element integer flow sequence") {
      TEST_COVERS(yaml2JSON);
      Json *root   = yamlFileToJSON("./configmgr/extract/extract.yaml");
      Json *test   = jsonObjectGetPropertyValue(jsonAsObject(root), "test");
      Json *array2 = jsonObjectGetPropertyValue(jsonAsObject(test), "array2");
      ASSERT_NOT_NULL(array2);
      ASSERT_TRUE(jsonIsArray(array2));
      JsonArray *arr = jsonAsArray(array2);
      ASSERT_EQUAL_INT(11, jsonArrayGetCount(arr));
      /* sequence is [3,1,4,1,5,9,2,6,5,3,5] */
      ASSERT_EQUAL_INT(3, (int)jsonAsInt64(jsonArrayGetItem(arr, 0)));
      ASSERT_EQUAL_INT(9, (int)jsonAsInt64(jsonArrayGetItem(arr, 5)));
    } IT_END

    IT("parses test.array4 as an empty array") {
      TEST_COVERS(yaml2JSON);
      Json *root   = yamlFileToJSON("./configmgr/extract/extract.yaml");
      Json *test   = jsonObjectGetPropertyValue(jsonAsObject(root), "test");
      Json *array4 = jsonObjectGetPropertyValue(jsonAsObject(test), "array4");
      ASSERT_NOT_NULL(array4);
      ASSERT_TRUE(jsonIsArray(array4));
      ASSERT_EQUAL_INT(0, jsonArrayGetCount(jsonAsArray(array4)));
    } IT_END

    IT("parses test.array5 [~,~,~] as a 3-element null array") {
      TEST_COVERS(yaml2JSON);
      Json *root   = yamlFileToJSON("./configmgr/extract/extract.yaml");
      Json *test   = jsonObjectGetPropertyValue(jsonAsObject(root), "test");
      Json *array5 = jsonObjectGetPropertyValue(jsonAsObject(test), "array5");
      ASSERT_NOT_NULL(array5);
      ASSERT_TRUE(jsonIsArray(array5));
      JsonArray *arr = jsonAsArray(array5);
      ASSERT_EQUAL_INT(3, jsonArrayGetCount(arr));
      ASSERT_TRUE(jsonIsNull(jsonArrayGetItem(arr, 0)));
      ASSERT_TRUE(jsonIsNull(jsonArrayGetItem(arr, 1)));
      ASSERT_TRUE(jsonIsNull(jsonArrayGetItem(arr, 2)));
    } IT_END

  } DESCRIBE_END
}

/* ------------------------------------------------------------------ */
/*
 * schemadata/example-zowe.yaml (~21 KB).
 * A real-world, comment-heavy, deeply-nested Zowe configuration file.
 * These tests verify load-and-convert works on a moderately large real file.
 */
static void testExampleZoweYAMLFile(void) {
  DESCRIBE("schemadata/example-zowe.yaml - real-world config load") {
    SET_BEFORE_EACH(beforeEachFile);
    SET_AFTER_EACH(afterEachFile);

    IT("loads and returns a non-null JSON object") {
      TEST_COVERS(readYAML);
      TEST_COVERS(yaml2JSON);
      Json *root = yamlFileToJSON("./schemadata/example-zowe.yaml");
      ASSERT_NOT_NULL(root);
      ASSERT_TRUE(jsonIsObject(root));
    } IT_END

    IT("has a top-level 'zowe' key that is an object") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlFileToJSON("./schemadata/example-zowe.yaml");
      ASSERT_NOT_NULL(root);
      Json *zowe = jsonObjectGetPropertyValue(jsonAsObject(root), "zowe");
      ASSERT_NOT_NULL(zowe);
      ASSERT_TRUE(jsonIsObject(zowe));
    } IT_END

  } DESCRIBE_END
}

/* ------------------------------------------------------------------ */
/*
 * Generates a ~1 MiB YAML mapping at runtime.  Each line has the form:
 *   item_NNNNNN: "value_NNNNNN"
 * Verifies that the parser and converter handle large inputs and return
 * correct values for specific known entries.
 */
static void testLargeYAMLFile(void) {
  DESCRIBE("yaml2JSON - ~1 MiB generated YAML mapping") {
    SET_BEFORE_EACH(beforeEachLargeYAML);
    SET_AFTER_EACH(afterEachLargeYAML);

    IT("reads and converts a ~1 MiB YAML file without error") {
      TEST_COVERS(readYAML);
      TEST_COVERS(yaml2JSON);
      char errorBuf[512] = {0};
      yaml_document_t *doc = readYAML(s_largeTempPath, errorBuf, sizeof(errorBuf));
      ASSERT_NOT_NULL(doc);
      Json *root = yaml2JSON(doc, s_slh);
      yaml_document_delete(doc);
      safeFree((char *)doc, sizeof(yaml_document_t));
      ASSERT_NOT_NULL(root);
      ASSERT_TRUE(jsonIsObject(root));
    } IT_END

    IT("first entry item_000000 equals \"value_000000\"") {
      TEST_COVERS(yaml2JSON);
      Json *root  = yamlFileToJSON(s_largeTempPath);
      ASSERT_NOT_NULL(root);
      Json *first = jsonObjectGetPropertyValue(jsonAsObject(root), "item_000000");
      ASSERT_NOT_NULL(first);
      ASSERT_TRUE(jsonIsString(first));
      ASSERT_EQUAL_STR("value_000000", jsonAsString(first));
    } IT_END

    IT("entry item_000100 equals \"value_000100\"") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlFileToJSON(s_largeTempPath);
      ASSERT_NOT_NULL(root);
      Json *mid  = jsonObjectGetPropertyValue(jsonAsObject(root), "item_000100");
      ASSERT_NOT_NULL(mid);
      ASSERT_TRUE(jsonIsString(mid));
      ASSERT_EQUAL_STR("value_000100", jsonAsString(mid));
    } IT_END

    IT("entry item_001000 equals \"value_001000\"") {
      TEST_COVERS(yaml2JSON);
      Json *root = yamlFileToJSON(s_largeTempPath);
      ASSERT_NOT_NULL(root);
      Json *mid  = jsonObjectGetPropertyValue(jsonAsObject(root), "item_001000");
      ASSERT_NOT_NULL(mid);
      ASSERT_TRUE(jsonIsString(mid));
      ASSERT_EQUAL_STR("value_001000", jsonAsString(mid));
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Entry point
 * =================================================================== */

int main(void) {
  zoweTestInit();

  testReadYAMLErrors();
  testYaml2JSONStrings();
  testYaml2JSONIntegers();
  testYaml2JSONBooleans();
  testYaml2JSONNulls();
  testYaml2JSONNestedObjects();
  testYaml2JSONArrays();
  testYaml2JSONMixed();
  testYamlTypesFile();
  testExtractYAMLFile();
  testExampleZoweYAMLFile();
  testLargeYAMLFile();

  return ZOWE_TEST_REPORT();
}
