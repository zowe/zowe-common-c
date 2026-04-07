
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  POSIX implementation of the unixfile.h interface for Linux and macOS.
  Uses standard POSIX APIs: fopen/fread/fwrite/fclose for files, and
  opendir/readdir/closedir for directories.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "unixfile.h"

static int fileTrace = FALSE;

int setFileTrace(int toWhat) {
  int was = fileTrace;
  fileTrace = toWhat;
  return was;
}

UnixFile *fileOpen(const char *filename, int options, int mode, int bufferSize,
                   int *returnCode, int *reasonCode) {
  const char *unixMode = "r";

  /*
   * FILE_OPTION_READ_ONLY is O_RDONLY (== 0) on POSIX, so we cannot use
   * bitwise AND to test for it.  Use O_ACCMODE to extract the access bits.
   */
  int accMode = options & O_ACCMODE;

  if (options & FILE_OPTION_APPEND) {
    unixMode = (accMode == O_RDWR) ? "a+" : "a";
  } else if (accMode == O_WRONLY) {
    unixMode = (options & FILE_OPTION_TRUNCATE) ? "w" : "w";
  } else if (accMode == O_RDWR) {
    unixMode = (options & FILE_OPTION_TRUNCATE) ? "w+" : "r+";
  } else {
    /* O_RDONLY (0) — read-only */
    unixMode = "r";
  }

  FILE *internalFile = fopen(filename, unixMode);
  if (internalFile == NULL) {
    *returnCode = errno;
    *reasonCode = errno;
    return NULL;
  }

  UnixFile *file = (UnixFile *)safeMalloc(sizeof(UnixFile), "POSIX File");
  memset(file, 0, sizeof(UnixFile));
  file->internalFile = internalFile;
  file->fd = fileno(internalFile);
  file->pathname = safeMalloc(strlen(filename) + 1, "POSIX File Name");
  strcpy(file->pathname, filename);
  file->isDirectory = FALSE;

  if (bufferSize > 0) {
    file->buffer = safeMalloc(bufferSize, "POSIX File Buffer");
    file->bufferSize = bufferSize;
    file->bufferPos = bufferSize;
    file->bufferFill = bufferSize;
  }

  *returnCode = 0;
  *reasonCode = 0;
  return file;
}

int fileRead(UnixFile *file, char *buffer, int desiredBytes,
             int *returnCode, int *reasonCode) {
  FILE *internalFile = file->internalFile;
  size_t bytesRead = fread(buffer, 1, desiredBytes, internalFile);

  *returnCode = 0;
  *reasonCode = 0;
  if (bytesRead < (size_t)desiredBytes) {
    file->eofKnown = TRUE;
    if (ferror(internalFile)) {
      *returnCode = errno;
      *reasonCode = errno;
    }
  }
  return (int)bytesRead;
}

int fileWrite(UnixFile *file, const char *buffer, int desiredBytes,
              int *returnCode, int *reasonCode) {
  FILE *internalFile = file->internalFile;
  size_t bytesWritten = fwrite(buffer, 1, desiredBytes, internalFile);

  *returnCode = 0;
  *reasonCode = 0;
  if (bytesWritten < (size_t)desiredBytes) {
    if (ferror(internalFile)) {
      *returnCode = errno;
      *reasonCode = errno;
    }
  }
  return (int)bytesWritten;
}

int fileGetChar(UnixFile *file, int *returnCode, int *reasonCode) {
  if (file->bufferSize == 0) {
    *returnCode = 8;
    *reasonCode = 0xBFF;
    return -1;
  } else if (file->bufferPos < file->bufferFill) {
    return (int)(file->buffer[file->bufferPos++]) & 0xFF;
  } else if (file->eofKnown) {
    return -1;
  } else {
    int bytesRead = fileRead(file, file->buffer, file->bufferSize, returnCode, reasonCode);
    if (bytesRead > 0) {
      if (bytesRead < file->bufferSize) {
        file->eofKnown = TRUE;
      }
      file->bufferFill = bytesRead;
      file->bufferPos = 1;
      return file->buffer[0] & 0xFF;
    } else {
      return -1;
    }
  }
}

