# Zowe Common C Changelog

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
