# configmgr CLI / JS-API regression tests

Portable POSIX-shell tests for the `configmgr` binary built by
`build/build_cmgr_clang.sh` (any of its modes that produces an executable
-- presently `zos` on z/OS USS and `linux` on Linux/WSL). The tests
exercise the binary's **public surfaces** (CLI commands and the embedded
JS Configuration native module) and capture explicit assertions about
return codes, output content, and contract details that have not been
asserted elsewhere in the tree.

## How to run

From this directory, after building configmgr:

```sh
sh run_all.sh
```

Override the binary path:

```sh
CONFIGMGR=/path/to/some/configmgr sh run_all.sh
```

Individual suites can be run on their own; they share no state.

```sh
sh test_smoke.sh
sh test_template_eval.sh
sh test_template_order.sh
sh test_schema_validation.sh
sh test_overlay.sh
sh test_configmgr_api.sh
```

Exit code is 0 if every assertion in every suite passed, non-zero otherwise.

## What each suite pins

| Suite | Surface | Notable assertions |
|---|---|---|
| `test_smoke.sh` | CLI: `validate`, `extract`, `env` against minimal fixtures | binary is buildable; each command emits expected output (12 assertions; jq lives in `known_issues/` due to a z/OS-only bug) |
| `test_template_eval.sh` | `${{ ... }}` template evaluation for every JSON return type (int, neg int, bool, string, null, array, object) + stdlib calls (`Math.*`, `String.prototype.replace`, `JSON.stringify`) + nested templates | the `extract` round-trip preserves types end-to-end |
| `test_template_order.sh` | dependency ordering between templates | (a) chain in document order resolves cleanly through 4 levels; (b) **forward references silently coerce the unevaluated marker to `"[object Object]"`** -- this is currently-broken behavior pinned as such; (c) cycles leave both keys as marker objects, `extract` exits 16 |
| `test_schema_validation.sh` | `validate` command outcomes for each violation class | clean exit 0 + "No validity Exceptions" on good config; exit 99 + diagnostic mentioning the violation on missing-required / wrong-type / out-of-range |
| `test_overlay.sh` | multi-source merge semantics | scalars: leftmost source wins; objects: deep key-union; **arrays: rightmost source's elements appear FIRST in the concatenated result -- asymmetric with scalar precedence**, pinned explicitly |
| `test_configmgr_api.sh` | embedded-JS Configuration native module via `-script` | every public method exercised; lifecycle isolation between ConfigManager instances; `validate()` response shape (including the **`ok` field that's always `true` regardless of exceptions** and the **legacy `shoeSize: 11` debugging field**); template eval observable through `getConfigData` |

## Known-broken behaviors pinned (intentionally)

These are tests that **assert the current incorrect behavior** so that a
future fix to the underlying code surfaces as a test diff. When you fix
the code, update the test in the same change.

1. **`validate().ok` is always `true`** in the JS API even when
   `exceptionTree` is populated (`configmgr.c` line ~1603). Asserted in
   `test_configmgr_api.sh` 4.2 and 6.1.
2. **`shoeSize: 11`** appears in every JS `validate()` response
   (`configmgr.c` line ~1612, leftover debug field). Asserted in
   `test_configmgr_api.sh` 4.4.
3. **Forward-reference templates** silently produce
   `"[object Object]"` instead of resolving topologically. Asserted in
   `test_template_order.sh` (forward_ref / result case).
4. **`jq -c`** (compact output mode) produces empty output on Linux.
   Worked around in `test_smoke.sh` by using the default pretty mode;
   no dedicated assertion yet.
5. **`jq` subcommand on z/OS** fails to parse any expression because
   `configmgr.c:1895` hardcodes `jqt.ccsid = 1208` (UTF-8) but z/OS
   argv is IBM-1047. Pinned in `known_issues/test_jq_ccsid.sh`
   (platform-conditional: passes on both z/OS and Linux today).

## Known issues -- separate runner

Tests that pin **platform-conditional or known-broken behavior** live
under `known_issues/`. They are NOT picked up by `run_all.sh` so that
the main suite stays a clean 100% benchmark on every platform.

Run them explicitly:

```sh
sh known_issues/test_jq_ccsid.sh
```

When the underlying bug is fixed, the pin will start failing -- that
is the signal to update or delete the file.

## Fixtures

All in `fixtures/`. Each yaml is small enough to read at a glance:

- `permissive_schema.json` -- type:object, additionalProperties:true. Used
  whenever the test isn't about validation.
- `types.yaml` -- one example of every JSON type a template can produce,
  plus a few stdlib invocations.
- `chain_ok.yaml` -- 4-deep cross-reference chain in document order.
- `chain_forward_ref.yaml` -- consumer above producer; demonstrates the
  forward-ref silent failure.
- `cycle.yaml` -- mutual reference.
- `port_range_schema.json` -- integer 1..65535, required:port.
- `port_good.yaml` -- valid against port_range_schema.
- `port_too_large.yaml`, `port_wrong_type.yaml`, `port_missing.yaml` --
  three classes of violation.
- `overlay_base.yaml`, `overlay_middle.yaml`, `overlay_top.yaml` --
  three-source merge scenarios.
- `configmgr_api.js` -- driver script for `test_configmgr_api.sh`.

## Coverage gaps (not pinned by this suite)

- PARMLIB / PARMLIBS path elements (z/OS-only; outside Linux smoke).
- Schema breadth: `allOf` / `anyOf` / `oneOf` / `const` / `patternProperties`
  / `$ref` across multiple files. `tests/schemadata/zowebundle.json`
  exercises some of this via `test_configmgr` makefile target but the
  result isn't asserted.
- Large-input scaling (`bigschema.json` is 78KB; not in the suite yet).
- Native-call exceptions across the FFI boundary (a native callback that
  throws -- does QuickJS catch it cleanly?).
- The `linux` mode of `build_cmgr_clang.sh` itself producing the binary
  is not asserted by these tests; they assume the binary exists. A
  separate build-driver test is appropriate.

## Build dependency for Linux mode

`deps/configmgr/quickjs/quickjs.c` line 67 has an unconditional
`typedef int ssize_t` that collides with glibc's `__ssize_t` (long).
Linux build currently requires a patch to the dep tree to gate the
typedef on Linux/Darwin/FreeBSD. The fix is local-only until pushed
upstream to the quickjs-portable fork. See the build script for context.