int fileClose(UnixFile *file, int *returnCode, int *reasonCode) {
  FILE *internalFile = file->internalFile;
  int status = fclose(internalFile);

  *returnCode = 0;
  *reasonCode = 0;
  if (status != 0) {
    *returnCode = errno;
    *reasonCode = errno;
  }

  if (file->pathname != NULL) {
    safeFree(file->pathname, strlen(file->pathname) + 1);
    file->pathname = NULL;
  }
  if (file->buffer != NULL) {
    safeFree(file->buffer, file->bufferSize);
    file->buffer = NULL;
  }
  safeFree((char *)file, sizeof(UnixFile));
  return status;
}

int fileInfo(const char *filename, FileInfo *info, int *returnCode, int *reasonCode) {
  int status = stat(filename, info);
  *returnCode = 0;
  *reasonCode = 0;
  if (status != 0) {
    *returnCode = errno;
    *reasonCode = errno;
  }
  return status;
}

int symbolicFileInfo(const char *filename, FileInfo *info, int *returnCode, int *reasonCode) {
  int status = lstat(filename, info);
  *returnCode = 0;
  *reasonCode = 0;
  if (status != 0) {
    *returnCode = errno;
    *reasonCode = errno;
  }
  return status;
}

int fileReadLink(char *fileName, char *buffer, int bufferSize,
                 int *returnCode, int *reasonCode) {
  ssize_t bytesRead = readlink(fileName, buffer, bufferSize - 1);
  *returnCode = 0;
  *reasonCode = 0;
  if (bytesRead < 0) {
    *returnCode = errno;
    *reasonCode = errno;
    return -1;
  }
  buffer[bytesRead] = '\0';
  return (int)bytesRead;
}

int fileEOF(const UnixFile *file) {
  return ((file->bufferPos >= file->bufferFill) && file->eofKnown);
}

int fileInfoIsDirectory(const FileInfo *info) {
  return S_ISDIR(info->st_mode) ? 1 : 0;
}

int64 fileInfoSize(const FileInfo *info) {
  return (int64)info->st_size;
}

/*
  CCSID is a z/OS concept. On POSIX systems we always treat files as UTF-8.
  Callers should be aware that this value may not be meaningful off z/OS.
*/
static int posixFileCCSID = 1208; /* UTF-8 */

int setFileInfoCCSID(int ccsid) {
  int previous = posixFileCCSID;
  posixFileCCSID = ccsid;
  return previous;
}

int fileInfoCCSID(const FileInfo *info) {
  return posixFileCCSID;
}

int fileInfoUnixCreationTime(const FileInfo *info) {
  return (int)info->st_ctime;
}

int fileInfoUnixModificationTime(const FileInfo *info) {
  return (int)info->st_mtime;
}

