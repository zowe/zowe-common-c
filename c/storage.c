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
#include <metal/math.h>
#include "metalio.h"
#include "qsam.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>  
#include <limits.h>
#include <math.h>
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
    *statusOut = STORAGE_STATUS_VALUE_NOT_INTEGER;
    return 0;
  }
  if (longValue < INT_MIN || longValue > INT_MAX) {
    *statusOut = STORAGE_STATUS_INTEGER_OUT_OF_RANGE;
    return 0;
  }
  *statusOut = STORAGE_STATUS_OK;
  return (int)longValue;
}

static double parseDouble(const char *str, int *statusOut) {
  char *end;
  double value = strtod(str, &end);
  if(*end != '\0') {
    *statusOut = STORAGE_STATUS_VALUE_NOT_DOUBLE;
    return 0.0;
  }
  if (value == HUGE_VAL || value == -HUGE_VAL) {
    *statusOut = STORAGE_STATUS_DOUBLE_OUT_OF_RANGE;
    return 0.0;
  }
  *statusOut = STORAGE_STATUS_OK;
  return value;
}

static bool parseBool(const char *str, int *statusOut) {
  *statusOut = STORAGE_STATUS_OK;
  if (!strcmp(str, TRUE_STRING)) {
    return true;
  }
  if (!strcmp(str, FALSE_STRING)) {
    return false;
  }
  *statusOut = STORAGE_STATUS_VALUE_NOT_BOOLEAN;
  return false;
}

static const char *MESSAGES[] = {
    [STORAGE_STATUS_OK] = "OK",
    [STORAGE_STATUS_KEY_NOT_FOUND] = "Key not found",
    [STORAGE_STATUS_VALUE_NOT_BOOLEAN] = "Value not boolean",
    [STORAGE_STATUS_VALUE_NOT_INTEGER] = "Value not integer",
    [STORAGE_STATUS_INTEGER_OUT_OF_RANGE] = "Integer out of range",
    [STORAGE_STATUS_VALUE_NOT_DOUBLE] = "Value not floating-point number",
    [STORAGE_STATUS_DOUBLE_OUT_OF_RANGE] = "Double out of range",
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

double storageGetDouble(Storage *storage, const char *key, int *statusOut) {
  int status = 0;
  const char *str = storageGetString(storage, key, &status);
  if (status) {
    *statusOut = status;
    return 0;
  }
  return parseDouble(str, statusOut);
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

#define DOUBLE_LEN 23

void storageSetDouble(Storage *storage, const char *key, double value, int *statusOut) {
  char buf[DOUBLE_LEN+1] = {0};
  snprintf (buf, sizeof(buf), "%f", value);
  storageSetString(storage, key, buf, statusOut);
}

void storageSetBool(Storage *storage, const char *key, bool value, int *statusOut) {
  storageSetString(storage, key, value ? TRUE_STRING : FALSE_STRING, statusOut);
}

void storageRemove(Storage *storage, const char *key, int *statusOut) {
  storage->remove(storage->userData, key, statusOut);
}

const char *storageGetStrStatus(Storage* storage, int status) {
  if (status >= STORAGE_STATUS_FIRST_CUSTOM_STATUS) {
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

#ifdef _TEST_STORAGE

#include <assert.h>
#include <math.h>
#include <float.h>

void testStorage(Storage *storage) {
  int status = STORAGE_STATUS_OK;
  const char *keyA = "keyA";
  const char *sVal = "string-value";
  storageSetString(storage, keyA, sVal, &status);
  printf ("set str ['%s'] = '%s', status %d - %s\n", keyA, sVal, status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_OK);
  
  const char *keyB = "keyB";
  int iVal = 123;
  storageSetInt(storage, keyB, iVal, &status);
  printf ("set int ['%s'] = %d, status %d - %s\n", keyB, iVal, status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_OK);

  const char *keyC = "keyC";
  bool bVal = false;
  storageSetBool(storage, keyC, bVal, &status);
  printf ("set bool ['%s'] = %s, status %d - %s\n", keyC, bVal ? "true" : "false", status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_OK);

  const char *keyD = "keyD";
  double dVal = 123.456;
  storageSetDouble(storage, keyD, dVal, &status);
  printf ("set double ['%s'] = %f, status %d - %s\n", keyD, dVal, status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_OK);

  sVal = storageGetString(storage, keyA, &status);
  printf ("get str ['%s'] = '%s', status %d - %s\n", keyA, sVal, status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_OK);
  assert(0 == strcmp(sVal, "string-value"));

  iVal = storageGetInt(storage, keyB, &status);
  printf ("get int ['%s'] = %d, status %d - %s\n", keyB, iVal, status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_OK);
  assert(iVal == 123);

  iVal = storageGetInt(storage, keyA, &status);
  printf ("get bad int ['%s'] = %d, status %d - %s\n", keyA, iVal, status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_VALUE_NOT_INTEGER);
  assert(iVal == 0);

  const char *badKey = "badKey";
  iVal =  storageGetInt(storage, badKey, &status);
  printf ("get int ['%s'] = %d, status %d - %s\n", badKey, iVal, status, storageGetStrStatus(storage, status));
  assert(status = STORAGE_STATUS_KEY_NOT_FOUND);
  assert(iVal == 0);
  
  bVal =  storageGetBool(storage, keyC, &status);
  printf ("get bool ['%s'] = %s, status %d - %s\n", keyC, bVal ? "true" : "false", status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_OK);
  assert(bVal == false);

  dVal =  storageGetDouble(storage, keyD, &status);
  printf ("get double ['%s'] = %f, status %d - %s\n", keyD, dVal, status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_OK);
  assert(fabs(dVal - 123.456) < DBL_EPSILON);

  dVal = storageGetDouble(storage, keyA, &status);
  printf ("get bad double ['%s'] = %f, status %d - %s\n", keyA, dVal, status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_VALUE_NOT_DOUBLE);
  assert(fabs(dVal) < DBL_EPSILON);
  
  storageRemove(storage, keyC, &status);
  printf ("remove '%s', status %d - %s\n", keyC, status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_OK);
  
  bVal = storageGetBool(storage, keyC, &status);
  printf ("get bool ['%s'] = %s, status %d - %s\n", keyC, bVal ? "true" : "false", status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_KEY_NOT_FOUND);
  assert(bVal == false);
  
  storageRemove(storage, keyC, &status);
  printf ("remove '%s', status %d - %s\n", keyC, status, storageGetStrStatus(storage, status));
  assert(status == STORAGE_STATUS_KEY_NOT_FOUND);
}

#endif // _TEST_STORAGE

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/