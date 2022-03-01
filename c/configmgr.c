

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
#include <metal/stdarg.h>
#include <metal/ctype.h>  
#include <metal/stdbool.h>  
#include "metalio.h"
#include "qsam.h"

#else

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>  
#include <sys/stat.h>
#include <sys/types.h> 
#include <stdint.h> 
#include <stdbool.h> 
#include <errno.h>
#include <math.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "bpxnet.h"
#include "unixfile.h"
#include "json.h"
#include "jsonschema.h"
#include "yaml.h"
#include "yaml2json.h"
#include "embeddedjs.h"
#include "charsets.h"
#include "collections.h"

#ifdef __ZOWE_OS_WINDOWS
typedef int64_t ssize_t;
#include "winregex.h"
#else
#include "psxregex.h"  /* works on ZOS and Linux */
#endif


/* A configuration manager is a facility providing access to a set of configuration source
   (most likely textual) in many formats in one or more locations whose data definition is
   provided by a set of JSON Schema Definions.

   Formats
     YAML
     JSON
     PARMLIB
     Environment Variables 

   Paths
     Configs and Schemas Live on paths of higher-to-lower precedence directories and libraries
     (PDS/PDSE's).  
 

   -- References ----------

     http://json-schema.org

     https://datatracker.ietf.org/doc/html/draft-bhutton-json-schema-validation)


    -- Compilation ----------

    set QJS=c:\repos\quickjs-portable  ## wherever you git cloned quickjs-portable
      
    set YAML=c:\repos\libyaml   ## Wherever you git clone'd libyaml

    clang++ -c ../platform/windows/cppregex.cpp ../platform/windows/winregex.cpp

    clang -I %QJS%\porting -I%YAML%/include -I %QJS% -I./src -I../h -I ../platform/windows -DCONFIG_VERSION=\"2021-03-27\" -Dstrdup=_strdup -D_CRT_SECURE_NO_WARNINGS -DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 -DYAML_VERSION_STRING=\"0.2.5\" -DYAML_DECLARE_STATIC=1 --rtlib=compiler-rt -o configmgr.exe configmgr.c embeddedjs.c %QJS%\quickjs.c %QJS%\cutils.c %QJS%\quickjs-libc.c %QJS%\libbf.c %QJS%\libregexp.c %QJS%\libunicode.c %QJS%\porting\winpthread.c %QJS%\porting\wintime.c %QJS%\porting\windirent.c %QJS%\porting\winunistd.c %YAML%/src/api.c %YAML%/src/reader.c %YAML%/src/scanner.c %YAML%/src/parser.c %YAML%/src/loader.c %YAML%/src/writer.c %YAML%/src/emitter.c %YAML%/src/dumper.c ../c/yaml2json.c ../c/jsonschema.c ../c/json.c ../c/xlate.c ../c/charsets.c ../c/winskt.c ../c/logging.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc.c cppregex.o winregex.o

    configmgr "../tests/schemadata" "" "LIBRARY(FOO):DIR(BAR)" yak

    configmgr -s "../tests/schemadata" -p "LIBRARY(FOO):DIR(BAR)" extract "/zowe/setup/"

    configmgr -s "../tests/schemadata" -p "FILE(../tests/schemadata/zoweoverrides.yaml):FILE(../tests/schemadata/zowebase.yaml)" validate

    configmgr -t 2 -s "../tests/schemadata" -p "FILE(../tests/schemadata/zoweoverrides.yaml):FILE(../tests/schemadata/zowebase.yaml)" validate

    configmgr -s "../tests/schemadata" -p "FILE(../tests/schemadata/zoweoverrides.yaml):FILE(../tests/schemadata/zowebase.yaml)" extract "/zowe/setup/mvs/proclib"


    -- Compilation with XLCLang on ZOS -------------------------------------
 
    export YAML="/u/zossteam/jdevlin/git2022/libyaml" 
    export QJS="/u/zossteam/jdevlin/git2022/quickjs"

    NOTE!! charsets.c requires a path that is NOT unix path.  xlclang and clang will not support this

      Easy, maybe temporary workaround is to copy this H file somewhere
    
      cp "//'SYS1.SCUNHF(CUNHC)'" ../h/CUNHC.h 

    xlclang -q64 -qascii -DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 -DYAML_VERSION_STRING=\"0.2.5\" -DYAML_DECLARE_STATIC=1 -DCONFIG_VERSION=\"2021-03-27\" -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DSUBPOOL=132 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -I ../h -I ../platform/posix -I ${YAML}/include -I ${QJS} -Wbitwise-op-parentheses -o configmgr configmgr.c yaml2json.c embeddedjs.c jsonschema.c json.c xlate.c charsets.c zosfile.c logging.c recovery.c scheduling.c zos.c le.c collections.c timeutls.c utils.c alloc.c ../platform/posix/psxregex.c ${QJS}/quickjs.c ${QJS}/cutils.c ${QJS}/quickjs-libc.c ${QJS}/libregexp.c ${QJS}/libunicode.c ${QJS}/porting/polyfill.c ${YAML}/src/api.c ${YAML}/src/reader.c ${YAML}/src/scanner.c ${YAML}/src/parser.c ${YAML}/src/loader.c ${YAML}/src/writer.c ${YAML}/src/emitter.c ${YAML}/src/dumper.c 

    

*/