int fileUnixMode(const FileInfo *info) {
  return (int)info->st_mode;
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

int fileDirname(const char *path, char *dirnameOut) {
  int pathLen = strlen(path);
  char *pathCopy = safeMalloc(pathLen + 1, "fileDirname path copy");
  strcpy(pathCopy, path);

  char *result = dirname(pathCopy);
  strcpy(dirnameOut, result);
  safeFree(pathCopy, pathLen + 1);
  return 0;
}

int fileRename(const char *oldFileName, const char *newFileName,
               int *returnCode, int *reasonCode) {
  int status = rename(oldFileName, newFileName);
  *returnCode = 0;
  *reasonCode = 0;
  if (status != 0) {
    *returnCode = errno;
    *reasonCode = errno;
  }
  return status;
}

int fileDelete(const char *fileName, int *returnCode, int *reasonCode) {
  int status = unlink(fileName);
  *returnCode = 0;
  *reasonCode = 0;
  if (status != 0) {
    *returnCode = errno;
    *reasonCode = errno;
  }
  return status;
}

#define FILE_BUFFER_SIZE 4096
#define MAX_CONVERT_FACTOR 4

int fileCopyConverted(const char *existingFileName, const char *newFileName,
                      int existingCCSID, int newCCSID,
                      int *retCode, int *resCode) {
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;

  int status = fileInfo(existingFileName, &info, &returnCode, &reasonCode);
  if (status != 0) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  UnixFile *existingFile = fileOpen(existingFileName, FILE_OPTION_READ_ONLY, 0, 0,
                                    &returnCode, &reasonCode);
  if (existingFile == NULL) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  UnixFile *newFile = fileOpen(newFileName,
                               FILE_OPTION_WRITE_ONLY | FILE_OPTION_TRUNCATE | FILE_OPTION_CREATE,
                               0700,
                               0,
                               &returnCode,
                               &reasonCode);
  if (newFile == NULL) {
    *retCode = returnCode;
    *resCode = reasonCode;
    fileClose(existingFile, &returnCode, &reasonCode);
    return -1;
  }

  char *fileBuffer = safeMalloc(FILE_BUFFER_SIZE, "fileCopyBuffer");
  int returnValue = 0;
  int bytesRead = 0;

  do {
    bytesRead = fileRead(existingFile, fileBuffer, FILE_BUFFER_SIZE, &returnCode, &reasonCode);
    if (bytesRead < 0) {
      *retCode = returnCode;
      *resCode = reasonCode;
      returnValue = -1;
      break;
    }
    if (bytesRead > 0) {
      int written = fileWrite(newFile, fileBuffer, bytesRead, &returnCode, &reasonCode);
      if (written < 0) {
        *retCode = returnCode;
        *resCode = reasonCode;
        returnValue = -1;
        break;
      }
    }
  } while (bytesRead > 0);

  safeFree(fileBuffer, FILE_BUFFER_SIZE);
  fileClose(existingFile, &returnCode, &reasonCode);
  fileClose(newFile, &returnCode, &reasonCode);
  return returnValue;
}

int fileCopy(const char *existingFileName, const char *newFileName,
             int *retCode, int *resCode) {
  return fileCopyConverted(existingFileName, newFileName, 0, 0, retCode, resCode);
}

/*
  directoryOpen / directoryRead / directoryClose use POSIX opendir/readdir.
  Each directoryRead call fills the buffer with one DirectoryEntry at a time,
  formatted as [entryLength:2][nameLength:2][name:nameLength], which matches
  the format expected by callers that iterate the buffer.
*/
UnixFile *directoryOpen(const char *directoryName, int *returnCode, int *reasonCode) {
  DIR *dir = opendir(directoryName);
  *returnCode = 0;
  *reasonCode = 0;
  if (dir == NULL) {
    *returnCode = errno;
    *reasonCode = errno;
    return NULL;
  }

  UnixFile *directory = (UnixFile *)safeMalloc(sizeof(UnixFile), "POSIX Directory");
  memset(directory, 0, sizeof(UnixFile));
  directory->internalFile = NULL;
  directory->fd = dirfd(dir);
  directory->dir = dir;
  directory->pathname = safeMalloc(strlen(directoryName) + 1, "POSIX Directory Name");
  strcpy(directory->pathname, directoryName);
  directory->isDirectory = TRUE;
  return directory;
}

int directoryRead(UnixFile *directory, char *entryBuffer, int entryBufferLength,
                  int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  errno = 0;
  struct dirent *entry = readdir(directory->dir);
  if (entry == NULL) {
    if (errno != 0) {
      *returnCode = errno;
      *reasonCode = errno;
      return -1;
    }
    /* End of directory */
    return 0;
  }

  int nameLength = (int)strlen(entry->d_name);
  /* Header is 4 bytes: 2 bytes entryLength + 2 bytes nameLength */
  int totalLength = 4 + nameLength;

  if (totalLength > entryBufferLength) {
    /* Buffer too small - return the entry truncated to fit */
    nameLength = entryBufferLength - 4;
    if (nameLength < 0) {
      nameLength = 0;
    }
    totalLength = 4 + nameLength;
  }

  DirectoryEntry *de = (DirectoryEntry *)entryBuffer;
  de->entryLength = (short)totalLength;
  de->nameLength = (short)nameLength;
  if (nameLength > 0) {
    memcpy(de->name, entry->d_name, nameLength);
  }

  return 1;
}

int directoryClose(UnixFile *directory, int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  int status = closedir(directory->dir);
  if (status != 0) {
    *returnCode = errno;
    *reasonCode = errno;
  }

  if (directory->pathname != NULL) {
    safeFree(directory->pathname, strlen(directory->pathname) + 1);
    directory->pathname = NULL;
  }
  safeFree((char *)directory, sizeof(UnixFile));
  return status;
}

int directoryMake(const char *pathName, int mode, int *returnCode, int *reasonCode) {
  int status = mkdir(pathName, (mode_t)mode);
  *returnCode = 0;
  *reasonCode = 0;
  if (status != 0) {
    *returnCode = errno;
    *reasonCode = errno;
  }
  return status;
}

int directoryDelete(const char *pathName, int *returnCode, int *reasonCode) {
  int status = rmdir(pathName);
  *returnCode = 0;
  *reasonCode = 0;
  if (status != 0) {
    *returnCode = errno;
    *reasonCode = errno;
  }
  return status;
}

static int deleteDirectoryRecursiveInternal(const char *pathName) {
  DIR *dir = opendir(pathName);
  if (dir == NULL) {
    return -1;
  }

  struct dirent *entry;
  int pathLen = (int)strlen(pathName);
  int status = 0;

  errno = 0;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    int childLen = pathLen + 1 + (int)strlen(entry->d_name) + 1;
    char *childPath = safeMalloc(childLen, "deleteRecursive child path");
    snprintf(childPath, childLen, "%s/%s", pathName, entry->d_name);

    struct stat st;
    if (lstat(childPath, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        status = deleteDirectoryRecursiveInternal(childPath);
      } else {
        status = unlink(childPath);
      }
    }

    safeFree(childPath, childLen);
    if (status != 0) {
      break;
    }
  }
  closedir(dir);

  if (status == 0) {
    status = rmdir(pathName);
  }
  return status;
}

