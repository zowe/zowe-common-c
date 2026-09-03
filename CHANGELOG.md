# Zowe Common C Changelog

## `3.6.0`
- Chore: the configmgr-rexx build (`ZWERXCFG`) now uses the same QuickJS as configmgr, `1000turquoisepogs/quickjs-portable` at `feature/update-2024-01-13`; it had stayed on `joenemo/quickjs-portable` `main` (engine 2021-03-27) when configmgr moved in March 2026, so the two shipped different engines and diverged at compile time.
- Bugfix: Embedded JS is expected to run in key 8 and problem state. [(#674)](https://github.com/zowe/zowe-common-c/pull/674)
- Bugfix: `icsfDigestInit` in `icsf.c` contained a typo in the SHA-256 ICSF rule array keyword (`"SHA246"` instead of `"SHA256"`), causing SHA-256 hashing to fail. [(#594)](https://github.com/zowe/zowe-common-c/issues/594)
- Bugfix: `ICSFDigest.hash` buffer in `icsf.h` was 32 bytes, too small for SHA-384 (48 bytes) and SHA-512 (64 bytes). Increased to 64 bytes to prevent buffer overflow. [(#594)](https://github.com/zowe/zowe-common-c/issues/594)
- Enhancement: Added SHA-3 (Keccak) algorithm support to the ICSF digest wrapper: `ICSF_DIGEST_SHA3_224`, `ICSF_DIGEST_SHA3_256`, `ICSF_DIGEST_SHA3_384`, `ICSF_DIGEST_SHA3_512` with corresponding cases in `icsfDigestInit`, `icsfDigestUpdate`, and `icsfDigestFinish`. [(#594)](https://github.com/zowe/zowe-common-c/issues/594)
- Bugfix: The logging component-ID walk in `logConfigureComponent()` and `getComponent()` now tests its loop bound before indexing, instead of reading `id[4]` two bytes past the `uint64` component ID. Unreachable on z/OS with the component IDs currently defined, but triggered on every call on a little-endian host. [(#667)](https://github.com/zowe/zowe-common-c/pull/667)
- Bugfix: `fileGetINode()` and `fileGetDeviceID()` are now implemented on POSIX hosts, and `fileGetDeviceID()` on Windows. Both are declared in `h/unixfile.h` for every platform and `fileGetINode()` is called from `httpserver.c`'s ETag computation, but only the z/OS implementations existed, so any build off z/OS that reached a file's identity failed to link. The z/OS implementations are unchanged. [(#666)](https://github.com/zowe/zowe-common-c/pull/666)
- Bugfix: `directoryMakeDirectoryRecursive()` now publishes the required size of its path-out buffer as `USS_MKDIR_PATH_BUFFER_SIZE` and rejects a `messageLength` below it, returning -1 before anything is created. Previously an undersized buffer was filled with a truncated path, naming a directory that need not exist, and was left without a null terminator when the path was at least `messageLength` long -- which the caller then read as a string. [(zss#2094)](https://github.com/zowe/zss/issues/2094)
- Bugfix: `directoryMakeDirectoryRecursive()` now sizes its internal buffer to the z/OS USS path maximum (`USS_MAX_PATH_LENGTH`, 1023) and only rejects paths that exceed that limit, instead of overrunning a fixed 256-byte stack buffer. This fixes a 0C4 ABEND (crash) when creating a directory with a path longer than 255 characters, while still accepting valid paths up to the z/OS maximum. [(zss#2094)](https://github.com/zowe/zss/issues/2094)
- Enhancement: `utils.c` gains bounded string helpers `strcpySafe`, `strncpySafe`, `strcatSafe`, `strncatSafe` and `strlenSafe`. They follow the argument shape of the C11 Annex K `_s` functions, so each buffer is followed immediately by its own size, treat that size as the size of the buffer, always null-terminate, and return -1 when the destination was too small to hold the whole result. They are deliberately not named `_s`: Annex K is optional, z/OS does not provide it, those identifiers are reserved for it, and these do not implement its semantics (constraint handlers, `errno_t`, `rsize_t`).
- Bugfix: A YAML configuration with a duplicate top-level key (e.g. two `zowe:` blocks) is now reduced to a single value (first occurrence wins) with a warning, instead of producing a JSON object with duplicate properties. This makes `configmgr` and the launcher evaluate the same configuration identically; previously they resolved duplicate keys differently (first-match vs last-match), which could silently change values or abort template evaluation. [(#581)](https://github.com/zowe/zowe-common-c/issues/581)
- Bugfix: configmgr now runs the JavaScript asynchronous continuation loop (`js_std_loop`) after evaluating a script, so `os.signal` handlers, `os.sleepAsync` timers, and Promise jobs scheduled during eval are dispatched instead of silently dropped. This lets JS-based components (e.g. a launcher) catch `SIGTERM` for graceful shutdown; a purely synchronous script is unaffected (the loop returns immediately when nothing is pending). [(#631)](https://github.com/zowe/zowe-common-c/pull/631)
- Bugfix: Internal Short Lived Heap allocation routine `SLHAlloc` checks the size [(#620)](https://github.com/zowe/zowe-common-c/issues/620)
- Bugfix: Return code 414 (`HTTP_STATUS_URI_TOO_LONG`) for too long URI [(#597)](https://github.com/zowe/zowe-common-c/issues/597)
- Bugfix: Various XML updates. [(#598)](https://github.com/zowe/zowe-common-c/pull/598)
- Enhancement: made zowe-common-c compatible with clang/llvm on z/OS and Linux. [(#596)](https://github.com/zowe/zowe-common-c/pull/596)
- Enhancement: move debug message to proper trace level [(#553)](https://github.com/zowe/zowe-common-c/pull/553)
- Bugfix: improved check of schema and configuration path for `configmgr` commands [(#553)](https://github.com/zowe/zowe-common-c/pull/553)
- Enhancement: take into account active PC callers during termination [(#569)](https://github.com/zowe/zowe-common-c/pull/569)
- Bugfix: Use "%.*s" version of snprintf to stop overreading in 'zosResolveSymbol()' which causes abend. [(#626)](https://github.com/zowe/zowe-common-c/pull/626)
- Bugfix: fix recovery in 64-bit httpserver [(#622)](https://github.com/zowe/zowe-common-c/issues/622)
- Bugfix: USS file content served via `respondWithUnixFile2()` now selects the HTTP target encoding from the resolved source (the caller's explicit `sourceEncoding` when given, else the file's CCSID) rather than a compile-time OS constant. Single-byte source CCSIDs (e.g. IBM-1047) continue to produce ISO-8859-1 output; multi-byte source CCSIDs (UTF-8 / UTF-16 / EBCDIC MIX) now produce UTF-8 output, preserving characters that ISO-8859-1 cannot represent. Previously, all files on z/OS were converted to ISO-8859-1 regardless of their tagged encoding, corrupting non-Latin-1 content. [(#592)](https://github.com/zowe/zowe-common-c/issues/592)
- Enhancement: Added `isMultiByteCCSID(int ccsid)` to `charsets.h`/`charsets.c`. Returns `TRUE` for UTF-8 (1208), UTF-16 (1200/1201/1202), and EBCDIC MIX code pages (930, 933, 935, 937, 939, 1364, 1388, 1390, 1399); `FALSE` for single-byte encodings and unknown values. Available on z/OS, Linux, and AIX. [(#592)](https://github.com/zowe/zowe-common-c/issues/592)
- Enhancement: Added `parseEncodingValue(const char *value)` to `charsets.h`/`charsets.c`. Accepts a charset name string (e.g. `"IBM-1047"`, `"UTF-8"`) or a decimal CCSID integer string (e.g. `"1047"`), and resolves the sentinel string `"binary"` (case-insensitive) to `CCSID_BINARY` (65535). Returns -1 for unrecognised values. Available on z/OS, Linux, and AIX. [(#593)](https://github.com/zowe/zowe-common-c/issues/593)
- Enhancement: `getCharsetCode()` in `charsets.c` now recognises all IBM codepage names from the IANA character sets registry (https://www.iana.org/assignments/character-sets/character-sets.xhtml) using the convention `"IBM-NNNN"` (dash-separated, no leading zeros). Covers IBM-37 through IBM-1149, including EBCDIC, PC, and extended-euro variants. Also accepts `"IBM1047"` as a legacy alias for IBM-1047, and `"IBM-819"` as an alias for ISO-8859-1. `"auto"` is now a recognised value in `sourceEncoding`/`targetEncoding` query parameters, equivalent to omitting the parameter. [(#593)](https://github.com/zowe/zowe-common-c/issues/593)
- Enhancement: `respondWithUnixFile2()` in `httpserver.c` now accepts independent `sourceEncoding` and `targetEncoding` query parameters (with `source`/`target` as legacy aliases). Either parameter may be omitted or set to `"auto"` to use the file's tagged CCSID (or `NATIVE_CODEPAGE` for untagged files) for the source, and the auto-selected web encoding for the target. If either is `"binary"`, raw bytes are streamed without conversion. Previously both parameters were required together, and `"auto"` was not a valid value. [(#593)](https://github.com/zowe/zowe-common-c/issues/593)
- Bugfix: `respondWithUnixFile2()` no longer serves an empty or truncated body when a multibyte character straddles a read buffer or is unmappable in the target. New `convertCharsetStreaming()` carries the straddling bytes to the next read and substitutes unmappable characters instead of aborting; `getCharsetName()` uses `"ISO8859-1"` (z/OS `iconv_open` rejects the dashed spelling, errno 121). [(zss#828)](https://github.com/zowe/zss/issues/828)
- Bugfix: `/unixfile` requests that force a `sourceEncoding`/`targetEncoding` pair the server cannot convert are rejected with 400 before the response starts, instead of returning 200 with an empty body; a default GET of a file whose CCSID tag cannot be converted falls back to raw binary streaming with a warning; and the streaming loop drains conversions that outgrow the translation buffer instead of truncating. [(#630)](https://github.com/zowe/zowe-common-c/pull/630)
- Bugfix: charset conversion under ibm-clang64 (Open XL) now uses the iconv path. ibm-clang64 is `__ZOWE_COMP_CLANG`, not `__ZOWE_COMP_XLCLANG`, so `charsets.c` was routing it to the metal/CUNLCNV branch and mishandling multibyte and streaming conversion. [(zss#828)](https://github.com/zowe/zss/issues/828)
- Enhancement: add a new function (`cmsTestAuth2`) to test any SAF level in xmem; fix ALTER SAF enum value [(#635)](https://github.com/zowe/zowe-common-c/issues/635)


## `3.5.0`
- Enhancement: YAML comment preservation tooling for the YAML-to-JSON-to-YAML round-trip pipeline. Comments are scanned separately from libyaml, attached to the JSON tree, and re-emitted with configurable alignment (none, fixed, original). Opt-in; not yet enabled in configmgr. [(#583)](https://github.com/zowe/zowe-common-c/issues/583)
- Enhancement: `TlsSettings` now supports a `clientLabel` field. When set, `tlsSocketInit` uses this label for outbound (client) TLS connections instead of `label`, allowing a separate certificate with a client-only EKU to be used. When `clientLabel` is NULL, `label` continues to be used for both server and client connections as before. [(#590)](https://github.com/zowe/zowe-common-c/pull/590)
- Bugfix: Set IO error flag in `jsonConvertAndWriteBuffer()` when character conversion or write operations fail, allowing callers to detect and stop processing early. (#590)
- Bugfix: Schema validation error is not properly formatted for enumerate type. [(#562)](https://github.com/zowe/zowe-common-c/pull/562)
- Bugfix: fix a typo in the cross-memory server's help text (#565)
- Enhancement: Move to later version of quickjs (#573)
- Enhancement: configmgr validation errors now use dot-formatted paths and can detect if a property that's unknown is likely to be at the wrong level of indentation [(#577)](https://github.com/zowe/zowe-common-c/pull/577)
- Enhancement: file API now returns the boolean "symlink" to state if a file is a symbolic link or not. [(#579)](https://github.com/zowe/zowe-common-c/pull/579)
- Enhancement: file API now includes the target path of a symlink in the field "symlinkTarget". [(#580)](https://github.com/zowe/zowe-common-c/pull/580)
- Enhancement: file API's "directory" value for symlinks now corresponds to whether the target is a directory or not. [(#580)](https://github.com/zowe/zowe-common-c/pull/580)

## `3.4.0`
- Enhancement: The tcpServer, tcpClient, httpServer and httpClient family of functions now detect and allow use of IPv6 addresses [(#554)](https://github.com/zowe/zowe-common-c/pull/554) [(#539)](https://github.com/zowe/zowe-common-c/pull/539)

## `3.3.0`
- Enhancement: add a `copyConfigurationAndDeleteKey` API to configmgr (#524)
- Enhancement: configmgr path elements of type PARMLIB can now contain a member name, in the format of "PARMLIB(DATA.SET(member))", as an alternative to the older format of "PARMLIB(DATA.SET)" with the member name being specified in a separate argument. This enhancement allows use of multiple members with different names within the same PARMLIB dataset. The older format can still be used as desired. (#522)
- Bugfix: Fix a leak in the safeFree64Internal by adding free(). (#540)

## `3.2.0`
- Bugfix: allocate storage to be executed with EXECUTABLE=YES (#507)
- Bugfix: fix a leak in the rsusermap code (#467)
- Bugfix: use single-line WTOs for single-line messages (#509)
- Bugfix: make sure modreg-based modules are never deleted (#517, zowe/zss#749)
- Feature: add a modify command for displaying the module registry information
  (#519)
- Feature: add a flag for resetting the module registry at the cross-memory
  server start-up (#519)

## `3.1.0`
- Enhancement: `configmgr extract` option supports simple array (#502)
- Bugfix: removed "ByteOutputStream" debug message, which was part of the `zwe` command output (#491)
- Bugfix: HEAPPOOLS and HEAPPOOLS64 no longer need to be set to OFF for configmgr (#497)
- Enhancement: module registry (#405)
- Enhancement: Adding more arguments to httpClientSessionInit to allow passing back internal rc and
  removing the reference from changelog in `3.0.0`. (#499).
- Bugfix: make sure CEE3ERP is invoked in LE 31-bit XPLINK (#504)

## `3.0.0`
- Feature: added javascript `zos.getStatvfs(path)` function to obtain file system information (#482).
- Add support for LE 64-bit in isgenq.c (#422).
- Bugfix: IARV64 results must be checked for 0x7FFFF000 (#474)
- Bugfix: SLH should not ABEND when MEMLIMIT is reached (additional NULL check)
- Bugfix: support cross-memory server parameters longer than 128 characters 
  (zowe/zss#684)

## `2.18.0`
- Minor `components.zss.logLevels._zss.httpserver=5` debug messages enhancement (#471)

## `2.17.0`
- Fixed `xplatform.loadFileUTF8` when trying to open nonexistent file (#454)
- Bugfix: fix an incorrect check in the recovery router code which might lead to
  the state cell-pool being released prematurely (#446)
- Allocating SLH for http server with configurable value 'httpRequestHeapMaxBlocks' in yaml (#447).
- Return error when last config file is non existent or has some error (#460).

## `2.16.0`
- No yaml value converted to null (#442)
- Added `zos.getZosVersion()` and `zos.getEsm()` calls for configmgr QJS (#429)
- For correct base64 encoding scheme the buffer size is made to be divisible by 3 (#431).
- Take into account leap seconds in xmem log messages' timestamps (#432, #433)
- Using a temporary buffer pointer to avoid pointer corruption during file write (#437).

## `2.15.0`
- Remove obsolete building script build_configmgr.sh (#410). (#423)
- Add flags to avoid linkage-stack queries in the recovery facility (#404, #412)

## `2.13.0`
- Added support for using "zowe.network" and "components.zss.zowe.network" to set TLS version properties. (#411)
- Added utility for general usage returning the name of External Security Manager

## `2.11.0`

- WTO printing methods have been moved to zos.c to be more available as utilities (for ex: for the Launcher)

## `2.10.0`
- This action making a CHANGELOG note via special syntax from the GitHub PR commit message, like it could automatically update CHANGELOG.md with the message. First job checks if PR body has changelog note or not if it's not there then it asked them to add it and second job is to check if changelog note has been added in changelog.md file or not. (#396)

- Feature: The configmgr can now use the 'zos' module in YAML config templates. The 'zos' module is only added when run on ZOS. For a list of available functions, see https://github.com/zowe/zowe-install-packaging/blob/v2.x/staging/build/zwe/types/%40qjstypes/zos.d.ts (#384)
- Bugfix: configmgr parsing of yaml to json was limited to 256 characters for strings. This has been updated to 1024 to allow for up to max unix path strings. (#383)

## `2.9.0`

- Feature: configmgr's zos module now has a "resolveSymbol" function which takes a string starting with & which can be used to resolve static and dynamic zos symbols

## `2.8.0`

- Bugfix: `fileCopy` would not work when convert encoding was not requested. The destination file would be created, but without the requested content.
- Feature: `fileCopy` now copies with the target having the permissions of the source, as opposed to the previous 700 permissions.
- Bugfix: respondWithUnixFileMetadata would not return UID or GID of a file if the id-to-name mapping failed, which is possible when an account is removed.

## `2.5.0`

- Added embeddedjs command "xplatform.appendFileUTF8" for appending to files rather than writing whole files.
- Bugfix that the configmgr binary would always return rc=0. Now, it has various return codes for the various internal errors or config invalid responses.

## `2.3.0`

- Bugfix for lht functions of collections.c to avoid memory issues on negative keys
- Added a new build target, 'configmgr-rexx', which builds a version of configmgr that can be used within rexx scripts.
- Bugfix the help message on configmgr

## `2.2.0`

- Added a script 'dependencies.sh' which assists in managing external dependencies needed for project compilation
- Added a new build target, 'configmgr', which builds a tool that can be called to either load, validate, and print the zowe configuration, or load, validate, and run a JS script that is given the configuration.
- Added an automated build for configmgr which is consumed by the zowe packaging

## `1.27.0`

- Enhancement: Allow to specify 31-bit and 64-bit version of dataService library using `libraryName64` and `libraryName31` keys in DataService definition.

## `1.25.0`

- Bugfix: `fileCopy` incorrectly processed files tagged as binary and mixed

## `1.23.0`

- Bugfix: HTTP server did not send empty files correctly.

## `1.22.0`

- Enhancement: Add "remoteStorage" pointer to dataservice struct, for accessing high availability remote storage in addition to or alternatively to local storage.
- Bugfix: Dataservice loading did not warn if program control was missing, which is essential, so plugin loading would fail silently in that case.

## `1.21.0`

- Set cookie path to root in order to avoid multiple cookies when browser tries to set path automatically

## `1.16.0`

- Fixed mimetype lookup for dotfiles

## `1.13.0`

- Initialized http server log earlier, a bugfix to show error messages that were hidden before.