#define CONFIG_PATH_OMVS_FILE    0x0001
#define CONFIG_PATH_OMVS_DIR     0x0002
#define CONFIG_PATH_OMVS_LIBRARY 0x0004

typedef struct ConfigPathElement_tag {
  int    flags;
  char  *name;
  char  *data;
  struct ConfigPathElement_tag *next;
} ConfigPathElement;

typedef struct ConfigManager_tag {
  ShortLivedHeap *slh;
  char *rootSchemaDirectory;
  ConfigPathElement *schemaPath;  /* maybe */
  ConfigPathElement *configPath;
  JsonSchema *topSchema;
  Json       *config;
  hashtable  *schemaCache;
  hashtable  *configCache;
  int         traceLevel;
  FILE       *traceOut;
  EmbeddedJS *ejs;
} ConfigManager;

#ifdef __ZOWE_OS_WINDOWS
static int stdoutFD(){
  return _fileno(stdout);
}

static int stderrFD(){
  return _fileno(stdout);
}
#else
static int stdoutFD(){
#ifdef STDOUT_FILENO
  return STDOUT_FILENO;
#else
  return 1;
#endif
}

static int stderrFD(){
#ifdef STDERR_FILENO
  return STDERR_FILENO;
#else
  return 2;
#endif
}
#endif


#define INFO 0
#define DEBUG 1
#define DEBUG2 2

static void trace(ConfigManager *mgr, int level, char *formatString, ...){
  if (mgr && mgr->traceLevel >= level){
    va_list argPointer; 
    va_start(argPointer,formatString);
    vfprintf(mgr->traceOut,formatString,argPointer);
    va_end(argPointer);
  }
}

/* configlet semantcs *IS* javascript semantics (and-or-not scalars)
   Configs live in product read-only directories  

   configrunner handles all the yaml/json/parmlib that the ZWE scripts need

   */

typedef struct ConfigletParm_tag {
  int type;  // int,string
  char *name;
} ConfigletParm;

typedef struct ConfigVersion_tag {
  int major;
  int minor;
  int patch;
} ConfigVersion;



#define CONFIG_ACTION_MKDIR  0x0001
#define CONFIG_ACTION_CHMOD  0x0002
#define CONFIG_ACTION_RMDIR  0x0003
#define CONFIG_ACTION_RM     0x0004

#define CONFIG_ACTION_SCRIPT_STEP 0x0005 

#define CONFIG_ACTION_SET_LOGICAL_DIR 0x0100

#define CONFIG_SERIAL_COMPOUND_ACTION
#define CONFIG_PARALLEL_COMPOUND_ACTION 

#define CONFIG_ACTION_UNPAX  0x0200

/* An action is an expression that
   1) returns status information
   2) logs the fact that it ran and status with args

   
   Only Actions change the status of the system.
   
   Expressions are exactly what they sound like
     1) Functional, stateless
     2) Arith, logic, 
     
*/


typedef struct ConfigAction_tag {
  int type;
  ArrayList parameters; /* ConfigletParms's.  Formal Parameters, not values  */
} ConfigAction;

typedef struct ConfigPrerequisite_tag {
  char *name;
  ConfigVersion *minVersion;
} ConfigPrerequisite;

typedef struct Configlet_tag {
  char   *fullyQualifiedName; // dotted namespace, like org.zowe...
  char   *displayName;
  char   *schemaFilename;
  ConfigVersion *version;
  ArrayList      prerequisites; 
} Configlet;



/* 
   zwe install doesn't set up to clone well HA

   targets:
   zowe_early 
   zowe 1.X_to_2.0 migrate 
   zss_install

   Scenario 1
   
   Optionally Get Pax into install (unpax) dir
     
   build instance dir if not there  
      copy files into instance directory 
      build instance yaml

   Saturday      
   What do these do
     Zwe install   carrying stuff into the right places  (node pre-req-no-code)
     Zwe init      need these post-install or post-SMPE

     Data first, for config

     QuickJS
     - Native Packed Decimal
   
*/

static char *substring(ConfigManager *mgr, char *s, int start, int end){
  int len = end-start;
  char *sub = (mgr ? SLHAlloc(mgr->slh,len+1) : safeMalloc(len+1,"ConfigMgr.substring"));
  memcpy(sub,s+start,len);
  sub[len] = 0;
  return sub;
}