int directoryDeleteRecursive(const char *pathName, int *retCode, int *resCode) {
  int status = deleteDirectoryRecursiveInternal(pathName);
  *retCode = (status != 0) ? errno : 0;
  *resCode = *retCode;
  return status;
}

int directoryMakeDirectoryRecursive(const char *pathName, char *message,
                                    int messageLength, int recursive, int forceCreate) {
  int pathLen = (int)strlen(pathName);
  char *pathCopy = safeMalloc(pathLen + 1, "mkdirp path copy");
  strcpy(pathCopy, pathName);

  int status = 0;
  for (int i = 1; i <= pathLen; i++) {
    if (pathCopy[i] == '/' || pathCopy[i] == '\0') {
      char saved = pathCopy[i];
      pathCopy[i] = '\0';

      struct stat st;
      if (stat(pathCopy, &st) != 0) {
        status = mkdir(pathCopy, 0755);
        if (status != 0 && errno != EEXIST) {
          if (message != NULL) {
            snprintf(message, messageLength, "mkdir(%s) failed: %s", pathCopy, strerror(errno));
          }
          safeFree(pathCopy, pathLen + 1);
          return -1;
        }
      }
      pathCopy[i] = saved;
    }
  }

  safeFree(pathCopy, pathLen + 1);
  return 0;
}

