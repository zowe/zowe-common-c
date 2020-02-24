
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
#include "logging.h"

#ifdef _LP64
#pragma linkage(BPX4RED,OS)
#pragma linkage(BPX4OPN,OS)
#pragma linkage(BPX4WRT,OS)
#pragma linkage(BPX4REN,OS)
#pragma linkage(BPX4CHR,OS)
#pragma linkage(BPX4CHM,OS)
#pragma linkage(BPX4CLO,OS)
#pragma linkage(BPX4LCO,OS)
#pragma linkage(BPX4STA,OS)
#pragma linkage(BPX4UNL,OS)
#pragma linkage(BPX4OPD,OS)
#pragma linkage(BPX4MKD,OS)
#pragma linkage(BPX4RDD,OS)
#pragma linkage(BPX4RMD,OS)
#pragma linkage(BPX4CLD,OS)
#pragma linkage(BPX4UMK,OS)
#pragma linkage(BPX4FCT,OS)
#pragma linkage(BPX4LST,OS)
#pragma linkage(BPX4GGN,OS)
#pragma linkage(BPX4GPN,OS)

#define BPXRED BPX4RED
#define BPXOPN BPX4OPN
#define BPXWRT BPX4WRT
#define BPXREN BPX4REN
#define BPXCHR BPX4CHR
#define BPXCHM BPX4CHM
#define BPXCLO BPX4CLO
#define BPXLCO BPX4LCO
#define BPXSTA BPX4STA
#define BPXUNL BPX4UNL
#define BPXOPD BPX4OPD
#define BPXMKD BPX4MKD
#define BPXRDD BPX4RDD
#define BPXRMD BPX4RMD
#define BPXCLD BPX4CLD
#define BPXUMK BPX4UMK
#define BPXFCT BPX4FCT
#define BPXLST BPX4LST
#define BPXGGN BPX4GGN
#define BPXGPN BPX4GPN

#else

#pragma linkage(BPX1RED,OS)
#pragma linkage(BPX1OPN,OS)
#pragma linkage(BPX1WRT,OS)
#pragma linkage(BPX1REN,OS)
#pragma linkage(BPX1CHR,OS)
#pragma linkage(BPX1CHM,OS)
#pragma linkage(BPX1CLO,OS)
#pragma linkage(BPX1LCO,OS)
#pragma linkage(BPX1STA,OS)
#pragma linkage(BPX1UNL,OS)
#pragma linkage(BPX1OPD,OS)
#pragma linkage(BPX1MKD,OS)
#pragma linkage(BPX1RDD,OS)
#pragma linkage(BPX1RMD,OS)
#pragma linkage(BPX1CLD,OS)
#pragma linkage(BPX1UMK,OS)
#pragma linkage(BPX1FCT,OS)
#pragma linkage(BPX1LST,OS)
#pragma linkage(BPX1GGN,OS)
#pragma linkage(BPX1GPN,OS)

#define BPXRED BPX1RED
#define BPXOPN BPX1OPN
#define BPXWRT BPX1WRT
#define BPXREN BPX1REN
#define BPXCHR BPX1CHR
#define BPXCHM BPX1CHM
#define BPXCLO BPX1CLO
#define BPXLCO BPX1LCO
#define BPXSTA BPX1STA
#define BPXUNL BPX1UNL
#define BPXOPD BPX1OPD
#define BPXMKD BPX1MKD
#define BPXRDD BPX1RDD
#define BPXRMD BPX1RMD
#define BPXCLD BPX1CLD
#define BPXUMK BPX1UMK
#define BPXFCT BPX1FCT
#define BPXLST BPX1LST
#define BPXGGN BPX1GGN
#define BPXGPN BPX1GPN
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXOPN (%s, 0%o, 0%o) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             filename, options, mode, returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXOPN (%s, 0%o, 0%o) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             filename, options, mode, returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXOPN (%s, 0%o, 0%o) OK: returnValue: %d\n",
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "initializing buffered reading, buffer size = %d\n", bufferSize);
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_WARNING, "fileRead: File is null\n");
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
    zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "Before read: fd: %d, buffer: %x\n", fd, buffer);
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
        zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXRED FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
               returnValue, *returnCode, *reasonCode);
  #else
        zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXRED FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
               returnValue, *returnCode, *reasonCode, strerror(*returnCode));
  #endif
    } else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXRED OK: Read %d bytes\n", returnValue);
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_WARNING, "fileWrite: File is null\n");
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXWRT (fd: %d, file: %s, desired bytes: %d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             fd, (file->pathname ? file->pathname : "unknown"), desiredBytes, returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXWRT (fd: %d, file: %s, desired bytes: %d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             fd, (file->pathname ? file->pathname : "unknown"), desiredBytes, returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXWRT OK: Wrote %d bytes\n", returnValue);
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
  zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "bufferSize = %d\n",file->bufferSize);
#endif
  if (file->bufferSize == 0){
#ifdef DEBUG
    zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "fgetc 1\n");
#endif
    *returnCode = 8;
    *reasonCode = 0xBFF;
    return -1;
  } else if (file->bufferPos < file->bufferFill){
#ifdef DEBUG
    zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "fgetc 2\n");
