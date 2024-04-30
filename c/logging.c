

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
#include "metalio.h"
#include "qsam.h"
#else
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#include "zowetypes.h"
#include "copyright.h"
#include "alloc.h"
#include "collections.h"
#include "utils.h"
#include "logging.h"
#include "printables_for_dump.h"
#include "zos.h"

#ifdef __ZOWE_OS_ZOS
#include "le.h"
#endif

ZOWE_PRAGMA_PACK

typedef struct LoggingHashEntry_tag {
  int64 key;
  struct LoggingHashEntry_tag *next;
  char value[0];
} LoggingHashEntry;

typedef struct LoggingHashTable_tag {
  char eyecatcher[8]; /* RSLOGHTBL */
  int backboneSize;
  int valueSize;
  LoggingHashEntry **backbone;
  fixedBlockMgr *mgr;
  void (*valueReclaimer)(void *value);
} LoggingHashTable;

ZOWE_PRAGMA_PACK_RESET

static LoggingHashTable *logHTCreate(int backboneSize, int valueSize, void (*valueReclaimer)(void *value)) {
  LoggingHashTable *logHT = (LoggingHashTable *)safeMalloc(sizeof(LoggingHashTable), "LoggingHashTable");
  memset(logHT, 0, sizeof(LoggingHashTable));
  memcpy(logHT->eyecatcher, "RSLOGHTBL", sizeof(logHT->eyecatcher));
  logHT->mgr = fbMgrCreate(sizeof(LoggingHashEntry) + valueSize, 256, logHT);
  logHT->valueReclaimer = valueReclaimer;
  logHT->backboneSize = backboneSize;
  logHT->valueSize = valueSize;
  logHT->backbone = (LoggingHashEntry **)safeMalloc(sizeof(LoggingHashEntry *) * backboneSize, "LGHT Backbone");
  for (int i = 0; i < backboneSize; i++) {
    logHT->backbone[i] = NULL;
  }

  return logHT;
}

static void logHTDestroy(LoggingHashTable *logHT) {

  LoggingHashEntry* entry, *prev;
  if (logHT->backbone != NULL) {
    for (int i = 0; i < logHT->backboneSize; ++i) {
      entry = logHT->backbone[i];
      while (entry != NULL) {
        if (logHT->valueReclaimer != NULL) {
          (logHT->valueReclaimer)(&entry->value);
        }
        prev = entry;
        entry = entry->next;
        fbMgrFree(logHT->mgr, prev);
      }
    }
  }

  fbMgrDestroy(logHT->mgr);
  logHT->mgr = NULL;
  safeFree((char *) logHT->backbone, sizeof(LoggingHashEntry *) * logHT->backboneSize);
  logHT->backbone = NULL;
  safeFree((char *) logHT, sizeof(LoggingHashTable));
  logHT = NULL;
}

static void *logHTGet(LoggingHashTable *logHT, int64 key) {
  int place = key % (logHT->backboneSize);
  LoggingHashEntry *prev = NULL;
  LoggingHashEntry *hashentry = logHT->backbone[place];
  int loopCount = 0;

  if (hashentry != NULL) {
    while (hashentry != NULL) {
      if (hashentry->key == key) {
        return &hashentry->value;
      } else {
        prev = hashentry;
        hashentry = hashentry->next;
      }
      loopCount++;
    }
    return NULL;
  } else {
    return NULL;
  }

  return NULL;
}

static void *logHTPut(LoggingHashTable *ht, int64 key, void *value) {
  int place = key % (ht->backboneSize);
  LoggingHashEntry *entry = entry = ht->backbone[place];
  LoggingHashEntry *tail = NULL;
  LoggingHashEntry *newEntry = NULL;

  if (entry != NULL) {
    while (entry != NULL) {
      if (entry->key == key) {
        entry->key = key;
        memcpy(entry->value, value, ht->valueSize);
        return &entry->value;
      } else {
        entry = entry->next;
      }
    }
    tail = ht->backbone[place];
  }

  newEntry = (LoggingHashEntry *)fbMgrAlloc(ht->mgr);
  newEntry->key = key;
  memcpy(newEntry->value, value, ht->valueSize);
  newEntry->next = tail;
  ht->backbone[place] = newEntry;
  return &newEntry->value;
}

#define LOG_DEFAULT_COMPONENT_COUNT 128
#define LOG_VENDOR_HT_BACKBONE_SIZE 127

