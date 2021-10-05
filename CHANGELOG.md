# Zowe Common C Changelog

## `1.25.0`

- Bugfix: `fileCopy` incorrectly processed files tagged as binary and mixed
- Bugfix: Unixfile Copy and Rename API doesn't support distinct error status response
- Bugfix: Dataset contents API returns invalid error status for undefined length dataset update request
- Bugfix: Unixfile Copy and Rename directory API doesn't support forceOverwrite query
- Bugfix: `safAuth` doesn't validate NULL entity

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