static char *makeFullPath(ConfigManager *mgr, char *path, char *simpleFilename, bool useSLH){
  int pathLen = strlen(path);
  int fileLen = strlen(simpleFilename);
  int len = pathLen + fileLen + 4;
  bool needsSlash = path[pathLen-1] != '/';
  char *result = (useSLH ? SLHAlloc(mgr->slh, len) : malloc(len));
  snprintf(result,len,"%s%s%s",
           path,(needsSlash ? "/" : ""),simpleFilename);
  return result;
}

static char *extractMatch(ConfigManager *mgr, char *s, regmatch_t *match){
  return substring(mgr,s,(int)match->rm_so,(int)match->rm_eo);
}

ConfigPathElement *makePathElement(ConfigManager *mgr, int flags, char *name, char *data){
  ConfigPathElement *element = (ConfigPathElement*)SLHAlloc(mgr->slh,sizeof(ConfigPathElement));
  memset(element,0,sizeof(ConfigPathElement));
  element->flags = flags;
  element->name = name;
  element->data = data;
  if (mgr->configPath == NULL){
    mgr->configPath = element;
  } else {
    ConfigPathElement *tail = mgr->configPath;
    while (tail->next){
      tail = tail->next;
    }
    tail->next = element;
  }
  return element;
}

static bool addPathElement(ConfigManager *mgr, char *pathElementArg){
  ConfigPathElement *element = (ConfigPathElement*)SLHAlloc(mgr->slh,sizeof(ConfigPathElement));
  memset(element,0,sizeof(ConfigPathElement));

  regmatch_t matches[10];
  regex_t *argPattern = regexAlloc();
  /* Nice Regex test site */
  char *pat = "^(LIBRARY|DIR|FILE)\\(([^)]+)\\)$";
  int compStatus = regexComp(argPattern,pat,REG_EXTENDED);
  if (compStatus != 0){
    trace(mgr,INFO,"Internal error, pattern compilation failed\n");
    return false;
  }
  if (!strcmp(pathElementArg,"PARMLIBS")){
    /* need ZOS impl */
    printf("implement me\n");
    return false;
  } else if (regexExec(argPattern,pathElementArg,10,matches,0) == 0){
    char *elementTypeName = extractMatch(mgr,pathElementArg,&matches[1]);
    char *elementData = extractMatch(mgr,pathElementArg,&matches[2]);
    int flags = 0;
    if (!strcmp(elementTypeName,"LIBRARY")){
      flags = CONFIG_PATH_OMVS_LIBRARY;
    } else if (!strcmp(elementTypeName,"DIR")){
      flags = CONFIG_PATH_OMVS_DIR;
    } else if (!strcmp(elementTypeName,"FILE")){
      flags = CONFIG_PATH_OMVS_FILE;
      
    } else {
      return false; /* internal logic error */
    }
    makePathElement(mgr,flags,pathElementArg,elementData);
    return true;
  } else {
    trace(mgr,DEBUG,"unhandled path element '%s'\n",pathElementArg);
    return false;
  }

}

static int buildConfigPath(ConfigManager *mgr, char *configPathArg){
  printf("JOE buildConfigPath\n");
  fflush(stdout);
  int pos = 0;
  int len = strlen(configPathArg);
  while (pos < len){
    int nextColon = indexOf(configPathArg,len,':',pos);
    int nextPos;
    int pathElementLength = 0;
    trace(mgr,DEBUG2,"nextColon = %d\n",nextColon);

    if (nextColon == -1){
      nextPos = len;
      pathElementLength = len - pos;
    } else {
      nextPos = nextColon+1;
      pathElementLength = nextColon - pos;
    }

    trace(mgr,DEBUG2,"ckpt.2 nextPos=%d\n",nextPos);

    char *pathElementArg = substring(mgr,configPathArg,pos,pos+pathElementLength);

    trace(mgr,DEBUG2,"ckpt.3\n");

    bool status = addPathElement(mgr,pathElementArg);

    if (status == false){
      trace(mgr,INFO,"path building failed\n");
      return 12;
    }
    pos = nextPos;
  }
  return 0;
}

static void printConfigPath(ConfigManager *mgr){
  /* Thurs AM */
  trace(mgr,INFO,"Path Elements: mgr=0x%p\n",mgr);
  ConfigPathElement *element = mgr->configPath;
  while (element){
    trace(mgr,INFO,"  %04X %s\n",element->flags,element->name);
    element = element->next;
  }
}

static void jsonPrettyPrint(ConfigManager *mgr, Json *json){
  jsonPrinter *p = makeJsonPrinter(mgr->traceOut == stdout ? stdoutFD() : stderrFD());
  jsonEnablePrettyPrint(p);
  jsonPrint(p,json);
  trace(mgr,INFO,"\n");
  
}

#define ZOWE_SCHEMA_FILE "zoweyaml.schema"

void freeConfigManager(ConfigManager *mgr);

