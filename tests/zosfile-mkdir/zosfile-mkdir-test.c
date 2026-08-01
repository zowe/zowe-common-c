/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * zosfile-mkdir-test.c -- directoryMakeDirectoryRecursive() path-length and
 * message-buffer behaviour.
 *
 * Runs entirely against directories that already exist, and passes over-long
 * or NULL paths that are rejected before any BPXMKD. It therefore CREATES NO
 * DIRECTORIES and writes nothing to the filesystem.
 *
 * Every case prints the arguments it passed and the values it got back, so a
 * reader can check the verdict rather than trust it.
 *
 * See README.md for the two-step before/after protocol.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "zowetypes.h"
#include "unixfile.h"
#include "utils.h"

/* Deliberately small: any caller may pass a buffer smaller than the path being
 * assembled, and the function must still hand back a usable C string. */
#define SMALL_MESSAGE_LENGTH 16
#define MESSAGE_BUFFER_SIZE  64
#define SENTINEL     'X'
#define SENTINEL_STR "X"

static int failures = 0;

static void verdict(int passed, const char *explanation) {
  if (passed) {
    printf("    VERDICT       : PASS\n");
  } else {
    printf("    VERDICT       : FAIL -- %s\n", explanation);
    failures++;
  }
  printf("\n");
}

/* Index of the first null within the first n bytes, or -1 if there is none. */
static int firstNullWithin(const char *buffer, int n) {
  for (int i = 0; i < n; i++) {
    if (buffer[i] == '\0') {
      return i;
    }
  }
  return -1;
}

/* True when every one of the first n bytes is still our fill byte, i.e. the
 * function wrote nothing at all. */
static int stillAllSentinel(const char *buffer, int n) {
  for (int i = 0; i < n; i++) {
    if (buffer[i] != SENTINEL) {
      return 0;
    }
  }
  return 1;
}

/* CASE 1 -- a path longer than z/OS supports must be rejected, not assembled
 * in the fixed-size stack buffer. Before the buffer-overflow fix this ABENDs
 * 0C4 (zss#2094) rather than returning. */
static void testOverlongPathRejected(void) {
  char longPath[USS_MAX_PATH_LENGTH + 100];
  int rc = 0;

  for (int i = 0; i < (int)sizeof(longPath) - 1; i++) {
    longPath[i] = (i % 32 == 0) ? '/' : 'a';
  }
  longPath[0] = '/';
  longPath[sizeof(longPath) - 1] = '\0';

  printf("[1] over-long path is rejected\n");
  printf("    pathName      : \"/aaaa...\" %d chars (limit is %d)\n",
         strlenSafe(longPath, sizeof(longPath)), USS_MAX_PATH_LENGTH);
  printf("    message       : NULL, messageLength 0\n");
  printf("    recursive     : 0    forceCreate: 0\n");

  rc = directoryMakeDirectoryRecursive(longPath, NULL, 0, 0, 0);

  printf("    returned      : %d (expected -1)\n", rc);
  verdict(rc == -1, "a path longer than USS_MAX_PATH_LENGTH was not rejected");
}

/* CASE 2 -- a NULL path must be rejected rather than dereferenced. On z/OS a
 * NULL dereference reads low core instead of failing, so this cannot be left
 * to chance. */
static void testNullPathRejected(void) {
  int rc = 0;

  printf("[2] NULL path is rejected\n");
  printf("    pathName      : NULL\n");
  printf("    message       : NULL, messageLength 0\n");

  rc = directoryMakeDirectoryRecursive(NULL, NULL, 0, 0, 0);

  printf("    returned      : %d (expected -1)\n", rc);
  verdict(rc == -1, "a NULL pathName was not rejected");
}

/* CASE 3 -- a buffer too small to hold the deepest path must be rejected.
 *
 * A short buffer can only be filled with a truncated path, which names a
 * directory that need not exist -- so the function refuses it up front, before
 * anything is created, and returns -1. Our buffer is much larger than the
 * length we declare, so this case can see exactly what was written (nothing)
 * without reading out of bounds itself.
 */
