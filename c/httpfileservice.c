

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

#endif

#include "zowetypes.h"
#include "utils.h"
#include "json.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "charsets.h"
#include "unixfile.h"
#include "httpfileservice.h"

#ifdef __ZOWE_OS_ZOS
#define NATIVE_CODEPAGE CCSID_EBCDIC_1047
#define DEFAULT_UMASK 0022
#elif defined(__ZOWE_OS_WINDOWS)
#define NATIVE_CODEPAGE CP_UTF8
#error Must_find_default_windows_umask
#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
#define NATIVE_CODEPAGE CCSID_ISO_8859_1
#warning ISO-8859-1 is not necessarily the default codepage on Linux
#define DEFAULT_UMASK 0022
#endif

void response200WithMessage(HttpResponse *response, char *msg) {
  setResponseStatus(response,200,"OK");
  setDefaultJSONRESTHeaders(response);
  addStringHeader(response,"Connection","close");
  writeHeader(response);
  jsonPrinter *out = respondWithJsonPrinter(response);
  jsonStart(out);
  jsonAddString(out, "msg", msg);
  jsonEnd(out);
  finishResponse(response);
}

int isDir(char *absolutePath) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;

  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    return -1;
  }

  return (fileInfoIsDirectory(&info));
}

int doesItExist(char *absolutePath) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;

  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    return FALSE;
  }

  return TRUE;
}

static int createUnixDirectory(char *absolutePath, int forceCreate) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;
  int dirExists = FALSE;

  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == 0) {
    if (!forceCreate) {
      return -1;
    }
    dirExists = TRUE;
    status = tmpDirMake(absolutePath);
    if (status == -1) {
      return -1;
    }
  }

  status = directoryMake(absolutePath,
                         0,
                         &returnCode,
                         &reasonCode);

  if (status == -1) {
#ifdef DEBUG
    printf("Failed to create directory %s: (return = 0x%x, reason = 0x%x)\n", absolutePath, returnCode, reasonCode);
#endif
    if (dirExists) {
      tmpDirCleanup(absolutePath);
    }
    return -1;
  }

  tmpDirDelete(absolutePath);
  return 0;
}

void createUnixDirectoryAndRespond(HttpResponse *response, char *absolutePath, int forceCreate) {
  if (!createUnixDirectory(absolutePath, forceCreate)) {
    response200WithMessage(response, "Successfully created a directory");
  }
  else {
    respondWithJsonError(response, "Failed to create a directory", 500, "Internal Server Error");
  }
}

static int deleteUnixDirectory(char *absolutePath) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;

  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    return -1;
  }

  status = directoryDeleteRecursive(absolutePath);
  if (status == -1) {
#ifdef DEBUG
    printf("Failed to delete directory %s\n", absolutePath);
#endif
    return -1;
  }

  return 0;
}

void deleteUnixDirectoryAndRespond(HttpResponse *response, char *absolutePath) {
  if (!deleteUnixDirectory(absolutePath)) {
    response200WithMessage(response, "Successfully deleted a directory");
  }
  else {
    respondWithJsonError(response, "Failed to delete a directory", 500, "Internal Server Error");
  }
}

static int deleteUnixFile(char *absolutePath) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;

  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    return -1;
  }

  status = fileDelete(absolutePath, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Failed to delete file %s: (return = 0x%x, reason = 0x%x)\n", absolutePath, returnCode, reasonCode);
#endif
    return -1;
  }

  return 0;
}

void deleteUnixFileAndRespond(HttpResponse *response, char *absolutePath) {
  if (!deleteUnixFile(absolutePath)) {
    response200WithMessage(response, "Successfully deleted a file");
  }
  else {
    respondWithJsonError(response, "Failed to delete a file", 500, "Internal Server Error");
  }
}

static int renameUnixDirectory(char *oldAbsolutePath, char *newAbsolutePath, int forceRename) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;

  status = fileInfo(oldAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    return -1;
  }

  status = fileInfo(newAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == 0 && !forceRename) {
    return -1;
  }

  status = directoryRename(oldAbsolutePath, newAbsolutePath, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Failed to rename directory %s: (return = 0x%x, reason = 0x%x)\n", oldAbsolutePath, returnCode, reasonCode);
#endif
    return -1;
  }

  return 0;
}

void renameUnixDirectoryAndRespond(HttpResponse *response, char *oldAbsolutePath, char *newAbsolutePath, int forceRename) {
  if (!renameUnixDirectory(oldAbsolutePath, newAbsolutePath, forceRename)) {
    response200WithMessage(response, "Successfully renamed a directory");
  }
  else {
    respondWithJsonError(response, "Failed to rename a directory", 500, "Internal Server Error");
  }
}

static int renameUnixFile(char *oldAbsolutePath, char *newAbsolutePath, int forceRename) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;

  status = fileInfo(oldAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    return -1;
  }

  status = fileInfo(newAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == 0 && !forceRename) {
    return -1;
  }

  status = fileRename(oldAbsolutePath, newAbsolutePath, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Failed to rename file %s: (return = 0x%x, reason = 0x%x)\n", oldAbsolutePath, returnCode, reasonCode);
#endif
    return -1;
  }

  return 0;
}

