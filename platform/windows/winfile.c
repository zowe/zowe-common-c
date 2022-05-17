

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "collections.h"
#include "unixfile.h"

/*
  this pragma tells the compiler/linker to link in the right, modern WinSock stuff 

 */

#pragma comment(lib, "Ws2_32.lib")


static int fileTrace = FALSE;

int setFileTrace(int toWhat) {
  int was = fileTrace;
  fileTrace = toWhat;
  return was;
}

UnixFile *fileOpen(const char *filename, int options, int mode, int bufferSize, int *returnCode, int *reasonCode){
  int returnValue = 0;
  int *reasonCodePtr;
  int status;
  int len = strlen(filename);
  char *unixMode = "r";

  /*   r      Open text file for reading.  The stream is positioned at the
              beginning of the file.

       r+     Open for reading and writing.  The stream is positioned at the
              beginning of the file.

       w      Truncate file to zero length or create text file for writing.
              The stream is positioned at the beginning of the file.

       w+     Open for reading and writing.  The file is created if it does
              not exist, otherwise it is truncated.  The stream is
              positioned at the beginning of the file.

       a      Open for appending (writing at end of file).  The file is
              created if it does not exist.  The stream is positioned at the
              end of the file.

       a+     Open for reading and appending (writing at end of file).  The
              file is created if it does not exist.  The initial file
              position for reading is at the beginning of the file, but
              output is always appended to the end of the file.
  */
  if (options & FILE_OPTION_APPEND){
    unixMode = (options & FILE_OPTION_READ_ONLY) ? "a+" : "a";
  } else if (options & FILE_OPTION_WRITE_ONLY){
    if (options & FILE_OPTION_READ_ONLY){
      unixMode = (options & FILE_OPTION_TRUNCATE) ? "w+" : "r+";
    } else {
      if (options & FILE_OPTION_TRUNCATE){
        unixMode = "w";
      } else {
        *returnCode = -1;
        return NULL;  /* parameter error */
      }
    }
  } else if (options & FILE_OPTION_READ_ONLY){
    unixMode = "r";
  } else {
    *returnCode = -1;
    return NULL; /* parameter error */
  }

  FILE *internalFile = fopen(filename,unixMode); 
  if (internalFile == NULL){
    *returnCode = -1;
    return NULL; /* open failed */
  }
  UnixFile *file = (UnixFile*)safeMalloc(sizeof(UnixFile),"OMVS File");
  memset(file,0,sizeof(UnixFile));
  file->internalFile = internalFile;
  file->fd = _fileno(internalFile);
  file->pathname = safeMalloc(strlen(filename)+1,"Unix File Name");
  strcpy(file->pathname, filename);
  file->isDirectory = FALSE;
  if (bufferSize > 0){
    file->buffer = safeMalloc(bufferSize,"OMVS File Buffer");
    file->bufferSize = bufferSize;
    file->bufferPos = bufferSize;
    file->bufferFill = bufferSize;
  }
  return file;
}

int fileRead(UnixFile *file, char *buffer, int desiredBytes, 
             int *returnCode, int *reasonCode){
  FILE *internalFile = file->internalFile;

  size_t bytesRead = fread(buffer,1,desiredBytes,internalFile);
  *returnCode = 0;
  if (bytesRead < desiredBytes){
    file->eofKnown = TRUE;
    if (ferror(internalFile)){
      *returnCode = errno;
      *reasonCode = errno;
    }
  }
  return (int)bytesRead;
}

int fileWrite(UnixFile *file, const char *buffer, int desiredBytes,
              int *returnCode, int *reasonCode) {
  FILE *internalFile = file->internalFile;

  size_t bytesWritten = fwrite(buffer,1,desiredBytes,internalFile);
  *returnCode = 0;
  if (bytesWritten < desiredBytes){
    file->eofKnown = TRUE;
    if (ferror(internalFile)){
      *returnCode = errno;
      *reasonCode = errno;
    }
  }
  return (int)bytesWritten;
}

int fileGetChar(UnixFile *file, int *returnCode, int *reasonCode){
  if (file->bufferSize == 0){
    *returnCode = 8;
    *reasonCode = 0xBFF;
    return -1;
  } else if (file->bufferPos < file->bufferFill){
    return (int)(file->buffer[file->bufferPos++])&0xFF;
  } else if (file->eofKnown){
    return -1;
  } else{
    /* go after next buffer and pray */
    int bytesRead = fileRead(file,file->buffer,file->bufferSize,returnCode,reasonCode);
    if (bytesRead >= 0) {
      if (bytesRead < file->bufferSize) { /* out of data after this read */
        file->eofKnown = TRUE;
      }
      file->bufferFill = bytesRead;
      file->bufferPos = 1;
      return file->buffer[0]&0xFF;
    } else{
      return -1;
    }
  }
}