static LoggingComponentTable *makeComponentTable(int componentCount) {

  int tableSize = sizeof(LoggingComponentTable) + componentCount * sizeof(LoggingComponent);
  LoggingComponentTable *table = (LoggingComponentTable *)safeMalloc(tableSize, "LoggingComponentTable");
  memset(table, 0, tableSize);
  memcpy(table->eyecatcher, "RSLOGCTB", sizeof(table->eyecatcher));
  table->componentCount = componentCount;

  return table;
}

static LoggingComponentTable *reallocComponentTable(LoggingComponentTable *existingTable, int newComponentCount) {

  if (existingTable != NULL && newComponentCount <= existingTable->componentCount) {
    return existingTable;
  }

  LoggingComponentTable *newTable = makeComponentTable(newComponentCount);

  if (existingTable != NULL) {
    int existingTableSize = sizeof(LoggingComponentTable) + existingTable->componentCount * sizeof(LoggingComponent);
    memcpy(newTable, existingTable, existingTableSize);
    newTable->componentCount = newComponentCount;
    safeFree((char *)existingTable, existingTableSize);
    existingTable = NULL;
  }

  return newTable;
}

static void removeComponentTable(LoggingComponentTable *table) {

  for (int i = 0; i < table->componentCount; i++) {
    LoggingComponent *component = &table->components[i];
    if (component->subcomponents != NULL) {
      removeComponentTable(component->subcomponents);
      component->subcomponents = NULL;
    }
  }

  int tableSize = sizeof(LoggingComponentTable) + table->componentCount * sizeof(LoggingComponent);
  safeFree((char *)table, tableSize);
  table = NULL;

}

static LoggingZoweAnchor *makeZoweAnchor() {

  LoggingZoweAnchor *anchor = (LoggingZoweAnchor *)safeMalloc(sizeof(LoggingZoweAnchor), "LoggingZoweAnchor");
  memset(anchor, 0, sizeof(LoggingZoweAnchor));
  memcpy(anchor->eyecatcher, "RSLOGRSA", sizeof(anchor->eyecatcher));
  memcpy(anchor->topLevelComponentTable.eyecatcher, "RSLOGCTB", sizeof(anchor->topLevelComponentTable.eyecatcher));
  anchor->topLevelComponentTable.componentCount = 1;
  anchor->topLevelComponent.subcomponents = makeComponentTable(LOG_DEFAULT_COMPONENT_COUNT);

  return anchor;
}

static void removeZoweAnchor(LoggingZoweAnchor *anchor) {
  if (anchor->topLevelComponent.subcomponents != NULL) {
    removeComponentTable(anchor->topLevelComponent.subcomponents);
    anchor->topLevelComponent.subcomponents = NULL;
  }
  safeFree((char *)anchor, sizeof(LoggingZoweAnchor));
  anchor = NULL;
}

static LoggingVendor *makeVendor(unsigned short vendorID) {

  LoggingVendor *vendor = (LoggingVendor *)safeMalloc(sizeof(LoggingVendor), "LoggingVendor");
  memset(vendor, 0, sizeof(LoggingVendor));
  memcpy(vendor->eyecatcher, "RSLOGVNR", sizeof(vendor->eyecatcher));
  vendor->vendorID = vendorID;

  return vendor;
}

static void removeVendor(LoggingVendor *vendor) {
  if (vendor->topLevelComponent.subcomponents != NULL) {
    logHTDestroy(vendor->topLevelComponent.subcomponents);
    vendor->topLevelComponent.subcomponents = NULL;
  }
  safeFree((char *)vendor, sizeof(LoggingVendor));
  vendor = NULL;
}

static void cleanLoggingComponentInLogHT(void *component) {
  LoggingComponent *comp = component;
  if (comp->subcomponents != NULL) {
    logHTDestroy(comp->subcomponents);
    comp->subcomponents = NULL;
  }
  memset(comp, 0, sizeof(LoggingComponent));
}

LoggingContext *makeLocalLoggingContext() {
  /* this allocation must be 31-bit because the pointer to it may be stored in
     4-byte slot in the CAA 
     */
  LoggingContext *context = (LoggingContext *)safeMalloc31(sizeof(LoggingContext),"LoggingContext");
  memcpy(context->eyecatcher, "RSLOGCTX", sizeof(context->eyecatcher));
  context->vendorTable = htCreate(LOG_VENDOR_HT_BACKBONE_SIZE, NULL, NULL, NULL, NULL);
  context->zoweAnchor = makeZoweAnchor();

  return context;
}