ConfigManager *makeConfigManager(char *configPathArg, char *rootSchemaDirectory,
                                 int traceLevel, FILE *traceOut){
  ConfigManager *mgr = (ConfigManager*)safeMalloc(sizeof(ConfigManager),"ConfigManager");

  memset(mgr,0,sizeof(ConfigManager));
  mgr->traceLevel = traceLevel;
  mgr->traceOut = traceOut;
  mgr->slh = makeShortLivedHeap(0x10000,0x100);
  EmbeddedJS *ejs = makeEmbeddedJS(NULL);
  printf("really\n");
  fflush(stdout);
  mgr->ejs = ejs;
  trace(mgr,DEBUG,"before build config path\n");
  if (buildConfigPath(mgr,configPathArg)){
    printf("built config path failed\n");fflush(stdout);
    safeFree((char*)mgr,sizeof(ConfigManager));
    return NULL;
  }
  trace(mgr,DEBUG,"built config path\n");
  
  mgr->rootSchemaDirectory = rootSchemaDirectory;
  char *zoweYamlPath = makeFullPath(mgr,rootSchemaDirectory,ZOWE_SCHEMA_FILE,true);
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo yamlFileInfo;
  trace(mgr,DEBUG,"before file info\n");
  int infoStatus = fileInfo(zoweYamlPath,&yamlFileInfo,&returnCode,&reasonCode);
  if (infoStatus){
    trace(mgr,INFO,"failed to get fileInfo of '%s', infoStatus=%d\n",zoweYamlPath,infoStatus);
    freeConfigManager(mgr);
    return NULL;
  }
  char errorBuffer[1024];
  trace(mgr,DEBUG,"before jsonParseFile info\n");
  Json *jsonWithSchema = jsonParseFile2(mgr->slh,zoweYamlPath,errorBuffer,1024);
  if (jsonWithSchema == NULL){
    trace(mgr,INFO,"failed to read JSON with base schema: %s\n",errorBuffer);
    freeConfigManager(mgr);
    return NULL;
  }
  if (mgr->traceLevel >= 1){
    jsonPrettyPrint(mgr,jsonWithSchema);

  }
  JsonSchemaBuilder *builder = makeJsonSchemaBuilder(DEFAULT_JSON_SCHEMA_VERSION);
  JsonSchema *schema = jsonBuildSchema(builder,jsonWithSchema);
  if (schema == NULL){
    printf("Schema Build Failed: %s\n",builder->errorMessage);
    freeJsonSchemaBuilder(builder); 
    freeConfigManager(mgr);
    return NULL;
  } else {
    trace(mgr,INFO,"JSON Schema built successfully\n");
    mgr->topSchema = schema;
  }
  freeJsonSchemaBuilder(builder); 
  return mgr;
}

#define YAML_ERROR_MAX 1024

static Json *readJson(ConfigManager *mgr, ConfigPathElement *pathElement){
  char errorBuffer[YAML_ERROR_MAX];
  if (pathElement->flags & CONFIG_PATH_OMVS_FILE){
    trace(mgr,DEBUG,"before read YAML file=%s\n",pathElement->data);
    yaml_document_t *doc = readYAML(pathElement->data,errorBuffer,YAML_ERROR_MAX);
    trace(mgr,DEBUG,"yaml doc at 0x%p\n",doc);
    if (doc){
      if (mgr->traceLevel >= 1){
        pprintYAML(doc);
      }
      return yaml2JSON(doc,mgr->slh);
    } else {
      trace(mgr,INFO,"WARNING, yaml read failed\n");
      return NULL;
    }
    
  } else {
    trace(mgr,INFO,"WARNING, only simple file case yet implemented\n");
    return NULL;
  }
}

void freeConfigManager(ConfigManager *mgr){
  SLHFree(mgr->slh);
  safeFree((char*)mgr,sizeof(ConfigManager));  
}

#define JSON_POINTER_TOO_DEEP 101
#define JSON_POINTER_ARRAY_INDEX_NOT_INTEGER 102
#define JSON_POINTER_ARRAY_INDEX_OUT_OF_BOUNDS 103

#define ZCFG_ALLOC_HEAP 1
#define ZCFG_ALLOC_SLH  2
#define ZCFG_ALLOC_MEMCPY 3

/* These eventually need ZWE unique messages */
#define ZCFG_SUCCESS 0
#define ZCFG_TYPE_MISMATCH 1
#define ZCFG_EVAL_FAILURE 2
#define ZCFG_POINTER_TOO_DEEP JSON_POINTER_TOO_DEEP
#define ZCFG_POINTER_ARRAY_INDEX_NOT_INTEGER JSON_POINTER_ARRAY_INDEX_NOT_INTEGER 
#define ZCFG_POINTER_ARRAY_INDEX_OUT_OF_BOUNDS JSON_POINTER_ARRAY_INDEX_OUT_OF_BOUNDS 


