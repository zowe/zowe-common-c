/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef STORAGE_H
#define STORAGE_H

typedef const char* (*StorageGet)(void *userData, const char *key, int *statusOut);
typedef void (*StorageSet)(void *userData, const char *key, const char *value, int *statusOut);
typedef void (*StorageRemove)(void *userData, const char *key, int *statusOut);
typedef const char* (*StorageGetStrStatus)(void *userData, int status);

typedef struct Storage_tag {
  StorageGet get;
  StorageSet set;
  StorageRemove remove;
  StorageGetStrStatus strStatus;
  void *userData;
} Storage;

void storageSetString(Storage *storage, const char *key, const char *value, int *statusOut);
void storageSetInt(Storage *storage, const char *key, int value, int *statusOut);
void storageSetDouble(Storage *storage, const char *key, double value, int *statusOut);
void storageSetBool(Storage *storage, const char *key, bool value, int *statusOut);
const char *storageGetString(Storage *storage, const char *key, int *statusOut);
int storageGetInt(Storage *storage, const char *key, int *statusOut);
double storageGetDouble(Storage *storage, const char *key, int *statusOut);
bool storageGetBool(Storage *storage, const char *key, int *statusOut);
void storageRemove(Storage *storage, const char *key, int *statusOut);
const char *storageGetStrStatus(Storage *storage, int status);

#ifndef METTLE
typedef struct MemoryStorageOptions_tag {
  int bucketCount;  // number of buckets in hashtable
} MemoryStorageOptions;

Storage *makeMemoryStorage(MemoryStorageOptions *options);
#endif //

#define STORAGE_STATUS_OK                       0 
#define STORAGE_STATUS_KEY_NOT_FOUND            1
#define STORAGE_STATUS_VALUE_NOT_BOOLEAN        2
#define STORAGE_STATUS_VALUE_NOT_INTEGER        3
#define STORAGE_STATUS_INTEGER_OUT_OF_RANGE     4
#define STORAGE_STATUS_VALUE_NOT_DOUBLE         5
#define STORAGE_STATUS_DOUBLE_OUT_OF_RANGE      6

#define STORAGE_STATUS_FIRST_CUSTOM_STATUS 100


#endif // STORAGE_H

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/