LoggingContext *makeLoggingContext() {

  LoggingContext *existingContext = getLoggingContext();
  if (existingContext == NULL) {

    LoggingContext *context = makeLocalLoggingContext();
    int setRC = setLoggingContext(context);
    if (setRC != RC_LOG_OK) {
      removeLocalLoggingContext(context);
      context = NULL;
    }

    return context;
  }

  return existingContext;
}

static int matchAll(void *userData, void *key, void *value) {
  return TRUE;
}

static void cleanVendorInHT(void *userData, void *vendor) {
  removeVendor(vendor);
}

void removeLocalLoggingContext(LoggingContext *context) {

  htPrune(context->vendorTable, matchAll, cleanVendorInHT, NULL);
  htDestroy(context->vendorTable);
  context->vendorTable = NULL;
  removeZoweAnchor(context->zoweAnchor);
  context->zoweAnchor = NULL;
  safeFree31((char *)context, sizeof(LoggingContext));
  context  = NULL;

}

void removeLoggingContext() {

  LoggingContext *context = getLoggingContext();
  removeLocalLoggingContext(context);
  setLoggingContext(NULL);

}

static void lastResortLog(char *s){
  printf("%s\n",s);
}

/* default dumper, based on dumpBufferToStream from utils.c */
static char *standardDumperFunction(char *workBuffer, int workBufferSize,
                                    void *data, int dataSize, int lineNumber){

  int formatWidth = 32;
  int index = lineNumber * formatWidth;
  int length = dataSize;
  int lastIndex = length - 1;
  const unsigned char *tranlationTable = printableEBCDIC;
  /* TODO should ASCII be default on non-z/OS systems? */

  if (index <= lastIndex){
    int pos;
    int linePos = 0;
    linePos = hexFill(workBuffer, linePos, 0, 8, 2, index);
    for (pos = 0; pos < 32; pos++){
      if (((index + pos) % 4 == 0) && ((index + pos) % 32 != 0)){
        if ((index + pos) % 16 == 0){
          workBuffer[linePos++] = ' ';
        }
        workBuffer[linePos++] = ' ';
      }
      if ((index + pos) < length){
        linePos = hexFill(workBuffer, linePos, 0, 2, 0, (0xFF & ((char *) data)[index + pos]));
      } else {
        linePos += sprintf(workBuffer + linePos, "  ");
      }
    }
    linePos += sprintf(workBuffer + linePos, " |");
    for (pos = 0; pos < 32 && (index + pos) < length; pos++){
      int ch = tranlationTable[0xFF & ((char *) data)[index + pos]];
      workBuffer[linePos++] = ch;
    }
    workBuffer[linePos++] = '|';
    workBuffer[linePos++] = 0;
    return workBuffer;
  }

  return NULL;
}

#ifdef __ZOWE_OS_ZOS
#define TCB_EXISTS (*((Addr31 *)0x21C))
#endif

#ifndef __ZOWE_OS_ZOS
LoggingContext *theLoggingContext = NULL;    /* this is a writable static, which is not always,
                                                available on ZOS, and probably if we ever write drivers
                                                on some operating systems.
                                             */
#endif

LoggingContext *getLoggingContext() {
#ifdef LOGGING_CUSTOM_CONTEXT_GETTER
  return logGetExternalContext();
#else /* LOGGING_CUSTOM_CONTEXT_GETTER */
#ifdef __ZOWE_OS_ZOS
  if (TCB_EXISTS) {
    CAA *caa = (CAA *)getCAA();
    return caa->loggingContext;
  } else {
    return NULL;
  }
#else /* __ZOWE_OS_ZOS */
  return theLoggingContext;
#endif /* not __ZOWE_OS_ZOS */
#endif /* not LOGGING_CUSTOM_CONTEXT_GETTER */
}

