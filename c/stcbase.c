

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

#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "openprims.h"
#include "collections.h"
#include "stcbase.h"

/* handleReadySockets is a highly system-specific way of waking 
   running asynchronous IO work.   Windows and linux are different enought to need some data-hiding 
   and abstraction. 
*/

#define MODULE_ID_TO_INDEX(id) ((id)>>16)
#define PAYLOAD_CODE_TO_INDEX(code) ((code & 0xFFFF0000) >> 16)

static int lowLevelTrace = FALSE;


static int stcInvokeModuleBackgroundHandlers(STCBase *base, int selectStatus)
{
  /* for every module that has a heartbeat handler defined,
   * call it with the base and the module */
  STCModule *module = NULL;
  for (int i=0; i < STC_MAX_MODULES; i++)
  {
    if ((NULL != (module = base->modules[i])) &&
        (NULL != module->backgroundHandler))
    {
      if (lowLevelTrace) {
        BASETRACE_ALWAYS("invoking heartbeat handler for module ID: %d\n", module->id);
      }
      module->backgroundHandler(base, module, selectStatus);
    }
  }
  return 0;
}

static int stcDrainQueueAndDispatchWork(STCBase *base)
{
  while (TRUE)
  {
    void *elementData = qRemove(base->workQueue);
    if (elementData == NULL) {
      break;
    }
    WorkElementPrefix *workElement = (WorkElementPrefix*)elementData;
    STCModule *module = base->modules[ PAYLOAD_CODE_TO_INDEX(workElement->payloadCode) ];
    if (NULL != module) {
      if (NULL != module->workElementHandler) {
        module->workElementHandler(base, module, workElement);
      } else {
        if (lowLevelTrace) {
          BASETRACE_ALWAYS("no workElementHandler defined for module (%d)\n",
                 PAYLOAD_CODE_TO_INDEX(workElement->payloadCode));
        }
        return 8;
      }
    } else {
      if (lowLevelTrace) {
        BASETRACE_ALWAYS("stcDrainQueue, moduleNotFound for %d\n", workElement->payloadCode);
      }
      return 8;
    }
  }
  return 0;
}


int stcBaseMainLoop(STCBase *base, int selectTimeoutMillis)
{
  int selectStatus = 0;
  int returnCode=0, reasonCode=0;

  resetQueuedWorkEvent(base);
  while (!base->stopRequested)
  {
#ifdef DEBUG
    BASETRACE_ALWAYS("before main loop wait\n");
#ifdef __ZOWE_OS_ZOS
    BASETRACE_ALWAYS("MAIN_LOOP: tcb=0x%x qReadyECB=0x%x\n",getTCB(),base->qReadyECB);
#endif
#endif
    int readReady = FALSE;
    int readTheQueue = FALSE;
    selectStatus = stcBaseSelect(base,
                                 selectTimeoutMillis,
                                 FALSE, /* check write? */
                                 TRUE, /* check read? */
                                 &returnCode,&reasonCode);
#ifdef STCBASE_TRACE
    STCTRACE_VERBOSE("SELECTX (Mk 2), selectStatus = 0x%x qReadyECB=0x%x\n",selectStatus,base->qReadyECB);
#endif

    if (selectStatus == STC_BASE_SELECT_FAIL)
    {
      /* treat this as a fatal error */
      base->stopRequested = 1;
    }
    else if (selectStatus == STC_BASE_SELECT_TIMEOUT)
    {
#ifdef STCBASE_TRACE
      STCTRACE_VERBOSE("SELECTX timed out\n", NULL);
#endif
#ifdef TRACK_MEMORY
#ifdef DEBUG
        showOutstanding ();
#endif
#endif
      readTheQueue = TRUE;
    }
    else if (selectStatus == STC_BASE_SELECT_NOTHING_OBVIOUS_HAPPENED)
    {
#ifdef STCBASE_TRACE
      STCTRACE_VERBOSE("stcBaseSelect didn't see anything\n");
#endif
      readTheQueue = TRUE;
    }
    else
    {
      readTheQueue = TRUE;
      if (selectStatus & STC_BASE_SELECT_SOCKETS_READY)
      {
#ifdef STCBASE_TRACE
        STCTRACE_VERBOSE("SELECTX succeeded, now which sockets are ready?\n", NULL);
#endif
        readReady = TRUE;
      }
    }

    if (base->stopRequested){
      break;
    }

    /* clear immediately to sync better with fast posting PC routine */
    resetQueuedWorkEvent(base);

    /* the real meat of the run loop */
    if (readReady) {
      stcHandleReadySockets(base, selectStatus);
    }
    stcInvokeModuleBackgroundHandlers(base, selectStatus);
    if (readTheQueue) {
      stcDrainQueueAndDispatchWork(base);
    }

  }
  return 0;
}

