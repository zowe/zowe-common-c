/*
  POSIX implementation of the zowe-common-c UnixFile / FileInfo API.
  Mirrors the Windows port at platform/windows/winfile.c and the z/OS
  implementation at c/zosfile.c. CCSID / EBCDIC bits are no-ops on POSIX
  hosts since the native execution codepage is already ASCII.

  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this distribution,
  and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include "zowetypes.h"

#if defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_MACOSX)

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "alloc.h"
#include "unixfile.h"
#include "utils.h"

static int fileTrace = 0;

int setFileTrace(int toWhat) {
  int prev = fileTrace;
  fileTrace = toWhat;
  return prev;
}

UnixFile *fileOpen(const char *filename,
                   int options, int mode, int bufferSize,
                   int *returnCode, int *reasonCode) {
  int posixMode = mode ? mode : 0644;
  int fd = open(filename, options, posixMode);
  if (fd < 0) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = errno;
    return NULL;
  }
  UnixFile *uf = (UnixFile *)safeMalloc(sizeof(UnixFile), "UnixFile");
  if (uf == NULL) {
    close(fd);
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = ENOMEM;
    return NULL;
  }
  memset(uf, 0, sizeof(*uf));
  uf->pathname = (filename ? strdup(filename) : NULL);
  uf->fd = fd;
  uf->bufferSize = bufferSize;
  uf->bufferFill = 0;
  uf->bufferPos = 0;
  uf->eofKnown = 0;
  uf->isDirectory = 0;
  uf->internalFile = NULL;
  uf->dir = NULL;
  if (bufferSize > 0) {
    uf->buffer = (char *)safeMalloc(bufferSize, "UnixFile.buffer");
    if (uf->buffer == NULL) {
      close(fd);
      if (uf->pathname) free(uf->pathname);
      safeFree((char *)uf, sizeof(*uf));
      if (returnCode) *returnCode = -1;
      if (reasonCode) *reasonCode = ENOMEM;
      return NULL;
    }
  }
  if (returnCode) *returnCode = 0;
  if (reasonCode) *reasonCode = 0;
  return uf;
}

int fileClose(UnixFile *file, int *returnCode, int *reasonCode) {
  if (file == NULL) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = EINVAL;
    return -1;
  }
  int rc = 0;
  if (file->fd >= 0) {
    rc = close(file->fd);
    if (rc != 0) {
      if (returnCode) *returnCode = -1;
      if (reasonCode) *reasonCode = errno;
    }
  }
  if (file->dir) {
    closedir(file->dir);
    file->dir = NULL;
  }
  if (file->buffer) {
    safeFree(file->buffer, file->bufferSize);
    file->buffer = NULL;
  }
  if (file->pathname) {
    free(file->pathname);
    file->pathname = NULL;
  }
  safeFree((char *)file, sizeof(*file));
  if (rc == 0) {
    if (returnCode) *returnCode = 0;
    if (reasonCode) *reasonCode = 0;
  }
  return rc;
}

/* Refill the UnixFile's internal buffer from the fd. Returns bytes read,
 * or -1 on error, or 0 at EOF. */
static int refillBuffer(UnixFile *file, int *returnCode, int *reasonCode) {
  if (file->buffer == NULL || file->bufferSize <= 0) {
    return 0;
  }
  ssize_t n = read(file->fd, file->buffer, (size_t)file->bufferSize);
  if (n < 0) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = errno;
    return -1;
  }
  file->bufferFill = (int)n;
  file->bufferPos = 0;
  if (n == 0) file->eofKnown = 1;
  return (int)n;
}

int fileGetChar(UnixFile *file, int *returnCode, int *reasonCode) {
  if (file == NULL) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = EINVAL;
    return -1;
  }
  if (file->bufferSize <= 0 || file->buffer == NULL) {
    unsigned char c;
    ssize_t n = read(file->fd, &c, 1);
    if (n == 1) return (int)c;
    if (n == 0) {
      file->eofKnown = 1;
      return -1;
    }
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = errno;
    return -1;
  }
  if (file->bufferPos >= file->bufferFill) {
    if (file->eofKnown) return -1;
    int n = refillBuffer(file, returnCode, reasonCode);
    if (n <= 0) return -1;
  }
  return (int)(unsigned char)file->buffer[file->bufferPos++];
}

int fileInfo(const char *filename, FileInfo *info,
             int *returnCode, int *reasonCode) {
  if (filename == NULL || info == NULL) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = EINVAL;
    return -1;
  }
  int rc = stat(filename, info);
  if (rc != 0) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = errno;
    return -1;
  }
  if (returnCode) *returnCode = 0;
  if (reasonCode) *reasonCode = 0;
  return 0;
}

int64 fileInfoSize(const FileInfo *info) {
  return info ? (int64)info->st_size : 0;
}

int fileInfoIsDirectory(const FileInfo *info) {
  return info ? S_ISDIR(info->st_mode) : 0;
}

/* CCSID is meaningless on a POSIX host. Source encoding is whatever the user
 * configured, but there's no per-file tag. Return a placeholder 1047 only when
 * asked; callers on POSIX shouldn't be branching on this. */
int fileInfoCCSID(const FileInfo *info) {
  (void)info;
  return 0;
}

int fileInfoUnixCreationTime(const FileInfo *info) {
  return info ? (int)info->st_ctime : 0;
}

