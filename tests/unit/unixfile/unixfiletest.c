
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * tests/unit/unixfile/unixfiletest.c - Unit tests for unixfile.h / zosfile.c
 *
 * Tests file and directory operations: create, read, write, copy, rename,
 * delete, stat, permission mode, and directory listing.  On z/OS, file-tag
 * (CCSID) setting is also covered.
 *
 * All files and directories created by these tests live under:
 *   /tmp/unixfiletest_<pid>/t<counter>/
 * Each IT block receives its own private working directory and never touches
 * anything outside it.  The base directory and all work directories are
 * cleaned up by the after-each hooks.
 *
 * Binary is expected to be run from the tests/ directory (matches the
 * Makefile "test_unixfile" rule) so that the CWD is tests/ -- but because
 * all paths are absolute (/tmp/...) this does not actually matter.
 *
 * Compile on z/OS (xlclang, lp64) -- see tests/Makefile target "test_unixfile".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
/* z/OS puts PATH_MAX in <sys/limits.h>; provide a safe fallback either way. */
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "unixfile.h"
#include "zowetests.h"

/* ===================================================================
 *  Shared state
 * =================================================================== */

static char s_baseDir[PATH_MAX];  /* /tmp/unixfiletest_<pid> */
static char s_workDir[PATH_MAX];  /* s_baseDir/t<counter> for the current IT  */
static int  s_workCounter = 0;
static int  s_baseCreated  = 0;

/* ===================================================================
 *  Helpers
 * =================================================================== */

/*
 * Create the base /tmp directory once per process.
 * Idempotent -- safe to call from every beforeEach.
 */
static void ensureBaseDir(void) {
  if (s_baseCreated) return;
  snprintf(s_baseDir, sizeof(s_baseDir),
           "/tmp/unixfiletest_%d", (int)getpid());
  int rc = 0, rsn = 0;
  directoryMake(s_baseDir, 0755, &rc, &rsn);
  s_baseCreated = 1;
}

/*
 * Allocate a fresh working directory for the current IT block. Stored in
 * s_workDir so tests can use it directly.
 */
static void beforeEachWork(void) {
  ensureBaseDir();
  s_workCounter++;
  snprintf(s_workDir, sizeof(s_workDir),
           "%s/t%d", s_baseDir, s_workCounter);
  int rc = 0, rsn = 0;
  directoryMake(s_workDir, 0755, &rc, &rsn);
}

/*
 * Recursively remove the current IT block's working directory.
 */
static void afterEachWork(void) {
  if (s_workDir[0] != '\0') {
    int rc = 0, rsn = 0;
    directoryDeleteRecursive(s_workDir, &rc, &rsn);
  }
}

/* Build an absolute path under the current working directory. */
static void makePath(char *out, size_t outSize, const char *name) {
  snprintf(out, outSize, "%s/%s", s_workDir, name);
}

/*
 * Create a plain file, write content, close.
 * Returns true on success -- useful as a shared setup step inside IT blocks.
 */
static bool createFile(const char *path, const char *data, int dataLen) {
  int rc = 0, rsn = 0;
  UnixFile *f = fileOpen(path,
                         FILE_OPTION_CREATE | FILE_OPTION_WRITE_ONLY
                                            | FILE_OPTION_TRUNCATE,
                         0600, 0, &rc, &rsn);
  if (f == NULL) return false;
  int written = fileWrite(f, data, dataLen, &rc, &rsn);
  fileClose(f, &rc, &rsn);
  return (written == dataLen);
}

/* ===================================================================
 *  Suite 1 - umask
 * =================================================================== */
