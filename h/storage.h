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
void storageSetBool(Storage *storage, const char *key, bool value, int *statusOut);
const char *storageGetString(Storage *storage, const char *key, int *statusOut);
int storageGetInt(Storage *storage, const char *key, int *statusOut);
bool storageGetBool(Storage *storage, const char *key, int *statusOut);
void storageRemove(Storage *storage, const char *key, int *statusOut);
const char *storageGetStrStatus(Storage *storage, int status);

#ifndef METTLE
Storage *makeMemoryStorage();
#endif //

#define STORAGE_OK                 0 
#define STORAGE_KEY_NOT_FOUND      1
#define STORAGE_VALUE_NOT_BOOLEAN  2
#define STORAGE_VALUE_NOT_INTEGER  3
#define STORAGE_INTEGER_TOO_LONG   4

#define STORAGE_FIRST_CUSTOM_STATUS 100


#endif // STORAGE_H