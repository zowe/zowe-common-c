# json-conversion tests

Tests for the `jsonPrinter` error-flag split in `c/json.c` / `h/json.h`:
`ioErrorFlag` (hard — gates all output via `jsonShouldStopWriting`) vs
`dataConversionErrorFlag` (soft — records an unmappable-byte failure without
suppressing the rest of the document).

Deliberately minimal: three cases, eight checks. It covers the regression and
the behaviour the fix must not have broken, and nothing else. See
[Adding more](#adding-more) for the obvious next tests.

## Running

z/OS, in this directory:

```
make            # build json-conversion-test
make test       # build + run
make clean
```

The test program prints one line per check and exits with the number of
failures, so `make test` fails the shell on a regression.

No ZIS, no TLS, no server and no datasets are involved — the object list in the
`Makefile` is a small subset of the `schematest` one in `../Makefile`, just the
JSON printer and the charset converter underneath it.

## What it covers

1. **The regression** — a value the converter cannot map used to call
   `jsonSetIOErrorFlag()`, which `jsonShouldStopWriting()` honours, so every
   later `jsonAddX`/`jsonEnd` became a no-op and one bad byte produced a
   truncated, unparseable document. The test drives a printer whose
   `inputCCSID` has no converter, then asserts the soft flag is set,
   `ioErrorFlag` is *not*, printing continued past the bad value, and the
   document is closed rather than cut off.
2. **The hard stop is preserved** — with `ioErrorFlag` raised, nothing more is
   written, not even the closing brace.
3. **The happy path is byte-for-byte unchanged** — exact match against
   `{"name":"zowe","n":5}`, with neither flag raised.

## How the failure is injected

`UNCONVERTIBLE_CCSID` is hardcoded to **9999**, which is not a registered CCSID
and has no converter anywhere in `c/charsets.c`:

- on the **iconv** builds (Linux, AIX, z/OS xlclang/clang) `getCharsetName()`
  only names 819, 1047, UTF-8 and the UTF-16 family, and `convertCharset()`
  returns `CHARSET_UNKNOWN_CCSID` for anything else — before `iconv_open()` is
  even reached;
- on the **metal/CUNLCNV** build `CUNLCNV` rejects it with a non-zero return
  code, giving `CHARSET_CONVERSION_ROUTINE_FAILURE`.

Either way `convertToUtf8()` returns `-1`, which is the only thing `json.c`
reacts to, so the constant is a faithful stand-in for the real-world cause (a
byte with no mapping in the target charset).

A printer built with that CCSID fails to convert every caller-supplied string,
while the printer's own structural characters (`{ } [ ] , : "`) are written from
`SOURCE_CODE_CHARSET` as usual — exactly the situation the fix is about.

The first check in case 1 asserts the constant really is unconvertible, so if
`charsets.c` ever grows a converter for 9999 the test says so plainly instead of
quietly passing for the wrong reason.

On z/OS `SOURCE_CODE_CHARSET` is `CCSID_IBM1047`, so the printer also converts
its own literals from EBCDIC — that path is healthy and unaffected by the
injected failure, which only hits data tagged with `p->inputCCSID`.

Note that keys go through the same conversion as values, so in case 1 the key
`"bad"` collapses to `""` along with its value; ints and booleans do not use
`inputCCSID` and come through intact. The document is `{"":"","":7}`.

## Adding more

Natural follow-ups, in rough order of value:

- `jsonPrinterReset` clears **both** flags — printers are recycled between
  requests, so a stale conversion error must not leak into the next response.
- Soft-flag **stickiness**: it stays set across a later successful write until
  `jsonClearDataConversionErrorFlag`, which is how `datasetjson.c` counts
  consecutive failures per record.
- **Multipart strings**: `jsonStartMultipartString` opens a quote, so a failing
  chunk must still leave the closing quote reachable.
- A **real write failure** via `makeCustomUtf8JsonPrinter` with a callback that
  refuses after N bytes, asserting it raises `ioErrorFlag` and *not* the
  conversion flag.
- A **structural balance check** on the output (matching `{`/`[`, quotes) if the
  assertions grow beyond "ends with `}`".