static void testUmask(void) {
  DESCRIBE("setUmask / getUmask") {

    IT("getUmask returns a non-negative value") {
      TEST_COVERS(getUmask);
      int mask = getUmask();
      ASSERT_GT_INT(mask, -1);
    } IT_END

    IT("setUmask changes the umask and returns the previous value") {
      TEST_COVERS(setUmask);
      TEST_COVERS(getUmask);
      int original = getUmask();
      int prev = setUmask(0022);
      ASSERT_EQUAL_INT(original, prev);
      ASSERT_EQUAL_INT(0022, getUmask());
      /* Restore */
      setUmask(original);
      ASSERT_EQUAL_INT(original, getUmask());
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 2 - File create / read / write
 * =================================================================== */
static void testFileCreateReadWrite(void) {
  DESCRIBE("fileOpen / fileWrite / fileRead / fileClose") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("creates a new file successfully") {
      TEST_COVERS(fileOpen);
      TEST_COVERS(fileClose);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "create.txt");
      int rc = 0, rsn = 0;
      UnixFile *f = fileOpen(path,
                             FILE_OPTION_CREATE | FILE_OPTION_WRITE_ONLY
                                                | FILE_OPTION_TRUNCATE,
                             0600, 0, &rc, &rsn);
      ASSERT_NOT_NULL(f);
      int status = fileClose(f, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
    } IT_END

    IT("writes data and reads it back correctly") {
      TEST_COVERS(fileWrite);
      TEST_COVERS(fileRead);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "rw.txt");
      const char *msg = "Hello, zowe!";
      int msgLen = (int)strlen(msg);

      ASSERT_TRUE(createFile(path, msg, msgLen));

      int rc = 0, rsn = 0;
      UnixFile *f = fileOpen(path, FILE_OPTION_READ_ONLY, 0, 0, &rc, &rsn);
      ASSERT_NOT_NULL(f);
      char buf[64] = {0};
      int bytesRead = fileRead(f, buf, sizeof(buf) - 1, &rc, &rsn);
      fileClose(f, &rc, &rsn);

      ASSERT_EQUAL_INT(msgLen, bytesRead);
      ASSERT_EQUAL_STR(msg, buf);
    } IT_END

    IT("reading an empty file returns 0 bytes") {
      TEST_COVERS(fileRead);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "empty.txt");
      ASSERT_TRUE(createFile(path, "", 0));

      int rc = 0, rsn = 0;
      UnixFile *f = fileOpen(path, FILE_OPTION_READ_ONLY, 0, 0, &rc, &rsn);
      ASSERT_NOT_NULL(f);
      char buf[16] = {0};
      int bytesRead = fileRead(f, buf, sizeof(buf), &rc, &rsn);
      fileClose(f, &rc, &rsn);

      ASSERT_EQUAL_INT(0, bytesRead);
    } IT_END

    IT("reads exactly the number of bytes written") {
      TEST_COVERS(fileWrite);
      TEST_COVERS(fileRead);
      /* Write 100 bytes of 'A', verify exactly 100 are read back */
      char path[PATH_MAX];
      makePath(path, sizeof(path), "hundred.txt");
      char adata[100];
      memset(adata, 'A', sizeof(adata));
      ASSERT_TRUE(createFile(path, adata, (int)sizeof(adata)));

      int rc = 0, rsn = 0;
      UnixFile *f = fileOpen(path, FILE_OPTION_READ_ONLY, 0, 0, &rc, &rsn);
      ASSERT_NOT_NULL(f);
      char readbuf[200] = {0};
      int bytesRead = fileRead(f, readbuf, sizeof(readbuf), &rc, &rsn);
      fileClose(f, &rc, &rsn);

      ASSERT_EQUAL_INT(100, bytesRead);
      /* Verify content */
      for (int i = 0; i < 100; i++) {
        if (readbuf[i] != 'A') {
          FAIL("byte mismatch in read-back of 'A' data");
        }
      }
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 3 - File append
 * =================================================================== */
static void testFileAppend(void) {
  DESCRIBE("fileOpen with FILE_OPTION_APPEND") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("appended data follows the original content") {
      TEST_COVERS(fileWrite);
      TEST_COVERS(fileRead);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "append.txt");

      /* Write first part */
      ASSERT_TRUE(createFile(path, "FIRST", 5));

      /* Append second part */
      int rc = 0, rsn = 0;
      UnixFile *fa = fileOpen(path,
                              FILE_OPTION_WRITE_ONLY | FILE_OPTION_APPEND,
                              0600, 0, &rc, &rsn);
      ASSERT_NOT_NULL(fa);
      int w = fileWrite(fa, "SECOND", 6, &rc, &rsn);
      ASSERT_EQUAL_INT(6, w);
      fileClose(fa, &rc, &rsn);

      /* Read back: should be "FIRSTSECOND" */
      UnixFile *fr = fileOpen(path, FILE_OPTION_READ_ONLY, 0, 0, &rc, &rsn);
      ASSERT_NOT_NULL(fr);
      char buf[32] = {0};
      int bytesRead = fileRead(fr, buf, sizeof(buf) - 1, &rc, &rsn);
      fileClose(fr, &rc, &rsn);

      ASSERT_EQUAL_INT(11, bytesRead);
      ASSERT_EQUAL_STR("FIRSTSECOND", buf);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 4 - fileGetChar
 * =================================================================== */
static void testFileGetChar(void) {
  DESCRIBE("fileGetChar - buffered single-character reads") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("reads characters one-by-one in the correct order") {
      TEST_COVERS(fileGetChar);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "chars.txt");
      const char *text = "ABC";
      ASSERT_TRUE(createFile(path, text, 3));

      int rc = 0, rsn = 0;
      /* bufferSize must be > 0 for fileGetChar to work */
      UnixFile *f = fileOpen(path, FILE_OPTION_READ_ONLY, 0, 4096, &rc, &rsn);
      ASSERT_NOT_NULL(f);

      int c0 = fileGetChar(f, &rc, &rsn);
      int c1 = fileGetChar(f, &rc, &rsn);
      int c2 = fileGetChar(f, &rc, &rsn);
      int c3 = fileGetChar(f, &rc, &rsn); /* EOF */
      fileClose(f, &rc, &rsn);

      ASSERT_EQUAL_INT('A', c0);
      ASSERT_EQUAL_INT('B', c1);
      ASSERT_EQUAL_INT('C', c2);
      ASSERT_EQUAL_INT(-1,  c3);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 5 - fileInfo on regular files
 * =================================================================== */
static void testFileInfo(void) {
  DESCRIBE("fileInfo on regular files") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("fileInfo returns 0 for an existing file") {
      TEST_COVERS(fileInfo);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "stat.txt");
      ASSERT_TRUE(createFile(path, "data", 4));

      FileInfo fi = {0};
      int rc = 0, rsn = 0;
      int status = fileInfo(path, &fi, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
    } IT_END

    IT("fileInfo returns -1 for a path that does not exist") {
      TEST_COVERS(fileInfo);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "nosuchfile.txt");
      FileInfo fi = {0};
      int rc = 0, rsn = 0;
      int status = fileInfo(path, &fi, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, status);
    } IT_END

    IT("fileInfoSize matches the number of bytes written") {
      TEST_COVERS(fileInfoSize);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "size.txt");
      const char *content = "0123456789"; /* 10 bytes */
      ASSERT_TRUE(createFile(path, content, 10));

      FileInfo fi = {0};
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, fileInfo(path, &fi, &rc, &rsn));
      ASSERT_EQUAL_INT(10, (int)fileInfoSize(&fi));
    } IT_END

    IT("fileInfoIsDirectory returns false for a regular file") {
      TEST_COVERS(fileInfoIsDirectory);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "notdir.txt");
      ASSERT_TRUE(createFile(path, "x", 1));

      FileInfo fi = {0};
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, fileInfo(path, &fi, &rc, &rsn));
      ASSERT_FALSE(fileInfoIsDirectory(&fi));
    } IT_END

    IT("fileInfoUnixModificationTime is positive after creation") {
      TEST_COVERS(fileInfoUnixModificationTime);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "mtime.txt");
      ASSERT_TRUE(createFile(path, "hello", 5));

      FileInfo fi = {0};
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, fileInfo(path, &fi, &rc, &rsn));
      ASSERT_GT_INT(fileInfoUnixModificationTime(&fi), 0);
    } IT_END

    IT("fileInfoOwnerUID and fileInfoOwnerGID are non-negative") {
      TEST_COVERS(fileInfoOwnerUID);
      TEST_COVERS(fileInfoOwnerGID);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "owner.txt");
      ASSERT_TRUE(createFile(path, "y", 1));

      FileInfo fi = {0};
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, fileInfo(path, &fi, &rc, &rsn));
      ASSERT_GT_INT(fileInfoOwnerUID(&fi), -1);
      ASSERT_GT_INT(fileInfoOwnerGID(&fi), -1);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 6 - fileInfo on directories
 * =================================================================== */
