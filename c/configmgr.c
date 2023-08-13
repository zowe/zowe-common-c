

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
#include "logging.h"
#include "openprims.h"
#include "bpxnet.h"
#include "unixfile.h"
#include "charsets.h"
#include "xlate.h"
#include "collections.h"
#include "json.h"
#include "jsonschema.h"
#include "yaml.h"
#include "yaml2json.h"
#include "embeddedjs.h"
#include "parsetools.h"
#include "microjq.h"
#include "configmgr.h"
#ifdef __ZOWE_OS_ZOS
#include "pdsutil.h"
#endif 

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

    ## wherever you git cloned quickjs-portable
    set QJS=c:\repos\quickjs-portable  
      
    ## Wherever you git clone'd libyaml
    set YAML=c:\repos\libyaml   

    clang++ -c ../platform/windows/cppregex.cpp ../platform/windows/winregex.cpp

     clang -I %QJS%/porting -I%YAML%/include -I %QJS% -I./src -I../h -I ../platform/windows -DCMGRTEST=1 -DCONFIG_VERSION=\"2021-03-27\" -Dstrdup=_strdup -D_CRT_SECURE_NO_WARNINGS -DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 -DYAML_VERSION_STRING=\"0.2.5\" -DYAML_DECLARE_STATIC=1 --rtlib=compiler-rt -o configmgr.exe configmgr.c embeddedjs.c %QJS%\quickjs.c %QJS%\cutils.c %QJS%\quickjs-libc.c %QJS%\libbf.c %QJS%\libregexp.c %QJS%\libunicode.c %QJS%\porting\winpthread.c %QJS%\porting\wintime.c %QJS%\porting\windirent.c %QJS%\porting\winunistd.c %YAML%/src/api.c %YAML%/src/reader.c %YAML%/src/scanner.c %YAML%/src/parser.c %YAML%/src/loader.c %YAML%/src/writer.c %YAML%/src/emitter.c %YAML%/src/dumper.c ../c/yaml2json.c ../c/microjq.c ../c/parsetools.c ../c/jsonschema.c ../c/json.c ../c/xlate.c ../c/charsets.c ../c/winskt.c ../c/logging.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc.c ../platform/windows/winfile.c cppregex.o winregex.o

    configmgr "../tests/schemadata" "" "LIBRARY(FOO):DIR(BAR)" yak

    configmgr -s "../tests/schemadata" -p "LIBRARY(FOO):DIR(BAR)" extract "/zowe/setup/"

    configmgr -s "../tests/schemadata" -p "FILE(../tests/schemadata/zoweoverrides.yaml):FILE(../tests/schemadata/zowebase.yaml)" validate

      PARMLIB<PDS(E)>   -> specifically named
                       
      PARMLIBS(STANDARD) -> what IEFPRMLIB SAYS
      PARMLIBS(JOBLEVEL) -> dynamic (doesn't work yet)
    - PARMLIB MEMBERNAME for a config


    configmgr -t 2 -s "../tests/schemadata/zoweyaml.schema" -p "FILE(../tests/schemadata/zoweoverrides.yaml):FILE(../tests/schemadata/zowebase.yaml)" validate

    configmgr -t 1 -m ZWEPARMS -s "../tests/schemadata/zoweappserver.json:../tests/schemadata/zowebase.json:../tests/schemadata/zowecommon.json" -p "PARMLIBS(DEFAULT):FILE(../tests/schemadata/bundle1.json)" validate

    configmgr -s "../tests/schemadata/zoweappserver.json:../tests/schemadata/zowebase.json:../tests/schemadata/zowecommon.json" -p "FILE(../tests/schemadata/zoweoverrides.yaml):FILE(../tests/schemadata/zowedefault.yaml)" extract "/zowe/setup/mvs/proclib"

    configmgr -t 1 -s "../tests/schemadata/bigschema.json" -p "FILE(../tests/schemadata/bigyaml.yaml)" validate

    configmgr -t 1 -s "../tests/schemadata/subClassSchema.json" -p "FILE(../tests/schemadata/subclasses1.json)" validate

    -- Compilation with XLCLang on ZOS -------------------------------------
 
    export YAML="/u/zossteam/jdevlin/git2022/libyaml" 
    export QJS="/u/zossteam/jdevlin/git2022/quickjs"

    NOTE!! charsets.c requires a path that is NOT unix path.  xlclang and clang will not support this

      Easy, maybe temporary workaround is to copy this H file somewhere
    
      cp "//'SYS1.SCUNHF(CUNHC)'" ../h/CUNHC.h 

    xlclang -q64 -qascii -DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 -DYAML_VERSION_STRING=\"0.2.5\" -DYAML_DECLARE_STATIC=1 -DCONFIG_VERSION=\"2021-03-27\" -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DSUBPOOL=132 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -I ../h -I ../platform/posix -I ${YAML}/include -I ${QJS} -Wbitwise-op-parentheses -o configmgr configmgr.c yaml2json.c embeddedjs.c jsonschema.c json.c xlate.c charsets.c zosfile.c logging.c recovery.c scheduling.c zos.c le.c collections.c timeutls.c utils.c alloc.c ../platform/posix/psxregex.c ${QJS}/quickjs.c ${QJS}/cutils.c ${QJS}/quickjs-libc.c ${QJS}/libregexp.c ${QJS}/libunicode.c ${QJS}/porting/polyfill.c ${YAML}/src/api.c ${YAML}/src/reader.c ${YAML}/src/scanner.c ${YAML}/src/parser.c ${YAML}/src/loader.c ${YAML}/src/writer.c ${YAML}/src/emitter.c ${YAML}/src/dumper.c 

    
    -------

    Supporting a subset of jq syntax for extraction:

      https://stedolan.github.io/jq/

    Examples:

      jq ".properties.zowe.type" ../tests/schemadata/zoweyaml.schema 

      Good examples in https://stedolan.github.io/jq/tutorial/

      curl https://api.github.com/repos/stedolan/jq/commits?per_page=5 > commits.json

    --- 
      1) top level call
      2) The "zowe" global, eg. zowe.validate(json,schema) -> null | [ <validationException> ]
      3) configmgr - make schema arg explicit and path
      4) build file references "jdevlin"
      5) semantic validation
      6) $ {{ zowe.runtimeDirectory }}
      
*/

#define CONFIG_PATH_OMVS_FILE    0x0001
#define CONFIG_PATH_OMVS_DIR     0x0002
#define CONFIG_PATH_OMVS_LIBRARY 0x0004
#define CONFIG_PATH_MVS_PARMLIB   0x0008
#define CONFIG_PATH_MVS_PARMLIBS  0x0010 /* STANDARD, JOBLEVEL */
#define CONFIG_PATH_WAS_EXPANDED  0x0100

#ifdef __ZOWE_OS_WINDOWS
static int stdoutFD(void){
  return _fileno(stdout);
}

static int stderrFD(void){
  return _fileno(stdout);
}
#else
static int stdoutFD(void){
#ifdef STDOUT_FILENO
  return STDOUT_FILENO;
#else
  return 1;
#endif
}

static int stderrFD(void){
#ifdef STDERR_FILENO
  return STDERR_FILENO;
#else
  return 2;
#endif
}
#endif

int __atoe_l(char *bufferptr, int leng);
int __etoa_l(char *bufferptr, int leng);

