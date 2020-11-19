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
#include <metal/stdbool.h>
#include <metal/string.h>
#include <metal/stdarg.h>  
#include <metal/limits.h>  
#include "metalio.h"
#include "qsam.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>  
#include <limits.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "storage.h"

static const char *TRUE_STRING = "true";
static const char *FALSE_STRING = "false";

static int parseInteger(const char *str, int *statusOut) {
  char *end;
  long longValue = strtol(str, &end, 10);
  if(*end != '\0') {
    *statusOut = STORAGE_VALUE_NOT_INTEGER;
    return 0;
  }
  if (longValue < INT_MIN || longValue > INT_MAX) {
    *statusOut = STORAGE_INTEGER_TOO_LONG;
    return 0;
  }
  *statusOut = STORAGE_OK;
  return (int)longValue;
}

static bool parseBool(const char *str, int *statusOut) {
  if (strcmp(str, TRUE_STRING) && strcmp(str, FALSE_STRING)) {
    *statusOut = STORAGE_VALUE_NOT_BOOLEAN;
    return false;
  }
  *statusOut = STORAGE_OK;
  return !strcmp(str, TRUE_STRING);
}

static const char *MESSAGES[] = {
    [STORAGE_OK] = "OK",
    [STORAGE_KEY_NOT_FOUND] = "Key not found",
    [STORAGE_VALUE_NOT_BOOLEAN] = "Value not boolean",
    [STORAGE_VALUE_NOT_INTEGER] = "Value not integer",
    [STORAGE_INTEGER_TOO_LONG] = "Integer value out of bounds",
};

#define MESSAGE_COUNT sizeof(MESSAGES)/sizeof(MESSAGES[0])

const char *storageGetString(Storage *storage, const char *key, int *statusOut) {
  return storage->get(storage->userData, key, statusOut);
}

int storageGetInt(Storage *storage, const char *key, int *statusOut) {
  int status = 0;
  const char *str = storageGetString(storage, key, &status);
  if (status) {
    *statusOut = status;
    return 0;
  }
  return parseInteger(str, statusOut);
}

bool storageGetBool(Storage *storage, const char *key, int *statusOut) {
  int status = 0;
  const char *str = storageGetString(storage, key, &status);
  if (status) {
    *statusOut = status;
    return false;
  }
  return parseBool(str, statusOut);
}

void storageSetString(Storage *storage, const char *key, const char *value, int *statusOut) {
  storage->set(storage->userData, key, value, statusOut);
}

#define INT_LEN 11

void storageSetInt(Storage *storage, const char *key, int value, int *statusOut) {
  char buf[INT_LEN+1] = {0};
  snprintf (buf, sizeof(buf), "%d", value);
  storageSetString(storage, key, buf, statusOut);
}

void storageSetBool(Storage *storage, const char *key, bool value, int *statusOut) {
  storageSetString(storage, key, value ? TRUE_STRING : FALSE_STRING, statusOut);
}

void storageRemove(Storage *storage, const char *key, int *statusOut) {
  storage->remove(storage->userData, key, statusOut);
}

const char *storageGetStrStatus(Storage* storage, int status) {
  if (status >= STORAGE_FIRST_CUSTOM_STATUS) {
    return storage->strStatus(storage->userData, status);
  }
  if (status >= MESSAGE_COUNT || status < 0) {
    return "Unknown status code";
  }
  const char *message = MESSAGES[status];
  if (!message) {
    return "Unknown status code";
  }
  return message;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/