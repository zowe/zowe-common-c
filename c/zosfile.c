

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include "metalio.h"

#else
#include <stdio.h>
#include <string.h>
#include <errno.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "unixfile.h"
#include "timeutls.h"

#ifdef _LP64
#pragma linkage(BPX4RED,OS)
#pragma linkage(BPX4OPN,OS)
#pragma linkage(BPX4WRT,OS)
#pragma linkage(BPX4REN,OS)
#pragma linkage(BPX4CHR,OS)
#pragma linkage(BPX4CLO,OS)
#pragma linkage(BPX4STA,OS)
#pragma linkage(BPX4UNL,OS)
#pragma linkage(BPX4OPD,OS)
#pragma linkage(BPX4MKD,OS)
#pragma linkage(BPX4RDD,OS)
#pragma linkage(BPX4RMD,OS)
#pragma linkage(BPX4CLD,OS)
#pragma linkage(BPX4UMK,OS)
#pragma linkage(BPX4FCT,OS)

#define BPXRED BPX4RED
#define BPXOPN BPX4OPN
#define BPXWRT BPX4WRT
#define BPXREN BPX4REN
#define BPXCHR BPX4CHR
#define BPXCLO BPX4CLO
#define BPXSTA BPX4STA
#define BPXUNL BPX4UNL
#define BPXOPD BPX4OPD
#define BPXMKD BPX4MKD
#define BPXRDD BPX4RDD
#define BPXRMD BPX4RMD
#define BPXCLD BPX4CLD
#define BPXUMK BPX4UMK
#define BPXFCT BPX4FCT

#else
#pragma linkage(BPX1RED,OS)
#pragma linkage(BPX1OPN,OS)
#pragma linkage(BPX1WRT,OS)
#pragma linkage(BPX1REN,OS)
#pragma linkage(BPX1CHR,OS)
#pragma linkage(BPX1CLO,OS)
#pragma linkage(BPX1STA,OS)
#pragma linkage(BPX1UNL,OS)
#pragma linkage(BPX1OPD,OS)
#pragma linkage(BPX1MKD,OS)
#pragma linkage(BPX1RDD,OS)
#pragma linkage(BPX1RMD,OS)
#pragma linkage(BPX1CLD,OS)
#pragma linkage(BPX1UMK,OS)
#pragma linkage(BPX1FCT,OS)

#define BPXRED BPX1RED
#define BPXOPN BPX1OPN
#define BPXWRT BPX1WRT
#define BPXREN BPX1REN
#define BPXCHR BPX1CHR
#define BPXCLO BPX1CLO
#define BPXSTA BPX1STA
#define BPXUNL BPX1UNL
#define BPXOPD BPX1OPD
#define BPXMKD BPX1MKD
#define BPXRDD BPX1RDD
#define BPXRMD BPX1RMD
#define BPXCLD BPX1CLD
#define BPXUMK BPX1UMK
#define BPXFCT BPX1FCT
#endif

#define MAX_ENTRY_BUFFER_SIZE 2550
#define MAX_NUM_ENTRIES       1000

static int fileTrace = FALSE;

static const char* fileTypeString(char fileType) {
  const char* result = "unknown";
  switch(fileType){
  case BPXSTA_FILETYPE_DIRECTORY:
    result = "directory";
    break;
  case BPXSTA_FILETYPE_CHARACTER_SPECIAL:
    result = "character special";
    break;
  case BPXSTA_FILETYPE_REGULAR:
    result = "regular";
    break;
  case BPXSTA_FILETYPE_FIFO:
    result = "fifo";
    break;
  case BPXSTA_FILETYPE_SYMLINK:
    result = "symlink";
    break;
  case BPXSTA_FILETYPE_BLOCK_SPECIAL:
    result = "block special";
    break;
  case BPXSTA_FILETYPE_SOCKET:
    result = "socket";
    break;
  default: break;
  }
  return result;
}

int tmpFileMake(const char *fileName) {
#define TEMP_BUFFER_SIZE 1000

  int returnCode = 0;
  int reasonCode = 0;
  int status = 0;

  char tempBuffer[TEMP_BUFFER_SIZE];
  strcpy(tempBuffer, fileName);
  strcat(tempBuffer, ".tmp");

  status = fileRename(fileName, tempBuffer, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Could not rename file %s: (return = 0x%x, reason = 0x%x)\n", tempBuffer, returnCode, reasonCode);
#endif
    return -1;
  }

  return 0;
}

int tmpFileRecover(const char *fileName) {
#define TEMP_BUFFER_SIZE 1000

  FileInfo info;
  int returnCode = 0;
  int reasonCode = 0;
  int status = 0;

  char tempBuffer[TEMP_BUFFER_SIZE];
  strcpy(tempBuffer, fileName);
  strcat(tempBuffer, ".tmp");

  status = fileRename(tempBuffer, fileName, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Could not rename file %s: (return = 0x%x, reason = 0x%x)\n", tempBuffer, returnCode, reasonCode);
#endif
    return -1;
  }

  return 0;
}

int tmpFileDelete(const char *fileName) {
#define TEMP_BUFFER_SIZE 1000

  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;

  char tempBuffer[TEMP_BUFFER_SIZE];
  strcpy(tempBuffer, fileName);
  strcat(tempBuffer, ".tmp");

  status = fileDelete(tempBuffer, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Could not delete file %s: (return = 0x%x, reason = 0x%x)\n", tempBuffer, returnCode, reasonCode);
#endif
  }

  return 0;
}