static int convertToNative(char *buf, size_t size) {
#ifdef __ZOWE_OS_ZOS
  return __atoe_l(buf, size);
#endif
  return 0;
}

static int convertFromNative(char *buf, size_t size) {
#ifdef __ZOWE_OS_ZOS
  return __etoa_l(buf, size);
#endif
  return 0;
}


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


typedef struct ConfigPathElement_tag{
  int    flags;
  char  *name;
  char  *data;
  struct ConfigPathElement_tag *next;
} ConfigPathElement;

#define PARMLIB_MEMBER_MAX 8

struct CFGConfig_tag {
  const char *name;
  ConfigPathElement *schemaPath;  /* maybe */
  ConfigPathElement *configPath;
  JsonSchema  *topSchema;
  JsonSchema **otherSchemas;
  int          otherSchemasCount;
  char        *parmlibMemberName;
  Json        *configData;
  struct CFGConfig_tag *next;
};

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

static ConfigPathElement *makePathElement(ConfigManager *mgr, CFGConfig *config, int flags, char *name, char *data){
  ConfigPathElement *element = (ConfigPathElement*)SLHAlloc(mgr->slh,sizeof(ConfigPathElement));
  memset(element,0,sizeof(ConfigPathElement));
  element->flags = flags;
  element->name = name;
  element->data = data;
  if (config->configPath == NULL){
    config->configPath = element;
  } else {
    ConfigPathElement *tail = config->configPath;
    while (tail->next){
      tail = tail->next;
    }
    tail->next = element;
  }
  return element;
}

#ifdef __ZOWE_OS_ZOS
static ConfigPathElement *addExpandedParmlibs(ConfigManager *mgr, CFGConfig *config, int flags, char *name, char *data){
  int getParmlibs(char *outputBuffer, int *reasonCode);
  char *outBuffer = safeMalloc31(PRMLB_BUFFER_SIZE,"IEFPRMLB Buffer");

  int reasonCode = 0;
  int returnCode =  getParmlibs(outBuffer,&reasonCode);
  ConfigPathElement *element = NULL;
  trace(mgr,DEBUG,"IEFPRMLB LIST ret=0x%x reason=0x%x\n",returnCode,reasonCode);
  if (returnCode == 0){
    int count = getParmlibCount(outBuffer);
    for (int i=0; i<count; i++){
      char *volser = NULL;
      char *dsn = getParmlib(outBuffer,i,&volser);
      if (volser == NULL){ /* means it's a catalogued data set */
	int dsnLen = 44;
	while (dsnLen > 0 && (dsn[dsnLen-1] <= 0x40)) dsnLen--;
	element = makePathElement(mgr,
				  config,
				  CONFIG_PATH_MVS_PARMLIB | CONFIG_PATH_WAS_EXPANDED, 
				  substring(mgr,"PARMLIB",0,7),
				  substring(mgr,dsn,0,dsnLen));
      }
    }
  }


  safeFree31(outBuffer,PRMLB_BUFFER_SIZE);
  return element;
}
#endif

static bool addPathElement(ConfigManager *mgr, CFGConfig *config, char *pathElementArg){
  ConfigPathElement *element = (ConfigPathElement*)SLHAlloc(mgr->slh,sizeof(ConfigPathElement));
  memset(element,0,sizeof(ConfigPathElement));

  regmatch_t matches[10];
  regex_t *argPattern = regexAlloc();
  /* Nice Regex test site */
  char *pat = "^(LIBRARY|DIR|FILE|PARMLIBS|PARMLIB)\\(([^)]+)\\)$";
  int compStatus = regexComp(argPattern,pat,REG_EXTENDED);
  if (compStatus != 0){
    
    trace(mgr,INFO,"Internal error, pattern compilation failed\n");
    return false;
  }
  if (regexExec(argPattern,pathElementArg,10,matches,0) == 0){
    char *elementTypeName = extractMatch(mgr,pathElementArg,&matches[1]);
    char *elementData = extractMatch(mgr,pathElementArg,&matches[2]);
    int flags = 0;
    if (!strcmp(elementTypeName,"LIBRARY")){
      flags = CONFIG_PATH_OMVS_LIBRARY;
    } else if (!strcmp(elementTypeName,"DIR")){
      flags = CONFIG_PATH_OMVS_DIR;
    } else if (!strcmp(elementTypeName,"FILE")){
      flags = CONFIG_PATH_OMVS_FILE;
    } else if (!strcmp(elementTypeName,"PARMLIB")){
      flags = CONFIG_PATH_MVS_PARMLIB;
    } else if (!strcmp(elementTypeName,"PARMLIBS")){
      flags = CONFIG_PATH_MVS_PARMLIBS;      
    } else {
      return false; /* internal logic error */
    }
    if (!(flags&CONFIG_PATH_MVS_PARMLIBS)){
      makePathElement(mgr,config,flags,pathElementArg,elementData);
    } else{
      /* zowe does nothing with PARMLIBS off-ZOS */
#ifdef __ZOWE_OS_ZOS
      addExpandedParmlibs(mgr,config,flags,pathElementArg,elementData);
#endif
    }
    return true;
  } else {
    trace(mgr,DEBUG,"unhandled path element '%s'\n",pathElementArg);
    return false;
  }
}

static int buildConfigPath(ConfigManager *mgr, CFGConfig *config, char *configPathArg){
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

    bool status = addPathElement(mgr,config,pathElementArg);

    if (status == false){
      trace(mgr,INFO,"path building failed\n");
      return 12;
    }
    pos = nextPos;
  }
  return 0;
}

static CFGConfig *getConfig(ConfigManager *mgr, const char *configName){
  CFGConfig *config = mgr->firstConfig;
  while (config){
    if (!strcmp(config->name,configName)){
      return config;
    }
    config = config->next;
  }
  return NULL;
}

static void printConfigPath(ConfigManager *mgr, const char *configName){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    fprintf(mgr->traceOut,"No Configuration named '%s'\n",configName);
    return;
  }

  trace(mgr,INFO,"Path Elements: config=0x%p\n",config);
  ConfigPathElement *element = config->configPath;
  while (element){
    trace(mgr,INFO,"  %04X %s, data='%s'\n",element->flags,element->name,element->data);
    element = element->next;
  }
}

static void jsonPrettyPrint(ConfigManager *mgr, Json *json){
  jsonPrinter *p = makeJsonPrinter(mgr->traceOut == stdout ? stdoutFD() : stderrFD());
  jsonEnablePrettyPrint(p);
  jsonPrint(p,json);
  trace(mgr,INFO,"\n");
  
}

/* #define ZOWE_SCHEMA_FILE "zoweyaml.schema"  */

void freeConfigManager(ConfigManager *mgr);