void renameUnixFileAndRespond(HttpResponse *response, char *oldAbsolutePath, char *newAbsolutePath, int forceRename) {
  if (!renameUnixFile(oldAbsolutePath, newAbsolutePath, forceRename)) {
  response200WithMessage(response, "Successfully renamed a file");
  }
  else {
  respondWithJsonError(response, "Failed to rename a file", 500, "Internal Server Error");
  }
}

static int copyUnixDirectory(char *oldAbsolutePath, char *newAbsolutePath, int forceCopy) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;

  status = fileInfo(oldAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    return -1;
  }

  status = directoryCopy(oldAbsolutePath, newAbsolutePath, forceCopy);
  if (status == -1) {
#ifdef DEBUG
    printf("Failed to copy directory %s\n", oldAbsolutePath);
#endif
    return -1;
  }

  return 0;
}

void copyUnixDirectoryAndRespond(HttpResponse *response, char *oldAbsolutePath, char *newAbsolutePath, int forceCopy) {
  if (!copyUnixDirectory(oldAbsolutePath, newAbsolutePath, forceCopy)) {
    response200WithMessage(response, "Successfully copied a directory");
  }
  else {
    respondWithJsonError(response, "Failed to copy a directory", 500, "Internal Server Error");
  }
}

static int copyUnixFile(char *oldAbsolutePath, char *newAbsolutePath, int forceCopy) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;

  status = fileInfo(oldAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    return -1;
  }

  status = fileCopy(oldAbsolutePath, newAbsolutePath, forceCopy);
  if (status == -1) {
#ifdef DEBUG
    printf("Failed to copy file %s\n", oldAbsolutePath);
#endif
    return -1;
  }

  return 0;
}

void copyUnixFileAndRespond(HttpResponse *response, char *oldAbsolutePath, char *newAbsolutePath, int forceCopy) {
  if (!copyUnixFile(oldAbsolutePath, newAbsolutePath, forceCopy)) {
    response200WithMessage(response, "Successfully copied a file");
  }
  else {
    respondWithJsonError(response, "Failed to copy a file", 500, "Internal Server Error");
  }
}

int writeBinaryDataFromBase64(UnixFile *file, char *fileContents, int contentLength) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  int convertBufferSize = contentLength * 2;

  char *convertBuffer = safeMalloc(convertBufferSize, "CONVERT BUFFER");
  int conversionLength = 0;

  char *resultBuffer = NULL;
  int resultBufferSize = 0;

  status = convertCharset(fileContents,
                          contentLength,
                          CCSID_UTF_8,
                          CHARSET_OUTPUT_USE_BUFFER,
                          &convertBuffer,
                          contentLength * 2,
                          NATIVE_CODEPAGE,
                          NULL,
                          &conversionLength,
                          &reasonCode);

  if (status == 0) {
   resultBufferSize = (conversionLength * 3) / 4;
   resultBuffer = safeMalloc(resultBufferSize, "ResultBuffer");
   int dataSize = decodeBase64(convertBuffer, resultBuffer);
   if (dataSize > 0) {
     int writtenLength = 0;
     char *dataToWrite = resultBuffer;
     while (dataSize != 0) {
       writtenLength = fileWrite(file, dataToWrite, dataSize, &returnCode, &reasonCode);
       if (writtenLength >= 0) {
         dataSize -= writtenLength;
         dataToWrite += writtenLength;
       }
       else {
         printf("Error writing to file: return: %d, rsn: %d.\n", returnCode, reasonCode);
         safeFree(resultBuffer, resultBufferSize);
         safeFree(convertBuffer, convertBufferSize);
         return -1;
       }
     }
   }
   else {
     printf("Error decoding BASE64.\n");
     safeFree(resultBuffer, resultBufferSize);
     safeFree(convertBuffer, convertBufferSize);
     return -1;
   }
  }
  else {
    printf("Error converting to EBCDIC.\n");
    safeFree(resultBuffer, resultBufferSize);
    safeFree(convertBuffer, convertBufferSize);
    return -1;
  }

  safeFree(resultBuffer, resultBufferSize);
  safeFree(convertBuffer, convertBufferSize);
  return 0;
}

