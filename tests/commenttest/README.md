# YAML Comment Preservation  Test Suite & Tools

## Overview

This directory contains a test harness for the **YAML comment preservation**
feature added to `zowe-common-c`.  The goal is to round-trip a YAML
configuration file through the `libyaml  JSON  YAML` pipeline used by
Zowe's `configmgr` while **retaining comments** that would normally be
discarded by libyaml's parser.

### Why is this hard?

[libyaml](https://github.com/yaml/libyaml) does not expose comments in its
document object model.  Comments are treated as whitespace and thrown away
during parsing.  Our approach works *around* this limitation:

1. **Pre-scan**  Before libyaml touches the file, `scanYamlComments()`
   performs a line-by-line scan of the raw YAML text.  It classifies every
   comment as one of three types:
   - **Document comment**  lines before any YAML key (the file header)
   - **Before comment**  a full-line comment preceding a key or array item
   - **Inline comment**  a `# ...` fragment after a value on the same line

2. **Attach**  After libyaml builds its node tree and `yaml2JSON()` converts
   it to a `Json*` tree, `attachComments()` matches each scanned comment to
   the corresponding JSON property or array element by key name and document
   order.

3. **Emit**  When the JSON tree is serialized back to YAML
   (`json2Yaml2BufferWithComments`), the comment metadata is written at the
   correct positions.  Inline comments support three **alignment modes**
   controlled at write time.

### What we do NOT change

We make **zero modifications** to libyaml itself.  All comment handling lives
in `yaml2json.c` (scan + attach) and `json.c` / `json.h` (storage + emit).
This keeps the third-party dependency pristine and avoids fork maintenance.

---

## Building

```sh
cd tests/commenttest
./build.sh
```

`build.sh` is a self-contained two-phase build:

| Phase | What it does |
|-------|------|
| 0 | `iconv` converts libyaml sources from ISO 8859-1  IBM-1047 into a temp dir |
| 1 | Compiles the converted libyaml with `xlclang -qascii` |
| 2 | Compiles zowe-common-c sources (native EBCDIC) and links with libyaml `.o` files |

> **Note:** There is no Makefile.  `build.sh` is the sole build method.

---

## Test Program  `commenttest`

```
commenttest <yamlfile|jsonfile> [options]
```

### Options

| Flag | Description |
|------|-------------|
| `-v` | Verbose  show internal comment scan details |
| `-j` | Print the JSON tree with `[BEFORE]`, `[INLINE]`, `[DOC COMMENT]` annotations |
| `-p` | Print raw JSON via `jsonPrintObject` (pretty-printed, with comments) |
| `-y` | Print YAML output with comments |
| `-o <file>` | Write YAML output to a file instead of stdout |
| `-r` | Round-trip test  write YAML, re-read, compare comment counts |
| `-a` | All: enable `-v -j -y -p` |
| `-A <mode>` | Comment alignment: `none`, `fixed`, `original` |
| `-W <width>` | Column width for `fixed` alignment (default: 40) |

---

## Comment Alignment Modes

Inline comments (e.g. `port: 8080  # standard HTTP`) can be aligned in three
ways when writing output:

### `none` (default)
The comment is placed two spaces after the value, exactly as libyaml would
end the line.

```sh
./commenttest test.yaml -y -A none
```
```yaml
host: localhost # default to loopback
port: 8080 # standard HTTP port
```

### `fixed`
Every inline comment is padded to at least column *W* (set with `-W`,
default 40).  This produces clean, tabular output.

```sh
./commenttest test.yaml -y -A fixed -W 40
```
```yaml
host: localhost                  # default to loopback
port: 8080                       # standard HTTP port
```

### `original`
The comment is placed at the same column where it appeared in the **original
source file**.  This is the closest to a lossless round-trip for hand-edited
files.

```sh
./commenttest test.yaml -y -A original
```
```yaml
host: localhost     # default to loopback
port: 8080            # standard HTTP port
```

---

## Example Files

| File | Purpose |
|------|---------|
| `test.yaml` | Comprehensive demo: document comments, before-comments, inline comments, quoted strings, arrays of objects |
| `inline-comments.yaml` | Focused on inline comment alignment  short keys, long keys, nested indentation |
| `nested-comments.yaml` | Deep nesting (4 levels) with comments at every depth |
| `array-comments.yaml`  | Array-focused: scalar arrays, object arrays, nested arrays |
| `loses-blank-lines.yaml` | Demonstrates cosmetic blank-line loss during round-trip |
| `loses-bogus-quotes.yaml` | Demonstrates quoting changes: safe, protected, and gray-area cases |

### Quick test commands

```sh
# All modes, comprehensive file
./commenttest test.yaml -y -A none
./commenttest test.yaml -y -A fixed -W 40
./commenttest test.yaml -y -A original

# JSON output (useful for inspecting the intermediate representation)
./commenttest test.yaml -p -A fixed -W 40

# Round-trip self-test
./commenttest test.yaml -r

# Focused alignment comparison
./commenttest inline-comments.yaml -y -A none
./commenttest inline-comments.yaml -y -A fixed -W 35
./commenttest inline-comments.yaml -y -A original

# Deep nesting
./commenttest nested-comments.yaml -y -A original

# Arrays
./commenttest array-comments.yaml -y -A fixed -W 40

# Full diagnostic dump
./commenttest test.yaml -a -A original
```

---

## configmgr Backward Compatibility

The `configmgr` binary (`build/build_cmgr_xlclang.sh`) compiles the same
`json.c` and `yaml2json.c` that we modified.  To verify our changes don't
break it:

```sh
cd tests/commenttest
./test_configmgr_compat.sh
```

This script compiles `json.c`, `yaml2json.c`, and a few other Phase 2 files
with the **exact same `xlclang` flags** used by the configmgr build.  It uses
`-c` (compile-only) since the full link requires GSK SSL libraries and
pre-converted third-party dependencies that may not be available on every
system.

**Key point:** Comment preservation is fully **opt-in**.  The existing
`configmgr` code calls `yaml2JSON()` (no comments) and standard
`jsonPrinter` methods (no comment output).  Our new code paths 
`yaml2JSONWithComments()`, `jsonEnableCommentPrint()`,
`jsonSetCommentAlignment()`  are only activated when a caller explicitly
requests them.  A later commit by other project members will enable these
capabilities in `configmgr`.

---

## Known Limitations

1. **Blank lines are lost.** libyaml does not preserve blank lines between
   sections.  The round-tripped YAML is semantically identical but visually
   more compact.  See `loses-blank-lines.yaml`.

2. **Quoting changes.** libyaml re-quotes string values according to its own
   rules.  Values that were unnecessarily quoted in the original (e.g.
   `"true"` for a string) may lose their quotes and change type semantics.
   See `loses-bogus-quotes.yaml`.

3. **Comment-to-key matching is order-based.**  If two sibling keys have the
   same name (unusual but legal in YAML), comments may attach to the wrong
   one.

4. **Flow-style sequences/mappings.**  Comments inside flow-style (`[a, b]`
   or `{a: 1}`) constructs are not preserved.

---

## Related Issues

- [zowe/zowe-common-c#582](https://github.com/zowe/zowe-common-c/issues/582)  Preserve comments in YAML when parsing in configmgr (moved from zowe/zss#629)
- [zowe/zowe-install-packaging#3183](https://github.com/zowe/zowe-install-packaging/issues/3183)  Comments in zowe.yaml deleted after running zwe components command

---

## Files Changed in This PR

| File | Change |
|------|--------|
| `c/yaml2json.c` | `scanYamlComments()`, `attachComments()`, `yaml2JSONWithComments()`, `getInlineCommentColumnForLine()`, inline comment column storage for ORIGINAL mode |
| `h/json.h` | `inlineCommentColumn` field on properties/elements, `currentColumn` tracking in `jsonPrinter`, alignment mode enum and setter |
| `c/json.c` | Column tracking in `jsonWriteBufferInternal`, FIXED/ORIGINAL padding in `jsonNewLine`, column capture in `jsonPrintObject`/`jsonPrintArray` |
| `tests/commenttest/` | This test suite, build script, example YAML files |
