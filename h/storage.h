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

/**
 * @brief Key/value storage
 */
typedef struct Storage_tag {
  StorageGet get;
  StorageSet set;
  StorageRemove remove;
  StorageGetStrStatus strStatus;
  void *userData;
} Storage;

/**
 * @brief Put a record with a string data into a key/value storage.
 * @param storage Storage where to put the record into.
 * @param key Unique key of the record.
 * @param value Record data.
 * @param statusOut The status of operation to be returned. STORAGE_STATUS_OK on success.
 */
void storageSetString(Storage *storage, const char *key, const char *value, int *statusOut);

/**
 * @brief Put a record with an integer data into a key/value storage.
 * @param storage Storage where to put the record into.
 * @param key Unique key of the record.
 * @param value Record data.
 * @param statusOut The status of operation to be returned. STORAGE_STATUS_OK on success.
 */
void storageSetInt(Storage *storage, const char *key, int value, int *statusOut);

/**
 * @brief Put a record with a floating-point data into a key/value storage.
 * @param storage Storage where to put the record into.
 * @param key Unique key of the record.
 * @param value Record data.
 * @param statusOut The status of operation to be returned. STORAGE_STATUS_OK on success.
 */
void storageSetDouble(Storage *storage, const char *key, double value, int *statusOut);

/**
 * @brief Put a record with a boolean data into a key/value storage.
 * @param storage Storage where to put the record into.
 * @param key Unique key of the record.
 * @param value Record data.
 * @param statusOut The status of operation to be returned. STORAGE_STATUS_OK on success.
 */
void storageSetBool(Storage *storage, const char *key, bool value, int *statusOut);

/**
 * @brief Get a record with a string data from a key/value storage.
 * @param storage Storage where to get the record from.
 * @param key Unique key of the record.
 * @param statusOut The status of operation to be returned. STORAGE_STATUS_OK on success.
 * @return Record data on success, NULL on failure.
 */
const char *storageGetString(Storage *storage, const char *key, int *statusOut);

/**
 * @brief Get a record with an integer data from a key/value storage.
 * @param storage Storage where to get the record from.
 * @param key Unique key of the record.
 * @param statusOut The status of operation to be returned. STORAGE_STATUS_OK on success.
 * @return Record data on success, NULL on failure.
 */
int storageGetInt(Storage *storage, const char *key, int *statusOut);

/**
 * @brief Get a record with a floating-point number data from a key/value storage.
 * @param storage Storage where to get the record from.
 * @param key Unique key of the record.
 * @param statusOut The status of operation to be returned. STORAGE_STATUS_OK on success.
 * @return Record data on success, NULL on failure.
 */
double storageGetDouble(Storage *storage, const char *key, int *statusOut);

/**
 * @brief Get a record with a boolean data from a key/value storage.
 * @param storage Storage where to get the record from.
 * @param key Unique key of the record.
 * @param statusOut The status of operation to be returned. STORAGE_STATUS_OK on success.
 * @return Record data on success, NULL on failure.
 */
bool storageGetBool(Storage *storage, const char *key, int *statusOut);

/**
 * @brief Remove a record from a key/value storage.
 * @param storage Storage where to remove the record from.
 * @param key Unique key of the record.
 * @param statusOut The status of operation to be returned. STORAGE_STATUS_OK on success.
 */
void storageRemove(Storage *storage, const char *key, int *statusOut);

/**
 * @brief Get a detailed info about status of operation.
 * @param storage Storage the operation made on.
 * @param status Status of operation.
 * @return Status message.
 */
const char *storageGetStrStatus(Storage *storage, int status);

#ifndef METTLE
typedef struct MemoryStorageOptions_tag {
  int bucketCount;  // number of buckets in hashtable
} MemoryStorageOptions;

/**
 * @brief Make in-memory key/value storage.
 * @param options Storage options.
 * @return In-memory storage, NULL on failure.
 */
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