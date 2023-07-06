# Zowe Common C Changelog

## `2.10.0`

- Feature: The configmgr can now use the zos module in embedded code inside yaml files.   The configmgr runs scripts with ES6 style modules, but has a more limited environment (currently) in embedded JS.   The modules made visible are done so explicitly in embeddedjs.c, but this eventually could be made configurable more softly.   The ZOS module is only added when run on ZOS and includes things like looking up dataset info, manipulating file tags, and using the 'extattr' function to change non-POSIX file attributes.

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