static JsonSchema *loadOneSchema(ConfigManager *mgr, CFGConfig *config, char *schemaFilePath){
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo yamlFileInfo;
  trace(mgr,DEBUG,"before file info\n");
  int infoStatus = fileInfo(schemaFilePath,&yamlFileInfo,&returnCode,&reasonCode);
  if (infoStatus){
    trace(mgr,INFO,"failed to get fileInfo of '%s', infoStatus=%d\n",schemaFilePath,infoStatus);
    return NULL;
  }
  char errorBuffer[1024];
  trace(mgr,DEBUG,"before jsonParseFile info\n");
  Json *jsonWithSchema = jsonParseFile2(mgr->slh,schemaFilePath,errorBuffer,1024);
  if (jsonWithSchema == NULL){
    trace(mgr,INFO,"ZWExxxxxE: JSON Schema file (in '%s') has syntax error: %s\n",
	  schemaFilePath,errorBuffer);
    return NULL;
  }
  if (mgr->traceLevel >= 1){
    jsonPrettyPrint(mgr,jsonWithSchema);

  }
  JsonSchemaBuilder *builder = makeJsonSchemaBuilder(DEFAULT_JSON_SCHEMA_VERSION);
  if (mgr->traceLevel >= 1){
    printf("about to build schema for '%s'\n",schemaFilePath);fflush(stdout);
  }
  builder->traceLevel = mgr->traceLevel;
  JsonSchema *schema = jsonBuildSchema(builder,jsonWithSchema);
  if (schema == NULL){
    trace(mgr,INFO,"Schema Build for '%s' Failed: %s\n",schemaFilePath,builder->errorMessage);
    freeJsonSchemaBuilder(builder); 
    return NULL;
  } else {
    trace(mgr,DEBUG,"JSON Schema built successfully\n");
  }
  freeJsonSchemaBuilder(builder);
  return schema;
}

#define MAX_SCHEMAS 100

ConfigManager *makeConfigManager(){
  ConfigManager *mgr = (ConfigManager*)safeMalloc(sizeof(ConfigManager),"ConfigManager");

  memset(mgr,0,sizeof(ConfigManager));
  mgr->traceLevel = 0;
  mgr->traceOut = stderr;
  mgr->slh = makeShortLivedHeap(0x10000,0x100);
  EmbeddedJS *ejs = allocateEmbeddedJS(NULL);
  configureEmbeddedJS(ejs,NULL,0,0,NULL);
  mgr->ejs = ejs;
  return mgr;
}

int cfgLoadSchemas(ConfigManager *mgr, const char *configName, char *schemaList){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }
  config->otherSchemas = (JsonSchema**)SLHAlloc(mgr->slh,MAX_SCHEMAS*sizeof(JsonSchema));

  int pos = 0;
  int len = strlen(schemaList);
  int schemaCount = 0;
  while (pos < len){
    int nextColon = indexOf(schemaList,len,':',pos);
    int nextPos;
    int listElementLength = 0;

    if (nextColon == -1){
      nextPos = len;
      listElementLength = len - pos;
    } else {
      nextPos = nextColon+1;
      listElementLength = nextColon - pos;
    }

    char *schemaFilePath = substring(mgr,schemaList,pos,pos+listElementLength);

    JsonSchema *schema = loadOneSchema(mgr,config,schemaFilePath);
    if (schema == NULL){
      fprintf(mgr->traceOut,"build schema failed\n");fflush(mgr->traceOut);
      return ZCFG_BAD_JSON_SCHEMA;
    }

    if (schemaCount == 0){
      config->topSchema = schema;
    } else {
      config->otherSchemas[schemaCount-1] = schema;
    }

    pos = nextPos;
    schemaCount++;
  }
  config->otherSchemasCount = schemaCount-1;
  return ZCFG_SUCCESS;
}

void cfgSetTraceLevel(ConfigManager *mgr, int traceLevel){
  mgr->traceLevel = traceLevel;
}

int cfgGetTraceLevel(ConfigManager *mgr){
  return mgr->traceLevel;
}

void cfgSetTraceStream(ConfigManager *mgr, FILE *traceOut){
  mgr->traceOut = traceOut;
}

int cfgSetConfigPath(ConfigManager *mgr, const char *configName, char *configPathArg){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }

  trace(mgr,DEBUG,"before build config path\n");
  if (buildConfigPath(mgr,config,configPathArg)){
    fprintf(mgr->traceOut,"built config path failed\n");fflush(mgr->traceOut);
    return ZCFG_BAD_CONFIG_PATH;
  }
  trace(mgr,DEBUG,"built config path\n");
  return ZCFG_SUCCESS;
}

#define YAML_ERROR_MAX 1024
#define MAX_PDS_NAME 1000 /* very generous */

static Json *readYamlIntoJson(ConfigManager *mgr, char *filename, bool allowMissingFile){
  char errorBuffer[YAML_ERROR_MAX];
  trace(mgr,DEBUG,"before read YAML mgr=0x%p file=%s\n",mgr,filename);
  bool wasMissing = false;
  yaml_document_t *doc = readYAML2(filename,errorBuffer,YAML_ERROR_MAX,&wasMissing);
  /*
   * errorBuffer is ebcdic.
   */
  trace(mgr,DEBUG,"yaml doc at 0x%p, allowMissing=%d wasMissing=%d\n",doc,allowMissingFile,wasMissing);
  if (doc){
    if (mgr->traceLevel >= 1){
      pprintYAML(doc);
    }
    return yaml2JSON(doc,mgr->slh);
  } else if (allowMissingFile && wasMissing){
    return NULL;
  } else{
#ifndef __ZOWE_OS_ZOS
    /*
     * Let's convert to ascii if we aren't on z/OS.
     */
    e2a(errorBuffer,YAML_ERROR_MAX);
#endif
    trace(mgr,INFO,"Couldn't read '%s', because, %s\n",filename,errorBuffer);
    return NULL;
  }
}

static Json *readJson(ConfigManager *mgr, CFGConfig *config, ConfigPathElement *pathElement, bool *nullAllowedPtr){
  *nullAllowedPtr = false;
  if (pathElement == NULL){
    trace(mgr,INFO,"PANIC! readJson got null pathElement\n");
    return NULL;
  }
  if (config == NULL){
    trace(mgr,INFO,"PANIC! readJson got null config\n");
    return NULL;
  }
  if (pathElement->flags & CONFIG_PATH_OMVS_FILE){
    return readYamlIntoJson(mgr,pathElement->data,false);
  } else if (pathElement->flags & CONFIG_PATH_MVS_PARMLIB){
    char pdsMemberSpec[MAX_PDS_NAME];
    trace(mgr,DEBUG,"pathElement=0x%p config=0x%p\n",pathElement,config);
    int charsWritten = snprintf(pdsMemberSpec,MAX_PDS_NAME,"//'%s(%s)'",
				pathElement->data,config->parmlibMemberName);
    trace(mgr,DEBUG,"PDS name = '%s'\n",pdsMemberSpec);
    bool nullAllowed = (pathElement->flags & CONFIG_PATH_WAS_EXPANDED);
    *nullAllowedPtr = nullAllowed;
    return readYamlIntoJson(mgr,pdsMemberSpec,nullAllowed);
  } else {
    trace(mgr,INFO,"WARNING, only simple file and PARMLIB(s) cases yet implemented, saw flags=0x%x\n",pathElement->flags);
    return NULL;
  }
}

CFGConfig *cfgAddConfig(ConfigManager *mgr, const char *configName){
  CFGConfig *config = getConfig(mgr,configName);
  if (config){
    return config;
  }
  CFGConfig *newConfig = (CFGConfig*)safeMalloc(sizeof(CFGConfig),"CFGConfig");
  memset(newConfig,0,sizeof(CFGConfig));
  newConfig->name = configName;
  if (mgr->firstConfig){
    mgr->lastConfig->next = newConfig;
    mgr->lastConfig = newConfig;
  } else {
    mgr->firstConfig = newConfig;
    mgr->lastConfig = newConfig;
  }
  return newConfig;
}

