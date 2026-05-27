# Zowe Common C Changelog

## `3.6.0`
- Enhancement: made zowe-common-c compatible with clang/llvm on z/OS and Linux. (#596)

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
- Enhancement: take into account active PC callers during termination (#569)

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
- Enhancement:  Adding more arguments to httpClientSessionInit to allow passing back internal rc and
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
- For correct base64 encoding scheme the buffer size is made to be divisble by 3 (#431). 
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

- Added a script 'dependencies.sh' which assists in managing external depedencies needed for project compilation
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