int setLoggingContext(LoggingContext *context) {
#ifdef LOGGING_CUSTOM_CONTEXT_GETTER
  return logSetExternalContext(context);
#else /* LOGGING_CUSTOM_CONTEXT_GETTER */
#ifdef __ZOWE_OS_ZOS
  if (TCB_EXISTS) {
    abortIfUnsupportedCAA();
    CAA *caa = (CAA *)getCAA();
    caa->loggingContext = context;
    return RC_LOG_OK;
  } else {
    /* TODO not ready for SRB */
    return RC_LOG_ERROR;
  }
#else /* __ZOWE_OS_ZOS */
  theLoggingContext = context;
  return RC_LOG_OK;
#endif /* not __ZOWE_OS_ZOS */
#endif /* not LOGGING_CUSTOM_CONTEXT_GETTER */
}

LoggingDestination *logConfigureDestination3(LoggingContext *context,
                                             unsigned int id,
                                             char *name,
                                             void *data,
                                             LogHandler handler,
                                             DataDumper dumper,
                                             LogHandler2 handler2) {

  if (context == NULL) {
    context = getLoggingContext();
  }

  LoggingDestination *destination = NULL;
  int destinationID = id & 0xFFFF;
  if (destinationID >= MAX_LOGGING_DESTINATIONS) {
    char message[128];
    sprintf(message,"logConfigureDestination: destination ID too big %d, original ID 0x%X\n",destinationID, id);
    lastResortLog(message);
    return NULL;
  }

  unsigned int vendorID = (id >> 16) & 0xFFFF;
  if (vendorID == LOG_ZOWE_VENDOR_ID) {
    destination = &context->zoweAnchor->destinations[destinationID];
  } else {
    LoggingVendor *vendor = htUIntGet(context->vendorTable, vendorID);
    if (vendor == NULL) {
      vendor = makeVendor(vendorID);
      htUIntPut(context->vendorTable, vendorID, vendor);
    }
    destination = &vendor->destinations[destinationID];
  }

  memset(destination,0,sizeof(LoggingDestination));
  memcpy(destination->eyecatcher,"RSLOGDST",8);
  destination->id = destinationID;
  destination->name = name;
  destination->data = data;
  destination->handler = handler;
  destination->handler2 = handler2;
  destination->dumper = dumper != NULL ? dumper : standardDumperFunction;
  destination->state = LOG_DESTINATION_STATE_INIT;
  return destination;
}

LoggingDestination *logConfigureDestination(LoggingContext *context,
                                            unsigned int id,
                                            char *name,
                                            void *data,
                                            LogHandler handler){

  return logConfigureDestination3(context, id, name, data, handler, NULL, NULL);
}

LoggingDestination *logConfigureDestination2(LoggingContext *context,
                                             unsigned int id,
                                             char *name,
                                             void *data,
                                             LogHandler handler,
                                             DataDumper dumper){

  return logConfigureDestination3(context, id, name, data, handler, dumper, NULL);
}

static char *logLevelToString(int level) {
  switch (level) {
    case ZOWE_LOG_SEVERE: return "CRITICAL";
    case ZOWE_LOG_WARNING: return "WARN";
    case ZOWE_LOG_INFO: return "INFO";
    case ZOWE_LOG_DEBUG: return "DEBUG";
    case ZOWE_LOG_DEBUG2: return "FINER";
    case ZOWE_LOG_DEBUG3: return "TRACE";
    default: return "NA";
  }
}

typedef struct zl_time_t {
  char value[32];
} zl_time_t;

static zl_time_t gettime(void) {

  time_t t = time(NULL);
  const char *format = "%Y-%m-%d %H:%M:%S";

  struct tm lt;
  zl_time_t result;

  gmtime_r(&t, &lt);

  strftime(result.value, sizeof(result.value), format, &lt);

  struct timeval now;
  gettimeofday(&now, NULL);
  int milli = now.tv_usec / 1000;
  snprintf(result.value+strlen(result.value), 5, ".%03d", milli);

  return result;
}

static char *getServiceName() {
  return "ZWESZ1";
}

static void getTaskInformation(char *taskInformation, unsigned int taskInformationSize) {
  pthread_t pThread = pthread_self();
  snprintf(taskInformation, taskInformationSize, "0x%p:0x%p:%d", getTCB(), &pThread, getpid());
}

static void getMessage(char *message, unsigned int messageSize, const char *formatString, va_list argList) {
  vsnprintf(message, messageSize, formatString, argList);
}

/*
 * 1. Try to get the name from the TCB.
 * 2. Try to get the name from the login data base.
 */
