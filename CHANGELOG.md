# Zowe Common C Changelog

## `2.18.0`
- As a part of curve customization support, supported curves and their mapping to iana numbers are defined in tls.h. They are set using 'gsk_attribute_set_buffer' in tls.c. Currently, the list of supported curves can be seen here https://www.ibm.com/docs/en/zos/3.1.0?topic=programming-cipher-suite-definitions#csdcwh__tttcsd (#466).

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