void tmpFileCleanup(const char *fileName) {
  int status = 0;

  status = tmpFileRecover(fileName);
  if (status == -1) {
#ifdef DEBUG
    printf("Unable to recover file %s. Find it at %s.tmp\n", fileName, fileName);
#endif
    return;
  }

  status = tmpFileDelete(fileName);
  if (status == -1) {
#ifdef DEBUG
    printf("Unable to delete file %s. Find it at %s.tmp\n", fileName, fileName);
#endif
    return;
  }
}

int tmpDirMake(const char *dirName) {
  #define TEMP_BUFFER_SIZE 1000

  int returnCode = 0;
  int reasonCode = 0;
  int status = 0;

  char tempBuffer[TEMP_BUFFER_SIZE];
  strcpy(tempBuffer, dirName);
  strcat(tempBuffer, ".tmp");

  status = directoryRename(dirName, tempBuffer, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Could not rename directory %s: (return = 0x%x, reason = 0x%x)\n", tempBuffer, returnCode, reasonCode);
#endif
    return -1;
  }

  return 0;
}

int tmpDirRecover(const char *dirName) {
  #define TEMP_BUFFER_SIZE 1000

  FileInfo info;
  int returnCode = 0;
  int reasonCode = 0;
  int status = 0;

  char tempBuffer[TEMP_BUFFER_SIZE];
  strcpy(tempBuffer, dirName);
  strcat(tempBuffer, ".tmp");

  status = directoryRename(tempBuffer, dirName, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Could not rename directory %s: (return = 0x%x, reason = 0x%x)\n", tempBuffer, returnCode, reasonCode);
#endif
    return -1;
  }

  return 0;
}

int tmpDirDelete(const char *dirName) {
  #define TEMP_BUFFER_SIZE 1000

  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;

  char tempBuffer[TEMP_BUFFER_SIZE];
  strcpy(tempBuffer, dirName);
  strcat(tempBuffer, ".tmp");

  int retCode;
  status = directoryDeleteRecursive(tempBuffer, &retCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Could not delete directory %s\n", tempBuffer);
#endif
  }

  return 0;
}

void tmpDirCleanup(const char *dirName) {
  int status = 0;

  status = tmpDirRecover(dirName);
  if (status == -1) {
#ifdef DEBUG
  printf("Unable to recover directory %s. Find it at %s.tmp\n", dirName, dirName);
#endif
  return;
  }

  status = tmpDirDelete(dirName);
  if (status == -1) {
#ifdef DEBUG
  printf("Unable to delete directory %s. Find it at %s.tmp\n", dirName, dirName);
#endif
  return;
  }
}

int setFileTrace(int toWhat) {
  int was = fileTrace;
#ifndef METTLE
  fileTrace = toWhat;
#endif
  return was;
}

