/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
  */

#ifndef __ZOWE_CONFIGMGR__
#define __ZOWE_CONFIGMGR__ 1

#include "jsonschema.h"
#include "embeddedjs.h"

typedef struct ConfigPathElement_tag{
  int    flags;
  char  *name;
  char  *data;
  struct ConfigPathElement_tag *next;
} ConfigPathElement;

typedef struct ConfigManager_tag{
  ShortLivedHeap *slh;
  char *rootSchemaDirectory;
  ConfigPathElement *schemaPath;  /* maybe */
  ConfigPathElement *configPath;
  JsonSchema  *topSchema;
  JsonSchema **otherSchemas;
  int          otherSchemasCount;
  Json       *config;
  hashtable  *schemaCache;
  hashtable  *configCache;
  int         traceLevel;
  FILE       *traceOut;
  EmbeddedJS *ejs;
} ConfigManager;


#define JSON_POINTER_TOO_DEEP 101
#define JSON_POINTER_ARRAY_INDEX_NOT_INTEGER 102
#define JSON_POINTER_ARRAY_INDEX_OUT_OF_BOUNDS 103

/* These eventually need ZWE unique messages */
#define ZCFG_SUCCESS 0
#define ZCFG_TYPE_MISMATCH 1
#define ZCFG_EVAL_FAILURE 2
#define ZCFG_POINTER_TOO_DEEP JSON_POINTER_TOO_DEEP
#define ZCFG_POINTER_ARRAY_INDEX_NOT_INTEGER JSON_POINTER_ARRAY_INDEX_NOT_INTEGER 
#define ZCFG_POINTER_ARRAY_INDEX_OUT_OF_BOUNDS JSON_POINTER_ARRAY_INDEX_OUT_OF_BOUNDS 


ConfigManager *makeConfigManager(char *configPathArg, char *schemaPath,
                                 int traceLevel, FILE *traceOut);

void freeConfigManager(ConfigManager *mgr);

int loadConfigurations(ConfigManager *mgr);

/* Json-oriented value getters */

int cfgGetStringJ(ConfigManager *mgr, char **result, JsonPointer *jp);

/* Convenience getters for C Programmers, that don't require building paths, string allocation, etc */

int cfgGetIntC(ConfigManager *mgr, int *result, int argCount, ...);
int cfgGetBooleanC(ConfigManager *mgr, bool *result, int argCount, ...);

/** result is null-terminated, when set, and not a copy */
int cfgGetStringC(ConfigManager *mgr, char **result, int argCount, ...);

#define cfgGetConfig(cmgr) ((cmgr)->config)

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
  */