static void testFileInfoDirectory(void) {
  DESCRIBE("fileInfoIsDirectory - directories vs files") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("fileInfoIsDirectory returns true for a directory") {
      TEST_COVERS(fileInfoIsDirectory);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "adir");
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, directoryMake(path, 0755, &rc, &rsn));

      FileInfo fi = {0};
      ASSERT_EQUAL_INT(0, fileInfo(path, &fi, &rc, &rsn));
      ASSERT_TRUE(fileInfoIsDirectory(&fi));
    } IT_END

    IT("the working directory itself reports as a directory") {
      TEST_COVERS(fileInfoIsDirectory);
      FileInfo fi = {0};
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, fileInfo(s_workDir, &fi, &rc, &rsn));
      ASSERT_TRUE(fileInfoIsDirectory(&fi));
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 7 - File mode bits
 * =================================================================== */
static void testFileMode(void) {
  DESCRIBE("fileUnixMode - permission mode bits") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("a file created with mode 0600 reports user read+write bits") {
      TEST_COVERS(fileUnixMode);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "mode600.txt");
      int rc = 0, rsn = 0;
      UnixFile *f = fileOpen(path,
                             FILE_OPTION_CREATE | FILE_OPTION_WRITE_ONLY
                                                | FILE_OPTION_TRUNCATE,
                             0600, 0, &rc, &rsn);
      ASSERT_NOT_NULL(f);
      fileClose(f, &rc, &rsn);

      FileInfo fi = {0};
      ASSERT_EQUAL_INT(0, fileInfo(path, &fi, &rc, &rsn));
      int mode = fileUnixMode(&fi);
      /*
       * The umask may mask some bits (e.g. 022 removes group/other write).
       * We only assert that the user read+write bits (0600) survive the
       * umask, and that the mode is non-zero.
       */
      ASSERT_GT_INT(mode, 0);
      ASSERT_TRUE((mode & 0400) != 0); /* user read  */
      ASSERT_TRUE((mode & 0200) != 0); /* user write */
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 8 - symbolicFileInfo
 * =================================================================== */
static void testSymbolicFileInfo(void) {
  DESCRIBE("symbolicFileInfo") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("symbolicFileInfo on a regular file returns 0 and same size as fileInfo") {
      TEST_COVERS(symbolicFileInfo);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "symstat.txt");
      const char *data = "symlink test";
      ASSERT_TRUE(createFile(path, data, (int)strlen(data)));

      FileInfo fi1 = {0}, fi2 = {0};
      int rc = 0, rsn = 0;
      int s1 = fileInfo(path, &fi1, &rc, &rsn);
      int s2 = symbolicFileInfo(path, &fi2, &rc, &rsn);

      ASSERT_EQUAL_INT(0, s1);
      ASSERT_EQUAL_INT(0, s2);
      ASSERT_EQUAL_INT((int)fileInfoSize(&fi1), (int)fileInfoSize(&fi2));
    } IT_END

#ifdef __ZOWE_OS_ZOS
    IT("symbolicFileInfo on a symlink does not follow the link") {
      TEST_COVERS(symbolicFileInfo);
      char target[PATH_MAX], link[PATH_MAX];
      makePath(target, sizeof(target), "symtarget.txt");
      makePath(link,   sizeof(link),   "symlink.lnk");
      ASSERT_TRUE(createFile(target, "target content", 14));

      /* Create a POSIX symbolic link using the standard C library.     */
      /* On z/OS this calls BPX1SYM under the covers.                   */
      int symlinkRc = symlink(target, link);
      ASSERT_EQUAL_INT(0, symlinkRc);

      /* fileInfo follows the link → sees a regular file */
      FileInfo fi1 = {0};
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, fileInfo(link, &fi1, &rc, &rsn));
      ASSERT_FALSE(fileInfoIsDirectory(&fi1));

      /* symbolicFileInfo does NOT follow → sees the symlink itself */
      FileInfo fi2 = {0};
      ASSERT_EQUAL_INT(0, symbolicFileInfo(link, &fi2, &rc, &rsn));
      ASSERT_EQUAL_INT(BPXSTA_FILETYPE_SYMLINK, (int)fi2.fileType);
    } IT_END