CFGConfig *addConfig(ConfigManager *mgr, const char *configName){
  return cfgAddConfig(mgr,configName);
}

int cfgSetParmlibMemberName(ConfigManager *mgr, const char *configName, const char *parmlibMemberName){
  CFGConfig *config = getConfig(mgr,configName);
  if (config){
    int len = strlen(parmlibMemberName);
    if (len < 3 || len  > PARMLIB_MEMBER_MAX){
      return ZCFG_BAD_PARMLIB_MEMBER_NAME;
    } else {
      config->parmlibMemberName = substring(mgr,(char*)parmlibMemberName,0,len);
      return ZCFG_SUCCESS;
    }
  } else {
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }
}

Json *cfgGetConfigData(ConfigManager *mgr, const char *configName){
  CFGConfig *config = getConfig(mgr,configName);
  if (config){
    return config->configData;
  } else {
    return NULL;
  }
}

int cfgValidate(ConfigManager *mgr, JsonValidator *validator, const char *configName){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }
  int validateStatus = jsonValidateSchema(validator,
                                          config->configData,
                                          config->topSchema,
                                          config->otherSchemas,
                                          config->otherSchemasCount);
  return validateStatus;
}

  
void freeConfigManager(ConfigManager *mgr){
  SLHFree(mgr->slh);
  safeFree((char*)mgr,sizeof(ConfigManager));  
}

#define ZCFG_ALLOC_HEAP 1
#define ZCFG_ALLOC_SLH  2
#define ZCFG_ALLOC_MEMCPY 3


/* need to collect violations as this goes */
static int overloadConfiguration(ConfigManager *mgr,
                                 CFGConfig *config,
                                 ConfigPathElement *pathElement,
                                 ConfigPathElement *pathTail,
                                 int arrayMergePolicy){
  if (pathTail == NULL){
    trace(mgr, DEBUG2, "at end of config path\n");
    bool dontCare = false;
    config->configData = readJson(mgr,config,pathElement,&dontCare);
    trace(mgr, DEBUG2, "mgr->config = 0x%p\n", config);
    return 0; /* success */
  } else {
    bool nullAllowed = false;
    trace(mgr,DEBUG,"overload mgr=0x%p config=0%p pathElement=0x%p\n",
	  mgr,config,pathElement);
    Json *overlay = readJson(mgr,config,pathElement,&nullAllowed);
    trace(mgr,DEBUG,"overlay = 0x%p tail=0x%p nullAllowed=%d\n",overlay,pathTail,nullAllowed);
    if ((overlay == NULL) && !nullAllowed){
      return ZCFG_MISSING_CONFIG_SOURCE;
    }
    int rhsStatus = overloadConfiguration(mgr,config,pathTail,pathTail->next,arrayMergePolicy);
    trace(mgr, DEBUG2, "read the overlay with json=0x%p and status=%d\n",overlay,rhsStatus);
    if (rhsStatus){
      return rhsStatus; /* don't merge if we couldn't load what's to the right in the list */
    }
    int mergeStatus = 0;
    if (overlay){
      config->configData = jsonMerge(mgr->slh,overlay,config->configData,
				     arrayMergePolicy,
				     &mergeStatus);
    } else{
      /* let it ride */
    }
    return mergeStatus;
  }
}

int cfgMakeModifiedConfiguration(ConfigManager *mgr, 
				 const char *configName, 
				 const char *modifiedConfigName,
				 Json *overlay, 
				 int arrayMergePolicy){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }
  int mergeStatus = 0;

  Json *mergedData = jsonMerge(mgr->slh,
			       overlay,
			       config->configData,
			       arrayMergePolicy,
			       &mergeStatus);
  if (mergeStatus){
    return mergeStatus;
  }
  CFGConfig *modifiedConfig = cfgAddConfig(mgr,modifiedConfigName);
  modifiedConfig->schemaPath = config->schemaPath;
  /* is this path really true anymore?? */
  modifiedConfig->configPath = config->configPath;
  modifiedConfig->topSchema = config->topSchema;
  modifiedConfig->otherSchemas = config->otherSchemas;
  modifiedConfig->otherSchemasCount = config->otherSchemasCount;
  modifiedConfig->parmlibMemberName = config->parmlibMemberName;
  modifiedConfig->configData = mergedData;
  return ZCFG_SUCCESS;
}

int cfgLoadConfiguration2(ConfigManager *mgr, const char *configName, int arrayMergePolicy){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }
  ConfigPathElement *pathElement = config->configPath;
  int overloadStatus = overloadConfiguration(mgr,config,pathElement,pathElement->next,arrayMergePolicy);
  trace(mgr,DEBUG,"Overload status = %d\n",overloadStatus);
  if (overloadStatus){
    return overloadStatus;
  } else {
    if (mgr->traceLevel >= 1){
      printf("config before template eval:\n");
      fflush(stdout);
      fflush(stderr);
      jsonPrettyPrint(mgr, config->configData);
    }
    Json *evaluatedConfig = evaluateJsonTemplates(mgr->ejs,mgr->slh,config->configData);
    if (evaluatedConfig){
      config->configData = evaluatedConfig;
      if (mgr->traceLevel >= 1){
        printf("config after template eval:\n");
        fflush(stdout);
        fflush(stderr);
        jsonPrettyPrint(mgr, config->configData);
      }
      return ZCFG_SUCCESS;
    } else {
      return ZCFG_EVAL_FAILURE;
    }
  }
}

int cfgLoadConfiguration(ConfigManager *mgr, const char *configName){
  return cfgLoadConfiguration2(mgr,configName,JSON_MERGE_FLAG_CONCATENATE_ARRAYS);
}

/* all calls return status */

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
      if (traceLevel >= 1){
        printf("cannot dereference scalar\n");
        fflush(stdout);
      }
      return NULL;
    }
  }
  return value;
}

static Json *varargsDereference(Json *json, int argCount, va_list args, int *errorReason, int traceLevel){
  bool innerTrace = true;
  Json *value = json;
  for (int i=0; i<argCount; i++){
    if (jsonIsArray(value)){
      JsonArray *array = jsonAsArray(value);
      int index = va_arg(args,int);
      if (traceLevel >= 1){
        printf("vdref array i=%d ndx=%d\n",i,index);
      }
      int arraySize = jsonArrayGetCount(array);
      if (index >= arraySize){
        *errorReason = JSON_POINTER_ARRAY_INDEX_OUT_OF_BOUNDS;
        return NULL;
      }
      value = jsonArrayGetItem(array,index);
    } else if (jsonIsObject(value)){
      JsonObject *object = jsonAsObject(value);
      char *key = va_arg(args,char *);
      if (traceLevel >= 1){
        printf("vdref object i=%d k=%s\n",i,key);
        fflush(stdout);
      }
      Json *newValue = jsonObjectGetPropertyValue(object,key);
      if (newValue == NULL){
        if (traceLevel >= 2){
          jsonObjectGetPropertyValueLoud(object,key);
          jsonPrinter *p = makeJsonPrinter(stdoutFD());
          jsonEnablePrettyPrint(p);
          jsonPrint(p,value);
          printf("\n");
          jsonDumpObj(object);
          fflush(stdout);
        }
        *errorReason = JSON_POINTER_TOO_DEEP;
        return NULL;
      } else{
        value = newValue;
      }
    } else {
      if (traceLevel >= 1){
        printf("cannot dereference scalar\n");
        fflush(stdout);
      }
      return NULL;
    }
  }
  return value;
}

