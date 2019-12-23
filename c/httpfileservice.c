

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

/* A generic function to return a 200 OK to the caller.
 * It takes a msg and prints it to JSON.
 */
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

/* Returns a boolean value for whether or not
 * the specified file is a directory or not.
 */
bool isDir(char *absolutePath) {
  int returnCode = 0, reasonCode = 0, status = 0;  
  FileInfo info = {0};

  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    return false;
  }

  return (fileInfoIsDirectory(&info));
}

/* Returns a boolean value for whether or not
 * the specified file/directory exists.
 */
bool doesFileExist(char *absolutePath) {
  int returnCode = 0, reasonCode = 0, status = 0;  
  FileInfo info = {0};

  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    return false;
  }
  
  return true;
}

/* Creates a new unix directory at the specified absolute
 * path. It will only overwrite an existing directory if
 * the forceCreate flag is on.
 */
static int createUnixDirectory(char *absolutePath, int forceCreate) {
  int returnCode = 0, reasonCode = 0, status = 0;  
  FileInfo info = {0};
  
  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == 0) {
    if (!forceCreate) {
      return -1;
    }
  }

  status = directoryMake(absolutePath,
                         0700,
                         &returnCode,
                         &reasonCode);

  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to create directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            absolutePath, returnCode, reasonCode);
    return -1;
  }

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

/* Deletes a unix directory at the specified absolute
 * path.
 */
static int deleteUnixDirectory(char *absolutePath) {
  int returnCode = 0, reasonCode = 0, status = 0;
  FileInfo info = {0};
  
  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to stat directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            absolutePath, returnCode, reasonCode);
    return -1;
  }

  status = directoryDeleteRecursive(absolutePath, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to delete directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            absolutePath, returnCode, reasonCode);
    return -1;
  }

  return 0;
}

void deleteUnixDirectoryAndRespond(HttpResponse *response, char *absolutePath) {
  if (!deleteUnixDirectory(absolutePath)) {
    response200WithMessage(response, "Successfully deleted a directory");
  }
  else {
    respondWithJsonError(response, "Failed to delete a directory", 400, "Bad Request");
  }
}

/* Deletes a unix file at the specified absolute
 * path.
 */
static int deleteUnixFile(char *absolutePath) {
  int returnCode = 0, reasonCode = 0, status = 0;
  FileInfo info = {0};

  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to stat file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            absolutePath, returnCode, reasonCode);
    return -1;
  }

  status = fileDelete(absolutePath, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to delete file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            absolutePath, returnCode, reasonCode);
    return -1;
  }

  return 0;
}

void deleteUnixFileAndRespond(HttpResponse *response, char *absolutePath) {
  if (!deleteUnixFile(absolutePath)) {
    response200WithMessage(response, "Successfully deleted a file");
  }
  else {
    respondWithJsonError(response, "Failed to delete a file", 400, "Bad Request");
  }
}


/* Renames a unix directory at the specified absolute
 * path. It will only overwrite an existing directory
 * if the forceRename flag is on.
 */
static int renameUnixDirectory(char *oldAbsolutePath, char *newAbsolutePath, int forceRename) {
  int returnCode = 0, reasonCode = 0, status = 0;
  FileInfo info = {0};

  status = fileInfo(oldAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to stat directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            oldAbsolutePath, returnCode, reasonCode);
    return -1;
  }

  status = fileInfo(newAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == 0 && !forceRename) {
    return -1;
  }

  status = directoryRename(oldAbsolutePath, newAbsolutePath, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to rename directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            oldAbsolutePath, returnCode, reasonCode);
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

/* Renames a unix file at the specified absolute
 * path. It will only overwrite an existing file
 * if the forceRename flag is on.
 */
static int renameUnixFile(char *oldAbsolutePath, char *newAbsolutePath, int forceRename) {
  int returnCode = 0, reasonCode = 0, status = 0;
  FileInfo info = {0};

  status = fileInfo(oldAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to stat file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            oldAbsolutePath, returnCode, reasonCode);
    return -1;
  }

  status = fileInfo(newAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == 0 && !forceRename) {
    return -1;
  }

  status = fileRename(oldAbsolutePath, newAbsolutePath, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to rename file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            oldAbsolutePath, returnCode, reasonCode);
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

/* Copies the contents of a unix directory to a new
 * directory. It will only overwrite an existing directory
 * if the forceCopy flag is on. At this time, copy will only
 * copy the contents and nothing else.
 */
