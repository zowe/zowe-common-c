

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_LOGGING_
#define __ZOWE_LOGGING_ 1

#ifdef METTLE
#include <metal/stdarg.h>
#else
#include <stdarg.h>
#endif

#include "collections.h"
#include "le.h"

#define ZOWELOG_SEVERE($id, $fmt, ...) \
  zowelog2(NULL, $id, ZOWE_LOG_SEVERE, \
          __FILE__, __FUNCTION__, __LINE__, $fmt, ##__VA_ARGS__)

#define ZOWELOG_WARNING($id, $fmt, ...) \
  zowelog2(NULL, $id, ZOWE_LOG_WARNING, \
          __FILE__, __FUNCTION__, __LINE__, $fmt, ##__VA_ARGS__)

#define ZOWELOG_INFO($id, $fmt, ...) \
  zowelog2(NULL, $id, ZOWE_LOG_INFO, \
          __FILE__, __FUNCTION__, __LINE__, $fmt, ##__VA_ARGS__)

#define ZOWELOG_DEBUG($id, $fmt, ...) \
  zowelog2(NULL, $id, ZOWE_LOG_DEBUG, \
          __FILE__, __FUNCTION__, __LINE__, $fmt, ##__VA_ARGS__)

#define ZOWELOG_DEBUG2($id, $fmt, ...) \
  zowelog2(NULL, $id, ZOWE_LOG_DEBUG2, \
          __FILE__, __FUNCTION__, __LINE__, $fmt, ##__VA_ARGS__)

#define ZOWELOG_DEBUG3($id, $fmt, ...) \
  zowelog2(NULL, $id, ZOWE_LOG_DEBUG3, \
          __FILE__, __FUNCTION__, __LINE__, $fmt, ##__VA_ARGS__)

/** \file
 *  \brief logging.h defines a platform-independent logging facility that echoes some of Java logging.
 *  
 *  This logging faclity supports a formal relation between producers and consumers along with (component X level)
 *  dynamic selectivity.  
 */

ZOWE_PRAGMA_PACK

typedef struct LoggingComponent_tag{
  void *subcomponents;
  unsigned char flags;
#define LOG_COMP_FLAG_REGISTERED  0x01
  signed char currentDetailLevel;
  unsigned short destination;
  char *name;
} LoggingComponent;

ZOWE_PRAGMA_PACK_RESET

#define RC_LOG_OK       0
#define RC_LOG_ERROR    8

#define ZOWE_LOG_NA      -1

#define ZOWE_LOG_SEVERE   0
#define ZOWE_LOG_ALWAYS   0
#define ZOWE_LOG_WARNING  1
#define ZOWE_LOG_INFO     2
#define ZOWE_LOG_DEBUG    3
#define ZOWE_LOG_DEBUG2   4
#define ZOWE_LOG_DEBUG3   5

#define LOG_DESTINATION_STATE_UNINITIALIZED 0
#define LOG_DESTINATION_STATE_INIT   1
#define LOG_DESTINATION_STATE_OPEN   2
#define LOG_DESTINATION_STATE_CLOSED 3
#define LOG_DESTINATION_STATE_ERROR  4

/*
 * Vendor component ID must be a unique 2-byte value. It's calculated using
 * the following formula: vendor_ECVTCTBL_offset / 4.
 *
 * WARNING: the result is a 0-based index into ECVTCTBL. It should not be
 * confused with the IBM-assigned slot values which are 1-based.
 *
 * Zowe slot ID is 0x023C / 4 = 0x008F
 *
 * */
#define LOG_ZOWE_VENDOR_ID     0x008F

#define LOG_PROD_COMMON        0x008F000100000000LLU
#define LOG_PROD_ZIS           0x008F000200000000LLU
#define LOG_PROD_ZSS           0x008F000300000000LLU
#define LOG_PROD_PLUGINS       0x008F000400000000LLU

#define LOG_COMP_ALLOC         0x008F000100010000LLU
#define LOG_COMP_UTILS         0x008F000100020000LLU
#define LOG_COMP_COLLECTIONS   0x008F000100030000LLU
#define LOG_COMP_SERIALIZATION 0x008F000100040000LLU
#define LOG_COMP_ZLPARSER      0x008F000100050000LLU
#define LOG_COMP_ZLCOMPILER    0x008F000100060000LLU
#define LOG_COMP_ZLRUNTIME     0x008F000100070000LLU
#define LOG_COMP_STCBASE       0x008F000100080000LLU
#define LOG_COMP_HTTPSERVER    0x008F000100090000LLU
#define LOG_COMP_DISCOVERY     0x008F0001000A0000LLU
#define LOG_COMP_DATASERVICE   0x008F0001000B0000LLU
#define LOG_COMP_CMS           0x008F0001000C0000LLU
#define LOG_COMP_LPA           0x008F0001000D0000LLU
#define LOG_COMP_RESTDATASET   0x008F0001000E0000LLU
#define LOG_COMP_RESTFILE      0x008F0001000F0000LLU
#define LOG_COMP_ZOS           0x008F000100130000LLU
#define LOG_COMP_HTTPCLIENT    0x008F000100140000LLU
#define LOG_COMP_JWT           0x008F000100150000LLU

#define LOG_DEST_DEV_NULL      0x008F0000
#define LOG_DEST_PRINTF_STDOUT 0x008F0001
#define LOG_DEST_PRINTF_STDERR 0x008F0002

typedef struct LogComponentsMap_tag {
  uint64 compID;
  const char* name;
} LogComponentsMap;


#define LOGGING_COMPONENTS_MAP(logComponents)\
 static const LogComponentsMap logComponents[] = {\
  {LOG_COMP_ALLOC, "alloc"},\
  {LOG_COMP_UTILS, "utils"},\
  {LOG_COMP_COLLECTIONS, "collections"},\
  {LOG_COMP_SERIALIZATION, "serialization"},\
  {LOG_COMP_ZLPARSER, "zlparser"},\
  {LOG_COMP_ZLCOMPILER, "zlcompiler"},\
  {LOG_COMP_ZLRUNTIME, "zlruntime"},\
  {LOG_COMP_STCBASE, "stcbase"},\
  {LOG_COMP_HTTPSERVER, "httpserver"},\
  {LOG_COMP_DISCOVERY, "discovery"},\
  {LOG_COMP_DATASERVICE, "dataservice"},\
  {LOG_COMP_CMS, "cms"},\
  {LOG_COMP_LPA, "lpa"},\
  {LOG_COMP_RESTDATASET, "resetdataset"},\
  {LOG_COMP_RESTFILE, "restfile"},\
  {LOG_COMP_ZOS, "zos"},\
  {LOG_COMP_HTTPCLIENT, "httpclient"},\
  {LOG_COMP_JWT, "jwt"},\
  {0, NULL}\
};

struct LoggingContext_tag;

typedef void (*LogHandler2)(struct LoggingContext_tag *context,
                           LoggingComponent *component, 
                           void *data, 
                           char *formatString,
                           va_list argList,
                           int logLevel,
                           char *fileName,
                           char *functionName,
                           unsigned int lineNumber);

typedef void (*LogHandler)(struct LoggingContext_tag *context,
                            LoggingComponent *component, 
                            void *data, 
                            char *formatString,
                            va_list argList);

typedef char *(*DataDumper)(char *workBuffer, int workBufferSize, void *data, int dataSize, int lineNumber);

ZOWE_PRAGMA_PACK

typedef struct LoggingDestination_tag{
  char   eyecatcher[8]; /* RSLOGDST */
  int    id;
  int    state;
  char  *name;
  void  *data;          /* used by destination to hold internal state */
  LogHandler handler;
  LogHandler2 handler2;
  DataDumper dumper;
} LoggingDestination;

#define MAX_LOGGING_COMPONENTS 256
#define MAX_LOGGING_DESTINATIONS 32

typedef struct LoggingVendor_tag {
  char eyecatcher[8]; /* RSLOGVNR */
  unsigned short vendorID;
  char reserved[6];
  LoggingDestination destinations[MAX_LOGGING_DESTINATIONS];
  LoggingComponent topLevelComponent;
} LoggingVendor;

typedef struct LoggingComponentTable_tag {
  char eyecatcher[8]; /* RSLOGCTB */
  unsigned short componentCount;
  unsigned short reseved;
  LoggingComponent components[0];
} LoggingComponentTable;

typedef struct LoggingZoweAnchor_tag {
  char eyecatcher[8]; /* RSLOGRSA */
  LoggingDestination destinations[MAX_LOGGING_DESTINATIONS];
  LoggingComponentTable topLevelComponentTable;
  LoggingComponent topLevelComponent;
} LoggingZoweAnchor;

typedef struct LoggingContext_tag{
  char eyecatcher[8];   /* RSLOGCTX */
  hashtable *vendorTable;
  LoggingZoweAnchor *zoweAnchor;
} LoggingContext;

ZOWE_PRAGMA_PACK_RESET

#ifndef __ZOWE_OS_ZOS
extern LoggingContext *theLoggingContext;
#endif

/* MACROS */
#ifndef LOGGING_CUSTOM_CONTEXT_GETTER

#ifdef __ZOWE_OS_ZOS

#if !defined(METTLE) && defined(_LP64) /* i.e. LE 64-bit */

#define GET_LOGGING_CONTEXT() \
({ \
  CAA *caa = NULL; \
  char *laa = *(char * __ptr32 * __ptr32)0x04B8; \
  char *lca = *(char **)(laa + 88); \
  caa = *(CAA **)(lca + 8); \
  caa->loggingContext; \
})

#else /* LE 64-bit */

#define GET_LOGGING_CONTEXT() \
({ \
  CAA *caa = NULL; \
  __asm( \
      ASM_PREFIX \
      "         LA    %0,0(,12) \n" \
      : "=r"(caa) \
      : \
      : \
  );\
  caa->loggingContext; \
})

#endif /* not LE 64-bit */

#else /* __ZOWE_OS_ZOS */

#define GET_LOGGING_CONTEXT() theLoggingContext

#endif /* not __ZOWE_OS_ZOS */

#else /* not a custom getter */

#define GET_LOGGING_CONTEXT() logGetExternalContext()

#endif /* custom getter */

#if defined(METTLE) || defined(__ZOWE_OS_ZOS)

#define LOGGING_CONTEXT() getLoggingContextFromDU()  /* zero args */

#elif defined(__ZOWE_OS_WINDOWS) || defined(__ZOWE_OS_ISERIES)

#define LOGGING_CONTEXT() getLoggingContextFromGlobal()

#else

#define LOGGING_CONTEXT() (NULL)

#endif

#ifndef __LONGNAME__

#define makeLoggingContext MKLOGCTX 
#define makeLocalLoggingContext MKLLGCTX
#define removeLoggingContext RMLOGCTX
#define removeLocalLoggingContext RMLLGCTX
#define getLoggingContext GTLOGCTX
#define setLoggingContext STLOGCTX
#define zowelog ZOWELOG
#define zowedump ZOWEDUMP
#define logConfigureDestination LGCFGDST
#define logConfigureDestination2 LGCFGDS2
#define logConfigureStandardDestinations LGCFGSTD
#define logConfigureComponent LGCFGCMP
#define logSetLevel LGSETLVL
#define logGetLevel LGGETLVL
#define logShouldTraceInternal LGSHTRCE
#define logGetExternalContext LGGLOGCX
#define logSetExternalContext LGSLOGCX
#define printStdout LGPRSOUT
#define printStderr LGPRSERR

#endif 

/* Note that makeLoggingContext attaches the one-and-only-one LoggingContext as a "hidden global"
   to the current thread or task.  This LoggingContext is global, but the establishment of its 
   reachability/findability is done on a thread/task basis.   Why?  Because globals cannot be 
   safely assumed to exist in all program execution contexts.   Very little of the COMMON
   library uses globals.
*/
LoggingContext *makeLoggingContext();
void removeLoggingContext();

/* create and remove the logging context but do not call the setter and getter function */
LoggingContext *makeLocalLoggingContext();
void removeLocalLoggingContext(LoggingContext *context);

LoggingContext *getLoggingContext();
int setLoggingContext(LoggingContext *context);

/* should trace macro, it will be a function call if the compiler doensn't support statement expressions  */
bool logShouldTraceInternal(LoggingContext *context, uint64 componentID, int level);

#if !defined(LOGGING_NO_MACRO) && (defined(__clang__) || defined(__GNUC__) || defined (__IBMC__) || defined (__IBMCPP__))

#define logShouldTrace(context, componentID, level) \
({\
  LoggingContext *cntx = context;\
  if (cntx == NULL) {\
    cntx = GET_LOGGING_CONTEXT();\
  }\
  uint64 componentIDLocal = componentID;\
  unsigned short *id = (unsigned short *)&componentIDLocal;\
  bool shouldTrace = FALSE;\
  if (id[0] == LOG_ZOWE_VENDOR_ID) {\
    LoggingComponent *component = &cntx->zoweAnchor->topLevelComponent;\
    do {\
      /* level #0 */\
      LoggingComponentTable *componentTable = &cntx->zoweAnchor->topLevelComponentTable;\
      if (id[1] == 0 || component->currentDetailLevel >= level) { break; }\
      /* level #1 */\
      componentTable = component->subcomponents;\
      component = &componentTable->components[id[1] % componentTable->componentCount];\
      if (id[2] == 0 || component->currentDetailLevel >= level || component->subcomponents == NULL) { break; }\
      /* level #2 */\
      componentTable = component->subcomponents;\
      component = &componentTable->components[id[2] % componentTable->componentCount];\
      if (id[3] == 0 || component->currentDetailLevel >= level || component->subcomponents == NULL) { break; }\
      /* level #3 */\
      componentTable = component->subcomponents;\
      component = &componentTable->components[id[3] % componentTable->componentCount];\
    } while (0);\
    shouldTrace = component->currentDetailLevel >= level;\
  } else {\
    shouldTrace = logShouldTraceInternal(cntx, componentID, level);\
  }\
  shouldTrace;\
})

#else /* compilers supporting statement expressions */

#define logShouldTrace logShouldTraceInternal

#endif /* compilers NOT supporting statement expressions */

/* this log message will be sent to the destination associated to the component 
 */

void zowelog(LoggingContext *context, uint64 compID, int level, char *formatString, ...);
void zowedump(LoggingContext *context, uint64 compID, int level, void *data, int dataSize);

#define LOGCHECK(context,component,level) \
  ((component > MAX_LOGGING_COMPONENTS) ? \
   (context->applicationComponents[component].level >= level) : \
   (context->coreComponents[component].level >= level) )

LoggingDestination *logConfigureDestination(LoggingContext *context,
                                            unsigned int id,
                                            char *name,
                                            void *data,
                                            LogHandler handler);

LoggingDestination *logConfigureDestination2(LoggingContext *context,
                                             unsigned int id,
                                             char *name,
                                             void *data,
                                             LogHandler handler,
                                             DataDumper dumper);

void logConfigureStandardDestinations(LoggingContext *context);
void logConfigureComponent(LoggingContext *context, 
                           uint64 compID,
                           char *compName,
                           int destination, 
                           int level);
void logSetLevel(LoggingContext *context, uint64 compID, int level);
int logGetLevel(LoggingContext *context, uint64 compID);

/* custom setter and getter functions for logging context */
extern int logSetExternalContext(LoggingContext *context);
extern LoggingContext *logGetExternalContext();

void printStdout(LoggingContext *context, LoggingComponent *component, void *data, char *formatString, va_list argList);
void printStdout2(LoggingContext *context, LoggingComponent *component, void *data, char *formatString, va_list argList, int logLevel, char *fileName, char *functionName, unsigned int lineNumber);
void printStderr(LoggingContext *context, LoggingComponent *component, void *data, char *formatString, va_list argList);
void printStderr2(LoggingContext *context, LoggingComponent *component, void *data, char *formatString, va_list argList, int logLevel, char *fileName, char *functionName, unsigned int lineNumber);

#endif



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