UnixFile *fileOpen(const char *filename, int options, int mode,
                  int bufferSize, int *returnCode, int *reasonCode) {
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int status;
  int len = strlen(filename);

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXOPN(&len,
         filename,
         &options,
         &mode,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if (returnValue < 0) {
#ifdef METTLE
      printf("BPXOPN (%s, 0%o, 0%o) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             filename, options, mode, returnValue, *returnCode, *reasonCode);
#else
      printf("BPXOPN (%s, 0%o, 0%o) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             filename, options, mode, returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      printf("BPXOPN (%s, 0%o, 0%o) OK: returnValue: %d\n",
             filename, options, mode, returnValue);
    }
  }

  if (returnValue < 0) {
    return NULL;
  }

  UnixFile *file = (UnixFile*)safeMalloc31(sizeof(UnixFile),"Unix File");
  memset(file,0,sizeof(UnixFile));
  file->fd = returnValue;
  file->pathname = safeMalloc31(strlen(filename)+1,"Unix File Name");
  strcpy(file->pathname, filename);
  file->isDirectory = FALSE;
  if (bufferSize > 0) {
    if (fileTrace) {
      printf("initializing buffered reading, buffer size = %d\n", bufferSize);
    }
    file->buffer = safeMalloc31(bufferSize,"Unix File Buffer");
    file->bufferSize = bufferSize;
    file->bufferPos = bufferSize;
    file->bufferFill = bufferSize;
  }
  return file;
}

int fileRead(UnixFile *file, char *buffer, int desiredBytes,
             int *returnCode, int *reasonCode) {
  if (file == NULL) {
    if (fileTrace) {
      printf("File is null\n");
    }
#ifdef METTLE
    *returnCode = -1;
    *reasonCode = 0;
#else
    *returnCode = EINVAL;
    *reasonCode = 0;
#endif
    return -1;
  }

  int fd = file->fd;
  int returnValue = 0;
  int *reasonCodePtr;
  int zero = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  if (fileTrace) {
    printf("Before read: fd: %d, buffer: %x\n", fd, buffer);
  }

  BPXRED(&fd,
         &buffer,
         &zero,
         &desiredBytes,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if (returnValue < 0) {
  #ifdef METTLE
        printf("BPXRED FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
               returnValue, *returnCode, *reasonCode);
  #else
        printf("BPXRED FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
               returnValue, *returnCode, *reasonCode, strerror(*returnCode));
  #endif
    } else {
      printf("BPXRED OK: Read %d bytes\n", returnValue);
    }
  }

  if (returnValue < 0) {
    returnValue = -1;
  } else{
    *returnCode = 0;
    *reasonCode = 0;
    if (returnValue < desiredBytes){
      file->eofKnown = TRUE;
    }
  }
  return returnValue;
}

int fileWrite(UnixFile *file, const char *buffer, int desiredBytes,
              int *returnCode, int *reasonCode) {
  if (file == NULL) {
    if (fileTrace) {
      printf("File is null\n");
    }
#ifdef METTLE
    *returnCode = -1;
    *reasonCode = 0;
#else
    *returnCode = EINVAL;
    *reasonCode = 0;
#endif
    return -1;
  }

  int fd = file->fd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int) reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXWRT(&fd,
         &buffer,
         &zero,
         &desiredBytes,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if (returnValue < 0) {
#ifdef METTLE
      printf("BPXWRT (fd: %d, file: %s, desired bytes: %d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             fd, (file->pathname ? file->pathname : "unknown"), desiredBytes, returnValue, *returnCode, *reasonCode);
#else
      printf("BPXWRT (fd: %d, file: %s, desired bytes: %d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             fd, (file->pathname ? file->pathname : "unknown"), desiredBytes, returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      printf("BPXWRT OK: Wrote %d bytes\n", returnValue);
    }
  }

  if (returnValue < 0) {
    returnValue = -1;
  } else {
    *returnCode = 0;
    *reasonCode = 0;
  }
  return returnValue;
}

int fileGetChar(UnixFile *file, int *returnCode, int *reasonCode) {
#ifdef DEBUG
  printf("bufferSize = %d\n",file->bufferSize);
#endif
  if (file->bufferSize == 0){
#ifdef DEBUG
    printf("fgetc 1\n");
#endif
    *returnCode = 8;
    *reasonCode = 0xBFF;
    return -1;
  } else if (file->bufferPos < file->bufferFill){
#ifdef DEBUG
    printf("fgetc 2\n");
#endif
    return (int)(file->buffer[file->bufferPos++]);
  } else if (file->eofKnown){
#ifdef DEBUG
    printf("fgetc 3\n");
#endif
    return -1;
  } else{
    /* go after next buffer and pray */
#ifdef DEBUG
    printf("fgetc 4\n");
#endif
    int bytesRead = fileRead(file,file->buffer,file->bufferSize,returnCode,reasonCode);
    if (bytesRead >= 0) {
#ifdef DEBUG
      printf("got more data, bytesRead=%d wanted=%d\n",bytesRead,file->bufferSize);
      dumpbuffer(file->buffer,file->bufferSize);
#endif
      if (bytesRead < file->bufferSize) { /* out of data after this read */
        file->eofKnown = TRUE;
      }
      file->bufferFill = bytesRead;
      file->bufferPos = 1;
      return file->buffer[0];
    } else{
      return -1;
    }
  }
}

int fileClose(UnixFile *file, int *returnCode, int *reasonCode) {
  if (file == NULL) {
    if (fileTrace) {
      printf("File is null\n");
    }
#ifdef METTLE
    *returnCode = -1;
    *reasonCode = 0;
#else
    *returnCode = EINVAL;
    *reasonCode = 0;
#endif
    return -1;
  }

  int fd = file->fd;
  int returnValue = 0;

  if (file->pathname != NULL) {
    safeFree31(file->pathname, strlen(file->pathname) + 1);
    file->pathname = NULL;
  }
  if (file->buffer != NULL){
    safeFree31(file->buffer,file->bufferSize);
    file->buffer = NULL;
  }
  safeFree31((void*)file,sizeof(UnixFile));
  file = NULL;

  int *reasonCodePtr;
  int status;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  status = BPXCLO(&fd,
                  &returnValue,
                  returnCode,
                  reasonCodePtr);

  if (fileTrace) {
    if (returnValue != 0) {
#ifdef METTLE
      printf("BPXCLO FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             returnValue, *returnCode, *reasonCode);
#else
      printf("BPXCLO FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      printf("BPXCLO OK: returnValue: %d\n", returnValue);
    }
  }

  if (returnValue != 0) {
    returnValue = -1;
  }
  else{
    *returnCode = 0;
    *reasonCode = 0;
  }
  return returnValue;
}

int fileChangeTag(const char *fileName, int *returnCode, int *reasonCode, int ccsid) {
  int nameLength = strlen(fileName);
  int attributeLength = sizeof(BPXYATT);
  int *reasonCodePtr;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  FileAttributes attributes = {0};
  memcpy(attributes.eyecatcher, "ATT ", 4);
  attributes.version = 3;
  attributes.flag3 = ATTCHARSETIDCHG;

  attributes.fileTagCCSID = ccsid;

  if (ccsid != CCSID_BINARY) {
    attributes.fileTagFlags = FILE_PURE_TEXT;
  }

  BPXCHR(nameLength,
         fileName,
         attributeLength,
         &attributes,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if (returnValue != 0) {
#ifdef METTLE
      printf("BPXCHR FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             returnValue, *returnCode, *reasonCode);
#else
      printf("BPXCHR FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      printf("BPXCHR (%s) OK: returnValue: %d\n\n", fileName, returnValue);
    }
  }

  if (returnValue != 0) {
    returnValue = -1;
  }
  else{
    *returnCode = 0;
    *reasonCode = 0;
  }
  return returnValue;
}

int fileCopy(const char *existingFile, const char *newFile, int forceCopy){
#define FILE_BUFFER_SIZE 4000

  FileInfo info;
  int returnCode = 0;
  int reasonCode = 0;
  int status = 0;
  int bytesRead = 0;
  int fileExists = 0;
  char buffer[FILE_BUFFER_SIZE];

  status = fileInfo(existingFile, &info, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("File %s most likely does not exist, please check the provided file name: (return = 0x%x, reason = 0x%x)\n", existingFile, returnCode, reasonCode);
#endif
    return -1;
  }

  int ccsid = fileInfoCCSID(&info);

  status = fileInfo(newFile, &info, &returnCode, &reasonCode);
  if (status == 0) {
    if (!forceCopy) {
#ifdef DEBUG
    printf("Copying has stopped because the file already exists and the force flag is off", newFile, returnCode, reasonCode);
#endif
      return -1;
    }
    fileExists = 1;
  }

  if (fileExists) {
    status = tmpFileMake(newFile);
    if (status == -1) {
      return -1;
    }
  }

  UnixFile *fileCheckFrom = fileOpen(existingFile, FILE_OPTION_READ_ONLY, 0, 0, &returnCode, &reasonCode);
  if (fileCheckFrom == NULL) {
#ifdef DEBUG
    printf("Failed to open file %s: (return = 0x%x, reason = 0x%x)\n", existingFile, returnCode, reasonCode);
#endif
    if (fileExists) {
      tmpFileCleanup(newFile);
    }
    return -1;
  }

  UnixFile *fileCheckTo = fileOpen(newFile,
                                  FILE_OPTION_WRITE_ONLY | FILE_OPTION_CREATE,
                                  0700,
                                  0,
                                  &returnCode,
                                  &reasonCode);

  if (fileCheckTo == NULL) {
#ifdef DEBUG
    printf("Failed to open file %s: (return = 0x%x, reason = 0x%x)\n", newFile, returnCode, reasonCode);
#endif
    if (fileExists) {
      tmpFileCleanup(newFile);
    }
    return -1;
  }

  /* If the file is not untagged.
   */
  if (ccsid != 0) {
    int pureText;
    status = fileChangeTag(newFile, &returnCode, &reasonCode, ccsid);
    if (status == -1) {
#ifdef DEBUG
      printf("Failed to change file tag for %s: (return = 0x%x, reason = 0x%x)\n", newFile, returnCode, reasonCode);
#endif
      if (fileExists) {
        tmpFileCleanup(newFile);
      }
      else {
        fileDelete(newFile, &returnCode, &reasonCode);
      }
    return -1;
    }
  }

  /* Disable automatic conversion to prevent any wacky
   * problems that may arise from auto convert.
   */
  status = fileDisableConversion(fileCheckTo, &returnCode, &reasonCode);
  if (status != 0) {
    printf("Failed to disable automatic conversion. Unexpected results may occur.\n");
  }
  status = fileDisableConversion(fileCheckFrom, &returnCode, &reasonCode);
  if (status != 0) {
    printf("Failed to disable automatic conversion. Unexpected results may occur.\n");
  }

  while (bytesRead = fileRead(fileCheckFrom, buffer, FILE_BUFFER_SIZE, &returnCode, &reasonCode)){
    if (bytesRead == -1) {
#ifdef DEBUG
      printf("Failed to read file %s: (return = 0x%x, reason = 0x%x)\n", existingFile, returnCode, reasonCode);
#endif
      if (fileExists) {
        tmpFileCleanup(newFile);
      }
      else {
        fileDelete(newFile, &returnCode, &reasonCode);
      }
      return -1;
    }
    status = fileWrite(fileCheckTo, buffer, bytesRead, &returnCode, &reasonCode);
    if (status == -1) {
#ifdef DEBUG
      printf("Failed to write to file %s: (return = 0x%x, reason = 0x%x)\n", newFile, returnCode, reasonCode);
#endif
      if (fileExists) {
        tmpFileCleanup(newFile);
      }
      else {
        fileDelete(newFile, &returnCode, &reasonCode);
      }
      return -1;
    }
    memset(buffer, 0, FILE_BUFFER_SIZE);
  }

  status = fileClose(fileCheckTo, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Failed to close file %s: (return = 0x%x, reason = 0x%x)\n", newFile, returnCode, reasonCode);
#endif
    if (fileExists) {
      fileClose(fileCheckFrom, &returnCode, &reasonCode);
      tmpFileCleanup(newFile);
    }
    else {
      fileDelete(newFile, &returnCode, &reasonCode);
    }
    return -1;
  }

  status = fileClose(fileCheckFrom, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Failed to close file %s: (return = 0x%x, reason = 0x%x)\n", existingFile, returnCode, reasonCode);
#endif
    if (fileExists) {
      tmpFileCleanup(newFile);
    }
    else {
      fileDelete(newFile, &returnCode, &reasonCode);
    }
    return -1;
  }

  tmpFileDelete(newFile);
  return 0;
}

int fileRename(const char *oldFileName, const char *newFileName, int *returnCode, int *reasonCode){
  int returnValue = 0;
  int *reasonCodePtr;
  int status;
  int oldLen = strlen(oldFileName);
  int newLen = strlen(newFileName);

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXREN(&oldLen,
         oldFileName,
         &newLen,
         newFileName,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if(returnValue != 0) {
#ifdef METTLE
      printf("BPXREN FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             returnValue, *returnCode, *reasonCode);
#else
      printf("BPXREN FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      printf("BPXREN (%s) OK: returnVal: %d\n", oldFileName, returnValue);
    }
  }

  if (returnValue != 0) {
    returnValue = -1;
  }
  else{
    *returnCode = 0;
    *reasonCode = 0;
  }
  return returnValue;
}

int fileDelete(const char *fileName, int *returnCode, int *reasonCode){
  int returnValue = 0;
  int *reasonCodePtr;
  int status;
  int len = strlen(fileName);

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXUNL(&len,
         fileName,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if(returnValue != 0) {
#ifdef METTLE
      printf("BPXUNL (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             fileName, returnValue, *returnCode, *reasonCode);
#else
      printf("BPXUNL (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             fileName, returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      printf("BPXUNL (%s) OK: returnVal: %d\n", fileName, returnValue);
    }
  }

  if (returnValue != 0) {
    returnValue = -1;
  }
  else{
    *returnCode = 0;
    *reasonCode = 0;
  }
  return returnValue;
}

int fileInfo(const char *filename, BPXYSTAT *stats, int *returnCode, int *reasonCode) {
  int nameLength = strlen(filename);
  int statsLength = sizeof(BPXYSTAT);
  int *reasonCodePtr;
  int returnValue = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXSTA(&nameLength,
         filename,
         &statsLength,
         stats,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if(returnValue != 0) {
#ifdef METTLE
      printf("BPXSTA (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             filename, returnValue, *returnCode, *reasonCode);
#else
      printf("BPXSTA (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             filename, returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      printf("BPXSTA (%s) OK: returnVal: %d, type: %s\n", filename, returnValue, fileTypeString(stats->fileType));
    }
  }

  if (returnValue != 0) {
    returnValue = -1;
  }
  else{
    *returnCode = 0;
    *reasonCode = 0;
  }
  return returnValue;
}

int fileInfoIsDirectory(const FileInfo *info) {
  return (info->fileType == BPXSTA_FILETYPE_DIRECTORY ? TRUE: FALSE);
}

int fileInfoCCSID(const FileInfo *info) {
  return (int)(info->ccsid);
}

int64 fileInfoSize(const FileInfo *info) {
  return info->fileSize;
}

int fileInfoUnixCreationTime(const FileInfo *info) {
  return info->creationTime;   /* unix time */
}

int fileEOF(const UnixFile *file) {
  return ((file->bufferPos >= file->bufferFill) && file->eofKnown);
}

int fileUnixMode(const FileInfo *info) {
  return ((info->flags2 & 0x0f) << 8) | (info->flags3 & 0xff);
}

int fileGetINode(const FileInfo *info) {
  return info->inode;
}

int fileGetDeviceID(const FileInfo *info) {
  return info->deviceID;
}

UnixFile *directoryOpen(const char *directoryName, int *returnCode, int *reasonCode) {
  int nameLength = strlen(directoryName);
  int *reasonCodePtr;
  int fd; /* of directory */
  *returnCode = *reasonCode = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXOPD(&nameLength,
         directoryName,
         &fd,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if (fd < 0) {
#ifdef METTLE
      printf("BPXOPD (%s) FAILED: fd: %d, returnCode: %d, reasonCode: 0x%08x\n",
             directoryName, fd, *returnCode, *reasonCode);
#else
      printf("BPXOPD (%s) FAILED: fd: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             directoryName, fd, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      printf("BPXOPD (%s) OK: fd: %d\n",
             directoryName, fd);
    }
  }

  if (fd < 0) {
    return NULL;
  }

  UnixFile *file = (UnixFile*)safeMalloc31(sizeof(UnixFile),"Unix File");

  memset(file,0,sizeof(UnixFile));
  file->fd = fd;
  file->pathname = safeMalloc31(strlen(directoryName)+1,"Unix File Name");
  strcpy(file->pathname, directoryName);
  file->isDirectory = TRUE;
  return file;
}

int directoryRead(UnixFile *directory, char *entryBuffer, int entryBufferLength, int *returnCode, int *reasonCode) {
  if (directory == NULL) {
    if (fileTrace) {
      printf("Directory is null\n");
    }
#ifdef METTLE
    *returnCode = -1;
    *reasonCode = 0;
#else
    *returnCode = EINVAL;
    *reasonCode = 0;
#endif
    return -1;
  }

  int fd = directory->fd;
  int *reasonCodePtr;
  int numberOfEntriesRead;
  int zero = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXRDD(&fd,
         &entryBuffer,
         &zero,                 /* buffer ALET */
         &entryBufferLength,
         &numberOfEntriesRead,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if (numberOfEntriesRead < 0) {
#ifdef METTLE
      printf("BPXRDD (fd=%d, path=%s) %s: %d entries read, "
             "returnCode: %d, reasonCode: 0x%08x\n",
             directory->fd, (directory->pathname ? directory->pathname : "unknown"),
             (numberOfEntriesRead == 0 ? "PROBABLE EOF" : "FAILED"), numberOfEntriesRead,
             *returnCode, *reasonCode);
#else
      printf("BPXRDD (fd=%d, path=%s) %s: %d entries read, "
             "returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             directory->fd, (directory->pathname ? directory->pathname : "unknown"),
             (numberOfEntriesRead == 0 ? "PROBABLE EOF" : "FAILED"), numberOfEntriesRead,
             returnCode, reasonCode, strerror(*returnCode));

#endif
    }
    else {
      printf("BPXRDD (fd=%d, path=%s) OK: %d entries read\n",
             directory->fd, (directory->pathname ? directory->pathname : "unknown"),
             numberOfEntriesRead);
    }
  }

  if (numberOfEntriesRead >= 0) {
    *returnCode = 0;
    *reasonCode = 0;
  }

  return numberOfEntriesRead;
}

int directoryClose(UnixFile *directory, int *returnCode, int *reasonCode){
  if (directory == NULL) {
    if (fileTrace) {
      printf("Directory is null\n");
    }
#ifdef METTLE
    *returnCode = -1;
    *reasonCode = 0;
#else
    *returnCode = EINVAL;
    *reasonCode = 0;
#endif
    return -1;
  }

  int fd = directory->fd;
  int *reasonCodePtr;
  int returnValue;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXCLD(&fd,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if (returnValue < 0) {
#ifdef METTLE
      printf("BPXCLD (fd=%d, path=%s) FAILED: returnValue: %d, "
             "returnCode: %d, reasonCode: 0x%08x\n",
             directory->fd, (directory->pathname ? directory->pathname : "unknown"), returnValue,
             returnValue, *returnCode, *reasonCode);
#else
      printf("BPXCLD (fd=%d, path=%s) FAILED: returnValue: %d, "
             "returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             directory->fd, (directory->pathname ? directory->pathname : "unknown"), returnValue,
             returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      printf("BPXCLD (fd=%d, path=%s) OK: returnValue: %d\n",
             directory->fd, (directory->pathname ? directory->pathname : "unknown"),
             returnValue);
    }
  }

  if (directory->pathname != NULL) {
    safeFree31(directory->pathname, strlen(directory->pathname) + 1);
    directory->pathname = NULL;
  }
  safeFree31((char *)directory, sizeof(UnixFile));
  directory = NULL;

  if (returnValue != 0) {
    returnValue = -1;
  }
  else{
    *returnCode = 0;
    *reasonCode = 0;
  }
  return returnValue;
}

int directoryMake(const char *pathName, int mode, int *returnCode, int *reasonCode) {
  int pathLength = strlen(pathName);
  int *reasonCodePtr;
  int returnValue;
  *returnCode = *reasonCode = 0;

  #ifndef _LP64
    reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
  #else
    reasonCodePtr = reasonCode;
  #endif

  BPXMKD(&pathLength,
         pathName,
         &mode,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if (returnValue != 0) {
#ifdef METTLE
      printf("BPXMKD (%s, 0%o) FAILED: returnVal: %d, "
             "returnCode: %d, reasonCode: 0x%08x\n",
             pathName, mode, returnValue,
             *returnCode, *reasonCode);
#else
      printf("BPXMKD (%s, 0%o) FAILED: returnVal: %d, "
             "returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             pathName, mode, returnValue,
             *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      printf("BPXMKD (%s,) OK: returnValue: %d\n",
             pathName, returnValue);
    }
  }

  if (returnValue != 0) {
    returnValue = -1;
  }
  else{
    *returnCode = 0;
    *reasonCode = 0;
  }
  return returnValue;
}

int directoryDelete(const char *pathName, int *returnCode, int *reasonCode){
  int pathLength = strlen(pathName);
  int *reasonCodePtr;
  int returnValue;
  *returnCode = *reasonCode = 0;

  #ifndef _LP64
    reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
  #else
    reasonCodePtr = reasonCode;
  #endif

  BPXRMD(&pathLength,
         pathName,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if (returnValue != 0) {
#ifdef METTLE
      printf("BPXMKD (%s) FAILED: returnVal: %d\n",
             pathName, returnValue);
#else
      printf("BPXMKD (%s) FAILED: returnVal: %d, "
             "returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             pathName, returnValue,
             *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      printf("BPXRMD (%s) OK: returnValue: %d\n", pathName, returnValue);
    }
  }

  if (returnValue != 0) {
    returnValue = -1;
  }
  else{
    *returnCode = 0;
    *reasonCode = 0;
  }
  return returnValue;
}

static int getValidDirectoryEntries(int entries, char *entryBuffer, const char **entryArray) {
  int entryOffset = 0;
  int validEntries = 0;
  for (int i = 0; i < entries; i++) {
    const DirectoryEntry *de = (const DirectoryEntry *) (entryBuffer + entryOffset);
    if (strcmp(".", de->name) && strcmp("..", de->name) && strcmp("", de->name)) {
      entryArray[validEntries] = de->name;
      validEntries++;
    }
    entryOffset += de->entryLength;
  }  
  return validEntries;
}
  
int directoryDeleteRecursive(const char *pathName, int *retCode){
  FileInfo info = {0};
  int returnCode = 0;
  int reasonCode = 0;
  int status = 0;
  int bytesRead = 0;

  status = fileInfo(pathName, &info, &returnCode, &reasonCode);
  if (status == -1){
    *retCode = returnCode;
    return -1;
  }

  UnixFile *dir = directoryOpen(pathName, &returnCode, &reasonCode);
  if (dir == NULL) {
    *retCode = returnCode;
    return -1;
  }

  char entryBuffer[MAX_ENTRY_BUFFER_SIZE] = {0};
  int entries = directoryRead(dir, entryBuffer, sizeof(entryBuffer), &returnCode, &reasonCode);
  if (entries == -1) {
    *retCode = returnCode;
    return -1;
  }
  
  const char *entryArray[MAX_NUM_ENTRIES] = {0};
  int validEntries = getValidDirectoryEntries(entries, entryBuffer, entryArray);
  if (validEntries <  1) {
    status = directoryDelete(pathName, &returnCode, &reasonCode);
    if (status == -1) {
      *retCode = returnCode;
      return -1;
    }
    return 0;
  }

  for (int i = 0; i < validEntries; i++) {
    char pathBuffer[USS_MAX_PATH_LENGTH + 1] = {0};
    snprintf(pathBuffer, sizeof(pathBuffer), "%s/%s", pathName, entryArray[i]);

    status = fileInfo(pathBuffer, &info, &returnCode, &reasonCode);
    if (status == -1){
      *retCode = returnCode;
      return -1;
    }

    if (fileInfoIsDirectory(&info)) {
      status = directoryDeleteRecursive(pathBuffer, retCode);
      if (status == -1) {
        return -1;
      }
    }
    else {
      status = fileDelete(pathBuffer, &returnCode, &reasonCode);
      if (status == -1) {
        *retCode = returnCode;
        return -1;
      }
    }
  }

  directoryDeleteRecursive(pathName, retCode);
  if (status == -1) {
    return -1;
  }

  return 0;
}

int directoryCopy(const char *existingPathName, const char *newPathName, int forceCopy){
#define ENTRY_BUFFER_SIZE 1000
#define PATH_BUFFER_SIZE 1000

  FileInfo info;
  int returnCode = 0;
  int reasonCode = 0;
  int status = 0;
  int bytesRead = 0;
  int fileExists = 0;
  char entryBuffer[ENTRY_BUFFER_SIZE];

  status = fileInfo(existingPathName, &info, &returnCode, &reasonCode);
  if (status == -1){
#ifdef DEBUG
    printf("Directory %s most likely does not exist, please check the provided file name: (return = 0x%x, reason = 0x%x)\n", existingPathName, returnCode, reasonCode);
#endif
    return -1;
  }

  status = fileInfo(newPathName, &info, &returnCode, &reasonCode);
  if (status == 0){
    if (!forceCopy) {
#ifdef DEBUG
      printf("Copying has stopped because the directory already exists and the force flag is off\n");
#endif
      return -1;
    }
    fileExists = 1;
  }

  if (fileExists) {
    status = tmpDirMake(newPathName);
    if (status == -1) {
      return -1;
    }
  }

  UnixFile *dirCheckFrom = directoryOpen(existingPathName, &returnCode, &reasonCode);
  if (dirCheckFrom == NULL) {
#ifdef DEBUG
    printf("Could not open directory %s: (return = 0x%x, reason = 0x%x)\n", existingPathName, returnCode, reasonCode);
#endif
    if (fileExists) {
      tmpDirCleanup(newPathName);
    }
    return -1;
  }

  int entries = directoryRead(dirCheckFrom, entryBuffer, ENTRY_BUFFER_SIZE, &returnCode, &reasonCode);
  if (entries < 0) {
#ifdef DEBUG
    printf("Could not read directory %s: (return = 0x%x, reason = 0x%x)\n", existingPathName, returnCode, reasonCode);
#endif
    if (fileExists) {
      tmpDirCleanup(newPathName);
    }
    return -1;
  }

  status = directoryMake(newPathName,
                        BPXOPN_MODE_USER_WRITE_PERMISSION | BPXOPN_MODE_USER_READ_PERMISSION | BPXOPN_MODE_USER_SEARCH_EXEC_PERMISSION,
                        &returnCode,
                        &reasonCode);

  if (status == -1) {
#ifdef DEBUG
    printf("Could not make directory %s: (return = 0x%x, reason = 0x%x)\n", newPathName, returnCode, reasonCode);
#endif
    if (fileExists) {
      tmpDirCleanup(newPathName);
    }
    return -1;
  }

  UnixFile *dirCheckTo = directoryOpen(newPathName, &returnCode, &reasonCode);
  if (dirCheckTo == NULL) {
#ifdef DEBUG
    printf("Could not open directory %s: (return = 0x%x, reason = 0x%x)\n", newPathName, returnCode, reasonCode);
#endif
    if (fileExists) {
      tmpDirCleanup(newPathName);
    }
    return -1;
  }

  /* The last entry is always a null. Disregard it to prevent issues. */
  int entryOffset = 0;
  const char *entryArray[entries - 1];
  for (int i = 0; i < entries - 1; i++) {
    const DirectoryEntry *de = (const DirectoryEntry*) (entryBuffer + entryOffset);
    entryArray[i] = de->name;
#ifdef DEBUG
    printf("%s in %s\n", entryArray[i], existingPathName);
#endif
    entryOffset += de->entryLength;
  }

  /* The first two entries of directoryRead are the current directory and previous directory */
  char newPathBuffer[PATH_BUFFER_SIZE];
  char oldPathBuffer[PATH_BUFFER_SIZE];
  for (int i = 0; i < entries - 1; i++) {
    if ((!strcmp(entryArray[i], ".")) || (!strcmp(entryArray[i], ".."))) {
      continue;
    }
    else {
      strcpy(oldPathBuffer, existingPathName);
      strcat(oldPathBuffer, "/");
      strcat(oldPathBuffer, entryArray[i]);
      strcpy(newPathBuffer, newPathName);
      strcat(newPathBuffer, "/");
      strcat(newPathBuffer, entryArray[i]);

      status = fileInfo(oldPathBuffer, &info, &returnCode, &reasonCode);
      if (status == -1) {
#ifdef DEBUG
        printf("Directory / File %s most likely does not exist, please check the provided file name: (return = 0x%x, reason = 0x%x)\n", oldPathBuffer, returnCode, reasonCode);
#endif
        if (fileExists) {
          tmpDirCleanup(newPathName);
        }
        return -1;
      }

      if (fileInfoIsDirectory(&info)) {
#ifdef DEBUG
        printf("Copying directory.\n");
        printf("from: %s\n", oldPathBuffer);
        printf("to: %s\n", newPathBuffer);
#endif
        status = directoryCopy(oldPathBuffer, newPathBuffer, forceCopy);
        if (status == -1) {
          if (fileExists) {
            tmpDirCleanup(newPathName);
          }
          return -1;
        }
      }
      else {
#ifdef DEBUG
        printf("Copying file.\n");
        printf("from: %s\n", oldPathBuffer);
        printf("to: %s\n", newPathBuffer);
#endif
        status = fileCopy(oldPathBuffer, newPathBuffer, forceCopy);
        if (status == -1) {
          if (fileExists) {
            tmpDirCleanup(newPathName);
          }
          return -1;
        }
      }
      memset(newPathBuffer, 0, PATH_BUFFER_SIZE);
      memset(oldPathBuffer, 0, PATH_BUFFER_SIZE);
    }
  }

  status = directoryClose(dirCheckTo, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Failed to close file %s: (return = 0x%x, reason = 0x%x)\n", newPathName, returnCode, reasonCode);
#endif
    if (fileExists) {
      fileClose(dirCheckFrom, &returnCode, &reasonCode);
      tmpFileCleanup(newPathName);
    }
    return -1;
  }

  status = directoryClose(dirCheckFrom, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Failed to close file %s: (return = 0x%x, reason = 0x%x)\n", existingPathName, returnCode, reasonCode);
#endif
    if (fileExists) {
      tmpDirCleanup(newPathName);
    }
    return -1;
  }

  tmpDirDelete(newPathName);
  return 0;
}

int directoryRename(const char *oldDirname, const char *newDirName, int *returnCode, int *reasonCode){
  int returnValue = fileRename(oldDirname, newDirName, returnCode, reasonCode);

  return returnValue;
}

int setUmask(int mask) {
  int returnValue;

  BPXUMK(mask,
         &returnValue);

  if (fileTrace){
    printf("setUmask(0%o) return 0%o\n", mask, returnValue);
  }
  return returnValue;
}

int getUmask() {
  printf("UMASK: This is a dirty hack.\n");

  int previous = setUmask(0000);
  setUmask(previous);

  return previous;
}

int fileDisableConversion(UnixFile *file, int *returnCode, int *reasonCode) {
  int *reasonCodePtr;
  int returnValue;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  int action = F_CONTROL_CVT;
  F_CVT fctl;
  fctl.cmd = F_CVT_SETCVTOFF;
  fctl.pccsid = 1047;
  fctl.fccsid = 1047;

  F_CVT *fnctl_ptr = &fctl;

  BPXFCT(&file->fd,
         &action,
         &fnctl_ptr,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (returnValue == 0) {
    *returnCode = 0;
    *reasonCode = 0;
  }

  return returnValue;
}

int fileSetLock(UnixFile *file, int *returnCode, int *reasonCode) {
  int *reasonCodePtr;
  int returnValue;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  int action = F_SET_LOCK;
  F_LOCK flockdata;
  flockdata.l_type = F_WRITE_LOCK;
  flockdata.l_whence = F_SEEK_SET;
  flockdata.l_start = 0;
  flockdata.l_len = F_WHENCE_TO_END;
  flockdata.l_pid = 0;

  F_LOCK *fnctl_ptr = &flockdata;

  BPXFCT(&file->fd,
         &action,
         &fnctl_ptr,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (returnValue == 0) {
    *returnCode = 0;
    *reasonCode = 0;
  }

  return returnValue;
}

int fileGetLock(UnixFile *file, int *returnCode, int *reasonCode, int *isLocked) {
  int *reasonCodePtr;
  int returnValue;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  int action = F_GET_LOCK;
  F_LOCK flockdata;
  flockdata.l_type = F_WRITE_LOCK;
  flockdata.l_whence = F_SEEK_SET;
  flockdata.l_start = 0;
  flockdata.l_len = F_WHENCE_TO_END;
  flockdata.l_pid = 0;

  F_LOCK *fnctl_ptr = &flockdata;

  BPXFCT(&file->fd,
         &action,
         &fnctl_ptr,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (returnValue == 0) {
    *returnCode = 0;
    *reasonCode = 0;
  }

  if (fnctl_ptr->l_pid != 0 || fnctl_ptr->l_type != F_UNLOCK) {
    *isLocked = TRUE;
  }

  return returnValue;
}

int fileUnlock(UnixFile *file, int *returnCode, int *reasonCode) {
  int *reasonCodePtr;
  int returnValue;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  int action = F_SET_LOCK;
  F_LOCK flockdata;
  flockdata.l_type = F_UNLOCK;
  flockdata.l_whence = F_SEEK_SET;
  flockdata.l_start = 0;
  flockdata.l_len = F_WHENCE_TO_END;
  flockdata.l_pid = 0;

  F_LOCK *fnctl_ptr = &flockdata;

  BPXFCT(&file->fd,
         &action,
         &fnctl_ptr,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (returnValue == 0) {
    *returnCode = 0;
    *reasonCode = 0;
  }

  return returnValue;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