int stcRegisterSocketExtension(STCBase *stcBase,
                               SocketExtension *socketExtension,
                               int moduleID)
{
  socketExtension->moduleID = moduleID;
  socketSetAdd(stcBase->socketSet,socketExtension->socket);
  return 0;
}

int stcReleaseSocketExtension(STCBase *stcBase,
                              SocketExtension *socketExtension)
{
  socketSetRemove(stcBase->socketSet, socketExtension->socket);
  return 0;
}

static int handleReadySocket(STCBase *base, Socket *readySocket){
  int sts = 0;
  SocketExtension *extension = (SocketExtension*)readySocket->userData;
  if (extension == NULL){
    BASETRACE_ALWAYS("need default protocol handling for socket %s\n",readySocket->debugName);
    sts = 12;
  } else if (memcmp(extension->eyecatcher,SOCKEXTN_TAG,8)){
    BASETRACE_ALWAYS("non-SocketExtension seen in user data\n");
    sts = 12;
  } else {
    
    STCModule *module = base->modules[MODULE_ID_TO_INDEX(extension->moduleID)];
    if (module != NULL){
      switch (readySocket->protocol){
      case IPPROTO_TCP:
        if (module->tcpHandler){
          sts = module->tcpHandler(base,module,readySocket);
        } else {
          BASETRACE_ALWAYS("IO ready on TCP socket %s but no module TCP handler!\n",readySocket->debugName);
          sts = 12;
        }
        break;
      case IPPROTO_UDP:
        if (module->udpHandler){
          sts = module->udpHandler(base,module,readySocket);
        } else {
          BASETRACE_ALWAYS("IO ready on UDP socket %s but no module UDP handler!\n",readySocket->debugName);
          sts = 12;
        }
        break;
      default:
        BASETRACE_ALWAYS("WARNING: non-TCP or UDP socket?\n");
        dumpbuffer((char*)readySocket, sizeof(Socket));
        sts = 12;
        break;
      }
    } else {
      BASETRACE_ALWAYS("handleReadySockets, moduleNotFound for %d\n",extension->moduleID);
      sts = 12;
    }
  }
  return sts;
}

#ifdef __ZOWE_OS_WINDOWS

int stcHandleReadySockets(STCBase *base,
                          int selectStatus){
  SocketSet *socketSet = base->socketSet;
  int currentEventSetSize = base->currentEventSetSize;
  for (int i=0; i<currentEventSetSize; i++){
    if (i>0){ /* socket == 0 is reserved for the q ready event */
      if (base->currentReadyEvents[i]){
        Socket *readySocket = socketSet->sockets[i-1];
        int handlerStatus = handleReadySocket(base,readySocket);
        if (8 <= handlerStatus) {
          BASETRACE_MAJOR("handleReadySocket error %d; removing sd=%d from socketSet\n", handlerStatus, sd);
          /* remove socket from socketSet to prevent tight loop */
          socketSetRemove(socketSet, readySocket);
        }
      }
      /*
        I have currently decided to always reset after the handler
        is called.  It is assumed in a well-written app that
        the network IO handler *WILL* read/write what it should.
        
        The other choice would be to leave this to be the responsibility
        of the handler.  This would be less unix-like and different from
        existing code.
      */
      int resetStatus = ResetEvent(base->currentEventSet[i]);
      BASETRACE_ALWAYS("STCBase.resetting event for place=%d status=%d\n",i,resetStatus);
    }
  }
  return 0;
}

#elif defined(__ZOWE_OS_ZOS)

