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

typedef struct CFGConfig_tag CFGConfig;

typedef struct ConfigManager_tag{
  ShortLivedHeap *slh;
  CFGConfig *firstConfig;
  CFGConfig *lastConfig;
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
#define ZCFG_UNKNOWN_CONFIG_NAME 3
#define ZCFG_BAD_CONFIG_PATH 4
#define ZCFG_BAD_JSON_SCHEMA 5
#define ZCFG_BAD_PARMLIB_MEMBER_NAME 6
#define ZCFG_MISSING_CONFIG_SOURCE 7
#define ZCFG_BAD_REXX_ARG_COUNT 8
#define ZCFG_BAD_TRACE_LEVEL 9
#define ZCFG_UNKNOWN_REXX_FUNCTION 10
#define ZCFG_NO_CONFIG_DATA 11
#define ZCFG_POINTER_TOO_DEEP JSON_POINTER_TOO_DEEP
#define ZCFG_POINTER_ARRAY_INDEX_NOT_INTEGER JSON_POINTER_ARRAY_INDEX_NOT_INTEGER 
#define ZCFG_POINTER_ARRAY_INDEX_OUT_OF_BOUNDS JSON_POINTER_ARRAY_INDEX_OUT_OF_BOUNDS 


ConfigManager *makeConfigManager(void);
bool cfgMakeConfig(char *configName,
                   char *configPathArg,
                   char *schemaPath);

CFGConfig *addConfig(ConfigManager *mgr, const char *configName);
CFGConfig *cfgAddConfig(ConfigManager *mgr, const char *configName);
int cfgGetTraceLevel(ConfigManager *mgr);
void cfgSetTraceLevel(ConfigManager *mgr, int traceLevel);
void cfgSetTraceStream(ConfigManager *mgr, FILE *traceOut);
int cfgSetConfigPath(ConfigManager *mgr, const char *configName, char *configPathArg);
int cfgSetParmlibMemberName(ConfigManager *mgr, const char *configName, const char *parmlibMemberName);
int cfgLoadSchemas(ConfigManager *mgr, const char *configName, char *schemaList);
int cfgLoadConfiguration(ConfigManager *mgr, const char *configName);
int cfgValidate(ConfigManager *mgr, JsonValidator *validator, const char *configName);

void freeConfigManager(ConfigManager *mgr);

int loadConfigurations(ConfigManager *mgr, const char *configName);

/* Json-oriented value getters */

int cfgGetStringJ(ConfigManager *mgr, const char *configName, char **result, JsonPointer *jp);

/* Convenience getters for C Programmers, that don't require building paths, string allocation, etc */

int cfgGetIntC(ConfigManager *mgr, const char *configName, int *result, int argCount, ...);
int cfgGetBooleanC(ConfigManager *mgr, const char *configName, bool *result, int argCount, ...);

/** result is null-terminated, when set, and not a copy */
int cfgGetStringC(ConfigManager *mgr, const char *configName, char **result, int argCount, ...);

Json *cfgGetConfigData(ConfigManager *mgr, const char *configName);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
  */