/* need to collect violations as this goes */
static int overloadConfiguration(ConfigManager *mgr,
                                 ConfigPathElement *pathElement,
                                 ConfigPathElement *pathTail){
  if (pathTail == NULL){
    trace(mgr, DEBUG2, "at end of config path\n");
    mgr->config = readJson(mgr,pathElement);
    trace(mgr, DEBUG2, "mgr->config = 0x%p\n", mgr->config);
    return 0; /* success */
  } else {
    Json *overlay = readJson(mgr,pathElement);
    int rhsStatus = overloadConfiguration(mgr,pathTail,pathTail->next);
    trace(mgr, DEBUG2, "read the overlay with json=0x%p and status=%d\n",overlay,rhsStatus);
    if (rhsStatus){
      return rhsStatus; /* don't merge if we couldn't load what's to the right in the list */
    }
    int mergeStatus = 0;
    mgr->config = jsonMerge(mgr->slh,overlay,mgr->config,
                            JSON_MERGE_FLAG_CONCATENATE_ARRAYS,
                            &mergeStatus);
    return mergeStatus;
  }
}

static int loadConfigurations(ConfigManager *mgr){
  ConfigPathElement *pathElement = mgr->configPath;
  int overloadStatus = overloadConfiguration(mgr,pathElement,pathElement->next);
  if (overloadStatus){
    return overloadStatus;
  } else {
    Json *evaluatedConfig = evaluateJsonTemplates(mgr->ejs,mgr->slh,mgr->config);
    if (evaluatedConfig){
      mgr->config = evaluatedConfig;
      return ZCFG_SUCCESS;
    } else {
      return ZCFG_EVAL_FAILURE;
    }
  }
}
  

/* Merging notes
   defaulting and merging requires the same data shape at all levels and sources 
*/

/* all calls return status */

/* zowe.setup.mvs.proclib */

int cfgGetInt(ConfigManager *mgr, int *value, ...){  /* path elements which are strings and ints for array indexing */
  return 0;
}

int cfgGetInt64(ConfigManager *mgr, int64 *value, ...){
  return 0;
}


static Json *jsonPointerDereference(Json *json, JsonPointer *jsonPointer, int *errorReason, int traceLevel){
  ArrayList *elements = &(jsonPointer->elements);
  Json *value = json;
  for (int i=0; i<elements->size; i++){
    JsonPointerElement *element = (JsonPointerElement*)arrayListElement(elements,i);
    if (traceLevel >= 1){
      printf("deref elt=0x%p, i=%d value=0x%p\n",element,i,value);fflush(stdout);
    }
    if (jsonIsArray(value)){
      JsonArray *array = jsonAsArray(value);
      if (element->type == JSON_POINTER_INTEGER){
        int index = atoi(element->string);
        int arraySize = jsonArrayGetCount(array);
        if (index >= arraySize){
          *errorReason = JSON_POINTER_ARRAY_INDEX_OUT_OF_BOUNDS;
          return NULL;
        }
        value = jsonArrayGetItem(array,index);
      } else {
        *errorReason = JSON_POINTER_ARRAY_INDEX_NOT_INTEGER;
        return NULL;
      }
    } else if (jsonIsObject(value)){
      JsonObject *object = jsonAsObject(value);
      value = jsonObjectGetPropertyValue(object,element->string);
      if (value == NULL){
        *errorReason = JSON_POINTER_TOO_DEEP;
        return NULL;
      }
    } else {
      printf("SSS\n");fflush(stdout);
      printf("cannot dereference scalar\n");
      fflush(stdout);
      return NULL;
    }
  }
  return value;
}

int cfgGetStringJ(ConfigManager *mgr, Json **result, JsonPointer *jp){
  int errorReason = 0;
  Json *value = jsonPointerDereference(mgr->config,jp,&errorReason,mgr->traceLevel);
  if (value){
    if (jsonIsString(value)){
      *result = value;
      return ZCFG_SUCCESS;
    } else {
      *result = NULL;
      return ZCFG_TYPE_MISMATCH;
    }
  } else {
    *result = NULL;
    return errorReason;
  }
}

int vcfgGetStringC(ConfigManager *mgr, char **value, int allocOptions, void *mem, va_list args){
  return ZCFG_SUCCESS;
}

int cfgGetStringC(ConfigManager *mgr, char **value, int allocOptions, void *mem, ...){
  va_list argPointer; 
  va_start(argPointer,mem);
  vcfgGetStringC(NULL,value,allocOptions,mem,argPointer);
  va_end(argPointer);

  return ZCFG_SUCCESS;
}

int cfgGetInstanceType(ConfigManager *mgr, int *type, int *subType, ...){
  return 0;
}

/* like get string, but doesn't care about data types */

