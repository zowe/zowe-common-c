/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * platform/posix/psxfile.c
 *
 * POSIX (Linux/macOS) implementation of the unixfile.h API.
 * This file provides the same functions that zosfile.c provides on z/OS,
 * using standard POSIX system calls (open, read, close, stat, etc.).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>
#include <dirent.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "unixfile.h"

static int fileTrace = 0;

int setFileTrace(int toWhat) {
  int was = fileTrace;
  fileTrace = toWhat;
  return was;
}

UnixFile *fileOpen(const char *filename, int options, int mode,
                   int bufferSize, int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  int flags = 0;

  /* Map FILE_OPTION_* to O_* flags.
     On Linux/macOS these are already mapped via unixfile.h macros,
     but callers may pass the raw values directly. */
  flags = options;

  int fd = open(filename, flags, (mode ? mode : 0644));
  if (fd < 0) {
    *returnCode = errno;
    return NULL;
  }

  int allocSize = sizeof(UnixFile) + (bufferSize > 0 ? bufferSize : 1024);
  UnixFile *file = (UnixFile *)safeMalloc(allocSize, "UnixFile");
  memset(file, 0, allocSize);
  file->fd = fd;
  file->pathname = NULL;
  file->bufferSize = (bufferSize > 0 ? bufferSize : 1024);
  file->buffer = ((char *)file) + sizeof(UnixFile);
  file->bufferFill = 0;
  file->bufferPos = 0;
  file->eofKnown = 0;
  file->isDirectory = 0;
  file->internalFile = NULL;

  return file;
}

int fileRead(UnixFile *file, char *buffer, int desiredBytes,
             int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  ssize_t bytesRead = read(file->fd, buffer, desiredBytes);
  if (bytesRead < 0) {
    *returnCode = errno;
    return -1;
  }
  return (int)bytesRead;
}

int fileWrite(UnixFile *file, const char *buffer, int desiredBytes,
              int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  ssize_t bytesWritten = write(file->fd, buffer, desiredBytes);
  if (bytesWritten < 0) {
    *returnCode = errno;
    return -1;
  }
  return (int)bytesWritten;
}

int fileGetChar(UnixFile *file, int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  /* Use internal buffer for efficiency */
  if (file->bufferPos >= file->bufferFill) {
    ssize_t n = read(file->fd, file->buffer, file->bufferSize);
    if (n < 0) {
      *returnCode = errno;
      return -1;
    }
    if (n == 0) {
      file->eofKnown = 1;
      return -1;
    }
    file->bufferFill = (int)n;
    file->bufferPos = 0;
  }
  return (unsigned char)file->buffer[file->bufferPos++];
}

int fileClose(UnixFile *file, int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  if (file == NULL) {
    *returnCode = EINVAL;
    return -1;
  }

  int rc = close(file->fd);
  if (rc < 0) {
    *returnCode = errno;
  }
  safeFree((char *)file, sizeof(UnixFile) + file->bufferSize);
  return rc;
}

int fileInfo(const char *filename, FileInfo *info, int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  int rc = stat(filename, info);
  if (rc < 0) {
    *returnCode = errno;
    return -1;
  }
  return 0;
}

int symbolicFileInfo(const char *filename, FileInfo *info, int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  int rc = lstat(filename, info);
  if (rc < 0) {
    *returnCode = errno;
    return -1;
  }
  return 0;
}

int64 fileInfoSize(const FileInfo *info) {
  return (int64)info->st_size;
}

int fileInfoIsDirectory(const FileInfo *info) {
  return S_ISDIR(info->st_mode);
}

int fileInfoIsRegularFile(const FileInfo *info) {
  return S_ISREG(info->st_mode);
}

int fileInfoIsSymbolicLink(const FileInfo *info) {
  return S_ISLNK(info->st_mode);
}

int fileInfoCCSID(const FileInfo *info) {
  /* CCSID concept doesn't apply on POSIX; return 0 (binary/unknown) */
  return 0;
}

int fileInfoUnixCreationTime(const FileInfo *info) {
#ifdef __APPLE__
  return (int)info->st_birthtimespec.tv_sec;
#else
  /* Linux doesn't have a standard creation time; use ctime */
  return (int)info->st_ctime;
#endif
}

int fileInfoUnixModificationTime(const FileInfo *info) {
  return (int)info->st_mtime;
}

int fileUnixMode(const FileInfo *info) {
  return (int)info->st_mode;
}

int fileEOF(const UnixFile *file) {
  return file->eofKnown;
}

int fileGetINode(const FileInfo *info) {
  return (int)info->st_ino;
}

int fileGetDeviceID(const FileInfo *info) {
  return (int)info->st_dev;
}

int fileInfoOwnerGID(const FileInfo *info) {
  return (int)info->st_gid;
}

int fileInfoOwnerUID(const FileInfo *info) {
  return (int)info->st_uid;
}

int fileCopy(const char *existingFileName, const char *newFileName,
             int *retCode, int *resCode) {
  *retCode = 0;
  *resCode = 0;

  int srcFD = open(existingFileName, O_RDONLY);
  if (srcFD < 0) {
    *retCode = errno;
    return -1;
  }

  int dstFD = open(newFileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (dstFD < 0) {
    *retCode = errno;
    close(srcFD);
    return -1;
  }

  char buf[4096];
  ssize_t n;
  while ((n = read(srcFD, buf, sizeof(buf))) > 0) {
    ssize_t written = write(dstFD, buf, n);
    if (written != n) {
      *retCode = errno;
      close(srcFD);
      close(dstFD);
      return -1;
    }
  }

  close(srcFD);
  close(dstFD);
  return 0;
}

int fileRename(const char *oldFileName, const char *newFileName,
               int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  int rc = rename(oldFileName, newFileName);
  if (rc < 0) {
    *returnCode = errno;
    return -1;
  }
  return 0;
}

int fileDelete(const char *fileName, int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  int rc = unlink(fileName);
  if (rc < 0) {
    *returnCode = errno;
    return -1;
  }
  return 0;
}

int fileReadLink(char *fileName, char *buffer, int bufferSize,
                 int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  ssize_t len = readlink(fileName, buffer, bufferSize - 1);
  if (len < 0) {
    *returnCode = errno;
    return -1;
  }
  buffer[len] = '\0';
  return (int)len;
}

int fileDirname(const char *path, char *dirnameBuf) {
  /* dirname may modify its argument, so copy */
  char *tmp = strdup(path);
  if (!tmp) return -1;
  char *dir = dirname(tmp);
  strcpy(dirnameBuf, dir);
  free(tmp);
  return 0;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