static void getUserID(char *user, unsigned int userSize) {
  TCB *tcb = getTCB();
  ACEE *acee = (ACEE *) tcb->tcbsenv;
  if (acee) {
    snprintf(user, userSize, "%.*s", acee->aceeuser[0], acee->aceeuser[1]);
  } else {
    getlogin_r(user, userSize);
  }
}

/*
 * These seem very generous.
 *
 */
#define LOG_USER_ID_MAX_LENGTH   8
#define LOG_MESSAGE_MAX_LENGTH   512
#define LOG_TASK_INFO_MAX_LENGTH 128

static void prependMetadata(int logLevel,
                            char *fullMessage,
                            unsigned int fullMessageSize,
                            char *formatString,
                            va_list argList,
                            char *fileName,
                            char *functionName,
                            unsigned int lineNumber) {
  char user[LOG_USER_ID_MAX_LENGTH + 1] = {0};
  getUserID((char *) &user, sizeof(user) - 1);
  zl_time_t time = {0};
  time = gettime();
  char message[LOG_MESSAGE_MAX_LENGTH + 1] = {0};
  getMessage((char *) &message, sizeof(message), formatString, argList);
  char taskInformation[LOG_TASK_INFO_MAX_LENGTH + 1] = {0};
  getTaskInformation((char *) &taskInformation, sizeof(taskInformation));
  char *logLevelAsString = logLevelToString(logLevel);
  /*
   * The following fields were added in the second iteration of LogHandler.
   *
   * 1. fileName
   * 2. functionName
   * 3. lineNumber
   * 4. logLevel
   *
   * (1,2,3) were never asked for, but (4) wasn't propogated down to the LogHandler and was just used to determine
   * if the message should be printed. (4) is now passed to here so that it can be used to construct the common
   * logging format.
   *
   * If one of these (1,2,3,4) isn't present, then they all aren't based on this implementation; therefore, we'll just format with what's
   * available to us without relying on new arguments. The previous LogHandler will return null or empty values for (1,2), a 0 for (3), and
   * ZOWE_LOG_NA for (4), which when going thorugh logLevelToString() will be NA.
   *
   * The existing logs in the server should contain the level, as long as logConfigureDestination3() is used. If not, it will have the placeholder.
   *
   */
  if (!fileName || strlen(fileName) == 0 || !functionName || strlen(functionName) == 0 || lineNumber == 0) {
    snprintf(fullMessage,
            fullMessageSize,
            "%s <%s:%s> %s %s %s",
            time.value,
            getServiceName(),
            taskInformation,
            user,
            logLevelAsString,
            message);
  } else {
    snprintf(fullMessage,
             fullMessageSize,
             "%s <%s:%s> %s %s (%s:%s:%d) %s",
             time.value,
             getServiceName(),
             taskInformation,
             user,
             logLevelAsString,
             basename(fileName),
             functionName,
             lineNumber,
             message);
  }
}

void printToLog(FILE *destination,
                char *formatString,
                va_list argList,
                int logLevel,
                char *fileName,
                char *functionName,
                unsigned int lineNumber) {
#ifdef METTLE 
  printf("broken printf in logging.c\n");
#else
  char fullMessage[LOG_USER_ID_MAX_LENGTH + LOG_MESSAGE_MAX_LENGTH + LOG_TASK_INFO_MAX_LENGTH + 1] = {0};
  prependMetadata(logLevel, (char *) &fullMessage, sizeof(fullMessage), formatString, argList, fileName, functionName, lineNumber);
  fprintf(destination, "%s\n", fullMessage);
#endif
}

void printStdout2(LoggingContext *context,
                  LoggingComponent *component,
                  void *data,
                  char *formatString,
                  va_list argList,
                  int logLevel,
                  char *fileName,
                  char *functionName,
                  unsigned int lineNumber) {
  printToLog(stdout, formatString, argList, logLevel, fileName, functionName, lineNumber);
}

void printStdout(LoggingContext *context, LoggingComponent *component, void *data, char *formatString, va_list argList){
  printStdout2(context, component, data, formatString, argList, ZOWE_LOG_NA, "", "", 0);
}

void printStderr2(LoggingContext *context,
                  LoggingComponent *component,
                  void *data,
                  char *formatString,
                  va_list argList,
                  int logLevel,
                  char *fileName,
                  char *functionName,
                  unsigned int lineNumber) {
  printToLog(stderr, formatString, argList, logLevel, fileName, functionName, lineNumber);
}