int stcHandleReadySockets(STCBase *base,
                          int selectStatus){
  SocketSet *socketSet = base->socketSet;
  int maxArrayLengthInWords = (socketSet->highestAllowedSD+32) /  32;
  int socketMaskSize = maxArrayLengthInWords*sizeof(int);
  int i;
  
  int sd = 0;
  if (lowLevelTrace){
    BASETRACE_ALWAYS("stcHandleReady server at 0x%x, maxArray=%d\n",base,maxArrayLengthInWords);
  }
  for (i=0; i<maxArrayLengthInWords; i++){
    int mask = socketSet->scratchReadMask[i];
    int errorMask = socketSet->scratchErrorMask[i];
    for (int j=0; j<32; j++){
      if (mask & (1<<j)){
        Socket *readySocket = socketSet->sockets[sd];
        int handlerStatus = handleReadySocket(base,readySocket);
        if (8 <= handlerStatus) {
          BASETRACE_MAJOR("handleReadySocket error %d; removing sd=%d from socketSet\n", handlerStatus, sd);
          /* remove socket from socketSet to prevent tight loop */
          socketSetRemove(base->socketSet, readySocket);
        }
      }
      sd++;
    }
  }
  return 0;
}

#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)

int stcHandleReadySockets(STCBase *base,
                          int selectStatus){
  SocketSet *socketSet = base->socketSet;
  if (lowLevelTrace){
    BASETRACE_ALWAYS("stcHandleReadySockets: maxSD=%d\n",socketSet->highestAllowedSD);
  }
  for (uint32_t sd = 0; sd <= socketSet->highestAllowedSD; ++sd) {
    if (FD_ISSET(sd, &(socketSet->scratchReadMask))) {
      Socket *readySocket = socketSet->sockets[sd];
      int handlerStatus = handleReadySocket(base,readySocket);
      if (8 <= handlerStatus) {
        BASETRACE_MAJOR("handleReadySocket error %d; removing sd=%d from socketSet\n", handlerStatus, sd);
        /* remove socket from socketSet to prevent tight loop */
        socketSetRemove(base->socketSet, readySocket);
      }
    }
  }
  return 0;
}

#else
#error Unknown OS  
#endif

int stcEnqueueWork(STCBase *base, WorkElementPrefix *element){
  qInsert(base->workQueue,element);
  /* does qInsert have any fail conditions that should skip the next statement and return
     to PC call */
#ifdef __ZOWE_OS_ZOS
  zosPost(&(base->qReadyECB),0);
#elif defined(__ZOWE_OS_WINDOWS)
  SetEvent(base->qReadyEvent);  /* the event does not carry another bit/word/byte, which is different from zOS
                                       So, we need some way of looking up data, maybe there can be a hashtable in the STC base
                                       to act as a "transfer register" to the main loop 
                                    */


#elif defined(__ZOWE_OS_AIX)
  postEventSocket(base->eventSockets[1]);
#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
  postEventSocket(base->eventSocket);
#else 
#error Unknown OS
#endif
  return 0; /* what other codes code happen? */
}

void resetQueuedWorkEvent(STCBase *stcBase){
#ifdef __ZOWE_OS_ZOS
  stcBase->qReadyECB = 0;
#elif defined(__ZOWE_OS_WINDOWS)
  ResetEvent(stcBase->qReadyEvent);
#elif defined(__ZOWE_OS_AIX)
  //clearEventSocket(stcBase->eventSockets[0]);
#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
  /* TBD: Should anything happen here? */
  clearEventSocket(stcBase->eventSocket);
#else
#error Unknown OS  
#endif 
}

#ifndef __ZOWE_OS_ZOS

int stcBaseInit(STCBase *stcBase){
#ifdef __ZOWE_OS_WINDOWS
  stcBase->qReadyEvent = CreateEvent(NULL,TRUE,FALSE,"STCBase Q Ready Event");
  if (stcBase->qReadyEvent == NULL){
    BASETRACE_ALWAYS("*** PANIC *** could not make qReadyEvent\n");
    fflush(stdout);
  } else {
    BASETRACE_ALWAYS("STCBASE Q Ready Event handle = 0x%x\n",stcBase->qReadyEvent);
  }
#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
  stcBase->socketSet = makeSocketSet(FD_SETSIZE-1);
#ifdef __ZOWE_OS_AIX
  stcBase->eventSockets = makeEventSockets();
  socketSetAdd(stcBase->socketSet, stcBase->eventSockets[0]);
#else
  stcBase->eventSocket = makeEventSocket();
  socketSetAdd(stcBase->socketSet, stcBase->eventSocket);
#endif
  stcBase->logContext = makeLoggingContext();
  logConfigureStandardDestinations(stcBase->logContext);
  logConfigureComponent(stcBase->logContext,LOG_COMP_STCBASE,"STCBASE",LOG_DEST_PRINTF_STDERR,ZOWE_LOG_INFO);
  stcBase->workQueue = makeQueue(QUEUE_ALL_BELOW_BAR);
#else
#error Unknown OS
#endif
  return 0;
}