/* like get string, but doesn't care about data types */

int cfgGetAnyJ(ConfigManager *mgr, const char *configName, Json **result, JsonPointer *jp){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }
  int errorReason = 0;
  Json *value = jsonPointerDereference(config->configData,jp,&errorReason,mgr->traceLevel);
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

int cfgGetAnyC(ConfigManager *mgr, const char *configName, Json **result, int argCount, ...){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }
  int errorReason = 0;
  va_list argPointer; 
  va_start(argPointer,argCount);
  Json *value = varargsDereference(config->configData,argCount,argPointer,&errorReason,mgr->traceLevel);
  va_end(argPointer);

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

int cfgGetStringJ(ConfigManager *mgr, const char *configName, char **result, JsonPointer *jp){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }
  Json *value = NULL;
  int status = cfgGetAnyJ(mgr,configName,&value,jp);
  if (value){
    if (jsonIsString(value)){
      *result = jsonAsString(value);
      return ZCFG_SUCCESS;
    } else {
      *result = NULL;
      return ZCFG_TYPE_MISMATCH;
    }
  } else {
    *result = NULL;
    return ZCFG_SUCCESS;
  }
}

int cfgGetStringC(ConfigManager *mgr, const char *configName, char **result, int argCount, ...){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }
  int errorReason = 0;
  va_list argPointer; 
  va_start(argPointer,argCount);
  Json *value = varargsDereference(config->configData,argCount,argPointer,&errorReason,mgr->traceLevel);
  va_end(argPointer);
  if (errorReason){
    return errorReason;
  } else if (value){
    if (jsonIsString(value)){
      *result = jsonAsString(value);
      return ZCFG_SUCCESS;
    } else {
      *result = NULL;
      return ZCFG_TYPE_MISMATCH;
    }
  } else {
    *result = NULL;
    return ZCFG_POINTER_TOO_DEEP;
  }
}

int cfgGetIntC(ConfigManager *mgr, const char *configName, int *result, int argCount, ...){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }
  int errorReason = 0;
  va_list argPointer; 
  va_start(argPointer,argCount);
  Json *value = varargsDereference(config->configData,argCount,argPointer,&errorReason,mgr->traceLevel);
  va_end(argPointer);
  if (errorReason){
    return errorReason;
  } else if (value){
    if (jsonIsNumber(value)){
      *result = jsonAsNumber(value);
      return ZCFG_SUCCESS;
    } else {
      *result = 0;
      return ZCFG_TYPE_MISMATCH;
    }
  } else {
    *result = 0;
    return ZCFG_POINTER_TOO_DEEP;
  }
}

int cfgGetBooleanC(ConfigManager *mgr, const char *configName, bool *result, int argCount, ...){
  CFGConfig *config = getConfig(mgr,configName);
  if (!config){
    return ZCFG_UNKNOWN_CONFIG_NAME;
  }
  int errorReason = 0;
  va_list argPointer; 
  va_start(argPointer,argCount);
  Json *value = varargsDereference(config->configData,argCount,argPointer,&errorReason,mgr->traceLevel);
  va_end(argPointer);
  if (errorReason){
    return errorReason;
  } else if (value){
    if (jsonIsBoolean(value)){
      *result = jsonAsBoolean(value);
      return ZCFG_SUCCESS;
    } else {
      *result = 0;
      return ZCFG_TYPE_MISMATCH;
    }
  } else {
    *result = 0;
    return ZCFG_POINTER_TOO_DEEP;
  }
}