void printStderr(LoggingContext *context, LoggingComponent *component, void *data, char *formatString, va_list argList){
  printStderr2(context, component, data, formatString, argList, ZOWE_LOG_NA, "", "", 0);
}

void logConfigureStandardDestinations(LoggingContext *context){
  if (context == NULL) {
    context = getLoggingContext();
  }
  logConfigureDestination3(context,LOG_DEST_DEV_NULL,"/dev/null",NULL,NULL,NULL,NULL);
  logConfigureDestination3(context,LOG_DEST_PRINTF_STDOUT,"printf(stdout)",NULL,printStdout,NULL,printStdout2);
  logConfigureDestination3(context,LOG_DEST_PRINTF_STDERR,"printf(stderr)",NULL,printStderr,NULL,printStderr2);
}

void logConfigureComponent(LoggingContext *context, uint64 compID, char *compName, int destination, int level){

  if (context == NULL) {
    context = getLoggingContext();
  }

  unsigned int vendorID = (compID >> 48) & 0xFFFF;
  unsigned short *id = (unsigned short *)&compID;

  if (vendorID == LOG_ZOWE_VENDOR_ID) {

    LoggingZoweAnchor *ranchor = context->zoweAnchor;

    LoggingComponent *component = &ranchor->topLevelComponent;
    LoggingComponentTable **componentTableHandle = (LoggingComponentTable **)&component->subcomponents;
    LoggingComponentTable *componentTable = component->subcomponents;

    for (int i = 1; id[i] != 0 && i < 4; i++) {
      if (componentTable == NULL || componentTable->componentCount <= id[i]) {
        componentTable = reallocComponentTable(componentTable, min(id[i] * 2 + 1, 0xFFFF));
        *componentTableHandle = componentTable;
      }
      component = &componentTable->components[id[i]];
      componentTableHandle = (LoggingComponentTable **)&component->subcomponents;
      componentTable = component->subcomponents;
    }

    component->flags |= LOG_COMP_FLAG_REGISTERED;
    component->currentDetailLevel = level;
    component->destination = destination;
    component->name = compName;

  } else {

    LoggingVendor *vendor = htUIntGet(context->vendorTable, vendorID);
    if (vendor == NULL) {
      vendor = makeVendor(vendorID);
      htUIntPut(context->vendorTable, vendorID, vendor);
    }

    LoggingComponent *component = &vendor->topLevelComponent;
    LoggingHashTable **componentTableHandle = (LoggingHashTable **)&component->subcomponents;
    LoggingHashTable *componentTable = component->subcomponents;

    for (int i = 1; id[i] != 0 && i < 4; i++) {
      LoggingComponent localLoggingComponent;
      memset(&localLoggingComponent, 0, sizeof(LoggingComponent));
      if (componentTable == NULL) {
        componentTable = logHTCreate(LOG_DEFAULT_COMPONENT_COUNT, sizeof(LoggingComponent), cleanLoggingComponentInLogHT);
        *componentTableHandle = componentTable;
      }
      component = logHTGet(componentTable, id[i]);
      if (component == NULL) {
        component = logHTPut(componentTable, id[i], &localLoggingComponent);
      }
      componentTableHandle = (LoggingHashTable **)&component->subcomponents;
      componentTable = component->subcomponents;
    }

    component->flags |= LOG_COMP_FLAG_REGISTERED;
    component->currentDetailLevel = level;
    component->destination = destination;
    component->name = compName;

  }

}

static LoggingDestination *getDestinationTable(LoggingContext *context, uint64 compID){

  unsigned int vendorID = (compID >> 48) & 0xFFFF;
  if (vendorID == LOG_ZOWE_VENDOR_ID) {
    LoggingZoweAnchor *ranchor = context->zoweAnchor;
    return ranchor->destinations;
  } else {
    LoggingVendor *vendor = htUIntGet(context->vendorTable, vendorID);
    if (vendor == NULL) {
      return NULL;
    }
    return vendor->destinations;
  }
}