int cfgGetAnyJ(ConfigManager *mgr, Json **result, JsonPointer *jp){
  int errorReason = 0;
  Json *value = jsonPointerDereference(mgr->config,jp,&errorReason,mgr->traceLevel);
  if (mgr->traceLevel >= 1){
    printf("cfgGetAny: value=0x%p error=%d\n",value,errorReason);
  }
  if (value){
    *result = value;
    return ZCFG_SUCCESS;
  } else {
    *result = NULL;
    return errorReason;
  }
}

int cfgGetAny(ConfigManager *mgr, char *value, int allocOptions, void *mem, ...){
  return 0;
}

static void extractText(ConfigManager *mgr, JsonPointer *jp, FILE *out){
  Json *value = NULL;
  int status = cfgGetAnyJ(mgr,&value,jp);
  if (status){
    fprintf(out,"error not found, reason=%d",status);
  } else {
    if (jsonIsObject(value) ||
        jsonIsArray(value)){
      fprintf(out,"error: cannot access whole objects or arrays");
    } else if (jsonIsString(value)){
      fprintf(out,"%s",jsonAsString(value));
    } else if (jsonIsInt64(value)){
      fprintf(out,"%lld",jsonAsInt64(value));
    } else if (jsonIsDouble(value)){
      fprintf(out,"%f",jsonAsDouble(value));
    } else if (jsonIsBoolean(value)){
      fprintf(out,"%s",jsonAsBoolean(value) ? "true" : "false");
    } else if (jsonIsNull(value)){
      fprintf(out,"null");
    } else {
      fprintf(out,"error: unhandled type");
    }
  }
}

#define MAX_PATH_NAME 1024
#define DATA_BUFFER_SIZE 2048

static void showDirectory(const char *dirname){
  int returnCode = 0;
  int reasonCode = 0;
  int stemLen = strlen(dirname);
  int needsSlash = dirname[stemLen-1] != '/';
  char path[MAX_PATH_NAME];
  char directoryDataBuffer[DATA_BUFFER_SIZE];
  bool includeDotted = false;
  UnixFile *directory = NULL;
  printf("in show directory\n");
  fflush(stdout);
  if ((directory = directoryOpen(dirname,&returnCode,&reasonCode)) == NULL){
    printf("directory open (%s) failure rc=%d reason=0x%x\n",dirname,returnCode,reasonCode);
  } else {
    
    int entriesRead = 0;
    while ((entriesRead = directoryRead(directory,directoryDataBuffer,DATA_BUFFER_SIZE,&returnCode,&reasonCode)) > 0){
      char *entryStart = directoryDataBuffer;
      for (int e = 0; e<entriesRead; e++){
        int entryLength = ((short*)entryStart)[0];  /* I *THINK* the entry names are null-terminated */
        int nameLength = ((short*)entryStart)[1];
        if(entryLength == 4 && nameLength == 0) { /* Null directory entry */
          break;
        }
        char *name = entryStart+4;
        entryStart += entryLength;
        if ((includeDotted && strncmp(name, ".", nameLength) && strncmp(name, "..", nameLength)) || name[0] != '.'){
          int len;
          snprintf(path,MAX_PATH_NAME,"%s%s%*.*s",
                   dirname,(needsSlash ? "/" : ""),nameLength,nameLength,name);
          printf("  %s\n",path);
        }
      }
    }
    directoryClose(directory,&returnCode,&reasonCode);
  }
}

char *getStringOption(int argc, char **argv, int *argxPtr, char *key){
  int argx = *argxPtr;
  if (argx+1 < argc){
    char *option = argv[argx];
    if (!strcmp(option,key)){
      char *value = argv[argx+1];
      *argxPtr = argx+2;
      return value;
    } else {
      return NULL;
    }
  } else {
    return NULL;
  }
}

static void showHelp(FILE *out){
  fprintf(out,"Usage:\n");
  fprintf(out,"  configmgr [options] <command> <args>\n");
  fprintf(out,"    options\n");
  fprintf(out,"      -h             : show help\n");
  fprintf(out,"      -t <level>     : enable tracing with level from 1-3\n");
  fprintf(out,"      -o <outStream> : OUT|ERR , ERR is default\n");
  fprintf(out,"      -s <path>      : root schema directory\n");
  fprintf(out,"      -w <path>      : workspace directory\n");
  fprintf(out,"      -p <configPath>: list of colon-separated configPathElements - see below\n");
  fprintf(out,"    commands:\n");
  fprintf(out,"      extract <jsonPath>  : prints value to stdout\n");
  fprintf(out,"      validate            : just loads and validates merged configuration\n");
  fprintf(out,"      env <outEnvPath>    : prints merged configuration to a file as a list of environment vars\n");
  fprintf(out,"    configPathElement: \n");
  fprintf(out,"      LIB(datasetName) - a library that can contain config data\n");
  fprintf(out,"      FILE(filename)   - the name of a file containing Yaml\n");
  fprintf(out,"      PARMLIBS         - all PARMLIBS that are defined to this running Program in ZOS, nothing if not on ZOS\n");
}