void stcBaseTerm(STCBase *stcBase) {
  /* TODO implement for other operating systems */
}

#endif /* not z/OS */

STCModule* stcRegisterModule(STCBase *base,
                             int moduleID,
                             void *moduleData,
                             int  (*tcpHandler)(STCBase *base, STCModule *module, Socket *socket),
                             int  (*udpHandler)(STCBase *base, STCModule *module, Socket *socket),
                             int  (*workElementHandler)(STCBase *base, STCModule *stcModule, WorkElementPrefix *prefix),
                             int  (*backgroundHandler)(struct STCBase_tag *base, struct STCModule_tag *stcModule, int selectStatus))
{
  STCModule *module = (STCModule*)safeMalloc(sizeof(STCModule),"STC Module");
#ifdef DEBUG
  BASETRACE_ALWAYS("beginning register module at 0x%p, moduleID=%d, stcBase=0x%p\n",module,moduleID,base);
  fflush(stdout);
#endif
  memset(module,0,sizeof(STCModule));
  memcpy(module->eyecatcher,"STCMODUL",8);
  module->id = moduleID;
  module->data = moduleData;
  module->tcpHandler = tcpHandler;
  module->udpHandler = udpHandler;
  module->workElementHandler = workElementHandler;
  module->backgroundHandler = backgroundHandler;
#ifdef DEBUG
  dumpbuffer((char*)module,sizeof(STCModule));
  fflush(stdout);
#endif
  base->modules[MODULE_ID_TO_INDEX(moduleID)] = module;
  return module;
}

/* For stcBaseSelect, Values <= 0 are non-normal statuses.
   positive values are masks */

#ifdef __ZOWE_OS_ZOS

int stcBaseSelect(STCBase *base,
                  int timeout,             /* in milliseconds */
                  int checkWrite, int checkRead, 
                  int *returnCode, int *reasonCode){
  BASETRACE_VERBOSE("before base select socketSet at 0x%x\n",base->socketSet);
  fflush(stdout);
  int status = extendedSelect(base->socketSet,
                              timeout,             /* in milliseconds */ 
                              &(base->qReadyECB),
                              checkWrite,checkRead,
                              returnCode,reasonCode);
  BASETRACE_VERBOSE("after base select status=%d\n",status);
  fflush(stdout);
  if (status == -1){
    return STC_BASE_SELECT_FAIL;
  } else if (status == 0){
    if (base->qReadyECB  & 0x40000000){
      BASETRACE_VERBOSE("work ready\n");
      return STC_BASE_SELECT_WORK_ELEMENTS_READY;
    } else {
      BASETRACE_VERBOSE("timed out\n");
      return STC_BASE_SELECT_TIMEOUT;
    }
  } else {
    int result = STC_BASE_SELECT_SOCKETS_READY  | (base->qReadyECB ? STC_BASE_SELECT_WORK_ELEMENTS_READY : 0);
    return result;
  }
}


#elif defined(__ZOWE_OS_WINDOWS) 