static void extractText(ConfigManager *mgr, const char *configName, JsonPointer *jp, FILE *out){
  Json *value = NULL;
  int status = cfgGetAnyJ(mgr,configName,&value,jp);
  if (status){
    fprintf(out,"error not found, reason=%d",status);
  } else {
    if (jsonIsObject(value) ||
        jsonIsArray(value)){
      fprintf(out,"error: cannot access whole objects or arrays");
    } else if (jsonIsString(value)){
      fprintf(out,"%s",jsonAsString(value));
    } else if (jsonIsInt64(value)){
      fprintf(out,"%lld",INT64_LL(jsonAsInt64(value)));
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
  fprintf(out,"      -h                  : show help\n");
  fprintf(out,"      -script <path>      : quickjs-compatible javascript file to run with configmgr utilities\n");
  fprintf(out,"      -t <level>          : enable tracing with level from 1-3\n");
  fprintf(out,"      -o <outStream>      : OUT|ERR , ERR is default\n");
  fprintf(out,"      -s <path:path...>   : <topSchema>(:<referencedSchema)+\n");
  fprintf(out,"      -w <path>           : workspace directory\n");
  fprintf(out,"      -c                  : compact output for jq and extract commands\n");
  fprintf(out,"      -r                  : raw string output for jq and extract commands\n");
  fprintf(out,"      -m <memberName>     : member name to find the zowe config in each PARMLIBs specified\n");
  fprintf(out,"      -p <configPath>     : list of colon-separated configPathElements - see below\n");
  fprintf(out,"    commands:\n");
  fprintf(out,"      extract <jsonPath>  : prints value to stdout\n");
  fprintf(out,"      validate            : just loads and validates merged configuration\n");
  fprintf(out,"      env <outEnvPath>    : prints merged configuration to a file as a list of environment vars\n");
  fprintf(out,"    configPathElement: \n");
  fprintf(out,"      PARMLIB(datasetName) - a library that can contain config data\n");
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
  fprintf(out, "%lld\n", INT64_LL(num));
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
  
  JS-ification 

  jsonSpec for higher-level foreign function interface with bignum function pointers 

  configmgr to compile with xlclang??

  globals
    configs["<topLevelName>"] (rather than rooted in top level of each file)
    config                    (the current config)
    ConfigMgr - a "constructor"
    and it's interfaces
      setTraceLevel
      makeConfiguration(<cname>)
      extractJ(<cname>, "jqSpec")
      extractJP(<cname>, p1, ... pn)
         - can printing be fully separate? 
         - is JQ even relevant for js interface
      validdate() -> ValResult 
      
 */

static void *makeConfigManagerWrapper(void *userData, EJSNativeInvocation *invocation){
  return makeConfigManager();
}

static void freeConfigManagerWrapper(EmbeddedJS *ejs, void *userData, void *nativePointer){
  freeConfigManager((ConfigManager*)nativePointer);
}

static int setTraceLevelWrapper(ConfigManager *mgr, EJSNativeInvocation *invocation){
  int traceLevel = 0;
  ejsIntArg(invocation,0,&traceLevel);
  cfgSetTraceLevel(mgr,traceLevel);
  return EJS_OK;
}

static int getTraceLevelWrapper(ConfigManager *mgr, EJSNativeInvocation *invocation){
  ejsReturnInt(invocation,cfgGetTraceLevel(mgr));
  return EJS_OK;
}

static int addConfigWrapper(ConfigManager *mgr, EJSNativeInvocation *invocation){
  const char *configName = NULL;
  ejsStringArg(invocation,0,&configName);
  addConfig(mgr,configName);
  ejsReturnInt(invocation,ZCFG_SUCCESS);
  return EJS_OK;
}

static int loadSchemasWrapper(ConfigManager *mgr, EJSNativeInvocation *invocation){
  const char *configName = NULL;
  ejsStringArg(invocation,0,&configName);
  const char *schemaList = NULL;
  ejsStringArg(invocation,1,&schemaList);
  int status = cfgLoadSchemas(mgr,configName,(char*)schemaList);
  trace(mgr, DEBUG, "loadSchemas ret = %d\n",status);
  ejsReturnInt(invocation,status);
  return EJS_OK;
}

static int setConfigPathWrapper(ConfigManager *mgr, EJSNativeInvocation *invocation){
  const char *configName = NULL;
  ejsStringArg(invocation,0,&configName);
  const char *configPath = NULL;
  ejsStringArg(invocation,1,&configPath);
  int configPathStatus = cfgSetConfigPath(mgr,configName,(char*)configPath);
  ejsReturnInt(invocation,configPathStatus);
  return EJS_OK;
}

static int loadConfigurationWrapper(ConfigManager *mgr, EJSNativeInvocation *invocation){
  const char *configName = NULL;
  ejsStringArg(invocation,0,&configName);
  int loadStatus = cfgLoadConfiguration(mgr,configName);
  ejsReturnInt(invocation,loadStatus);
  return EJS_OK;
}

static int makeModifiedConfigurationWrapper(ConfigManager *mgr, EJSNativeInvocation *invocation){
  const char *configName = NULL;
  ejsStringArg(invocation,0,&configName);
  const char *modifiedConfigName = NULL;
  ejsStringArg(invocation,1,&modifiedConfigName);
  Json *overlay = NULL;
  ejsJsonArg(invocation,2,&overlay);
  int arrayMergePolicy = 0;
  ejsIntArg(invocation,3,&arrayMergePolicy);
  
  int status = cfgMakeModifiedConfiguration(mgr,configName,modifiedConfigName,overlay,arrayMergePolicy);
  ejsReturnInt(invocation,status);
  return EJS_OK;
}

static int setParmlibMemberNameWrapper(ConfigManager *mgr, EJSNativeInvocation *invocation){
  const char *configName = NULL;
  ejsStringArg(invocation,0,&configName);
  const char *memberName = NULL;
  ejsStringArg(invocation,1,&memberName);
  int status = cfgSetParmlibMemberName(mgr,configName,memberName);
  ejsReturnInt(invocation, status);
  return EJS_OK;
}

static char *extractString(JsonBuilder *b, char *s){
  int len = strlen(s);
  char *copy = SLHAlloc(b->parser.slh,len+1);
  memcpy(copy,s,len);
  copy[len] = 0;
  return copy;
}

static void copyValidityException(JsonBuilder *b, Json *parent, char *key, ValidityException *exception){
  int errorCode = 0;
  Json *o = jsonBuildObject(b,parent,key,&errorCode);
  char *message = extractString(b,exception->message);
  jsonBuildString(b,o,"message",message,strlen(message),&errorCode);
  ValidityException *child = exception->firstChild;
  if (child){
    Json *a = jsonBuildArray(b,o,"subExceptions",&errorCode);
    while (child){
      copyValidityException(b,a,NULL,child);
      child = child->nextSibling;
    }
  }
}

static int validateWrapper(ConfigManager *mgr, EJSNativeInvocation *invocation){
  const char *configName = NULL;
  ejsStringArg(invocation,0,&configName);
  JsonValidator *validator = makeJsonValidator();
  EmbeddedJS *ejs = ejsGetEnvironment(invocation);
  JsonBuilder *builder = ejsMakeJsonBuilder(ejs);
  validator->traceLevel = mgr->traceLevel;
  int errorCode = 0;
  Json *result = jsonBuildObject(builder,NULL,NULL,&errorCode);
  trace(mgr,DEBUG,"Before Validate\n");
  int validateStatus = cfgValidate(mgr,validator,configName);
  switch (validateStatus){
  case JSON_VALIDATOR_NO_EXCEPTIONS:
    jsonBuildBool(builder,result,"ok",true,&errorCode);
    break;
  case JSON_VALIDATOR_HAS_EXCEPTIONS:
    {
      jsonBuildBool(builder,result,"ok",true,&errorCode);
      ValidityException *e = validator->topValidityException;
      copyValidityException(builder,result,"exceptionTree",e);
    }
    break;
  case JSON_VALIDATOR_INTERNAL_FAILURE:
    jsonBuildBool(builder,result,"ok",false,&errorCode);
    break;
  }
  jsonBuildInt(builder,result,"shoeSize",11,&errorCode);
  freeJsonValidator(validator);
  ejsReturnJson(invocation,result);
  freeJsonBuilder(builder,false); 
  return EJS_OK;
}

static int getConfigDataWrapper(ConfigManager *mgr, EJSNativeInvocation *invocation){
  const char *configName = NULL;
  ejsStringArg(invocation,0,&configName);
  Json *data = cfgGetConfigData(mgr,configName);
  ejsReturnJson(invocation,data);
  return EJS_OK;
}

#define YAML_BUFFER_SIZE 4096

static int writeYAMLWrapper(ConfigManager *mgr, EJSNativeInvocation *invocation){
  EmbeddedJS *ejs = ejsGetEnvironment(invocation);
  const char *configName = NULL;
  ejsStringArg(invocation,0,&configName);
  Json *data = cfgGetConfigData(mgr,configName);
  char *buffer = NULL;
  int bufferLength = 0;
  int status = json2Yaml2Buffer(data,&buffer,&bufferLength);
  JsonBuilder *builder = ejsMakeJsonBuilder(ejs);
  int errorCode;
  char *nativeBuffer = NULL;
  Json *result = jsonBuildArray(builder,NULL,NULL,&errorCode);
  jsonBuildInt(builder,result,NULL,status,&errorCode);
  if (status == 0 && buffer){
    nativeBuffer = safeMalloc(bufferLength,"NativeYAML");
    memcpy(nativeBuffer,buffer,bufferLength);
    convertToNative(nativeBuffer,bufferLength);
    jsonBuildString(builder,result,NULL,nativeBuffer,bufferLength,&errorCode);
  } else {
    jsonBuildNull(builder,result,NULL,&errorCode);
  }
  ejsReturnJson(invocation,result);
  if (nativeBuffer){
    safeFree(nativeBuffer,bufferLength);
  }
  freeJsonBuilder(builder,false);
  return EJS_OK;
}


static EJSNativeModule *exportConfigManagerToEJS(EmbeddedJS *ejs){
  EJSNativeModule *module = ejsMakeNativeModule(ejs,"Configuration");
  EJSNativeClass *configmgr = ejsMakeNativeClass(ejs,module,"ConfigManager",
                                                 makeConfigManagerWrapper,
                                                 freeConfigManagerWrapper,
                                                 NULL);
  EJSNativeMethod *setTraceLevel = ejsMakeNativeMethod(ejs,configmgr,"setTraceLevel",
                                                       EJS_NATIVE_TYPE_VOID,
                                                       (EJSForeignFunction*)setTraceLevelWrapper);
  ejsAddMethodArg(ejs,setTraceLevel,"traceLevel",EJS_NATIVE_TYPE_INT32);
  EJSNativeMethod *getTraceLevel = ejsMakeNativeMethod(ejs,configmgr,"getTraceLevel",
                                                       EJS_NATIVE_TYPE_INT32,
                                                       (EJSForeignFunction*)getTraceLevelWrapper);
  EJSNativeMethod *addConfig = ejsMakeNativeMethod(ejs,configmgr,"addConfig",
                                                   EJS_NATIVE_TYPE_INT32,
                                                   (EJSForeignFunction*)addConfigWrapper);
  ejsAddMethodArg(ejs,addConfig,"configName",EJS_NATIVE_TYPE_CONST_STRING);

  EJSNativeMethod *setConfigPath = ejsMakeNativeMethod(ejs,configmgr,"setConfigPath",
                                                       EJS_NATIVE_TYPE_INT32,
                                                       (EJSForeignFunction*)setConfigPathWrapper);
  ejsAddMethodArg(ejs,setConfigPath,"configName",EJS_NATIVE_TYPE_CONST_STRING);
  ejsAddMethodArg(ejs,setConfigPath,"configPath",EJS_NATIVE_TYPE_CONST_STRING);
  
  EJSNativeMethod *loadSchemas = ejsMakeNativeMethod(ejs,configmgr,"loadSchemas",
                                                     EJS_NATIVE_TYPE_INT32,
                                                     (EJSForeignFunction*)loadSchemasWrapper);
  ejsAddMethodArg(ejs,loadSchemas,"configName",EJS_NATIVE_TYPE_CONST_STRING);
  ejsAddMethodArg(ejs,loadSchemas,"schemaList",EJS_NATIVE_TYPE_CONST_STRING);

  EJSNativeMethod *loadConfiguration = ejsMakeNativeMethod(ejs,configmgr,"loadConfiguration",
                                                          EJS_NATIVE_TYPE_INT32,
                                                          (EJSForeignFunction*)loadConfigurationWrapper);
  ejsAddMethodArg(ejs,loadConfiguration,"configName",EJS_NATIVE_TYPE_CONST_STRING);

  EJSNativeMethod *makeModifiedConfiguration = ejsMakeNativeMethod(ejs,configmgr,"makeModifiedConfiguration",
								   EJS_NATIVE_TYPE_INT32,
								   (EJSForeignFunction*)makeModifiedConfigurationWrapper);
  ejsAddMethodArg(ejs,makeModifiedConfiguration,"configName",EJS_NATIVE_TYPE_CONST_STRING);
  ejsAddMethodArg(ejs,makeModifiedConfiguration,"modifiedConfigName",EJS_NATIVE_TYPE_CONST_STRING);
  ejsAddMethodArg(ejs,makeModifiedConfiguration,"overrides",EJS_NATIVE_TYPE_JSON);
  ejsAddMethodArg(ejs,makeModifiedConfiguration,"arrayMergePolicy",EJS_NATIVE_TYPE_INT32);

  EJSNativeMethod *setParmlibMemberName = ejsMakeNativeMethod(ejs,configmgr,"setParmlibMemberName",
                                                              EJS_NATIVE_TYPE_INT32,
                                                              (EJSForeignFunction*)setParmlibMemberNameWrapper);
  ejsAddMethodArg(ejs,setParmlibMemberName,"configName",EJS_NATIVE_TYPE_CONST_STRING);
  ejsAddMethodArg(ejs,setParmlibMemberName,"parmlibMemberName",EJS_NATIVE_TYPE_CONST_STRING);

  EJSNativeMethod *getConfigData = ejsMakeNativeMethod(ejs,configmgr,"getConfigData",
                                                       EJS_NATIVE_TYPE_JSON,
                                                       (EJSForeignFunction*)getConfigDataWrapper);
  ejsAddMethodArg(ejs,getConfigData,"configName",EJS_NATIVE_TYPE_CONST_STRING);

  EJSNativeMethod *writeYAML = ejsMakeNativeMethod(ejs,configmgr,"writeYAML",
                                                   EJS_NATIVE_TYPE_JSON,
                                                   (EJSForeignFunction*)writeYAMLWrapper);
  ejsAddMethodArg(ejs,writeYAML,"configName",EJS_NATIVE_TYPE_CONST_STRING);

  EJSNativeMethod *validate = ejsMakeNativeMethod(ejs,configmgr,"validate",
                                                  EJS_NATIVE_TYPE_JSON,
                                                  (EJSForeignFunction*)validateWrapper);
  ejsAddMethodArg(ejs,validate,"configName",EJS_NATIVE_TYPE_CONST_STRING);
  
  return module;
}

static void traceValidityException(ConfigManager *mgr, int depth, ValidityException *exception){
  for (int i=0; i<depth; i++){
    fprintf(mgr->traceOut,"  ");
  }
  fprintf(mgr->traceOut,"%s\n",exception->message);
  ValidityException *child = exception->firstChild;
  while (child){
    traceValidityException(mgr,depth+1,child);
    child = child->nextSibling;
  }
}

static int simpleMain(int argc, char **argv){
  char *schemaList = NULL;
  char *zoweWorkspaceHome = NULL; // Read-write      is there always a zowe.yaml in there
  char *configPath = NULL;
  char *parmlibMemberName = NULL;
  char *command = NULL;
  char *traceArg = NULL;
  int argx = 1;
  int traceLevel = 0;
  FILE *traceOut = stderr;
  bool jqCompact = false;
  bool jqRaw     = false;
  if (argc == 1){
    showHelp(traceOut);
    return ZCFG_BAD_ARGS;
  }

  while (argx < argc){
    char *optionValue = NULL;
    if (getStringOption(argc,argv,&argx,"-h")){
      showHelp(traceOut);
      return 0;
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-s")) != NULL){
      schemaList = optionValue;
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-t")) != NULL){
      traceArg = optionValue;
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-o")) != NULL){
      if (!strcmp(optionValue,"OUT")){
        traceOut = stdout;
      }
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-w")) != NULL){
      zoweWorkspaceHome = optionValue;
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-m")) != NULL){
      parmlibMemberName = optionValue;
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-p")) != NULL){
      configPath = optionValue;
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-c")) != NULL){
      jqCompact = true;
    } else if ((optionValue = getStringOption(argc,argv,&argx,"-r")) != NULL){
      jqRaw = true;
    } else {
      char *nextArg = argv[argx];
      if (strlen(nextArg) && nextArg[0] == '-'){
        fprintf(traceOut,"\n *** unknown option *** '%s'\n",nextArg);
        showHelp(traceOut);
        return ZCFG_BAD_ARGS;
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

  if (schemaList == NULL){
    fprintf(traceOut,"Must specify schema list with at least one schema");
    showHelp(traceOut);
    return ZCFG_BAD_JSON_SCHEMA;
  }
  if (configPath == NULL){
    fprintf(traceOut,"Must specify config path\n");
    showHelp(traceOut);
    return ZCFG_BAD_CONFIG_PATH;
  }

  ConfigManager *mgr = makeConfigManager();
  const char *configName = "only";
  cfgSetTraceLevel(mgr,traceLevel);
  cfgSetTraceStream(mgr,traceOut);
  CFGConfig *config = addConfig(mgr,configName);
  int schemaLoadStatus = cfgLoadSchemas(mgr,configName,schemaList);
  if (schemaLoadStatus){
    fprintf(traceOut,"Failed to load schemas rsn=%d\n",schemaLoadStatus);
    return schemaLoadStatus;
  }
  if (parmlibMemberName != NULL){
    cfgSetParmlibMemberName(mgr,configName,parmlibMemberName);
  }

  int configPathStatus = cfgSetConfigPath(mgr,configName,configPath);
  if (configPathStatus){
    fprintf(traceOut,"Problems with config path rsn=%d\n",configPathStatus);
    return configPathStatus;
  }

  trace(mgr,DEBUG,"ConfigMgr built at 0x%p\n",mgr);


  if (argx >= argc){
    printf("\n *** No Command Seen ***\n\n");
    showHelp(traceOut);
    return ZCFG_BAD_ARGS;
  }
  command = argv[argx++];
  trace(mgr,DEBUG,"command = %s\n",command);
  if (mgr->traceLevel >= 1){
    printConfigPath(mgr,configName);
  }
  int loadStatus = cfgLoadConfiguration(mgr,configName);
  if (loadStatus){
    trace(mgr,INFO,"Failed to load configuration, element may be bad, or less likely a bad merge.\n");
    return loadStatus;
  }
  trace(mgr,DEBUG,"configuration parms are loaded\n");
  if (!strcmp(command,"validate")){ /* just a testing mode */
    trace(mgr,INFO,"about to validate merged yamls as\n");
    jsonPrettyPrint(mgr,cfgGetConfigData(mgr,configName));
    JsonValidator *validator = makeJsonValidator();
    validator->traceLevel = mgr->traceLevel;
    trace(mgr,DEBUG,"Before Validate\n");
    int validateStatus = cfgValidate(mgr,validator,configName);
    trace(mgr,INFO,"validate status = %d\n",validateStatus);
    switch (validateStatus){
    case JSON_VALIDATOR_NO_EXCEPTIONS:
      trace(mgr,INFO,"No validity Exceptions\n");
      return ZCFG_SUCCESS;
      break;
    case JSON_VALIDATOR_HAS_EXCEPTIONS:
      {
        trace(mgr,INFO,"Validity Exceptions:\n");
        traceValidityException(mgr,0,validator->topValidityException);
      }
      return ZCFG_CONFIG_FAILED_VALIDATION;
      break;
    case JSON_VALIDATOR_INTERNAL_FAILURE:
      trace(mgr,INFO,"validation internal failure");
      return ZCFG_VALIDATION_INTERNAL_ERROR;
      break;
    }
    freeJsonValidator(validator);
  } else if (!strcmp(command,"jq")){
    if (argx >= argc){
      trace(mgr,INFO,"jq requires at least one filter argument");
      return ZCFG_BAD_ARGS;
    } else {
      JQTokenizer jqt;
      memset(&jqt,0,sizeof(JQTokenizer));
      jqt.data = argv[argx++];
      jqt.length = strlen(jqt.data);
      jqt.ccsid = 1208;

      Json *jqTree = parseJQ(&jqt,mgr->slh,0);
      if (jqTree){
        int flags = ((jqCompact ? 0 : JQ_FLAG_PRINT_PRETTY)|
                     (jqRaw     ? JQ_FLAG_RAW_STRINGS : 0));
        int evalStatus = evalJQ(cfgGetConfigData(mgr,configName),jqTree,stdout,flags,mgr->traceLevel);
        if (evalStatus != 0){
          trace(mgr, INFO,"micro jq eval problem %d\n",evalStatus);
          return evalStatus;
        }
      } else {
        trace(mgr, INFO, "Failed to parse jq expression\n");
        return ZCFG_JQ_PARSE_ERROR;
      }
    }
  } else if (!strcmp(command,"extract")){
    if (argx >= argc){
      trace(mgr,INFO,"extract command requires a json path\n");
      return ZCFG_BAD_ARGS;
    } else {
      char *path = argv[argx++];
      JsonPointer *jp = parseJsonPointer(path);
      if (jp == NULL){
        trace(mgr,INFO,"Could not parse JSON pointer '%s'\n",path);
        return ZCFG_POINTER_PARSE_ERROR;
      }
      if (mgr->traceLevel >= 1){
        printJsonPointer(mgr->traceOut,jp);
        fflush(mgr->traceOut);
      }
      extractText(mgr,configName,jp,stdout);
      printf("\n");
      fflush(stdout);
    }
  } else if (!strcmp(command, "env")) {
    if (argx >= argc){
      trace(mgr, INFO, "env command requires an env file path\n");
      return ZCFG_BAD_ARGS;
    } else {
      char *outputPath = argv[argx++];
      Json *config = cfgGetConfigData(mgr,configName);
      FILE *out = fopen(outputPath, "w");
      if (out) {
        convertJsonToEnv(out, ZWE_PREFIX, config);
        fclose(out);
      } else {
        trace (mgr, INFO, "failed to open output file '%s' - %s\n", outputPath, strerror(errno));
        return ZCFG_IO_ERROR;
      }
    }
  }
  return ZCFG_SUCCESS;
}

/* a diagnostic function that can be used if other logging initialization bugs come up */
static void ensureLE64(){
  char *realCAA = NULL;

  char *laa = *(char * __ptr32 * __ptr32)0x04B8;
  printf("LAA at 0x%p\n",laa);
  dumpbuffer((char*)laa,0x180);
  char *lca = *(char **)(laa + 88);
  printf("LCA at 0%p\n",lca);
  dumpbuffer((char*)lca,0x358);
  realCAA = *(char **)(lca + 8);
  printf("realCAA = 0x%p\n",realCAA);
  dumpbuffer(realCAA,0x450);
  /* 
     memset(realCAA+0x2A0,0,8);
     printf("realCAA again = 0x%p\n",realCAA);
     dumpbuffer(realCAA,0x450);
     */
}

#ifdef CMGRTEST
int main(int argc, char **argv){
  LoggingContext *logContext = makeLoggingContext();
  logConfigureStandardDestinations(logContext);
  if (argc >= 3 && 
      (!strcmp("-script",argv[1]) ||
       !strcmp("-embeddedScript",argv[1]))){
    char *filename = argv[2];
    EJSNativeModule *modules[1];
    int loadMode = EJS_LOAD_IS_MODULE;
    if (!strcmp("-embeddedScript",argv[1])){
      loadMode = EJS_LOAD_NOT_MODULE;
    }
    EmbeddedJS *ejs = allocateEmbeddedJS(NULL);
    modules[0] = exportConfigManagerToEJS(ejs);
    configureEmbeddedJS(ejs,modules,1,argc,argv);
    /* disabled due to verbosity causing scripting errors for those who read std as a return value 
      printf("configured EJS, and starting script_______________________________________________\n");
      fflush(stdout);
      fflush(stderr);
    */
    int evalStatus = ejsEvalFile(ejs,filename,loadMode);
    /* printf("Done with EJS: File eval returns %d_______________________________________________\n",evalStatus); */
  } else {
    return simpleMain(argc,argv);
  }
}
#endif
 

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