#define ZWE_PREFIX "ZWE"
#define ZOWE_ENV_PATH ZWE_PREFIX "_zowe_environments"

static void convertJsonToEnv(FILE *fp, const char *path, Json *value);
static void convertJsonObjectToEnv(FILE *fp, const char *path, JsonObject *object);
static void convertJsonArrayToEnv(FILE *fp, const char *path, JsonArray *array);

static int numberLen(int number) {
  if (number == 0) {
    return 1;
  }
  if (number < 0) {
    return 1 + numberLen(-number);
  }
  return (int)(log10((double)number) + 1);
}

static size_t escapeForEnv(const char *s, bool isKey, char *buffer, size_t bufferSize) {
  size_t pos = 0;
  int len = strlen(s);
  memset(buffer, 0, bufferSize);
  for (int i = 0; i < len; i++) {
    char c = s[i];
    if (c == '\"' || c == '\"') {
      pos += snprintf (buffer + pos, bufferSize - pos, "\\");
    }
    if (isKey && c == '-') {
      c = '_';
    }
    pos += snprintf (buffer + pos, bufferSize - pos, "%c", c);
  }
  return pos;
}

static void outputEnvKey(FILE * out, const char *str) {
  size_t escapedSize = strlen(str) * 2 + 1;
  char escaped[escapedSize];
  escapeForEnv(str, true, escaped, escapedSize);
  fprintf(out, "%s=", escaped);
}

static void outputEnvString(FILE * out, const char *str) {
  size_t escapedSize = strlen(str) * 2 + 1;
  char escaped[escapedSize];
  escapeForEnv(str, false, escaped, escapedSize);
  fprintf(out, "\"%s\"\n", escaped);
}

static void outputEnvInt64(FILE * out, int64_t num) {
  fprintf(out, "%lld\n", num);
}

static void outputEnvDouble(FILE * out, double num) {
  fprintf(out, "%f\n", num);
}

static void outputEnvBoolean(FILE * out, bool b) {
  fprintf(out, "%s\n", b ? "true": "false");
}

static void convertJsonToEnv(FILE *out, const char *path, Json *value) {
  if (jsonIsObject(value)) {
    convertJsonObjectToEnv(out, path, jsonAsObject(value));
  } else if (jsonIsArray(value)) {
    convertJsonArrayToEnv(out, path, jsonAsArray(value));
  } else {
    if (jsonIsString(value)) {
      outputEnvKey(out, path);
      outputEnvString(out, jsonAsString(value));
    } else if (jsonIsInt64(value)) {
      outputEnvKey(out, path);
      outputEnvInt64(out, jsonAsInt64(value));
    } else if (jsonIsDouble(value)) {
      outputEnvKey(out, path);
      outputEnvDouble(out, jsonAsDouble(value));
    } else if (jsonIsBoolean(value)) {
      outputEnvKey(out, path);
      outputEnvBoolean(out, jsonAsBoolean(value));
    } else if (jsonIsNull(value)) {
      /* omit null */
    }
  }
}


static void convertJsonObjectToEnv(FILE *out, const char *path, JsonObject *object) {
  bool isEnvVar = (0 == strcmp(path, ZOWE_ENV_PATH));
  for (JsonProperty *prop = jsonObjectGetFirstProperty(object); prop != NULL; prop = jsonObjectGetNextProperty(prop)) {
    const char *key = jsonPropertyGetKey(prop);
    size_t pathSize = strlen(path) + strlen(key) + 2;
    char currentPath[pathSize];
    snprintf (currentPath, pathSize, "%s_%s", path, key);
    Json *value = jsonPropertyGetValue(prop);
    convertJsonToEnv(out, isEnvVar ? key : currentPath, value);
  }
}

static void convertJsonArrayToEnv(FILE *out, const char *path, JsonArray *array) {
  int count = jsonArrayGetCount(array);
  for (int i = 0; i < count; i ++) {
    int len = numberLen(i);
    size_t pathSize = strlen(path) + len + 2;
    char currentPath[pathSize];
    snprintf (currentPath, pathSize, "%s_%d", path, i);
    Json *value = jsonArrayGetItem(array, i);
    convertJsonToEnv(out, currentPath, value);
  }
}

/* 
   Here:
   1) Detect qascii or not
   2) store in code
   3) BPX routines do NOT tolerate qASCII like c-library 
   4) fileInfo tag must be used on ZOS target to potentially preprocess input
   5) maybe Leonty has done this for yaml (or not)
 */