int stcBaseSelect(STCBase *stcBase,
                  int timeout,             /* in milliseconds */
                  int checkWrite, int checkRead, 
                  int *returnCode, int *reasonCode){

  SocketSet *socketSet = stcBase->socketSet;
  HANDLE *currentEventSet = NULL;
  unsigned long *currentReadyEvents;      // for this socket set
  int failedToBuildEventSet = FALSE;
  int currentEventSetSize = -1;
  if ((stcBase->currentEventSet == NULL) ||
      (socketSet->revisionNumber > stcBase->socketSetRevisionNumber)) {
    if (stcBase->currentEventSet != NULL){
      BASETRACE_VERBOSE("should probably closeHandle on all previous events\n");
      safeFree((char*)stcBase->currentEventSet,sizeof(HANDLE)*stcBase->currentEventSetSize);
      safeFree((char*)stcBase->currentReadyEvents,sizeof(unsigned long)*stcBase->currentEventSetSize);
    }
    currentEventSetSize = socketSet->socketCount+1;
    currentEventSet = (HANDLE*)safeMalloc(sizeof(HANDLE)*currentEventSetSize,"Windows Event Set");
    currentReadyEvents = (int*)safeMalloc(sizeof(unsigned long)*currentEventSetSize,"Windows Ready Socket booleans");
    for (int i=0; i<currentEventSetSize; i++){
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "event set build loop = %d\n",i);
      if (i==0){
        currentEventSet[i] = stcBase->qReadyEvent;
      } else {
        Socket *socket = socketSet->sockets[i-1];
        HANDLE socketEvent = CreateEvent(NULL,TRUE,FALSE,"Socket Event");
        
        int eventSelectStatus= WSAEventSelect(socket->windowsSocket, socketEvent, FD_READ | FD_WRITE | FD_ACCEPT);
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "eventSelectStatus=%d fpor socket handle=0x%x\n",eventSelectStatus,socket->windowsSocket);
        if (eventSelectStatus != 0){
          zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "WSAEventSelectFailed status=%d WSA lasteError=%d\n",eventSelectStatus,WSAGetLastError());
          fflush(stdout);
          failedToBuildEventSet = TRUE;
          break;
        }
        currentEventSet[i] = socketEvent;
      }
    }
    stcBase->currentEventSetSize = currentEventSetSize;
    stcBase->currentEventSet = currentEventSet;
    stcBase->currentReadyEvents = currentReadyEvents;
    stcBase->socketSetRevisionNumber = socketSet->revisionNumber;
  } else {
    currentEventSetSize = stcBase->currentEventSetSize;
    currentEventSet = stcBase->currentEventSet;
    currentReadyEvents = stcBase->currentReadyEvents;
  }
  
  if (failedToBuildEventSet){
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING, "*** WARNING FAILED TO BUILD EVENT SET\n");
    return STC_BASE_SELECT_FAIL;
  } 
  memset(currentReadyEvents,0,sizeof(unsigned long)*currentEventSetSize);

  int status = WaitForMultipleObjects(currentEventSetSize,currentEventSet,FALSE,timeout);  /* windows *DOES* use milliseconds for this parm */
  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "waitForMultiple status=%d\n",status);
  if (status == WAIT_TIMEOUT){
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "woke up due to timeout\n");
    fflush(stdout);
    return STC_BASE_SELECT_TIMEOUT;
  } else if (status == -1){
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "WaitForMultipleObjects status=%d WSA lasteError=%d\n",status,GetLastError());
    fflush(stdout);
    return STC_BASE_SELECT_FAIL; 
  } else if (status == WAIT_OBJECT_0){  /* we have set up the first element of the event array to be that of the work Q */
    return STC_BASE_SELECT_WORK_ELEMENTS_READY;
  } else if ((status >=  WAIT_OBJECT_0) && (status < (WAIT_OBJECT_0 + currentEventSetSize))){
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "FD_READ=0x%x FD_WRITE=0x%x FD_ACCEPT=0x%x\n",FD_READ,FD_WRITE,FD_ACCEPT);
    for (int j=1; j<currentEventSetSize; j++){
      WSANETWORKEVENTS networkEvents;
      HANDLE hEvent = currentEventSet[j];
      Socket *socketToPoll = socketSet->sockets[j-1];
      int enumStatus = WSAEnumNetworkEvents(socketToPoll->windowsSocket,hEvent,&networkEvents);

      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "enum for j=%d status = %d for socket handle 0x%x -> networkEvents=0x%x\n",
             j,enumStatus,socketToPoll->windowsSocket,networkEvents.lNetworkEvents);
      currentReadyEvents[j] = networkEvents.lNetworkEvents;
    }
    return STC_BASE_SELECT_SOCKETS_READY; /* the set is set in currentReadyEvents, see msdn for WSAEnumNetworkEvents */
  } else if ((status >= WAIT_ABANDONED_0) && (status < (WAIT_ABANDONED_0 + currentEventSetSize))){
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "thread woke up becase event %d was abandoned, which isn't really a failure, but needs a new status to clean up sockets\n",status);
    return STC_BASE_SELECT_FAIL; 
  } else {
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "What is this status=%d\n",status);
    return STC_BASE_SELECT_FAIL;
  }
}

