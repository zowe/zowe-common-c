

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

void response200WithMessage(HttpResponse *response, char *msg);

bool isDir(char *absolutePath);
bool doesFileExist(char *absolutePath);

int createUnixDirectory(char *absolutePath, int forceCreate);
void createUnixDirectoryAndRespond(HttpResponse *response, char *absolutePath, 
                                   int recursive, int forceCreate);

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

