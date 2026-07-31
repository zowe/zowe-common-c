# directoryMakeDirectoryRecursive path and buffer-contract tests

Four checks against `c/zosfile.c`, runnable on z/OS:

1. **over-long path is rejected** -- a path longer than `USS_MAX_PATH_LENGTH`
   returns -1 instead of being assembled in the fixed-size stack buffer.
   Before the overflow fix this ABENDs 0C4 ([zss#2094]).
2. **NULL path is rejected** -- returns -1 rather than dereferencing. On z/OS a
   NULL dereference reads low core instead of failing, so this cannot be left
   to chance.
3. **an undersized output buffer is rejected** -- a buffer too small to hold
   the deepest path could only be filled with a truncated one, which names a
   directory that need not exist. The function refuses it up front, before
   anything is created, so -1 unambiguously means nothing was done.
4. **a correctly sized buffer receives the complete, real path** -- the
   reported path equals the path that was asked for, and it exists on disk.

Cases 3 and 4 are the pair that matters: together they say the function never
reports a directory that isn't real, and never half-fills a caller's buffer.

The test walks only directories that already exist and passes over-long or NULL
paths that are rejected before any `BPXMKD`. **It creates no directories and
writes nothing to the filesystem.** Every case prints the arguments it passed
and the values it got back, so a reader can check the verdict rather than
trust it.

## Running

```sh
# interactive shell with env.sh sourced (ibm-clang64 on PATH)
cd <zowe-common-c>
sh tests/zosfile-mkdir/run-zos.sh
```

Exit status is the number of failing checks (0 = all passed). Objects, per-file
compile errors and the untranscoded report land in `tests/zosfile-mkdir/zbuild/`.

If your output arrives as line noise, something in your toolchain is already
transcoding the command's output and the script's own conversion is running on
top of it; re-run with `ZOWE_TEST_RAW=1` and let that layer do the conversion.

## Before/after protocol

Case 3 is a reproducer, not just a regression guard. To see the defect and the
fix, check out this branch with `c/zosfile.c` reverted to its state before the
buffer contract was enforced and run:

```sh
sh tests/zosfile-mkdir/run-zos.sh     # 3 PASS, 1 FAIL (case 3), exit 1
```

Case 3 fails because the function accepts a 16-byte buffer, returns 0, and
writes a partial path into it. Restore the fix and re-run:

```sh
sh tests/zosfile-mkdir/run-zos.sh     # 4 PASS, 0 failures, exit 0
```

Cases 1 and 2 pass in both steps -- they guard the buffer-overflow fix itself.

## Why the sizes are what they are

`USS_MKDIR_PATH_BUFFER_SIZE` is `USS_MAX_PATH_LENGTH + 4`, and the `+4` is easy
to get wrong. The relative-path branch prepends `"./"` without consuming a
character, and the loop appends a trailing `/` after the final segment, so the
assembled string reaches `strlen(pathName) + 3` characters and needs one more
byte for the terminator. A bound computed by looking only at absolute paths
(`+2`) still overflows on relative ones.

The same constant sizes the internal buffer, the minimum the caller must offer,
and the caller's own declaration in `c/httpfileservice.c` -- so no call site has
to redo that arithmetic.

[zss#2094]: https://github.com/zowe/zss/issues/2094