static LoggingComponent *getComponent(LoggingContext *context, uint64 compID, int *maxDetailLevel){

  unsigned int vendorID = (compID >> 48) & 0xFFFF;
  int maxLevel = 0;
  LoggingComponent *component = NULL;

  if (vendorID == LOG_ZOWE_VENDOR_ID) {

    LoggingZoweAnchor *ranchor = context->zoweAnchor;

    component = &ranchor->topLevelComponent;
    LoggingComponentTable *componentTable = component->subcomponents;
    maxLevel = component->currentDetailLevel > maxLevel ? component->currentDetailLevel : maxLevel;

    unsigned short *id = (unsigned short *)&compID;
    for (int i = 1; id[i] != 0 && i < 4; i++) {
      if (componentTable != NULL && id[i] < componentTable->componentCount) {
        component = &componentTable->components[id[i]];
        maxLevel = component->currentDetailLevel > maxLevel ? component->currentDetailLevel : maxLevel;
        componentTable = component->subcomponents;
      } else {
        component = NULL;
        break;
      }
    }

  } else {

    LoggingVendor *vendor = htUIntGet(context->vendorTable, vendorID);
    if (vendor == NULL) {
      return NULL;
    }

    component = &vendor->topLevelComponent;
    LoggingHashTable *componentTable = component->subcomponents;
    maxLevel = component->currentDetailLevel > maxLevel ? component->currentDetailLevel : maxLevel;

    unsigned short *id = (unsigned short *)&compID;
    for (int i = 1; id[i] != 0 && i < 4; i++) {
      component = componentTable != NULL ? logHTGet(componentTable, id[i]) : NULL;
      if (component != NULL) {
        maxLevel = component->currentDetailLevel > maxLevel ? component->currentDetailLevel : maxLevel;
        componentTable = component->subcomponents;
      } else {
        break;
      }
    }

  }

  if (component != NULL && !(component->flags & LOG_COMP_FLAG_REGISTERED)) {
    component = NULL;
  }

  if (maxDetailLevel != NULL) {
    *maxDetailLevel = maxLevel;
  }

  return component;
}

void logSetLevel(LoggingContext *context, uint64 compID, int level){

  if (context == NULL) {
    context = getLoggingContext();
  }

  LoggingComponent *component = getComponent(context, compID, NULL);
  if (component != NULL) {
    component->currentDetailLevel = level;
  }

}

int logGetLevel(LoggingContext *context, uint64 compID){

  if (context == NULL) {
    context = getLoggingContext();
  }

  LoggingComponent *component = getComponent(context, compID, NULL);
  return component ? component->currentDetailLevel : ZOWE_LOG_NA;
}

void zowelogInner(LoggingContext *context,
                  uint64 compID,
                  int level,
                  char *fileName,
                  char *functionName,
                  unsigned int lineNumber,
                  char *formatString,
                  va_list argPointer) {

  if (logShouldTrace(context, compID, level) == FALSE) {
    return;
  }

  if (context == NULL) {
    context = getLoggingContext();
  }

  int maxDetailLevel = 0;
  LoggingComponent *component = getComponent(context, compID, &maxDetailLevel);
  if (component == NULL) {
    return;
  }

  if (maxDetailLevel >= level){
    LoggingDestination *destination = &getDestinationTable(context, compID)[component->destination];
    if (component->destination >= MAX_LOGGING_DESTINATIONS){
      char message[128];
      sprintf(message,"Destination %d is out of range (log)\n",component->destination);
      lastResortLog(message);
      return;
    } else if (component->destination == 0 && destination->state != LOG_DESTINATION_STATE_UNINITIALIZED){
      /* silently do nothing, /dev/null is always destination 0 and always does nothing */
      printf("dev/null case\n");
      return;
    } 
    if (destination->state == LOG_DESTINATION_STATE_UNINITIALIZED){
      char message[128];
      sprintf(message,"Destination %d is not initialized for logging\n",component->destination);
      lastResortLog(message);
      return;
    }
    if (destination->handler2) {
      destination->handler2(context,component,destination->data,formatString,argPointer,level,fileName,functionName,lineNumber);
    } else {
      destination->handler(context,component,destination->data,formatString,argPointer);
    }
  }
}

void zowelog2(LoggingContext *context, uint64 compID, int level, char *fileName, char *functionName, unsigned int lineNumber, char *formatString, ...) {
  va_list argPointer;
  va_start(argPointer, formatString);
  zowelogInner(context, compID, level, fileName, functionName, lineNumber, formatString, argPointer);
  va_end(argPointer);
}

void zowelog(LoggingContext *context, uint64 compID, int level, char *formatString, ...){
  va_list argPointer;
  va_start(argPointer, formatString);
  zowelogInner(context, compID, level, "", "", 0, formatString, argPointer);
  va_end(argPointer);
}