static int copyUnixDirectory(char *oldAbsolutePath, char *newAbsolutePath, int forceCopy) {
  int returnCode = 0, reasonCode = 0, status = 0;
  FileInfo info = {0};

  status = fileInfo(oldAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to stat directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            oldAbsolutePath, returnCode, reasonCode);
    return -1;
  }
  
  status = fileInfo(newAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == 0 && !forceCopy) {
    return -1;
  }

  status = directoryCopy(oldAbsolutePath, newAbsolutePath, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to copy directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            oldAbsolutePath, returnCode, reasonCode);
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

/* Copies the contents of a unix file to a new
 * file. It will only overwrite an existing file
 * if the forceCopy flag is on. At this time, copy will only
 * copy the contents and nothing else.
 */
static int copyUnixFile(char *oldAbsolutePath, char *newAbsolutePath, int forceCopy) {
  int returnCode = 0, reasonCode = 0, status = 0;
  FileInfo info = {0};

  status = fileInfo(oldAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to stat file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            oldAbsolutePath, returnCode, reasonCode);
    return -1;
  }
  
  status = fileInfo(newAbsolutePath, &info, &returnCode, &reasonCode);
  if (status == 0 && !forceCopy) {
    return -1;
  }

  status = fileCopy(oldAbsolutePath, newAbsolutePath, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to copy file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            oldAbsolutePath, returnCode, reasonCode);
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

/* Creates an empty unix file with no tag. This is
 * done by mimicing the touch command.
 */
static int writeEmptyUnixFile(char *absolutePath, int forceWrite) {
  int returnCode = 0, reasonCode = 0, status = 0;
  FileInfo info = {0};

  status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  if (status == 0 && !forceWrite) {
    return -1;
  }

  UnixFile *dest = fileOpen(absolutePath,
                            FILE_OPTION_CREATE | FILE_OPTION_TRUNCATE | FILE_OPTION_WRITE_ONLY,
                            0700,
                            0,
                            &returnCode,
                            &reasonCode);

  if (dest == NULL) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to open file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            absolutePath, returnCode, reasonCode);
    return -1;
  }

  status = fileClose(dest, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to close file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            absolutePath, returnCode, reasonCode);
    return -1;
  }

  return 0;
}

#define CCSID_MESSAGE_LENGTH  60
/* Modifies the mode of files/directories */
int directoryChangeDeleteTagAndRespond(HttpResponse *response, char *file,
            char *type, char *codepage, char *Recursive, char *pattern) {

  int ccsid;
  ccsid =  findCcsidId(codepage);
  if (codepage != NULL){
    respondWithJsonError(response, "DELETE request with codeset", 400, "Bad Request");
    return 0;
  }
  return directoryChangeTagAndRespond(response, file,
               type, codepage, Recursive, pattern); 
}

/* Change Tag Recursively */
int directoryChangeTagAndRespond(HttpResponse *response, char *file,
            char *type, char *codepage, char *Recursive, char *pattern) {
  int returnCode = 0, reasonCode = 0;
  int recursive = 0;
  int ccsid;
  bool pure;
  char message[CCSID_MESSAGE_LENGTH] = {0};

  if (!strcmp(strupcase(Recursive),"TRUE")) {
    recursive = 1;
  }

  if (-1 ==  patternChangeTagTest(message, sizeof (message),
                     type, codepage, &pure, &ccsid)){
    respondWithJsonError(response, message, 400, "Bad Request");
    return 0;
  }

  /* Call recursive change mode */
  if (!directoryChangeTagRecursive(file, type, codepage, recursive, pattern,
      &returnCode, &reasonCode )) {
    response200WithMessage(response, "Successfully Modify Tags");

  }
  else {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_WARNING,
            "Failed to change tag file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n",
            file, returnCode, reasonCode);
    respondWithJsonError(response, "Failed to Change file tag", 500, "Bad Request");
  }
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

/* Gets the metadata of a unix file and returns it to the
 * caller.
 */
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

/* Writes binary data to a unix file by:
 *
 * 1. Decoding the data from base64.
 * 2. Converting the data to EBCDIC.
 */
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

/* Writes text data to a unix file by:
 *
 * 1. Decoding the data from base64.
 * 2. Converting the data to the specified encoding.
 * 3. Disabling auto conversion.
 */
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

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

