

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef HTTP_FILE_SERVICE_H
#define HTTP_FILE_SERVICE_H

#include "httpserver.h"

#define RC_HTTP_FILE_SERVICE_SUCCESS                             0 
#define RC_HTTP_FILE_SERVICE_NOT_FOUND                           10 
#define RC_HTTP_FILE_SERVICE_ALREADY_EXISTS                      11 
#define RC_HTTP_FILE_SERVICE_PERMISSION_DENIED                   12 
#define RC_HTTP_FILE_SERVICE_INVALID_PATH                        13 
#define RC_HTTP_FILE_SERVICE_UNDEFINED_ERROR                     14
#define RC_HTTP_FILE_SERVICE_INVALID_INPUT                       15 
#define RC_HTTP_FILE_SERVICE_NOT_ABSOLUTE_PATH                   16 
#define RC_HTTP_FILE_SERVICE_CLEANUP_TARGET_DIR_FAILED           17

void response200WithMessage(HttpResponse *response, char *msg);

bool isDir(char *absolutePath);
bool doesFileExist(char *absolutePath);

int createUnixDirectory(char *absolutePath, int forceCreate);
void createUnixDirectoryAndRespond(HttpResponse *response, char *absolutePath, 
                                   int recursive, int forceCreate);

void directoryChangeOwnerAndRespond(HttpResponse *response, char *absolutePath,
        char *userId, char *groupId, char *recursive, char *pattern);

static int deleteUnixDirectory(char *absolutePath);
void deleteUnixDirectoryAndRespond(HttpResponse *response, char *absolutePath);

static int deleteUnixFile(char *absolutePath);
void deleteUnixFileAndRespond(HttpResponse *response, char *absolutePath);

static int renameUnixDirectory(char *oldAbsolutePath, char *newAbsolutePath, int forceRename);
void renameUnixDirectoryAndRespond(HttpResponse *response, char *oldAbsolutePath, char *newAbsolutePath, int forceRename);

static int renameUnixFile(char *oldAbsolutePath, char *newAbsolutePath, int forceRename);
void renameUnixFileAndRespond(HttpResponse *response, char *oldAbsolutePath, char *newAbsolutePath, int forceRename);

static int copyUnixDirectory(char *oldAbsolutePath, char *newAbsolutePath, int forceCopy);
void copyUnixDirectoryAndRespond(HttpResponse *response, char *oldAbsolutePath, char *newAbsolutePath, int forceCopy);

static int copyUnixFile(char *oldAbsolutePath, char *newAbsolutePath, int forceCopy);
void copyUnixFileAndRespond(HttpResponse *response, char *oldAbsolutePath, char *newAbsolutePath, int forceCopy);

void respondWithUnixFileMetadata(HttpResponse *response, char *absolutePath);

static int writeEmptyUnixFile(char *absolutePath, int forceWrite);
void writeEmptyUnixFileAndRespond(HttpResponse *response, char *absolutePath, int forceWrite);
void  directoryChangeModeAndRespond(HttpResponse *response, char * routeFileName,
          char * Recursive, char * mode, char *compare);

int directoryChangeTagAndRespond(HttpResponse *response, char *file,
            char *type, char *codepage, char *Recursive, char *pattern);
int directoryChangeDeleteTagAndRespond(HttpResponse *response, char *file,
            char *type, char *codepage, char *Recursive, char *pattern);

int writeBinaryDataFromBase64(UnixFile *file, char *fileContents, int contentLength);
int writeAsciiDataFromBase64(UnixFile *file, char *fileContents, int contentLength, int sourceEncoding, int targetEncoding);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