static void testShortBufferRejected(void) {
  char cwd[USS_MAX_PATH_LENGTH + 1];
  char message[MESSAGE_BUFFER_SIZE];
  int rc = 0;
  int untouched = 0;

  printf("[3] a messageLength below the required minimum is rejected\n");

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    printf("    pathName      : (getcwd failed)\n");
    verdict(0, "getcwd() failed, cannot run this case");
    return;
  }

  memset(message, SENTINEL, sizeof(message));

  printf("    pathName      : \"%s\" (%d chars, exists)\n",
         cwd, strlenSafe(cwd, sizeof(cwd)));
  printf("    messageLength : %d  (minimum is %d)\n",
         SMALL_MESSAGE_LENGTH, USS_MKDIR_PATH_BUFFER_SIZE);
  printf("    buffer        : %d bytes, pre-filled with '%c'\n",
         MESSAGE_BUFFER_SIZE, SENTINEL);

  rc = directoryMakeDirectoryRecursive(cwd, message, SMALL_MESSAGE_LENGTH, 0, 0);

  untouched = stillAllSentinel(message, MESSAGE_BUFFER_SIZE);

  printf("    returned      : %d (expected -1)\n", rc);
  printf("    buffer written: %s\n",
         untouched ? "no, still all '" SENTINEL_STR "'"
                   : "YES -- a partial path was written");
  verdict(rc == -1 && untouched,
          "an undersized buffer was accepted; a truncated path would name a "
          "directory that need not exist");
}

/* CASE 4 -- with a correctly sized buffer the reported path is complete and
 * real: it must equal the path we asked for, which exists. */
static void testCorrectBufferReportsRealPath(void) {
  char cwd[USS_MAX_PATH_LENGTH + 1];
  char message[USS_MKDIR_PATH_BUFFER_SIZE];
  FileInfo info = {0};
  int returnCode = 0;
  int reasonCode = 0;
  int rc = 0;
  int exists = 0;
  int matches = 0;

  printf("[4] a correctly sized buffer receives the complete, real path\n");

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    printf("    pathName      : (getcwd failed)\n");
    verdict(0, "getcwd() failed, cannot run this case");
    return;
  }

  memset(message, SENTINEL, sizeof(message));

  printf("    pathName      : \"%s\" (%d chars, exists)\n",
         cwd, strlenSafe(cwd, sizeof(cwd)));
  printf("    messageLength : %d  (the published minimum)\n",
         USS_MKDIR_PATH_BUFFER_SIZE);
  printf("    recursive     : 0    forceCreate: 0   (nothing is created)\n");

  /* recursive=0 is safe here: every component of cwd exists, so the function
   * never reaches its directoryMake() branch. */
  rc = directoryMakeDirectoryRecursive(cwd, message, sizeof(message), 0, 0);

  if (firstNullWithin(message, sizeof(message)) < 0) {
    printf("    returned      : %d\n", rc);
    printf("    reported path : (not null-terminated)\n");
    verdict(0, "the reported path is not a valid string");
    return;
  }

  matches = (strcmp(message, cwd) == 0);
  exists = (fileInfo(message, &info, &returnCode, &reasonCode) == 0);

  printf("    returned      : %d\n", rc);
  printf("    reported path : \"%s\" (%d chars)\n", message,
         strlenSafe(message, sizeof(message)));
  printf("    equals input  : %s\n", matches ? "yes" : "NO");
  printf("    exists on disk: %s\n", exists ? "yes" : "NO");
  verdict(matches && exists,
          "the reported path is truncated or names a directory that does not "
          "exist");
}

int main(int argc, char **argv) {
  printf("\n");
  printf("directoryMakeDirectoryRecursive -- path length and path-out buffer\n");
  printf("USS_MAX_PATH_LENGTH       = %d\n", USS_MAX_PATH_LENGTH);
  printf("USS_MKDIR_PATH_BUFFER_SIZE = %d  (required minimum for the buffer)\n",
         USS_MKDIR_PATH_BUFFER_SIZE);
  printf("no directories are created and nothing is written\n");
  printf("\n");

  testOverlongPathRejected();
  testNullPathRejected();
  testShortBufferRejected();
  testCorrectBufferReportsRealPath();

  printf("%d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