#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)

int stcBaseSelect(STCBase *base,
                  int timeout,             /* in milliseconds */
                  int checkWrite, int checkRead, 
                  int *returnCode, int *reasonCode)
{
  SocketSet* socketSet = base->socketSet;
  BASETRACE_VERBOSE("before base select socketSet at 0x%x\n",socketSet);
  fflush(stdout);
  int status = extendedSelect(socketSet,
                              timeout,             /* in milliseconds */ 
                              checkWrite,checkRead,
                              returnCode,reasonCode);
  BASETRACE_VERBOSE("after base select status=%d\n",status);
  fflush(stdout);

  int result = 0;
  if (status == -1){
    result =  STC_BASE_SELECT_FAIL;
  } else if (status == 0){
    result = STC_BASE_SELECT_TIMEOUT;
  } else {
    /* At least one socket is ready. If we were enabled for reads, and the
       event "socket" is ready, then work elements are ready. */
#ifdef __ZOWE_OS_AIX
    Socket* es = (Socket*)base->eventSockets[0];
#else
    Socket* es = (Socket*)base->eventSocket;
#endif
    if (checkRead && 
        FD_ISSET(es->sd, &(socketSet->scratchReadMask))) {
      result = STC_BASE_SELECT_WORK_ELEMENTS_READY;
      FD_CLR(es->sd, &(socketSet->scratchReadMask));
      clearEventSocket(es); /* read and discard count */
      if (--status > 0) {
        result |= STC_BASE_SELECT_SOCKETS_READY;
      }
    } else {
      result = STC_BASE_SELECT_SOCKETS_READY;
    }
  }
  return result; 
}

#else
#error unknown OS
#endif

#ifdef __ZOWE_OS_ZOS /* all this console task stuff is z/OS only */

ZOWE_PRAGMA_PACK

typedef struct ExtractParameters_tag{
  Addr31 listAddress;
  Addr31 tcbAddress;
  char   fieldByte1;
  char   fieldByte2;
  char   reserved0A[2];
} ExtractParameters;

typedef struct IEZCOM_tag{
  Addr31 comecbpt;
  Addr31 comcibpt;
  char   comtoken[4];    /* high bit indicated token set */
} IEZCOM;

ZOWE_PRAGMA_PACK_RESET

/* static utilities copied from stcmain */
static void extract(IEZCOM * __ptr32 * __ptr32 iezcom)
{
  ExtractParameters *parameters = (ExtractParameters*)safeMalloc31(sizeof(ExtractParameters),"ExtractParameters");
  memset(parameters,0,sizeof(ExtractParameters));
  parameters->listAddress = (Addr31)iezcom;
  parameters->fieldByte1 = 1;
  parameters->fieldByte2 = 0;
#ifdef DEBUG
  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "EXTRACT(2) parameters at 0x%p\n",parameters);
  dumpbuffer((char*)parameters,sizeof(ExtractParameters));
#endif
  int parametersAsInt = (int)parameters;
  __asm(
        " SAM31  \n"
        " XGR 1,1 \n"
        " LLGT 1,%0 \n"
        " OILH 1,X'8000' \n"
        " SVC 40 \n"
#ifdef _LP64
        " SAM64 "
#else
        " DS 0H "
#endif
        :
        : "m"(parametersAsInt)
        : "r15");
#ifdef DEBUG
  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "EXTRACT parms after SVC\n");
  dumpbuffer((char*)parameters,sizeof(ExtractParameters));
#endif
}

static void makeTCB(RLEAnchor *anchor,
                    int (*taskMain)(RLETask*),
                    char *name, void *context)
{
  RLETask *task = makeRLETask(anchor,
                              RLE_TASK_TCB_CAPABLE,
                              taskMain);
  task->userPointer = context;
  startRLETask(task,NULL);
}

int stcLaunchConsoleTask(STCBase *base,
                         int (*commandReceiverMain)(RLETask *task),
                         void *userdata)
{
  int iezcomStorage = 0;
  IEZCOM * __ptr32 * __ptr32 iezcomHandle = (IEZCOM * __ptr32 * __ptr32 )&iezcomStorage; /* An agressive cast ! */
  extract(iezcomHandle);
  base->iezcom = (IEZCOM*)iezcomStorage;
  makeTCB(base->iezcom, commandReceiverMain, "consoleTask", userdata);
  bpxSleep (1);
  return 0;
}