#endif /* __ZOWE_OS_ZOS */

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 9 - fileDirname
 * =================================================================== */
static void testFileDirname(void) {
  DESCRIBE("fileDirname - path stripping") {

    IT("extracts the directory from a multi-component path") {
      TEST_COVERS(fileDirname);
      char out[PATH_MAX] = {0};
      /* Note: fileDirname may modify its input (calls dirname internally). */
      char path[] = "/path/to/file.txt";
      fileDirname(path, out);
      ASSERT_EQUAL_STR("/path/to", out);
    } IT_END

    IT("returns '/' for a top-level file") {
      TEST_COVERS(fileDirname);
      char out[PATH_MAX] = {0};
      char path[] = "/singlecomponent";
      fileDirname(path, out);
      ASSERT_EQUAL_STR("/", out);
    } IT_END

    IT("returns '.' for a bare filename with no directory") {
      TEST_COVERS(fileDirname);
      char out[PATH_MAX] = {0};
      char path[] = "justname";
      fileDirname(path, out);
      ASSERT_EQUAL_STR(".", out);
    } IT_END

    IT("returns '/' for the root directory itself") {
      TEST_COVERS(fileDirname);
      char out[PATH_MAX] = {0};
      char path[] = "/";
      fileDirname(path, out);
      ASSERT_EQUAL_STR("/", out);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 10 - fileCopy
 * =================================================================== */
static void testFileCopy(void) {
  DESCRIBE("fileCopy") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("creates a copy with identical content") {
      TEST_COVERS(fileCopy);
      char src[PATH_MAX], dst[PATH_MAX];
      makePath(src, sizeof(src), "original.txt");
      makePath(dst, sizeof(dst), "copy.txt");
      const char *content = "copytest data";
      int contentLen = (int)strlen(content);
      ASSERT_TRUE(createFile(src, content, contentLen));

      int rc = 0, rsn = 0;
      int status = fileCopy(src, dst, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);

      /* Read dst and verify contents match */
      UnixFile *f = fileOpen(dst, FILE_OPTION_READ_ONLY, 0, 0, &rc, &rsn);
      ASSERT_NOT_NULL(f);
      char buf[64] = {0};
      int bytesRead = fileRead(f, buf, sizeof(buf) - 1, &rc, &rsn);
      fileClose(f, &rc, &rsn);

      ASSERT_EQUAL_INT(contentLen, bytesRead);
      ASSERT_EQUAL_STR(content, buf);
    } IT_END

    IT("the copy has the same file size as the original") {
      TEST_COVERS(fileCopy);
      char src[PATH_MAX], dst[PATH_MAX];
      makePath(src, sizeof(src), "sz_orig.txt");
      makePath(dst, sizeof(dst), "sz_copy.txt");
      ASSERT_TRUE(createFile(src, "HELLO WORLD", 11));

      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, fileCopy(src, dst, &rc, &rsn));

      FileInfo fi = {0};
      ASSERT_EQUAL_INT(0, fileInfo(dst, &fi, &rc, &rsn));
      ASSERT_EQUAL_INT(11, (int)fileInfoSize(&fi));
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 11 - fileRename
 * =================================================================== */
static void testFileRename(void) {
  DESCRIBE("fileRename") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("renamed file is accessible at the new path") {
      TEST_COVERS(fileRename);
      char oldPath[PATH_MAX], newPath[PATH_MAX];
      makePath(oldPath, sizeof(oldPath), "before.txt");
      makePath(newPath, sizeof(newPath), "after.txt");
      const char *content = "rename me";
      ASSERT_TRUE(createFile(oldPath, content, (int)strlen(content)));

      int rc = 0, rsn = 0;
      int status = fileRename(oldPath, newPath, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);

      /* New path must exist and contain the original data */
      UnixFile *f = fileOpen(newPath, FILE_OPTION_READ_ONLY, 0, 0, &rc, &rsn);
      ASSERT_NOT_NULL(f);
      char buf[32] = {0};
      fileRead(f, buf, sizeof(buf) - 1, &rc, &rsn);
      fileClose(f, &rc, &rsn);
      ASSERT_EQUAL_STR("rename me", buf);
    } IT_END

    IT("old path no longer exists after rename") {
      TEST_COVERS(fileRename);
      char oldPath[PATH_MAX], newPath[PATH_MAX];
      makePath(oldPath, sizeof(oldPath), "gone_old.txt");
      makePath(newPath, sizeof(newPath), "gone_new.txt");
      ASSERT_TRUE(createFile(oldPath, "x", 1));

      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, fileRename(oldPath, newPath, &rc, &rsn));

      /* stat old path -- must fail */
      FileInfo fi = {0};
      int s = fileInfo(oldPath, &fi, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, s);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 12 - fileDelete
 * =================================================================== */
static void testFileDelete(void) {
  DESCRIBE("fileDelete") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("deletes an existing file successfully") {
      TEST_COVERS(fileDelete);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "todelete.txt");
      ASSERT_TRUE(createFile(path, "delete me", 9));

      int rc = 0, rsn = 0;
      int status = fileDelete(path, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
    } IT_END

    IT("fileInfo returns -1 after deleting a file") {
      TEST_COVERS(fileDelete);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "gone.txt");
      ASSERT_TRUE(createFile(path, "bye", 3));

      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, fileDelete(path, &rc, &rsn));

      FileInfo fi = {0};
      int s = fileInfo(path, &fi, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, s);
    } IT_END

    IT("returns -1 when asked to delete a non-existent file") {
      TEST_COVERS(fileDelete);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "nonexistent.txt");
      int rc = 0, rsn = 0;
      int status = fileDelete(path, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, status);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 13 - directoryMake / directoryDelete
 * =================================================================== */
static void testDirectoryMakeDelete(void) {
  DESCRIBE("directoryMake / directoryDelete") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("creates a directory and verifies it with fileInfo") {
      TEST_COVERS(directoryMake);
      char dirPath[PATH_MAX];
      makePath(dirPath, sizeof(dirPath), "newdir");
      int rc = 0, rsn = 0;
      int status = directoryMake(dirPath, 0755, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);

      FileInfo fi = {0};
      ASSERT_EQUAL_INT(0, fileInfo(dirPath, &fi, &rc, &rsn));
      ASSERT_TRUE(fileInfoIsDirectory(&fi));
    } IT_END

    IT("deletes an empty directory successfully") {
      TEST_COVERS(directoryDelete);
      char dirPath[PATH_MAX];
      makePath(dirPath, sizeof(dirPath), "emptydir");
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, directoryMake(dirPath, 0755, &rc, &rsn));
      int status = directoryDelete(dirPath, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
    } IT_END

    IT("returns -1 when deleting a non-existent directory") {
      TEST_COVERS(directoryDelete);
      char dirPath[PATH_MAX];
      makePath(dirPath, sizeof(dirPath), "nosuchdir");
      int rc = 0, rsn = 0;
      int status = directoryDelete(dirPath, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, status);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 14 - directoryRename
 * =================================================================== */
static void testDirectoryRename(void) {
  DESCRIBE("directoryRename") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("renames a directory and new name is visible via fileInfo") {
      TEST_COVERS(directoryRename);
      char oldPath[PATH_MAX], newPath[PATH_MAX];
      makePath(oldPath, sizeof(oldPath), "old_dir");
      makePath(newPath, sizeof(newPath), "new_dir");
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, directoryMake(oldPath, 0755, &rc, &rsn));

      int status = directoryRename(oldPath, newPath, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);

      FileInfo fi = {0};
      ASSERT_EQUAL_INT(0, fileInfo(newPath, &fi, &rc, &rsn));
      ASSERT_TRUE(fileInfoIsDirectory(&fi));
    } IT_END

    IT("old directory name no longer exists after rename") {
      TEST_COVERS(directoryRename);
      char oldPath[PATH_MAX], newPath[PATH_MAX];
      makePath(oldPath, sizeof(oldPath), "vanish_dir");
      makePath(newPath, sizeof(newPath), "appear_dir");
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, directoryMake(oldPath, 0755, &rc, &rsn));
      ASSERT_EQUAL_INT(0, directoryRename(oldPath, newPath, &rc, &rsn));

      FileInfo fi = {0};
      ASSERT_EQUAL_INT(-1, fileInfo(oldPath, &fi, &rc, &rsn));
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 15 - directoryOpen / directoryRead / directoryClose
 * =================================================================== */
static void testDirectoryListing(void) {
  DESCRIBE("directoryOpen / directoryRead / directoryClose") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("opens and closes a directory without error") {
      TEST_COVERS(directoryOpen);
      TEST_COVERS(directoryClose);
      int rc = 0, rsn = 0;
      UnixFile *dir = directoryOpen(s_workDir, &rc, &rsn);
      ASSERT_NOT_NULL(dir);
      int status = directoryClose(dir, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
    } IT_END

    IT("directoryOpen returns NULL for a non-existent directory") {
      TEST_COVERS(directoryOpen);
      char noDir[PATH_MAX];
      makePath(noDir, sizeof(noDir), "nosuchdir");
      int rc = 0, rsn = 0;
      UnixFile *dir = directoryOpen(noDir, &rc, &rsn);
      ASSERT_NULL(dir);
    } IT_END

    IT("reads back a file created inside the directory") {
      TEST_COVERS(directoryRead);
      /* Create a sentinel file inside the work dir */
      char filePath[PATH_MAX];
      makePath(filePath, sizeof(filePath), "sentinel.txt");
      ASSERT_TRUE(createFile(filePath, "data", 4));

      int rc = 0, rsn = 0;
      UnixFile *dir = directoryOpen(s_workDir, &rc, &rsn);
      ASSERT_NOT_NULL(dir);

      /*
       * directoryRead fills entryBuffer with packed DirectoryEntry records.
       * Returns the number of entries placed in the buffer, or -1 on error,
       * or 0 at EOF.  Read in chunks until EOF.
       */
      char entryBuf[2550] = {0};
      bool found = false;
      int entries;
      while ((entries = directoryRead(dir, entryBuf, sizeof(entryBuf),
                                      &rc, &rsn)) > 0) {
        int offset = 0;
        for (int i = 0; i < entries; i++) {
          DirectoryEntry *de = (DirectoryEntry *)(entryBuf + offset);
          if (strcmp(de->name, "sentinel.txt") == 0) {
            found = true;
          }
          offset += de->entryLength;
        }
      }
      ASSERT_EQUAL_INT(0, entries); /* clean EOF */
      directoryClose(dir, &rc, &rsn);

      ASSERT_TRUE(found);
    } IT_END

    IT("reads all three files created within the directory") {
      TEST_COVERS(directoryRead);
      char a[PATH_MAX], b[PATH_MAX], c[PATH_MAX];
      makePath(a, sizeof(a), "alpha.txt");
      makePath(b, sizeof(b), "beta.txt");
      makePath(c, sizeof(c), "gamma.txt");
      ASSERT_TRUE(createFile(a, "a", 1));
      ASSERT_TRUE(createFile(b, "b", 1));
      ASSERT_TRUE(createFile(c, "c", 1));

      int rc = 0, rsn = 0;
      UnixFile *dir = directoryOpen(s_workDir, &rc, &rsn);
      ASSERT_NOT_NULL(dir);

      char entryBuf[2550] = {0};
      bool sawAlpha = false, sawBeta = false, sawGamma = false;
      int entries;
      while ((entries = directoryRead(dir, entryBuf, sizeof(entryBuf),
                                      &rc, &rsn)) > 0) {
        int offset = 0;
        for (int i = 0; i < entries; i++) {
          DirectoryEntry *de = (DirectoryEntry *)(entryBuf + offset);
          if (strcmp(de->name, "alpha.txt") == 0) sawAlpha = true;
          if (strcmp(de->name, "beta.txt")  == 0) sawBeta  = true;
          if (strcmp(de->name, "gamma.txt") == 0) sawGamma = true;
          offset += de->entryLength;
        }
      }
      directoryClose(dir, &rc, &rsn);

      ASSERT_TRUE(sawAlpha);
      ASSERT_TRUE(sawBeta);
      ASSERT_TRUE(sawGamma);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 16 - directoryDeleteRecursive
 * =================================================================== */
static void testDirectoryDeleteRecursive(void) {
  DESCRIBE("directoryDeleteRecursive") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("deletes a directory tree containing files and subdirectories") {
      TEST_COVERS(directoryDeleteRecursive);
      /* Build:
       *   s_workDir/tree/
       *     file1.txt
       *     sub/
       *       file2.txt
       */
      char tree[PATH_MAX], sub[PATH_MAX], f1[PATH_MAX], f2[PATH_MAX];
      makePath(tree, sizeof(tree), "tree");
      snprintf(sub, sizeof(sub), "%s/sub", tree);
      snprintf(f1,  sizeof(f1),  "%s/file1.txt", tree);
      snprintf(f2,  sizeof(f2),  "%s/file2.txt", sub);

      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, directoryMake(tree, 0755, &rc, &rsn));
      ASSERT_EQUAL_INT(0, directoryMake(sub,  0755, &rc, &rsn));
      ASSERT_TRUE(createFile(f1, "one", 3));
      ASSERT_TRUE(createFile(f2, "two", 3));

      int status = directoryDeleteRecursive(tree, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);

      /* The top-level tree directory must be gone */
      FileInfo fi = {0};
      ASSERT_EQUAL_INT(-1, fileInfo(tree, &fi, &rc, &rsn));
    } IT_END

    IT("deletes a deeply nested directory tree") {
      TEST_COVERS(directoryDeleteRecursive);
      /* Build: topDir/a/b/c/deep.txt */
      char topDir[PATH_MAX], a[PATH_MAX], ab[PATH_MAX], abc[PATH_MAX], deep[PATH_MAX];
      makePath(topDir, sizeof(topDir), "deep_tree");
      snprintf(a,    sizeof(a),    "%s/a",         topDir);
      snprintf(ab,   sizeof(ab),   "%s/a/b",       topDir);
      snprintf(abc,  sizeof(abc),  "%s/a/b/c",     topDir);
      snprintf(deep, sizeof(deep), "%s/a/b/c/deep.txt", topDir);

      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, directoryMake(topDir, 0755, &rc, &rsn));
      ASSERT_EQUAL_INT(0, directoryMake(a,      0755, &rc, &rsn));
      ASSERT_EQUAL_INT(0, directoryMake(ab,     0755, &rc, &rsn));
      ASSERT_EQUAL_INT(0, directoryMake(abc,    0755, &rc, &rsn));
      ASSERT_TRUE(createFile(deep, "leaf", 4));

      int status = directoryDeleteRecursive(topDir, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);

      FileInfo fi = {0};
      ASSERT_EQUAL_INT(-1, fileInfo(topDir, &fi, &rc, &rsn));
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 17 - fileChangeTag (z/OS only)
 *
 *  On z/OS every HFS/zFS file has an optional file tag that records the
 *  CCSID of its text content (see `chtag`).  The default is untagged
 *  (CCSID 0 / 0x0000).  After calling fileChangeTag(path, &rc, &rsn, 1047)
 *  the CCSID should be 1047 (IBM-1047, the standard z/OS EBCDIC code page).
 * =================================================================== */
#ifdef __ZOWE_OS_ZOS
static void testFileChangeTag(void) {
  DESCRIBE("fileChangeTag - CCSID tagging (z/OS only)") {
    SET_BEFORE_EACH(beforeEachWork);
    SET_AFTER_EACH(afterEachWork);

    IT("fileChangeTag sets CCSID 1047 on a newly created file") {
      TEST_COVERS(fileChangeTag);
      TEST_COVERS(fileInfoCCSID);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "tagged.txt");
      ASSERT_TRUE(createFile(path, "ebcdic content", 14));

      int rc = 0, rsn = 0;
      int status = fileChangeTag(path, &rc, &rsn, 1047);
      ASSERT_EQUAL_INT(0, status);

      FileInfo fi = {0};
      ASSERT_EQUAL_INT(0, fileInfo(path, &fi, &rc, &rsn));
      ASSERT_EQUAL_INT(1047, fileInfoCCSID(&fi));
    } IT_END

    IT("fileChangeTag sets CCSID 819 (ISO-8859-1 / Latin-1)") {
      TEST_COVERS(fileChangeTag);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "latin1.txt");
      ASSERT_TRUE(createFile(path, "latin1 content", 14));

      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, fileChangeTag(path, &rc, &rsn, 819));

      FileInfo fi = {0};
      ASSERT_EQUAL_INT(0, fileInfo(path, &fi, &rc, &rsn));
      ASSERT_EQUAL_INT(819, fileInfoCCSID(&fi));
    } IT_END

    IT("fileChangeTag returns -1 for a non-existent file") {
      TEST_COVERS(fileChangeTag);
      char path[PATH_MAX];
      makePath(path, sizeof(path), "nofile_tag.txt");
      int rc = 0, rsn = 0;
      int status = fileChangeTag(path, &rc, &rsn, 1047);
      ASSERT_EQUAL_INT(-1, status);
    } IT_END

  } DESCRIBE_END
}
#endif /* __ZOWE_OS_ZOS */

/* ===================================================================
 *  Entry point
 * =================================================================== */

int main(void) {
  zoweTestInit();

  testUmask();
  testFileCreateReadWrite();
  testFileAppend();
  testFileGetChar();
  testFileInfo();
  testFileInfoDirectory();
  testFileMode();
  testSymbolicFileInfo();
  testFileDirname();
  testFileCopy();
  testFileRename();
  testFileDelete();
  testDirectoryMakeDelete();
  testDirectoryRename();
  testDirectoryListing();
  testDirectoryDeleteRecursive();
#ifdef __ZOWE_OS_ZOS
  testFileChangeTag();
#endif

  /* Clean up the base /tmp directory */
  if (s_baseCreated) {
    int rc = 0, rsn = 0;
    directoryDeleteRecursive(s_baseDir, &rc, &rsn);
  }

  return ZOWE_TEST_REPORT();
}
