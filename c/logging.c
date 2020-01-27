

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

#endif

#include "zowetypes.h"
#include "copyright.h"
#include "alloc.h"
#include "collections.h"
#include "utils.h"
#include "logging.h"
#include "printables_for_dump.h"

#include "timeutls.h"
#include "zos.h"
#include "openprims.h"
#include "zssLogging.h"
#include <pthread.h>

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

  LoggingContext *context = (LoggingContext *)safeMalloc(sizeof(LoggingContext),"LoggingContext");
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
  safeFree((char *)context, sizeof(LoggingContext));
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

LoggingDestination *logConfigureDestination(LoggingContext *context,
                                            unsigned int id,
                                            char *name,
                                            void *data,
                                            LogHandler handler){

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
  destination->dumper = standardDumperFunction;
  destination->state = LOG_DESTINATION_STATE_INIT;
  return destination;
}

LoggingDestination *logConfigureDestination2(LoggingContext *context,
                                             unsigned int id,
                                             char *name,
                                             void *data,
                                             LogHandler handler,
                                             DataDumper dumper){
  LoggingDestination *destination = logConfigureDestination(context, id, name, data, handler);
  if (destination != NULL) {
    destination->dumper = dumper;
  }
  return destination;
}

void printStdout(LoggingContext *context, LoggingComponent *component, void *data, char *formatString, va_list argList){
#ifdef METTLE 
  printf("broken printf in logging.c\n");
#else 
  vfprintf(stdout,formatString,argList);
#endif
}

void printStderr(LoggingContext *context, LoggingComponent *component, void *data, char *formatString, va_list argList){
#ifdef METTLE 
  printf("broken printf in logging.c\n");
#else 
  vfprintf(stderr,formatString,argList);
#endif
}