int main(int argc, char **argv){
  char *rootSchemaDirectory = NULL;   // Read-only   ZOWE runtime_directory (maybe 
  char *zoweWorkspaceHome = NULL; // Read-write      is there always a zowe.yaml in there
  char *configPath = NULL;
  char *command = NULL;
  char *traceArg = NULL;
  int argx = 1;
  int traceLevel = 0;
  FILE *traceOut = stderr;
  if (argc == 1){
    showHelp(traceOut);
    return 0;
  }
  while (argx < argc){
    char *optionValue = NULL;
    if (getStringOption(argc,argv,&argx,"-h")){
      showHelp(traceOut);
      return 0;
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-s")) != NULL){
      rootSchemaDirectory = optionValue;
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-t")) != NULL){
      traceArg = optionValue;
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-o")) != NULL){
      if (!strcmp(optionValue,"OUT")){
        traceOut = stdout;
      }
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-w")) != NULL){
      zoweWorkspaceHome = optionValue;
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-p")) != NULL){
      configPath = optionValue;
    } else {
      char *nextArg = argv[argx];
      if (strlen(nextArg) && nextArg[0] == '-'){
        fprintf(traceOut,"\n *** unknown option *** '%s'\n",nextArg);
        showHelp(traceOut);
        return 0;
      } else {
        break;
      }
    }
  }

  if (traceArg){
    traceLevel = atoi(traceArg);
    if (traceLevel >= 1){
      printf("ConfigMgr tracelevel set to %d\n",traceLevel);
    }
  }

  if (rootSchemaDirectory == NULL){
    fprintf(traceOut,"Must specify root schema directory\n");
    showHelp(traceOut);
    return 0;
  }
  if (configPath == NULL){
    fprintf(traceOut,"Must specify config path\n");
    showHelp(traceOut);
    return 0;
  }

  ConfigManager *mgr = makeConfigManager(configPath,rootSchemaDirectory,traceLevel,traceOut);
  if (mgr == NULL){
    trace(mgr,INFO,"Failed to build configmgr\n");
    return 0;
  }
  trace(mgr,DEBUG,"ConfigMgr built at 0x%p\n",mgr);


  if (argx >= argc){
    printf("\n *** No Command Seen ***\n\n");
    showHelp(traceOut);
    return 0;
  }
  command = argv[argx++];
  trace(mgr,DEBUG,"command = %s\n",command);
  if (mgr->traceLevel >= 1){
    printConfigPath(mgr);
  }
  int loadStatus = loadConfigurations(mgr);
  if (loadStatus){
    trace(mgr,INFO,"Failed to load configuration, element may be bad, or less likey a bad merge\n");
  }
  trace(mgr,DEBUG,"configuration parms are loaded\n");
  if (!strcmp(command,"validate")){ /* just a testing mode */
    trace(mgr,INFO,"about to validate merged yamls as\n");
    jsonPrettyPrint(mgr,mgr->config);
    JsonValidator *validator = makeJsonValidator();
    trace(mgr,DEBUG,"Before Validate\n");
    int validateStatus = jsonValidateSchema(validator,mgr->config,mgr->topSchema);
    trace(mgr,INFO,"validate status = %d\n",validateStatus);
    switch (validateStatus){
    case JSON_VALIDATOR_NO_EXCEPTIONS:
      trace(mgr,INFO,"No validity Exceptions\n");
      break;
    case JSON_VALIDATOR_HAS_EXCEPTIONS:
      {
        trace(mgr,INFO,"Validity Exceptions:\n");
        ValidityException *e = validator->firstValidityException;
        while (e){
          trace(mgr,INFO,"  %s\n",e->message);
          e = e->next;
        }
      }
      break;
    case JSON_VALIDATOR_INTERNAL_FAILURE:
      trace(mgr,INFO,"validation internal failure");
      break;
    }
    freeJsonValidator(validator);
  } else if (!strcmp(command,"extract")){
    if (argx >= argc){
      trace(mgr,INFO,"extract command requires a json path\n");
    } else {
      char *path = argv[argx++];
      JsonPointer *jp = parseJsonPointer(path);
      if (jp == NULL){
        trace(mgr,INFO,"Could not parse JSON pointer '%s'\n",path);
        return 0;
      }
      if (mgr->traceLevel >= 1){
        printJsonPointer(mgr->traceOut,jp);
        fflush(mgr->traceOut);
      }
      /* Friday, extract some text and show Jack */
      extractText(mgr,jp,stdout);
      printf("\n");
      fflush(stdout);
    }
  } else if (!strcmp(command, "env")) {
    if (argx >= argc){
      trace(mgr, INFO, "env command requires an env file path\n");
    } else {
      char *outputPath = argv[argx++];
      Json *config = mgr->config;
      FILE *out = fopen(outputPath, "w");
      if (out) {
        convertJsonToEnv(out, ZWE_PREFIX, config);
        fclose(out);
      } else {
        trace (mgr, INFO, "failed to open output file '%s' - %s\n", outputPath, strerror(errno));
      }
    }
  }
  return 0;
}