static void printToDestination(LoggingDestination *destination,
                               struct LoggingContext_tag *context,
                               LoggingComponent *component,
                               int level, char *fileName,
                               char *functionName, unsigned int lineNumber,
                               void *data, char *formatString, ...){
  va_list argPointer;
  va_start(argPointer, formatString);
  if (destination->handler2) {
    destination->handler2(context,component,destination->data,formatString,argPointer,level,fileName,functionName,lineNumber);
  } else {
    destination->handler(context,component,destination->data,formatString,argPointer);
  }
  va_end(argPointer);
}

void zowedumpInner(LoggingContext *context,
                   uint64 compID,
                   int level,
                   void *data,
                   int dataSize,
                   char *fileName,
                   char *functionName,
                   unsigned int lineNumber) {

  if (logShouldTrace(context, compID, level) == FALSE) {
    return;
  }

  if (context == NULL) {
    context = getLoggingContext();
  }

  int maxDetailLevel = 0;
  LoggingComponent *component = getComponent(context, compID, &maxDetailLevel);
  if (component == NULL) {
    return;
  }

  if (maxDetailLevel >= level){

    LoggingDestination *destination = &getDestinationTable(context, compID)[component->destination];
    if (component->destination >= MAX_LOGGING_DESTINATIONS){
      char message[128];
      sprintf(message,"Destination %d is out of range (dump)\n",component->destination);
      lastResortLog(message);
      return;
    } else if (component->destination == 0 && destination->state != LOG_DESTINATION_STATE_UNINITIALIZED){
      /* silently do nothing, /dev/null is always destination 0 and always does nothing */
      printf("dev/null case\n");
      return;
    }

    if (destination->state == LOG_DESTINATION_STATE_UNINITIALIZED){
      char message[128];
      sprintf(message,"Destination %d is not initialized for dumping\n",component->destination);
      lastResortLog(message);
      return;
    }

    char workBuffer[4096];
    for (int i = 0; ; i++){
      char *result = destination->dumper(workBuffer, sizeof(workBuffer), data, dataSize, i);
      if (result != NULL){
        printToDestination(destination, context, component, level, fileName, functionName, lineNumber, destination->data, "%s\n", result);
      }
      else {
        break;
      }
    }

  }
}

void zowedump2(LoggingContext *context, uint64 compID, int level, void *data, int dataSize, char *fileName, char *functionName, unsigned int lineNumber) {
  zowedumpInner(context, compID, level, data, dataSize, fileName, functionName, lineNumber);
}

void zowedump(LoggingContext *context, uint64 compID, int level, void *data, int dataSize){
  zowedumpInner(context, compID, level, data, dataSize, "", "", 0);
}

bool logShouldTraceInternal(LoggingContext *context, uint64 componentID, int level) {

  if (context == NULL) {
    context = getLoggingContext();
  }

  unsigned short *id = (unsigned short *)&componentID;
  bool shouldTrace = FALSE;
  if (id[0] == LOG_ZOWE_VENDOR_ID) {
    LoggingComponent *component = &context->zoweAnchor->topLevelComponent;
    do {
      /* level #0 */
      LoggingComponentTable *componentTable = &context->zoweAnchor->topLevelComponentTable;
      if (id[1] == 0 || component->currentDetailLevel >= level) {
        break;
      }
      /* level #1 */
      componentTable = component->subcomponents;
      component = &componentTable->components[id[1] % componentTable->componentCount];
      if (id[2] == 0 || component->currentDetailLevel >= level || component->subcomponents == NULL) {
        break;
      }
      /* level #2 */
      componentTable = component->subcomponents;
      component = &componentTable->components[id[2] % componentTable->componentCount];
      if (id[3] == 0 || component->currentDetailLevel >= level || component->subcomponents == NULL) {
        break;
      }
      /* level #3 */
      componentTable = component->subcomponents;
      component = &componentTable->components[id[3] % componentTable->componentCount];
    } while (0);
    return component->currentDetailLevel >= level;
  } else {
    int maxDetailLevel = 0;
    LoggingComponent *component = getComponent(context, componentID, &maxDetailLevel);
    if (maxDetailLevel >= level){
      return TRUE;
    }
    /* we do not check if the component is NULL yet, zowelog and zowedump are responsible for that
     * this is done so that the behavior for Rocket and other vendors was the same */
    return FALSE;
  }
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