void logConfigureStandardDestinations(LoggingContext *context){
  if (context == NULL) {
    context = getLoggingContext();
  }
  logConfigureDestination(context,LOG_DEST_DEV_NULL,"/dev/null",NULL,NULL);
  logConfigureDestination(context,LOG_DEST_PRINTF_STDOUT,"printf(stdout)",NULL,printStdout);
  logConfigureDestination(context,LOG_DEST_PRINTF_STDERR,"printf(stderr)",NULL,printStderr);
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

// creates a formatted timestamp to be used in zowe log
static void getLogTimestamp(char *date) {
   int64 stck = 0;      
   char timestamp[14]; // unformated time
   uint32 out_secs = 0;
   uint32 out_micros = 0;

   getSTCK(&stck);
   int64 unixTime = stckToUnix(stck); // gets unixTime
   stckToUnixSecondsAndMicros(stck, &out_secs, &out_micros); // calculates microseconds
   out_micros = out_micros/1000;
   unixToTimestamp((uint64) unixTime, timestamp);

   // format: 2019-03-19 11:23:57.776
   snprintf(date,24, "%.4s-%.2s-%.2s %.2s:%.2s:%.2s.%.3d"
      ,timestamp, timestamp+4,timestamp+6,timestamp+8,timestamp+10,timestamp+12,(unsigned int) out_micros);
}

static void getLevel(int level, char *logLevel) {

  switch(level) {
    case 0: snprintf(logLevel,7,"SEVERE");
            break;
    case 1: snprintf(logLevel,8,"WARN");
            break;
    case 2: snprintf(logLevel,5,"INFO"); 
            break;
    case 3: snprintf(logLevel,5,"DEBUG");
            break;
    case 4: snprintf(logLevel,6,"DEBUG");
            break;
    case 5: snprintf(logLevel,7,"TRACE");
            break;
  }
}

static void getLocationData(char *path, int line, char **locationInfo, uint64 compID, LoggingComponent *component) {

  char prefix[128]; 
  char suffix[128]; 

  unsigned int id = compID & 0xFFFFFFFF;
  if(compID >= LOG_PROD_COMMON && compID < LOG_PROD_ZIS) {
    snprintf(prefix,5,"_zcc");
    switch (id) { // most of these don't actally log, but are defined in logging.h
      case 0x10000: snprintf(suffix,6,"alloc");
                    break;
      case 0x20000: snprintf(suffix,6,"utils");
                    break;
      case 0x30000: snprintf(suffix,12,"collections");    
                    break;
      case 0x40000: snprintf(suffix,14,"serialization");
                    break;
      case 0x50000: snprintf(suffix,9,"zlparser");
                    break;
      case 0x60000: snprintf(suffix,11,"zlcompiler");
                    break;
      case 0x70000: snprintf(suffix,10,"zlruntime"); 
                    break;
      case 0x80000: snprintf(suffix,8,"stcbase");
                    break;
      case 0x90000: snprintf(suffix,11,"httpserver");
                    break;
      case 0xA0000: snprintf(suffix,10,"discovery");
                    break;
      case 0xB0000: snprintf(suffix,12,"dataservice");
                    break;
      case 0xC0000: snprintf(suffix,4,"cms");
                    break;
      case 0xC0001: snprintf(suffix,4,"cms");
                    break;
      case 0xC0002: snprintf(suffix,6,"cmspc");
                    break;
      case 0xD0000: snprintf(suffix,4,"lpa");
                    break;
      case 0xE0000: snprintf(suffix,12,"restdataset");
                    break;
      case 0xF0000: snprintf(suffix,9,"restfile");
                    break;
    }
  }
  else if (compID >= LOG_PROD_ZIS && compID < LOG_PROD_ZSS) {
    snprintf(prefix,5,"_zis");
  }
  else if (compID >= LOG_PROD_ZSS && compID < LOG_PROD_PLUGINS ) {
    snprintf(prefix,5,"_zss"); 
    switch (id) {
      case 0x10000: snprintf(suffix,4,"zss");
                    break;
      case 0x20000: snprintf(suffix,5,"ctds");
                    break;
      case 0x30000: snprintf(suffix,9,"security");
                    break;
      case 0x40000: snprintf(suffix,9, "unixfile");
                    break;
    }
  }
  else if (compID > LOG_PROD_PLUGINS) {
    snprintf(suffix,strlen(component->name),"%s",(strrchr(component->name, '/')+1)); // given more characters than it writes
    snprintf(prefix,strlen(component->name)-strlen(suffix),"%s",component->name);
  }

  char *filename;
  filename = strrchr(path, '/'); // returns a pointer to the last occurence of '/'
  filename+=1; 
  //               formatting + prefix + suffix + filename + line number
  int locationSize =  7  + strlen(prefix) + strlen(suffix) + strlen(filename) + 5; 
  *locationInfo = (char*) safeMalloc(locationSize,"locationInfo");
  snprintf(*locationInfo,locationSize+1," (%s:%s,%s:%d) ",prefix,suffix,filename,line); 
}

void _zowelog(LoggingContext *context, uint64 compID, char* path, int line, int level, char *formatString, ...){
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
//    printf("log.2 comp.dest=%d\n",component->destination);fflush(stdout);
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
  //  printf("log.3\n");fflush(stdout);
    if (destination->state == LOG_DESTINATION_STATE_UNINITIALIZED){
      char message[128];
      sprintf(message,"Destination %d is not initialized for logging\n",component->destination);
      lastResortLog(message);
      return;
    }

    ACEE *acee;
    acee = getAddressSpaceAcee();
    char user[7] = { 0 }; // wil this always be 7?
    snprintf(user,7,acee->aceeuser+1);

    char *locationInfo; // largest possible variation in size
    getLocationData(path,line,&locationInfo,compID,component); // location info is allocated in getLocationData

    char logLevel[8] = { 0 }; // logLevel will be a max of 8 characters
    getLevel(level, logLevel);

    char timestamp[24] = { 0 }; // formatted date is 24 characters
    getLogTimestamp(timestamp); // UTC time

    pthread_t threadID = pthread_self();
    char thread[10];
    snprintf(thread,10,"%d",threadID);

    //    timestamp + " <ZWESA1:thread> " + user + log level + locationInfo + " " + format string
    int newSize = 24 + 11 + 10 + 7 + strlen(logLevel) + strlen(locationInfo) + 1 + strlen(formatString); //accounts for formatting I know it's garbage
    char *newFormatString = (char *) safeMalloc(sizeof(char) * newSize,"newFormatString");
    snprintf(newFormatString, newSize,"%s <ZWESA1:%s> %s %s%s%s",timestamp,thread,user,logLevel,locationInfo,formatString); // final format string

    va_list argPointer;
    /* here, pass to a var-args handler */
    va_start(argPointer, formatString);

    destination->handler(context,component,destination->data,newFormatString,argPointer);
 
    va_end(argPointer);

    safeFree(locationInfo,sizeof(locationInfo));
    safeFree(newFormatString,sizeof(char) * newSize);
  }
}

static void printToDestination(LoggingDestination *destination,
                               struct LoggingContext_tag *context,
                               LoggingComponent *component,
                               void *data, char *formatString, ...){
  va_list argPointer;
  va_start(argPointer, formatString);
  destination->handler(context,component,destination->data,formatString,argPointer);
  va_end(argPointer);
}

void zowedump(LoggingContext *context, uint64 compID, int level, void *data, int dataSize){

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
        printToDestination(destination, context, component, destination->data, "%s\n", result);
      }
      else {
        break;
      }
    }

  }

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