int fileInfo(const char *filename, FileInfo *info, int *returnCode, int *reasonCode){
  int nameLength = strlen(filename);

  int status = _stat(filename,info);
  if (status != 0){
    *returnCode = errno;
    *reasonCode = errno;
  }
  return status;
}

int fileEOF(const UnixFile *file){
  return ((file->bufferPos >= file->bufferFill) && file->eofKnown);
}

int fileClose(UnixFile *file, int *returnCode, int *reasonCode){
  FILE *internalFile = file->internalFile;

  int status = fclose(internalFile);
  if (status != 0){
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
  file = NULL;
  
  return status;
}

int fileInfoIsDirectory(const FileInfo *info){
  return info->st_mode & _S_IFDIR;
}

int64 fileInfoSize(const FileInfo *info){
  return info->st_size;
}

int fileInfoCCSID(const FileInfo *info){
  printf("***WARNING*** guessing about File's  code page in Windows\n");
  return CP_UTF8;
}

int fileInfoUnixCreationTime(const FileInfo *info){
  return info->st_ctime;
}

int fileInfoUnixModificationTime(const FileInfo *info){
  return info->st_mtime;
}

int fileUnixMode(const FileInfo *info) {
  return info->st_mode;
}

int fileInfoOwnerGID(const FileInfo *info) {
  return info->st_gid;
}

int fileInfoOwnerUID(const FileInfo *info) {
  return info->st_uid;
}

int fileGetINode(const FileInfo *info) {
  return info->st_ino;
}

#define FILE_BUFFER_SIZE 4096
#define MAX_CONVERT_FACTOR 4

int fileCopyConverted(const char *existingFileName, const char *newFileName,
                      int existingCCSID, int newCCSID,
                      int *retCode, int *resCode) {
  int returnCode = 0, reasonCode = 0, status = 0;
  bool shouldConvert = (existingCCSID != newCCSID);
  FileInfo info = {0};

  status = fileInfo(existingFileName, &info, &returnCode, &reasonCode);
  if (status == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;
    return -1;
  }

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

  int bytesRead = 0;
  char *fileBuffer = safeMalloc(FILE_BUFFER_SIZE,"fileCopyBuffer");
  int conversionBufferLength = FILE_BUFFER_SIZE * MAX_CONVERT_FACTOR;
  char *conversionBuffer = (shouldConvert ? safeMalloc(conversionBufferLength,"fileCopyConvertBuffer"): NULL);
  char *writeBuffer = (shouldConvert ? conversionBuffer : writeBuffer);
  int writeLength;
  int returnValue = 0;
  
  do {
    bytesRead = fileRead(existingFile, fileBuffer, FILE_BUFFER_SIZE, &returnCode, &reasonCode);
    if (bytesRead == -1) {
      *retCode = returnCode;
      *resCode = reasonCode;
      returnValue = -1;
      goto cleanup;
    }

    if (bytesRead > 0 && shouldConvert){
      int convertStatus = convertCharset(fileBuffer,bytesRead,existingCCSID,
                                         CHARSET_OUTPUT_USE_BUFFER,&conversionBuffer,conversionBufferLength,newCCSID,
                                         NULL,&writeLength,resCode);
      if (convertStatus){
        *retCode = convertStatus;
        returnValue = -1;
        goto cleanup;
      }
    } else {
      writeLength = bytesRead;
    }
    
    status = fileWrite(newFile, writeBuffer, writeLength, &returnCode, &reasonCode);
    if (status == -1) {
      *retCode = returnCode;
      *resCode = reasonCode;
      returnValue = -1;
      goto cleanup;
    } 
  } while (bytesRead != 0);

  status = fileClose(existingFile, &returnCode, &reasonCode);
  if (status == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;
    returnValue = -1;
    goto cleanup;
  }

  status = fileClose(newFile, &returnCode, &reasonCode);
  if (status == -1) {
    *retCode = returnCode;
    *resCode = reasonCode;
    returnValue = -1;
  }
 cleanup:
  if (fileBuffer){
    safeFree(fileBuffer,FILE_BUFFER_SIZE);
  }
  if (conversionBuffer){
    safeFree(conversionBuffer,conversionBufferLength);
  }

  return returnValue;
}

int fileCopy(const char *existingFileName, const char *newFileName,
             int *retCode, int *resCode) {
  return fileCopyConverted(existingFileName,newFileName,0,0,retCode,resCode);
}

int fileDirname(const char *path, char *dirname){
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  char fname[_MAX_FNAME];
  char ext[_MAX_EXT];
  errno_t err = _splitpath_s(path,
                             drive,_MAX_DRIVE,
                             dir,_MAX_DIR,
                             fname,_MAX_FNAME,
                             ext,_MAX_EXT);
  if (err){
    strcpy(dirname,".");
    return 0;
  } else{
    sprintf(dirname,"%s%s",drive,dir);
    int len = strlen(dirname);
    char last = dirname[len-1];
    if (last == '/' || last == '\\'){
      /* kill the last characer if it's a (back)slash */
      dirname[len-1] = 0;
    }
  }
  return 0;
}

UnixFile *directoryOpen(const char *directoryName, int *returnCode, int *reasonCode){
  int nameLength = strlen(directoryName);
  WIN32_FIND_DATA findData;
  int directoryNameBufferSize = nameLength + 10;
  char *directoryNameBuffer = safeMalloc(directoryNameBufferSize,"Windows Directory Name");
  HANDLE hFind = INVALID_HANDLE_VALUE;

  int len = sprintf(directoryNameBuffer,"%s\\*",directoryName);
  hFind = FindFirstFile(directoryNameBuffer, &findData);

  safeFree(directoryNameBuffer, directoryNameBufferSize);
  directoryNameBuffer = NULL;
 
  if (hFind == INVALID_HANDLE_VALUE){
    *returnCode = GetLastError();
    *reasonCode = *returnCode;
    return NULL;
  } else {
    UnixFile *directory = (UnixFile*)safeMalloc(sizeof(UnixFile),"Windows Directory");
    memset(directory,0,sizeof(UnixFile));
    directory->internalFile = NULL;
    directory->fd = 0;
    directory->hFind = hFind;
    memcpy(&(directory->findData),&findData,sizeof(WIN32_FIND_DATA));
    directory->pathname = safeMalloc(strlen(directoryName)+1,"Unix File Name");
    strcpy(directory->pathname, directoryName);
    directory->isDirectory = TRUE;
    directory->hasMoreEntries = TRUE;
    
    *returnCode = 0;
    *reasonCode = 0;
    return directory;
  }
}

int directoryRead(UnixFile *directory, char *entryBuffer, int entryBufferLength, int *returnCode, int *reasonCode){
  HANDLE hFind = directory->hFind;
  WIN32_FIND_DATA *findData = &(directory->findData);
  
  if (directory->hasMoreEntries){
    int filenameLength = strlen(findData->cFileName);
    DirectoryEntry *entry = (DirectoryEntry*)entryBuffer;
    entry->entryLength = filenameLength+2;
    entry->nameLength = filenameLength;
    memcpy(entry->name,findData->cFileName,filenameLength);
    if (FindNextFile(hFind, findData)){
      directory->hasMoreEntries = TRUE;
    } else {
      /* find loop is done */
      directory->hasMoreEntries = FALSE;
    }
    return 1;
  } else {
    return 0;
  }
}

int directoryClose(UnixFile *directory, int *returnCode, int *reasonCode){
  HANDLE hFind = directory->hFind;

  FindClose(hFind);
  if (directory->pathname != NULL) {
    safeFree(directory->pathname, strlen(directory->pathname)+1);
    directory->pathname = NULL;
  }
  safeFree((char*)directory,sizeof(UnixFile));
  *returnCode = 0;
  *reasonCode = 0;
  return 0;
}

/* Temp home for windows users and groups */

/*
  https://social.msdn.microsoft.com/Forums/WINDOWS/en-US/dde0d103-7c21-4dfa-9266-754e59f0a11e/geteuid-and-getpwuid?forum=windowssdk

  id command

  
  "id -u"
  "id -g"  group
  "id -G"  all groups

   wmic useraccount

   Crypto stuff
   
   mscat.h

   https://github.com/cygwin/cygwin/tree/master/newlib/libm/common

   https://github.com/cygwin/cygwin/blob/8050ef207494e6d227e968cc7e5850153f943320/newlib/libc/unix/getpwent.c

   https://www.cygwin.com/cygwin-ug-net/cygwin-ug-net.pdf

   https://github.com/dscho/msys/blob/master/msys/packages/sh-utils/2.0/src/id.c
*/

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