int writeAsciiDataFromBase64(UnixFile *file, char *fileContents, int contentLength, int sourceEncoding, int targetEncoding) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  int dataToWriteSize = contentLength * 2;

  char *dataToWrite = safeMalloc(dataToWriteSize, "CONVERT BUFFER");
  int dataSize = 0;

  char *resultBuffer = NULL;
  int resultBufferSize = 0;

  status = convertCharset(fileContents,
                          contentLength,
                          CCSID_UTF_8,
                          CHARSET_OUTPUT_USE_BUFFER,
                          &dataToWrite,
                          contentLength * 2,
                          NATIVE_CODEPAGE,
                          NULL,
                          &dataSize,
                          &reasonCode);

  if (status == 0) {
   resultBufferSize = (dataSize * 3) / 4;
   resultBuffer = safeMalloc(resultBufferSize, "ResultBuffer");
   int decodedLength = decodeBase64(dataToWrite, resultBuffer);
   if (decodedLength > 0) {
     /* Disable automatic conversion to prevent any wacky
      * problems that may arise from auto convert.
      */
     status = fileDisableConversion(file, &returnCode, &reasonCode);
     if (status != 0) {
       printf("Failed to disable automatic conversion. Unexpected results may occur.\n");
     }
     status = convertCharset(resultBuffer,
                             decodedLength,
                             sourceEncoding,
                             CHARSET_OUTPUT_USE_BUFFER,
                             &dataToWrite,
                             decodedLength * 2,
                             targetEncoding,
                             NULL,
                             &dataSize,
                             &reasonCode);
     if (status == 0) {
       int writtenLength = 0;
       while (dataSize != 0) {
         writtenLength = fileWrite(file, dataToWrite, dataSize, &returnCode, &reasonCode);
         if (writtenLength >= 0) {
           dataSize -= writtenLength;
           dataToWrite -= writtenLength;
         }
         else {
           printf("Error writing to file: return: %d, rsn: %d.\n", returnCode, reasonCode);
           safeFree(resultBuffer, resultBufferSize);
           safeFree(dataToWrite, dataToWriteSize);
           return -1;
         }
       }
     }
     else {
       printf("Unsupported encoding specified.\n");
       safeFree(resultBuffer, resultBufferSize);
       safeFree(dataToWrite, dataToWriteSize);
       return -1;
     }
   }
   else {
     printf("Error decoding BASE64.\n");
     safeFree(resultBuffer, resultBufferSize);
     safeFree(dataToWrite, dataToWriteSize);
     return -1;
   }
  }
  else {
    printf("Error converting to EBCDIC.\n");
    safeFree(resultBuffer, resultBufferSize);
    safeFree(dataToWrite, dataToWriteSize);
    return -1;
  }

  safeFree(resultBuffer, resultBufferSize);
  safeFree(dataToWrite, dataToWriteSize);
  return 0;
}

static int writeEmptyUnixFile(char *absolutePath, int forceWrite) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;
  int fileExists = FALSE;

  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == 0) {
    if (!forceWrite) {
#ifdef DEBUG
      printf("Writing has stopped because the file already exists and the force flag is off\n");
#endif
      return -1;
    }
    fileExists = TRUE;
    status = tmpFileMake(absolutePath);
    if (status == -1) {
#ifdef DEBUG
      printf("Error occurred while creating tmp file: %s.tmp\n", absolutePath);
#endif
      return -1;
    }
  }

  setUmask(DEFAULT_UMASK);
  UnixFile *dest = fileOpen(absolutePath,
                            FILE_OPTION_CREATE | FILE_OPTION_TRUNCATE | FILE_OPTION_WRITE_ONLY,
                            0,
                            0,
                            &returnCode,
                            &reasonCode);

  if (dest == NULL) {
#ifdef DEBUG
    printf("Failed to open file %s: (return = 0x%x, reason = 0x%x)\n", absolutePath, returnCode, reasonCode);
#endif
    if (fileExists) {
      tmpFileCleanup(absolutePath);
    }
    return -1;
  }

  status = fileClose(dest, &returnCode, &reasonCode);
  if (status == -1) {
#ifdef DEBUG
    printf("Failed to close file %s: (return = 0x%x, reason = 0x%x)\n", absolutePath, returnCode, reasonCode);
#endif
    if (fileExists) {
      tmpFileCleanup(absolutePath);
    }
    else {
      fileDelete(absolutePath, &returnCode, &reasonCode);
    }
    return -1;
  }

  tmpFileDelete(absolutePath);
  return 0;
}

void writeEmptyUnixFileAndRespond(HttpResponse *response, char *absolutePath, int forceWrite) {
  if (!writeEmptyUnixFile(absolutePath, forceWrite)) {
    response200WithMessage(response, "Successfully wrote a file");
  }
  else {
    respondWithJsonError(response, "Failed to write a file", 500, "Internal Server Error");
  }
}

void respondWithUnixFileMetadata(HttpResponse *response, char *absolutePath) {
  FileInfo info;
  int returnCode;
  int reasonCode;
  int status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);

  if (status == 0) {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setResponseStatus(response, 200, "OK");
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);

    int decimalMode = fileUnixMode(&info);
    int octalMode = decimalToOctal(decimalMode);

    ISOTime timeStamp;
    int unixTime = fileInfoUnixCreationTime(&info);
    convertUnixToISO(unixTime, &timeStamp);

    jsonStart(out);
    {
      jsonAddString(out, "path", absolutePath);
      jsonAddBoolean(out, "directory", fileInfoIsDirectory(&info));
      jsonAddInt64(out, "size", fileInfoSize(&info));
      jsonAddInt(out, "ccsid", fileInfoCCSID(&info));
      jsonAddString(out, "createdAt", timeStamp.data);
      jsonAddInt(out, "mode", octalMode);
    }
    jsonEnd(out);

    finishResponse(response);
  }
  else {
    respondWithUnixFileNotFound(response, TRUE);
  }
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

