# charset-streaming tests

Regression tests for `convertCharsetStreaming()` (zowe-common-c `charsets.c`) --
the streaming charset converter behind the #828 fix. Runs under AddressSanitizer.

```
sh run.sh          # native clang (Linux/WSL), ASan on
SAN= sh run.sh     # no sanitizer
```

## What it covers

The fix has two behaviours; both were broken before (silent empty body / lost
characters). The tests pin them down:

**1. Carry-forward** -- a multibyte character split across a read-buffer boundary
must be reassembled, never lost or corrupted.
- *Absolute anchors* (T1-T4): exact-byte assertions -- a held partial `0xC3`, its
  completion to `0xE9`, an unmappable char -> `?`, a representable char kept.
- **Exhaustive straddle sweep** *(the important one)*: a mixed 1/2/3/4-byte UTF-8
  string (ASCII, e-acute, a-grave, coffee-cup, two CJK chars, an emoji -- all as
  numeric bytes) is streamed at **every** chunk size and the concatenated output
  must equal the single-shot conversion. This covers every possible split
  position of every character length.

**2. Substitution** -- characters the target cannot represent become `?`/TRANSLIT
instead of failing the response; representable characters are preserved.

**Edge cases**: file ending mid-character (truncated/mistagged source -> the
incomplete tail is held across the boundary then dropped at EOF, leaving a clean
prefix -- a well-formed file never reaches this), an invalid source byte
mid-stream (-> `?` and keep going), and ASCII identity across all chunk sizes.

## z/OS

`run.sh` is Linux/WSL only (Linux libs). `run-zos.sh` builds and runs this same
test on z/OS with ibm-clang64 (Open XL) against real z/OS `iconv` -- 9/9,
matching WSL. The z/OS `iconv` behaviour the fix relies on, confirmed by that run:

- `iconv_open` accepts `"ISO8859-1"` (no dash), rejects `"ISO-8859-1"` (errno
  121) -- the reason for the getCharsetName correction.
- an incomplete trailing multibyte sequence returns `EINVAL` and is left for the
  caller to carry forward (POSIX); a character unmappable in the target is
  substituted with SUB (0x1A), differing from glibc's `?` (cosmetic, noted for
  the UI). So the #828 empty body cannot return.
- a multibyte character split across two reads is reassembled.

Open XL requires the iconv branch: ibm-clang64 is `__ZOWE_COMP_CLANG`, not
`__ZOWE_COMP_XLCLANG` (see `charsets.c`). To validate the built product, exercise
a UTF-8 unixfile GET against a real server; this suite pins down the conversion.