int directoryCopy(const char *existingPathName, const char *newPathName,
                  int *retCode, int *resCode) {
  /* Basic implementation: create destination and copy files */
  *retCode = 0;
  *resCode = 0;

  struct stat st;
  if (stat(existingPathName, &st) != 0) {
    *retCode = errno;
    *resCode = errno;
    return -1;
  }

  if (mkdir(newPathName, st.st_mode) != 0 && errno != EEXIST) {
    *retCode = errno;
    *resCode = errno;
    return -1;
  }

  DIR *dir = opendir(existingPathName);
  if (dir == NULL) {
    *retCode = errno;
    *resCode = errno;
    return -1;
  }

  int srcLen = (int)strlen(existingPathName);
  int dstLen = (int)strlen(newPathName);
  struct dirent *entry;
  int status = 0;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    int nameLen = (int)strlen(entry->d_name);
    int srcChildLen = srcLen + 1 + nameLen + 1;
    int dstChildLen = dstLen + 1 + nameLen + 1;

    char *srcChild = safeMalloc(srcChildLen, "directoryCopy src");
    char *dstChild = safeMalloc(dstChildLen, "directoryCopy dst");
    snprintf(srcChild, srcChildLen, "%s/%s", existingPathName, entry->d_name);
    snprintf(dstChild, dstChildLen, "%s/%s", newPathName, entry->d_name);

    struct stat childSt;
    if (lstat(srcChild, &childSt) == 0) {
      if (S_ISDIR(childSt.st_mode)) {
        status = directoryCopy(srcChild, dstChild, retCode, resCode);
      } else {
        status = fileCopy(srcChild, dstChild, retCode, resCode);
      }
    }

    safeFree(srcChild, srcChildLen);
    safeFree(dstChild, dstChildLen);
    if (status != 0) {
      break;
    }
  }
  closedir(dir);
  return status;
}

int directoryRename(const char *oldDirName, const char *newDirName,
                    int *returnCode, int *reasonCode) {
  int status = rename(oldDirName, newDirName);
  *returnCode = 0;
  *reasonCode = 0;
  if (status != 0) {
    *returnCode = errno;
    *reasonCode = errno;
  }
  return status;
}

int directoryChangeModeRecursive(const char *pathName, int flag,
                                 int mode, const char *compare,
                                 int *retCode, int *resCode) {
  *retCode = 0;
  *resCode = 0;

  int status = chmod(pathName, (mode_t)mode);
  if (status != 0) {
    *retCode = errno;
    *resCode = errno;
    return -1;
  }

  struct stat st;
  if (stat(pathName, &st) != 0 || !S_ISDIR(st.st_mode)) {
    return 0;
  }

  DIR *dir = opendir(pathName);
  if (dir == NULL) {
    *retCode = errno;
    *resCode = errno;
    return -1;
  }

  int pathLen = (int)strlen(pathName);
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    int childLen = pathLen + 1 + (int)strlen(entry->d_name) + 1;
    char *childPath = safeMalloc(childLen, "directoryChangeModeRecursive child");
    snprintf(childPath, childLen, "%s/%s", pathName, entry->d_name);

    status = directoryChangeModeRecursive(childPath, flag, mode, compare, retCode, resCode);
    safeFree(childPath, childLen);
    if (status != 0) {
      break;
    }
  }
  closedir(dir);
  return status;
}

int directoryChangeOwner(char *message, int messageLength, char *directory,
                         int userId, int groupId, bool recursion, char *pattern,
                         int *returnCode, int *reasonCode) {
  *returnCode = 0;
  *reasonCode = 0;

  int status = chown(directory, (uid_t)userId, (gid_t)groupId);
  if (status != 0) {
    *returnCode = errno;
    *reasonCode = errno;
    if (message != NULL) {
      snprintf(message, messageLength, "chown(%s) failed: %s", directory, strerror(errno));
    }
    return -1;
  }

  if (!recursion) {
    return 0;
  }

  DIR *dir = opendir(directory);
  if (dir == NULL) {
    *returnCode = errno;
    *reasonCode = errno;
    return -1;
  }

  int dirLen = (int)strlen(directory);
  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    int childLen = dirLen + 1 + (int)strlen(entry->d_name) + 1;
    char *childPath = safeMalloc(childLen, "directoryChangeOwner child");
    snprintf(childPath, childLen, "%s/%s", directory, entry->d_name);

    status = directoryChangeOwner(message, messageLength, childPath,
                                  userId, groupId, recursion, pattern,
                                  returnCode, reasonCode);
    safeFree(childPath, childLen);
    if (status != 0) {
      break;
    }
  }
  closedir(dir);
  return status;
}

int directoryChangeTagRecursive(const char *pathName, char *type,
                                char *codepage, int recursive, char *pattern,
                                int *retCode, int *resCode) {
  /* File tagging is a z/OS concept; no-op on POSIX systems */
  *retCode = 0;
  *resCode = 0;
  return 0;
}

int setUmask(int mask) {
  return (int)umask((mode_t)mask);
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