#endif
    return (int)(file->buffer[file->bufferPos++]);
  } else if (file->eofKnown){
#ifdef DEBUG
    zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "fgetc 3\n");
#endif
    return -1;
  } else{
    /* go after next buffer and pray */
#ifdef DEBUG
    zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "fgetc 4\n");
#endif
    int bytesRead = fileRead(file,file->buffer,file->bufferSize,returnCode,reasonCode);
    if (bytesRead >= 0) {
#ifdef DEBUG
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "got more data, bytesRead=%d wanted=%d\n",bytesRead,file->bufferSize);
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_WARNING, "fileClose: File is null\n");
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXCLO FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXCLO FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXCLO OK: returnValue: %d\n", returnValue);
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

int fileChangeTagPure(const char *fileName, int *returnCode, int *reasonCode,
                      int ccsid, bool pure) ;

int fileChangeTag(const char *fileName, int *returnCode, int *reasonCode, int ccsid) {
  bool pure = true;
  fileChangeTagPure(fileName, returnCode, reasonCode, ccsid, pure);
  }

int fileChangeTagPure(const char *fileName, int *returnCode, int *reasonCode,
                      int ccsid, bool pure) {
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

  if (pure) {
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXCHR FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXCHR FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXCHR (%s) OK: returnValue: %d\n\n", fileName, returnValue);
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

int fileChangeMode(const char *fileName, int *returnCode, int *reasonCode, int mode) {
  int nameLength = strlen(fileName);
  int *reasonCodePtr;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXCHM(nameLength,
         fileName,
         mode,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if (returnValue != 0) {
# ifdef METTLE
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXCHM FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             returnValue, *returnCode, *reasonCode);
# else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXCHM FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             returnValue, *returnCode, *reasonCode, strerror(*returnCode));
# endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXCHM (%s) OK: returnValue: %d\n\n", fileName, returnValue);
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

int fileCopy(const char *existingFileName, const char *newFileName, int *retCode, int *resCode) {
  int returnCode = 0, reasonCode = 0, status = 0;
  FileInfo info = {0};

  status = fileInfo(existingFileName, &info, &returnCode, &reasonCode);
  if (status == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  short ccsid = info.ccsid;
  
  UnixFile *existingFile = fileOpen(existingFileName, FILE_OPTION_READ_ONLY, 0, 0, &returnCode, &reasonCode);
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
    return -1;
  }

  if (ccsid != CCSID_UNTAGGED) {
    status = fileChangeTag(newFileName, &returnCode, &reasonCode, ccsid);
    if (status == -1) {
      *retCode = returnCode;
      *resCode = reasonCode;
      return -1;
    }
  }

  status = fileDisableConversion(existingFile, &returnCode, &reasonCode);
  if (status != 0) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }
  
  status = fileDisableConversion(newFile, &returnCode, &reasonCode);
  if (status != 0) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  int bytesRead = 0;
  do {
#define FILE_BUFFER_SIZE 4000
    char fileBuffer[FILE_BUFFER_SIZE] = {0};
    
    bytesRead = fileRead(existingFile, fileBuffer, sizeof(fileBuffer), &returnCode, &reasonCode);
    if (bytesRead == -1) {
      *retCode = returnCode;
      *resCode = reasonCode;
      return -1;
    }
    
    status = fileWrite(newFile, fileBuffer, bytesRead, &returnCode, &reasonCode);
    if (status == -1) {
      *retCode = returnCode;
      *resCode = reasonCode;
      return -1;
    } 
  } while (bytesRead != 0);

  status = fileClose(existingFile, &returnCode, &reasonCode);
  if (status == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  status = fileClose(newFile, &returnCode, &reasonCode);
  if (status == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXREN FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXREN FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXREN (%s) OK: returnVal: %d\n", oldFileName, returnValue);
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXUNL (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             fileName, returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXUNL (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             fileName, returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXUNL (%s) OK: returnVal: %d\n", fileName, returnValue);
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXSTA (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             filename, returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXSTA (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             filename, returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXSTA (%s) OK: returnVal: %d, type: %s\n", filename, returnValue, fileTypeString(stats->fileType));
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

int symbolicFileInfo(const char *filename, BPXYSTAT *stats, int *returnCode, int *reasonCode) {
  int nameLength = strlen(filename);
  int statsLength = sizeof(BPXYSTAT);
  int *reasonCodePtr;
  int returnValue = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXLST(&nameLength,
         filename,
         &statsLength,
         stats,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if(returnValue != 0) {
#ifdef METTLE
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXLST (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             filename, returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXLST (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             filename, returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXLST (%s) OK: returnVal: %d, type: %s\n", filename, returnValue, fileTypeString(stats->fileType));
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

int fileChangeOwner(const char *fileName, int *returnCode, int *reasonCode, 
                    int usrId, int grpId) {
  int nameLength = strlen(fileName);
  int *reasonCodePtr;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXLCO(&nameLength,
         fileName,
         usrId,
         grpId,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (fileTrace) {
    if (returnValue != 0) {
#ifdef METTLE
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXLCO FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
             returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXLCO FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXLCO (%s) OK: returnValue: %d\n\n", fileName, returnValue);
    }
  }

  if (returnValue != 0) {
    returnValue = -1;
  }
  return returnValue;
}

int fileInfoIsDirectory(const FileInfo *info) {
  return (info->fileType == BPXSTA_FILETYPE_DIRECTORY ? TRUE: FALSE);
}

int fileInfoIsRegularFile(const FileInfo *info) {
  return (info->fileType == BPXSTA_FILETYPE_REGULAR ? TRUE: FALSE);
}

int fileInfoIsSymbolicLink(const FileInfo *info) {
  return (info->fileType == BPXSTA_FILETYPE_SYMLINK ? TRUE: FALSE);
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXOPD (%s) FAILED: fd: %d, returnCode: %d, reasonCode: 0x%08x\n",
             directoryName, fd, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXOPD (%s) FAILED: fd: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             directoryName, fd, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXOPD (%s) OK: fd: %d\n",
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_WARNING, "Directory is null\n");
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_WARNING, "BPXRDD (fd=%d, path=%s) %s: %d entries read, "
             "returnCode: %d, reasonCode: 0x%08x\n",
             directory->fd, (directory->pathname ? directory->pathname : "unknown"),
             (numberOfEntriesRead == 0 ? "PROBABLE EOF" : "FAILED"), numberOfEntriesRead,
             *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_WARNING, "BPXRDD (fd=%d, path=%s) %s: %d entries read, "
             "returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             directory->fd, (directory->pathname ? directory->pathname : "unknown"),
             (numberOfEntriesRead == 0 ? "PROBABLE EOF" : "FAILED"), numberOfEntriesRead,
             returnCode, reasonCode, strerror(*returnCode));

#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXRDD (fd=%d, path=%s) OK: %d entries read\n",
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_WARNING, "Directory is null\n");
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXCLD (fd=%d, path=%s) FAILED: returnValue: %d, "
             "returnCode: %d, reasonCode: 0x%08x\n",
             directory->fd, (directory->pathname ? directory->pathname : "unknown"), returnValue,
             returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXCLD (fd=%d, path=%s) FAILED: returnValue: %d, "
             "returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             directory->fd, (directory->pathname ? directory->pathname : "unknown"), returnValue,
             returnValue, *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXCLD (fd=%d, path=%s) OK: returnValue: %d\n",
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXMKD (%s, 0%o) FAILED: returnVal: %d, "
             "returnCode: %d, reasonCode: 0x%08x\n",
             pathName, mode, returnValue,
             *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXMKD (%s, 0%o) FAILED: returnVal: %d, "
             "returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             pathName, mode, returnValue,
             *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXMKD (%s,) OK: returnValue: %d\n",
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
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXMKD (%s) FAILED: returnVal: %d\n",
             pathName, returnValue);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_SEVERE, "BPXMKD (%s) FAILED: returnVal: %d, "
             "returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
             pathName, returnValue,
             *returnCode, *reasonCode, strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "BPXRMD (%s) OK: returnValue: %d\n", pathName, returnValue);
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
  
int directoryDeleteRecursive(const char *pathName, int *retCode, int *resCode){
  int returnCode = 0, reasonCode = 0, status = 0, symstatus = 0;
  FileInfo    info = {0};
  FileInfo syminfo = {0};
 
  status = fileInfo(pathName, &info, &returnCode, &reasonCode);
  if (status == -1) {
    status = symbolicFileInfo(pathName, &info, &returnCode, &reasonCode);
  }
  if (status == -1){
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  UnixFile *dir = directoryOpen(pathName, &returnCode, &reasonCode);
  if (dir == NULL) {
    *retCode = returnCode;
    *resCode = reasonCode;    
    return -1;
  }

  char entryBuffer[MAX_ENTRY_BUFFER_SIZE] = {0};
  int entries = directoryRead(dir, entryBuffer, sizeof(entryBuffer), &returnCode, &reasonCode);
  if (entries == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;    
    return -1;
  }
  
  const char *entryArray[MAX_NUM_ENTRIES] = {0};
  int validEntries = getValidDirectoryEntries(entries, entryBuffer, entryArray);
  if (validEntries <  1) {
    status = directoryDelete(pathName, &returnCode, &reasonCode);
    if (status == -1) {
      *retCode = returnCode;
      *resCode = reasonCode;      
      return -1;
    }
    return 0;
  }

  for (int i = 0; i < validEntries; i++) {
    char pathBuffer[USS_MAX_PATH_LENGTH + 1] = {0};
    snprintf(pathBuffer, sizeof(pathBuffer), "%s/%s", pathName, entryArray[i]);

    status    = fileInfo(pathBuffer, &info, &returnCode, &reasonCode);
    symstatus = symbolicFileInfo(pathName, &syminfo, &returnCode, &reasonCode);

    if ((status == -1) && (symstatus == -1)){
      *retCode = returnCode;
      *resCode = reasonCode;
      return -1;
    }

    /* If pathBuffer is directory, then recursively call.    */
    /* Note: system marks symbolic as a directory            */
    if ((status != -1 ) && fileInfoIsDirectory(&info)) {
      status = directoryDeleteRecursive(pathBuffer, retCode, resCode);
      if (status == -1) {
        return -1;
      }
    }
    else {
      status = fileDelete(pathBuffer, &returnCode, &reasonCode);
      if (status == -1) {
        *retCode = returnCode;
        *resCode = reasonCode;
        return -1;
      }
    }
  }

  directoryDeleteRecursive(pathName, retCode, resCode);
  if (status == -1) {
    return -1;
  }

  return 0;
}

#define PATH_MAX 256
/* 
 * Recursively, make directory tree.
 */
int directoryMakeDirectoryRecursive(const char *pathName, 
                                   char * message, int messageLength,
                                   int recursive, int forceCreate){
  int returnCode = 0, reasonCode = 0, status = 0;
  int returnValue = 0;
  const char * nextField, *tempField, *endField;
  FileInfo info = {0};
  int done = 0;
  char Path[PATH_MAX];
  char *path = Path;
  path[0] = '\0';
  nextField = pathName;

  /* Determine if absolute path or relative path */
  if (0 != strncmp (nextField,"/",1)) {
    strcat(path, "./");
    //nextField ++;
  } else {
    strcat(path, "/");
    nextField ++;
  }
  endField  = NULL;

  /* Cycle through path to find directory to make */
  /* Check for recursive after first one is made  */
  while (!done) {
    /* Last field in path */
    if ( !(tempField = strchr(nextField, '/'))) {
      done = 1;
      endField = &pathName[strlen(pathName)];

    } else {
      /* Still in the middle of the path */
      if (endField != NULL) {
        nextField = endField+1;
      }
      endField  = tempField;
    } 

    /* Copy next field onto path */
    strncat (path, nextField, (size_t)(endField - nextField) );
            
    /* Create directory if does not exist */  
    if( -1 == fileInfo(path, &info, &returnCode, &reasonCode)) {
      returnValue = directoryMake(path, 0700, &returnCode, &reasonCode);
      if ((returnValue ) || !recursive) {
        goto ExitCode;
      }
    }

    /* Update pointers                       */
    /* Copy directory name to return message */
    strncpy (message, path, messageLength);
    strcat (path, "/");
    nextField = endField + 1;
  }

ExitCode:
  return returnValue;
}


int directoryCopy(const char *existingPathName, const char *newPathName, int *retCode, int *resCode) {
  int returnCode = 0, reasonCode = 0, status = 0;
  FileInfo info = {0};

  status = fileInfo(existingPathName, &info, &returnCode, &reasonCode);
  if (status == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  UnixFile *existingDirectory = directoryOpen(existingPathName, &returnCode, &reasonCode);
  if (existingDirectory == NULL) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  char entryBuffer[MAX_ENTRY_BUFFER_SIZE] = {0};
  int entries = directoryRead(existingDirectory, entryBuffer, sizeof(entryBuffer), &returnCode, &reasonCode);
  if (entries == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;    
    return -1;
  }
  
  const char *entryArray[MAX_NUM_ENTRIES] = {0};
  int validEntries = getValidDirectoryEntries(entries, entryBuffer, entryArray);
  if (validEntries <  1) {
    
  }
  
  status = directoryMake(newPathName,
                         0700,
                         &returnCode,
                         &reasonCode);

  if (status == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  UnixFile *newDirectory = directoryOpen(newPathName, &returnCode, &reasonCode);
  if (newDirectory == NULL) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  for (int i = 0; i < validEntries; i++) {
    char existingPathBuffer[USS_MAX_PATH_LENGTH + 1] = {0};
    snprintf(existingPathBuffer, sizeof(existingPathBuffer), "%s/%s", existingPathName, entryArray[i]);
    
    char newPathBuffer[USS_MAX_PATH_LENGTH + 1] = {0};
    snprintf(newPathBuffer, sizeof(newPathBuffer), "%s/%s", newPathName, entryArray[i]);

    status = fileInfo(existingPathBuffer, &info, &returnCode, &reasonCode);
    if (status == -1) {
      *retCode = returnCode;
      *resCode = reasonCode;
      return -1;
    }
    
    if (fileInfoIsDirectory(&info)) {
      status = directoryCopy(existingPathBuffer, newPathBuffer, retCode, resCode);
      if (status == -1) {
        return -1;
      }
    }
    else {
      status = fileCopy(existingPathBuffer, newPathBuffer, retCode, resCode);
      if (status == -1) {
        return -1;
      }
    }
  }

  status = directoryClose(existingDirectory, &returnCode, &reasonCode);
  if (status == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  status = directoryClose(newDirectory, &returnCode, &reasonCode);
  if (status == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

  return 0;
}

/*
 * Recursively, change the file tags of the requested file/tree 
*/
#include "ccsidList.c"

int findCcsidId( const char *string){
  int ccsid = -1;
  int i = 0;
  int tccsid;
  char *pName;

  /* Some type commands cannot specify ccsid */
  if (string == NULL) {
    return 0;
  }

  /* search table for valid  */
  if (stringIsDigit(string)) {
    sscanf (string, "%d", &tccsid);

    while (ccsidList[i].idName != NULL) {
      if (ccsidList[i].ccsid == tccsid) {
        ccsid = tccsid;
        return ccsid;
      }
      i ++;
    }
  } else {
    /* Search array for matching name */
    while (ccsidList[i].idName != NULL) {
      char tstring[40] = {0};
      strncpy (tstring, string, sizeof(tstring) -1);

      strupcase(tstring);
      if ((strlen(ccsidList[i].idName) == strlen(tstring)) &&
          (strcmp (ccsidList[i].idName, tstring) == 0)) {
        ccsid =ccsidList[i].ccsid;
        return ccsid;
      }
      i ++;
    }
  }
  return -1;
}

int patternChangeTagTest(char *message, int message_length, 
                char *type, char *codePage, bool *pure, int *ccsid) {
  int lccsid =  findCcsidId(codePage);

  /* Switch to do proper change tag */
  if  (!strcmp( strupcase(type), "BINARY"))  {
    *pure = FALSE;
    if (lccsid != 0) {
      *ccsid =  -1;
      snprintf(message, message_length, "%s", 
               "binary specified with codeset");
      return -1;
    }
    else {
      *ccsid =  CCSID_BINARY;
      return 0;
    }
  } 

  if  (!strcmp( strupcase(type), "TEXT"))  {
    *pure = TRUE;
    if (lccsid <= 0) {
      snprintf(message, message_length, "%s", 
               "text specified, undefined codeset");

      return -1;
    }
    else {
      *ccsid =  lccsid;
      return 0;
    }
  } 

  if ((!strcmp( strupcase(type), "DELETE")) ||
      (!strcmp( strupcase(type), "UNTAGGED"))) {
    *pure = FALSE;
    if (lccsid > 0) {
      *ccsid =  -1;
      snprintf(message, message_length, "%s", 
               "delete specified with codeset");
      return -1;
    }
    else {
      *ccsid =  CCSID_UNTAGGED;
      return 0;
    }
  } 

  if  (!strcmp( strupcase(type), "MIXED"))  {
    *pure = FALSE;
    if (lccsid <= 0) {
      snprintf(message, message_length, "%s", 
               "mixed specified with undefined codeset");
      return -1;
    }
    else {
      *ccsid =  lccsid;
      return 0;
    }
  } 

  *pure = FALSE;
  *ccsid = lccsid;
  return 0;
}

/* Check to see if the code should change the tag field */
static int patternChangeTagCheck(const char *fileName, int *retCode,
       int *resCode, char *codepage, char * type, const char *pattern) {

  bool pure = FALSE;
  int ccsid;
  char message[30];

  const char * baseName;
  int returnCode = 0, reasonCode = 0;
  int returnValue = 0;

  /* Test to see if a substring is part of base name */
  /* If not, then return                             */
  if (pattern != NULL) {
    if ((baseName = strrstr(fileName, "/")) == NULL ) {
      baseName = fileName;
    }
    if (strstr(baseName, pattern) == NULL) {
       return 0;
    }
  }

  if (-1 == patternChangeTagTest(message, sizeof (message),
                                type, codepage, &pure, &ccsid)) {
    returnValue = -1;
    goto ExitCode;
  } 

  /* Change tag */
  if (-1 == (fileChangeTagPure(fileName, &returnCode, &reasonCode, ccsid, pure))) {
    returnValue = -1;
    goto ExitCode;
  }


ExitCode:
  *retCode = returnCode;
  *resCode = reasonCode;
  return     returnValue;
}


/* Recursively, trace down directory tree and change file tags */
int directoryChangeTagRecursive(const char *pathName, char *type,
          char *codepage, int recursive, char * pattern,
          int *retCode, int *resCode){

  int returnCode = 0, reasonCode = 0, status = 0;
  int returnValue = 0;
  FileInfo info = {0};
  int CCSID = 0;

  /* Find the ccsid for the codepage */
  if (-1 == (CCSID = findCcsidId(codepage))) {
    goto ExitCodeError;
  }

  /* Get initial file info*/
  status = fileInfo(pathName, &info, &returnCode, &reasonCode);
  if (status == -1){
    goto ExitCodeError;
  }

  /* If directory and not recursive, just return */
  if ((fileInfoIsDirectory(&info) && !recursive)) {
    goto ExitCode;
    }

  /* Request is for a file. Handle it and exit */
  if (fileInfoIsRegularFile(&info)) {
    if( -1 == patternChangeTagCheck (pathName, &returnCode, 
                            &reasonCode, codepage, type, pattern)) {
      goto ExitCodeError;
    } else {
      goto ExitCode;
    }
  } 

  /* Get list of files in this directory */
  const char *entryArray[MAX_NUM_ENTRIES] = {0};
  char entryBuffer[MAX_ENTRY_BUFFER_SIZE] = {0};
  UnixFile *dir = directoryOpen(pathName, &returnCode, &reasonCode);
  if (dir == NULL) {
    goto ExitCodeError;
  }

  int entries = directoryRead(dir, entryBuffer, sizeof(entryBuffer),
                              &returnCode, &reasonCode);
  if (entries == -1) {
    goto ExitCodeError;
  }

  int validEntries = getValidDirectoryEntries(entries, entryBuffer, 
                                              entryArray);
  /* Loop through all files in directory                     */
  /* If a subdirectory found, recursively call this function */
  /* At this point, we know recursive is true.               */
  for (int i = 0; i < validEntries; i++) {
    char pathBuffer[USS_MAX_PATH_LENGTH + 1] = {0};
    snprintf(pathBuffer, sizeof(pathBuffer), "%s/%s", pathName, entryArray[i]);

    if (-1 == (fileInfo(pathBuffer, &info, &returnCode, &reasonCode))){
      returnValue = -1;
      goto ExitCodeError;
    }

    if (fileInfoIsDirectory(&info)) {
      /* Change tag of all sub-directories and files there-in */
      if (-1 ==  directoryChangeTagRecursive(
                         pathBuffer, type, codepage, recursive, pattern,
                         &returnCode, &reasonCode) ){
        goto ExitCodeError;
      }
    }
    else {
      /* change mode of this file, not a directory */
      if (fileInfoIsRegularFile(&info)) {
        if( -1 == patternChangeTagCheck (pathBuffer, &returnCode, &reasonCode, 
                                  codepage, type, pattern)) {
          goto ExitCodeError;
        }
      }
    }
  } /* End of for loop */

  goto ExitCode;
ExitCodeError:
    returnValue = -1;
ExitCode:
    *retCode = returnCode;
    *resCode = reasonCode;
  if (fileTrace) {
    if (returnValue  != 0) {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_WARNING, "directoryChangeModeRecursive: Failed\n");
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "directoryChangeModeRecursive: Passed\n");
   }
  }
  return returnValue;
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
    zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "setUmask(0%o) return 0%o\n", mask, returnValue);
  }
  return returnValue;
}

int getUmask() {
  zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_WARNING, "UMASK: This is a dirty hack.\n");

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

static int patternChangeModeFile (const char *fileName,
                                 int mode, const char *compare,
                                 int *returnCode, int *reasonCode){
  int status;
  const char * baseName;

  if (fileTrace) {
   zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "ChangeModeFile: %s TO %3o  \n", fileName, mode);
  }

  /* Test to see if a substring is part of base name */
  /* If not, then return                             */
  if (compare != NULL) {
    if ((baseName = strrstr(fileName, "/")) == NULL ) {
      baseName = fileName;
    }
    if (strstr(baseName, compare) == NULL) {
       return 0;
    }
  }

  /* Check to see if change in mode is requested */
  status = fileChangeMode(fileName, returnCode, reasonCode, mode);
    if (status == -1) {
     return -1;
    }

  return 0;
}

#define CHANGE_MODE_RECURSIVE  0x1
 
int directoryChangeModeRecursive(const char *pathName, int flag,
               int mode, const char * compare, int *retCode, int *resCode){
  int returnCode = 0, reasonCode = 0, status = 0;
  int returnValue = 0;
  FileInfo info = {0};
  status = fileInfo(pathName, &info, &returnCode, &reasonCode);
  if (status == -1){
    *retCode = returnCode;
    *resCode = reasonCode;
    returnValue = -1;
    goto ExitCode;
  }

  /* Request is for a file. Handle it and exit */
  if (!fileInfoIsDirectory(&info)) {
    if (fileInfoIsRegularFile(&info)) {
      if( -1 == patternChangeModeFile (pathName, mode, compare,
                                         &returnCode, &reasonCode)) {
        *retCode = returnCode;
        *resCode = reasonCode;
        returnValue = -1;
      }
    } else {
        *retCode = 0;
        *resCode = 0;
        returnValue = -1;
    }
    goto ExitCode;
  } else {
    if (!(flag & CHANGE_MODE_RECURSIVE)) { 
      if( -1 == patternChangeModeFile (pathName, mode, compare,
                                         &returnCode, &reasonCode)) {
        *retCode = returnCode;
        *resCode = reasonCode;
        returnValue = -1;
      }
    goto ExitCode;
    } 
  }

  UnixFile *dir = directoryOpen(pathName, &returnCode, &reasonCode);
  if (dir == NULL) {
    *retCode = returnCode;
    *resCode = reasonCode;    
    returnValue = -1;
    goto ExitCode;
  }

  char entryBuffer[MAX_ENTRY_BUFFER_SIZE] = {0};
  int entries = directoryRead(dir, entryBuffer, sizeof(entryBuffer), 
                              &returnCode, &reasonCode);
  if (entries == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;    
    returnValue = -1;
    goto ExitCode;
  }
  
  const char *entryArray[MAX_NUM_ENTRIES] = {0};
  int validEntries = getValidDirectoryEntries(entries, entryBuffer, entryArray);

  for (int i = 0; i < validEntries; i++) {
    char pathBuffer[USS_MAX_PATH_LENGTH + 1] = {0};
    snprintf(pathBuffer, sizeof(pathBuffer), "%s/%s", pathName, entryArray[i]);

    status = fileInfo(pathBuffer, &info, &returnCode, &reasonCode);
    if (status == -1){
      *retCode = returnCode;
      *resCode = reasonCode;
      returnValue = -1;
      goto ExitCode;
    }

    if (fileInfoIsDirectory(&info)) {
      /* Change mode of all sub-directories and files there-in */
      if (flag & CHANGE_MODE_RECURSIVE) { 
        if (-1 ==  directoryChangeModeRecursive(
                               pathBuffer, flag, mode, compare,
                               &returnCode, &reasonCode) ){
          *retCode = returnCode;
          *resCode = reasonCode;
          returnValue = -1;
          goto ExitCode;
        }
      }
    }
    else {
      /* change mode of this file, not a directory */
      if (fileInfoIsRegularFile(&info)) {
        if( -1 == patternChangeModeFile (pathBuffer, mode, compare,
                                         &returnCode, &reasonCode)) { 
          *retCode = returnCode;
          *resCode = reasonCode;
          returnValue = -1;
          goto ExitCode;
        }
      }
    }
  }

  /* Change mode of this directory */
  if( -1 == patternChangeModeFile (pathName, mode, compare,
                                    &returnCode, &reasonCode)) { 
    *retCode = returnCode;
    *resCode = reasonCode;
    returnValue = -1;
    goto ExitCode;
  }

ExitCode:
  if (fileTrace) {
    if (returnValue  != 0) {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_WARNING, "directoryChangeModeRecursive: Failed\n");
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "directoryChangeModeRecursive: Passed\n");
   }
  }
  return returnValue;
}

/* Check for pattern match */
int fileChangeOwnerPatternCheck(const char *pathName, int *retCode,
          int *resCode, char *pattern, int userId, int groupId) {
  const char *baseName;
  int  returnValue;

  /* test to see if file name matches pattern if requested */
  if (pattern != NULL) {
    if ((baseName = strrstr(pathName, "/")) == NULL ) {
      baseName = pathName;
    }

    if (strstr(baseName, pattern) == NULL) {
       return 0;
    }
  }

  returnValue = fileChangeOwner(pathName,  retCode, resCode, userId, groupId);
  return returnValue;
}

/* Recursively, trace down directory tree and change file ownership */
int directoryChangeOwnerRecursive(char * message, int messageLength,
              const char *pathName, int userId, int groupId,
              int recursive, char * pattern,
              int *retCode, int *resCode){

  int status = 0;
  int returnValue = 0;
  FileInfo info = {0};

  /* Get initial file info*/
  status = symbolicFileInfo(pathName, &info, retCode, resCode);
  if (status == -1){
    goto ExitCodeError;
  }

  /* Request is for a file or non recursive directory. Handle it and exit */
  if (((fileInfoIsDirectory(&info)   && !recursive))  ||
       fileInfoIsRegularFile(&info)  ||
       fileInfoIsSymbolicLink(&info)) {
    returnValue = fileChangeOwnerPatternCheck(pathName, retCode,
                  resCode, pattern, userId, groupId);

    if (returnValue != 0){
      goto ExitCodeError;
    }
    else {
      goto ExitCode;
    }
  }

/* Get list of files in this directory */
  const char *entryArray[MAX_NUM_ENTRIES] = {0};
  char entryBuffer[MAX_ENTRY_BUFFER_SIZE] = {0};
  UnixFile *dir = directoryOpen(pathName, retCode, resCode);
  if (dir == NULL) {
    goto ExitCodeError;
  }
  int entries = directoryRead(dir, entryBuffer, sizeof(entryBuffer),
                              retCode, resCode);

  /* Empty Directory.  Will be change on return */
  if (entries == -1) {
    goto ExitCode;
  }

  int validEntries = getValidDirectoryEntries(entries, entryBuffer,
                                              entryArray);
  /* Loop through all files in directory                     */
  /* If a subdirectory found, recursively call this function */
  /* At this point, we know recursive is true.               */
  for (int i = 0; i < validEntries; i++) {
    char pathBuffer[USS_MAX_PATH_LENGTH + 1] = {0};
    snprintf(pathBuffer, sizeof(pathBuffer), "%s/%s", pathName, entryArray[i]);

    if (-1 == (symbolicFileInfo(pathBuffer, &info, retCode, resCode))){
      goto ExitCodeError;
    }

    if (fileInfoIsDirectory(&info)) {
      /* Change ownership of all sub-directories and files there-in */
      if (-1 ==  directoryChangeOwnerRecursive( message, messageLength,
                         pathBuffer, userId, groupId, recursive, pattern,
                         retCode, resCode) ){
        goto ExitCodeError;
      }
    }

    /* Change ownership of individual file or current directory */
    if (fileInfoIsDirectory(&info)     ||
        fileInfoIsRegularFile(&info)   ||
        fileInfoIsSymbolicLink(&info))  {
      returnValue = fileChangeOwnerPatternCheck(pathBuffer,  retCode, resCode,
                 pattern, userId, groupId);
      if (returnValue != 0){
        goto ExitCodeError;
      }
    }
  } /* End of for loop */


  goto ExitCode;
ExitCodeError:
    returnValue = -1;
ExitCode:
  if (fileTrace) {
    if (returnValue  != 0) {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_WARNING, "directoryChangeOwnerRecursive: Failed\n");
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_INFO, "directoryChangeOwnerRecursive: Passed\n");
   }
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

