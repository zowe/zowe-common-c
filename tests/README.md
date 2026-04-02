# zowe-common-c Test Suite

This directory contains the unit test infrastructure for `zowe-common-c`. Tests
are written in C99 and compile with `xlclang` on z/OS as well as `clang`/`gcc`
on Linux and macOS for local development.

## Directory structure

```
tests/
  Makefile            - Builds all test binaries and exposes run targets
  README.md           - This file
  unit/               - One subdirectory per tested module
    json/
      jsontest.c      - Tests for h/json.h and c/json.c
    yaml2json/
      yaml2jsontest.c - Tests for h/yaml2json.h and c/yaml2json.c
```

The naming rule is simple: tests for `h/foo.h` live at `unit/foo/footest.c`.
Keeping the test source in its own subdirectory prevents its object file from
colliding with the library object of the same name (e.g. `json.o` from
`c/json.c` vs the test's `unit/json/jsontest.o`). Each `unit/*/...test.c` file
has its own `main()` and compiles to a standalone executable named after the
module under test (e.g. `jsontest`).

The files outside `unit/` that pre-date this structure (e.g. `parsetest.c`,
`schematest.c`) remain as-is; they are not yet integrated into the framework.

## Test framework — `zowetests.h`

The framework lives in:
- `h/zowetests.h` — public API (macros and type declarations)
- `c/zowetests.c` — implementation

It is modelled after the popular JavaScript testing library Mocha, providing a
`describe`/`it` vocabulary that should be immediately recognisable to anyone
who has written JavaScript tests.

The implementation deliberately has no dependencies beyond C99 standard
headers (`stdio.h`, `string.h`, `setjmp.h`, `stdarg.h`, `stdbool.h`). It
works with the `xlclang` compiler on z/OS and imposes no requirement on
third-party libraries.

### Lifecycle

```
zoweTestInit()  -- called once in main() before any DESCRIBE block
DESCRIBE(...)   -- one or more named suites
ZOWE_TEST_REPORT() -- prints the summary and returns an exit code
```

Calling `ZOWE_TEST_REPORT()` as the return value of `main()` means the process
exits with code `0` when all tests pass and `1` when any test fails. This
integrates naturally with `make` and CI pipelines.

### `DESCRIBE` and `DESCRIBE_END`

Groups related test cases into a named suite. Suites print their own
pass/fail totals. Per-suite lifecycle hooks can be registered inside the
block with `SET_BEFORE_EACH` and `SET_AFTER_EACH`.

```c
DESCRIBE("JSON parser") {
  /* tests go here */
} DESCRIBE_END
```

Everything between `DESCRIBE` and `DESCRIBE_END` is ordinary C code, so you
can declare variables, register hooks, and structure helper code however you
like.

### `IT` and `IT_END`

Defines a single named test case. If any assertion inside the block fails,
execution jumps immediately to `IT_END` via `longjmp`; all code after the
failing assertion within that `IT` block is skipped. The test is recorded as
failed and execution continues with the next `IT` block.

```c
IT("parses a positive integer") {
  char errorBuf[256] = {0};
  Json *json = jsonParseString(slh, "42", errorBuf, sizeof(errorBuf));
  ASSERT_NOT_NULL(json);
  ASSERT_TRUE(jsonIsNumber(json));
  ASSERT_EQUAL_INT(jsonAsNumber(json), 42);
} IT_END
```

### `SET_BEFORE_EACH` and `SET_AFTER_EACH`

Registers a hook function to be called before or after every `IT` block in
the enclosing `DESCRIBE`. The argument must be a pointer to a function with
signature `void fn(void)`. Hooks are reset to `NULL` at the start of each new
`DESCRIBE` block.

```c
static ShortLivedHeap *slh = NULL;

static void setupHeap(void)   { slh = makeShortLivedHeap(0x10000, 100); }
static void teardownHeap(void){ SLHFree(slh); slh = NULL; }

DESCRIBE("JSON parser") {
  SET_BEFORE_EACH(setupHeap);
  SET_AFTER_EACH(teardownHeap);

  IT("parses null") {
    Json *json = jsonParseString(slh, "null", NULL, 0);
    ASSERT_NOT_NULL(json);
    ASSERT_TRUE(jsonIsNull(json));
  } IT_END
} DESCRIBE_END
```

The after-each hook is called even when a test fails, so it is safe to do
resource cleanup there.

### `TEST_COVERS`

Declares that the current `IT` block exercises a particular API function.
Pass the bare function name — no quotes, no parentheses. Duplicate
registrations for the same name are suppressed silently. `ZOWE_TEST_REPORT()`
prints a deduplicated list of every function exercised by the entire run.

```c
IT("writes a string property") {
  TEST_COVERS(jsonAddString);
  TEST_COVERS(jsonStart);
  TEST_COVERS(jsonEnd);
  /* ... */
} IT_END
```

This provides a lightweight form of functional coverage that works without any
compiler instrumentation.

### `ZOWE_TEST_REPORT`

Call this as the `return` statement of `main()`. It prints a final summary
table and returns `0` (success) or `1` (at least one failure).

```c
int main(void) {
  zoweTestInit();
  /* ... DESCRIBE blocks ... */
  return ZOWE_TEST_REPORT();
}
```

---

## Assertion reference

All assertions are macros. They increment an internal assertion counter and,
on failure, record the file name, line number, and a descriptive message before
calling `longjmp` to exit the enclosing `IT` block.

| Macro | Passes when |
|---|---|
| `ASSERT_TRUE(expr)` | `expr` is non-zero |
| `ASSERT_FALSE(expr)` | `expr` is zero |
| `ASSERT_NOT_NULL(ptr)` | `ptr != NULL` |
| `ASSERT_NULL(ptr)` | `ptr == NULL` |
| `ASSERT_EQUAL_INT(actual, expected)` | `(int)actual == (int)expected` |
| `ASSERT_NOT_EQUAL_INT(actual, unexpected)` | `(int)actual != (int)unexpected` |
| `ASSERT_GT_INT(actual, threshold)` | `(int)actual > (int)threshold` |
| `ASSERT_LT_INT(actual, threshold)` | `(int)actual < (int)threshold` |
| `ASSERT_EQUAL_STR(actual, expected)` | `strcmp(actual, expected) == 0` |
| `ASSERT_STR_CONTAINS(haystack, needle)` | `strstr(haystack, needle) != NULL` |
| `ASSERT_STR_NOT_CONTAINS(haystack, needle)` | `strstr(haystack, needle) == NULL` |
| `FAIL(message)` | Never — always records a failure with the given message |

### Integer assertions

Both sides are cast to `int` before comparison. Use `ASSERT_TRUE` with an
explicit cast for comparisons involving `long`, `size_t`, or pointer
differences.

```c
ASSERT_EQUAL_INT(jsonArrayGetCount(arr), 3);
ASSERT_GT_INT(buf->len, 0);
ASSERT_LT_INT(errorCode, 0);
```

### String assertions

`ASSERT_EQUAL_STR` uses `strcmp` and fails if either argument is `NULL`.
`ASSERT_STR_CONTAINS` uses `strstr`; a `NULL` haystack triggers a failure.
`ASSERT_STR_NOT_CONTAINS` treats a `NULL` haystack as not containing anything
(assertion passes).

```c
ASSERT_EQUAL_STR(jsonObjectGetString(obj, "city"), "Raleigh");
ASSERT_STR_CONTAINS(printerBuf, "\"enabled\"");
ASSERT_STR_NOT_CONTAINS(printerBuf, "\"x\"");
```

### Pointer assertions

```c
ASSERT_NOT_NULL(json);
ASSERT_NULL(jsonObjectGetPropertyValue(obj, "missing"));
```

---

## Sample test file

The following is a minimal but complete example demonstrating all framework
features. It would live at `tests/unit/collections.c` if you were testing
`h/collections.h`.

```c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "collections.h"
#include "zowetests.h"

static ShortLivedHeap *slh = NULL;

static void setupSlh(void) {
  slh = makeShortLivedHeap(0x10000, 100);
}

static void teardownSlh(void) {
  SLHFree(slh);
  slh = NULL;
}

static void testArrayList(void) {
  DESCRIBE("ArrayList") {
    SET_BEFORE_EACH(setupSlh);
    SET_AFTER_EACH(teardownSlh);

    IT("starts with zero elements") {
      TEST_COVERS(makeArrayList);
      TEST_COVERS(arrayListSize);
      ArrayList *list = makeArrayList(slh, 4, sizeof(int));
      ASSERT_NOT_NULL(list);
      ASSERT_EQUAL_INT(arrayListSize(list), 0);
    } IT_END

    IT("grows after an insert") {
      TEST_COVERS(arrayListAdd);
      ArrayList *list = makeArrayList(slh, 4, sizeof(int));
      int value = 42;
      arrayListAdd(list, &value);
      ASSERT_EQUAL_INT(arrayListSize(list), 1);
    } IT_END

    IT("retrieves the inserted element") {
      TEST_COVERS(arrayListGet);
      ArrayList *list = makeArrayList(slh, 4, sizeof(int));
      int value = 99;
      arrayListAdd(list, &value);
      int *retrieved = (int *)arrayListGet(list, 0);
      ASSERT_NOT_NULL(retrieved);
      ASSERT_EQUAL_INT(*retrieved, 99);
    } IT_END

  } DESCRIBE_END
}

int main(void) {
  zoweTestInit();
  testArrayList();
  return ZOWE_TEST_REPORT();
}
```

---

## Building and running

### On z/OS

The `tests/Makefile` uses the `xlclang` compiler with the same flags as the
rest of the build. A `make prepare` step is required only once to clone
third-party dependencies needed by `schematest`; the JSON tests have no such
dependency.

```sh
cd tests
make jsontest          # compile only
make test_json         # compile and run
make test              # run all unit/ tests
```

The `make test` target runs every `unit/` test binary and exits non-zero if
any of them fail, making it suitable for a CI gate.

### On Linux or macOS (local development)

You can compile a test binary directly with `clang` or `gcc` without needing
the full z/OS build environment:

```sh
cd tests
clang -I../h -I../platform/posix -D__ZOWE_OS_LINUX \
      -o json_test \
      unit/json.c \
      ../c/zowetests.c \
      ../c/json.c \
      ../c/alloc.c \
      ../c/utils.c \
      ../c/collections.c \
      ../c/charsets.c \
      ../c/xlate.c \
      ../c/timeutls.c \
      ../c/logging.c
./json_test
```

---

## Output format

The output is designed to be human-readable in a terminal and grep-friendly in
CI logs. A passing run looks like:

```
  JSON Writer - scalar values
    (/) writes a string property (4 assertions)
    (/) writes an integer property (2 assertions)
    (/) writes a null property (2 assertions)

    3 passing, 0 failing (8 assertions)

  ======================================
  Test Results
  ======================================
  Total:   3
  Passing: 3
  Failing: 0

  Functions exercised by these tests (3):
    - jsonAddString
    - jsonAddInt
    - jsonAddNull
  ======================================
```

A failing test prints the file name, line number, and assertion message:

```
    (X) writes a string property
        unit/json.c:87
        AssertionError: Expected "\"greeting\"" to be contained in ""
```

---

## Adding a new test file

1. Create `tests/unit/<name>/` and add `<name>test.c` inside it.
2. Copy the boilerplate from `tests/unit/json/jsontest.c` (includes, capture helpers if
   needed, `zoweTestInit()`, `ZOWE_TEST_REPORT()`).
3. Add the new binary to the Makefile:
   - Define a `<NAME>TESTOBJS` variable listing `unit/<name>/<name>test.o`, `zowetests.o`,
     and every `.o` the library under test depends on.
   - Add a link rule: `<name>test: $(<NAME>TESTOBJS)`.
   - Add an explicit compile rule for `unit/<name>.o`.
   - Extend the `test` phony target to depend on `test_<name>`.
   - Add `<name>test` to the `clean` rule.
4. Add `unit/<name>.o` to the `clean` pattern if not already covered.