ZOWE_PRAGMA_PACK

typedef struct CommandHandlerContext_tag {
  char eyecatcher[8];
#define COMMAND_HANDLER_CONTEXT_EYECATCHER "RSCHCEYE"
  STCBase *stcBase;
  STCUserCommandHandler *userHandler;
  void *userData;
} CommandHandlerContext;

ZOWE_PRAGMA_PACK_RESET

static STCConsoleCommandType translateCIDVerbToCommandType(char verb) {

  STCConsoleCommandType commandType;

  if (verb == CID_COMMAND_VERB_START) {
    commandType = STC_COMMAND_START;
  } else if (verb == CID_COMMAND_VERB_STOP) {
    commandType = STC_COMMAND_STOP;
  } else if (verb == CID_COMMAND_VERB_MODIFY) {
    commandType = STC_COMMAND_MODIFY;
  } else {
    commandType = STC_COMMAND_UNKNOWN;
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "unexpected CIB command value 0x%X\n", verb);
  }

  return commandType;
}

#define COMMAND_BUFFER_SIZE 512

static int consoleTaskMain(RLETask *task) {

  CommandHandlerContext *context = task->userPointer;
  STCBase *base = context->stcBase;
  IEZCOM *iezcom = base->iezcom;

  int iezcomECBAddress = (int)iezcom->comecbpt & 0x7FFFFFFF;
  int commandShutdownECBAddress = (int)&base->commandShutdownECB;
  int ecbList[2];
  ecbList [0] = iezcomECBAddress;
  ecbList [1] = commandShutdownECBAddress;
  ecbList [1] |= 0x80000000;

  __asm (
      ASM_PREFIX
      "         QEDIT ORIGIN=(%0),CIBCTR=1                                   \n"
      :
      : "r"(&(iezcom->comcibpt))
      : "r15"
  );

  while (!base->stopRequested) {

    while (iezcom->comcibpt != NULL) {

      CIB *cib = (CIB *)iezcom->comcibpt;

      STCConsoleCommandType commandType = translateCIDVerbToCommandType(cib->cibverb);
      char commandBuffer[COMMAND_BUFFER_SIZE];

      unsigned short commandLength = cib->cibdatln;
      char *command = &cib->cibdata;
      if (commandLength <= sizeof(commandBuffer)) {
        context->userHandler(base, cib, commandType, command, commandLength, context->userData);
      } else {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_DEBUG, "command length too big %hu\n", commandLength);
      }

      if (commandType == STC_COMMAND_STOP) {
        base->stopRequested = TRUE;
        zosPost(&base->qReadyECB, 0);
        break;
      }

      __asm (
          ASM_PREFIX
          "         QEDIT ORIGIN=(%0),BLOCK=(%1)                                 \n"
          :
          : "r"(&(iezcom->comcibpt)), "r"(cib)
          : "r15"
      );

    }

    zosWaitList((void *)ecbList, 1);
  }

  safeFree31((char *)context, sizeof(CommandHandlerContext));
  context = NULL;

  return 0;
}

int stcLaunchConsoleTask2(STCBase *base, STCUserCommandHandler *userHandler, void *userData) {

  IEZCOM * __ptr32 iezcomHandle = NULL;
  extract(&iezcomHandle);
  base->iezcom = iezcomHandle;

  CommandHandlerContext *context = (CommandHandlerContext *)safeMalloc31(sizeof(CommandHandlerContext), "CommandHandlerContext");
  memcpy(context->eyecatcher, COMMAND_HANDLER_CONTEXT_EYECATCHER, sizeof(context->eyecatcher));
  context->stcBase = base;
  context->userHandler = userHandler;
  context->userData = userData;

  RLETask *task = makeRLETask(base->rleAnchor, RLE_TASK_DISPOSABLE | RLE_TASK_RECOVERABLE | RLE_TASK_TCB_CAPABLE, consoleTaskMain);
  task->userPointer = context;
  startRLETask(task, NULL);

  context = NULL;

  return 0;
}

int stcBaseShutdown(STCBase *stcBase) {

  stcBase->stopRequested = TRUE;
  zosPost(&stcBase->commandShutdownECB, 0);

  return 0;
}

#endif /* __ZOWE_OS_ZOS */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