int fileInfoUnixModificationTime(const FileInfo *info) {
  return info ? (int)info->st_mtime : 0;
}

int fileInfoOwnerGID(const FileInfo *info) {
  return info ? (int)info->st_gid : 0;
}

int fileInfoOwnerUID(const FileInfo *info) {
  return info ? (int)info->st_uid : 0;
}

/* File serial number. The declared return type is int because BPXYSTAT's inode
 * field is an int; ino_t is wider than that on most POSIX hosts, so this
 * narrows. Callers use the value as a hash input or as one half of an identity
 * pair with the device ID, never as something to hand back to the filesystem,
 * so the narrowing costs uniqueness rather than correctness. */
int fileGetINode(const FileInfo *info) {
  return info ? (int)info->st_ino : 0;
}

/* Device number the file lives on. Narrows from dev_t for the same reason. */
int fileGetDeviceID(const FileInfo *info) {
  return info ? (int)info->st_dev : 0;
}

/* Returns number of bytes written to dirname (not counting trailing NUL).
 * POSIX dirname(3) mutates its argument, so we stage on a scratch copy. */
int fileDirname(const char *path, char *dirname_out) {
  if (path == NULL || dirname_out == NULL) return -1;
  char *scratch = strdup(path);
  if (scratch == NULL) return -1;
  char *d = dirname(scratch);
  size_t len = strlen(d);
  memcpy(dirname_out, d, len);
  dirname_out[len] = '\0';
  free(scratch);
  return (int)len;
}

int fileCopy(const char *existingFile, const char *newFile,
             int *retCode, int *resCode) {
  int in = open(existingFile, O_RDONLY);
  if (in < 0) {
    if (retCode) *retCode = -1;
    if (resCode) *resCode = errno;
    return -1;
  }
  int out = open(newFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (out < 0) {
    int e = errno;
    close(in);
    if (retCode) *retCode = -1;
    if (resCode) *resCode = e;
    return -1;
  }
  char buf[8192];
  ssize_t n;
  int rc = 0;
  while ((n = read(in, buf, sizeof(buf))) > 0) {
    ssize_t off = 0;
    while (off < n) {
      ssize_t w = write(out, buf + off, (size_t)(n - off));
      if (w <= 0) { rc = -1; if (resCode) *resCode = errno; break; }
      off += w;
    }
    if (rc != 0) break;
  }
  if (n < 0 && rc == 0) { rc = -1; if (resCode) *resCode = errno; }
  close(in);
  close(out);
  if (retCode) *retCode = rc;
  if (rc == 0 && resCode) *resCode = 0;
  return rc;
}

/* Conversion between CCSIDs is a no-op on POSIX hosts (no per-file tag model).
 * This shim just delegates to fileCopy so callers that expect the "converted"
 * variant to exist can keep compiling and running. */
int fileCopyConverted(const char *existingFileName, const char *newFileName,
                      int existingCCSID, int newCCSID,
                      int *retCode, int *resCode) {
  (void)existingCCSID;
  (void)newCCSID;
  return fileCopy(existingFileName, newFileName, retCode, resCode);
}

int fileRename(const char *oldName, const char *newName,
               int *returnCode, int *reasonCode) {
  int rc = rename(oldName, newName);
  if (rc != 0) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = errno;
    return -1;
  }
  if (returnCode) *returnCode = 0;
  if (reasonCode) *reasonCode = 0;
  return 0;
}

int fileDelete(const char *fileName, int *returnCode, int *reasonCode) {
  int rc = unlink(fileName);
  if (rc != 0) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = errno;
    return -1;
  }
  if (returnCode) *returnCode = 0;
  if (reasonCode) *reasonCode = 0;
  return 0;
}

int fileRead(UnixFile *file, char *buffer, int desiredBytes,
             int *returnCode, int *reasonCode) {
  if (file == NULL || buffer == NULL || desiredBytes <= 0) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = EINVAL;
    return -1;
  }
  ssize_t n = read(file->fd, buffer, (size_t)desiredBytes);
  if (n < 0) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = errno;
    return -1;
  }
  if (n == 0) file->eofKnown = 1;
  if (returnCode) *returnCode = 0;
  if (reasonCode) *reasonCode = 0;
  return (int)n;
}

int fileWrite(UnixFile *file, const char *buffer, int desiredBytes,
              int *returnCode, int *reasonCode) {
  if (file == NULL || buffer == NULL || desiredBytes <= 0) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = EINVAL;
    return -1;
  }
  ssize_t n = write(file->fd, buffer, (size_t)desiredBytes);
  if (n < 0) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = errno;
    return -1;
  }
  if (returnCode) *returnCode = 0;
  if (reasonCode) *reasonCode = 0;
  return (int)n;
}

int fileReadLink(char *fileName, char *buffer, int bufferSize,
                 int *returnCode, int *reasonCode) {
  ssize_t n = readlink(fileName, buffer, (size_t)bufferSize - 1);
  if (n < 0) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = errno;
    return -1;
  }
  buffer[n] = '\0';
  if (returnCode) *returnCode = 0;
  if (reasonCode) *reasonCode = 0;
  return (int)n;
}

#endif /* __ZOWE_OS_LINUX || __ZOWE_OS_AIX || __ZOWE_OS_MACOSX */
