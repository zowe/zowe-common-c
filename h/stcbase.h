

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __STCBASE__
#define __STCBASE__ 1

/*
  What prerequisites does STCBASE assume?

  The ground level stuff
  - zowetypes
  - alloc
  - utils

  Data Structures for thread-safe queues, hash tables, etc
  - collections
  
  Socket Stuff
  - bpxskt/winskt/mtlskt
  - sktmgmt

  But, should HttpServer really be presest?
  - referring to HttpServer as "struct HttpServer" is less strict than using HttpServer
 */

#include "bpxnet.h"
#include "collections.h"
#include "le.h"
#include "logging.h"
#include "socketmgmt.h"
#include "utils.h"
#include "zowetypes.h"

#define STC_MODULE_GENERIC          0x00000000
#define STC_MODULE_JEDHTTP          0x00010000
#define STC_MODULE_BACKGROUND              0x00020000

#define STC_MODULE_OOBSVCS_CLIENT   0x00030000
#define STC_MODULE_OOBSVCS_SERVER   0x00040000

#define STC_MAX_MODULES        100

#ifndef BASETRACE_ALWAYS
#define BASETRACE_ALWAYS(...) \
    zowelog(base->logContext,LOG_COMP_STCBASE,ZOWE_LOG_WARNING,__VA_ARGS__)
#define BASETRACE_MAJOR(...) \
    zowelog(base->logContext,LOG_COMP_STCBASE,ZOWE_LOG_DEBUG,__VA_ARGS__)
#define BASETRACE_MINOR(...) \
    zowelog(base->logContext,LOG_COMP_STCBASE,ZOWE_LOG_DEBUG2,__VA_ARGS__)
#define BASETRACE_VERBOSE(...) \
    zowelog(base->logContext,LOG_COMP_STCBASE,ZOWE_LOG_DEBUG3,__VA_ARGS__)
#endif

typedef struct WorkElementPrefix_tag{
  char eyecatcher[8];  /* e.g "WRKELMNT" */
  int  version;
  int  flags;
  int  payloadCode;    /* high half word is STC module code, low half is product specific code */
  int  payloadLength;
  char rfu [8];             /* RFU */
} WorkElementPrefix;

struct STCBase_tag; 

typedef struct STCModule_tag {
  char eyecatcher[8]; /* "STCMODUL"; */
  int  id;            /* the module id */
  void *data;
  int  (*tcpHandler)(struct STCBase_tag *stcBase, struct STCModule_tag *module, Socket *socket);  /* module ID's need to replace HTTP protocol ID's */
  int  (*udpHandler)(struct STCBase_tag *stcBase, struct STCModule_tag *module, Socket *socket);
  int  (*workElementHandler)(struct STCBase_tag *stcBase, struct STCModule_tag *stcModule, WorkElementPrefix *prefix);
  int  (*backgroundHandler)(struct STCBase_tag *stcBase, struct STCModule_tag *stcModule, int selectReturnCode);
} STCModule;

typedef struct STCBase_tag{
  char        eyecatcher[8]; /* "STCBASE " */
  int         version;
#ifdef __ZOWE_OS_ZOS
  int         qReadyECB;
  RLEAnchor  *rleAnchor;
#elif defined(__ZOWE_OS_WINDOWS)
  HANDLE      qReadyEvent;
#elif defined(__ZOWE_OS_AIX)
  void      **eventSockets;
  RLEAnchor  *rleAnchor;
#elif defined(__ZOWE_OS_LINUX)
  void       *eventSocket;
  RLEAnchor  *rleAnchor;
#else
#error Unknown OS
#endif
#ifdef __ZOWE_64
  Queue      *workQueue;
#else
  int         alignmentInt1;
  Queue      *workQueue;
#endif
  int         stopRequested;
  int         commandShutdownECB;
  void       *iezcom;                    /* this is really a mainframe-specific system command handler */
  SocketSet  *socketSet;
  struct      HttpServer *httpServer;    /* this is more chicken-shitty than I would really like */
  STCModule  *modules[STC_MAX_MODULES];
/*  int64       traceControlWord; replaced with logging context*/

  LoggingContext *logContext;
#ifdef __ZOWE_OS_WINDOWS
  int         socketSetRevisionNumber;
  int         currentEventSetSize;
  HANDLE     *currentEventSet;           /* represents an event array for sockets and internal "work ready" events */
  unsigned long *currentReadyEvents;
#endif
} STCBase;

int stcEnqueueWork(STCBase *stcBase, WorkElementPrefix *element); /* safe, proper, courteous to other modules */

#ifndef __ZOWE_OS_ZOS /* On z/OS we use a macro because a function call clobbers R12 on return. */
int stcBaseInit(STCBase *stcBase);
#else
#define stcBaseInit(base) \
  { \
    memset(base, 0x00, sizeof(STCBase)); \
    memcpy(base->eyecatcher, "STCBASE", sizeof(base->eyecatcher)); \
    base->rleAnchor = makeRLEAnchor(); \
    initRLEEnvironment(base->rleAnchor); \
    CAA *caa = (CAA *)getCAA(); \
    (base)->rleAnchor = ((RLETask *)caa->rleTask)->anchor; \
    (base)->logContext = makeLoggingContext(); \
    logConfigureStandardDestinations((base)->logContext); \
    logConfigureComponent((base)->logContext,LOG_COMP_STCBASE,"STCBASE",LOG_DEST_DEV_NULL,ZOWE_LOG_INFO); \
    (base)->socketSet = makeSocketSet(1023); \
    (base)->workQueue = makeQueue(QUEUE_ALL_BELOW_BAR); \
  }
#endif

#ifndef __ZOWE_OS_ZOS
void stcBaseTerm(STCBase *stcBase);
#else
#define stcBaseTerm(base) \
  { \
    destroyQueue((base)->workQueue); \
    (base)->workQueue = NULL; \
    freeSocketSet((base)->socketSet); \
    (base)->socketSet = NULL; \
    removeLoggingContext(); \
    termRLEEnvironment(); \
    deleteRLEAnchor((base)->rleAnchor); \
  }
#endif


STCModule* stcRegisterModule(STCBase *stcBase,
                             int moduleID,
                             void *moduleData,
                             int  (*tcpHandler)(STCBase *stcBase, STCModule *module, Socket *socket),
                             int  (*udpHandler)(STCBase *stcBase, STCModule *module, Socket *socket),
                             int  (*workElementHandler)(STCBase *stcBase, STCModule *stcModule, WorkElementPrefix *prefix),
                             int  (*backgroundHandler)(struct STCBase_tag *stcBase, struct STCModule_tag *stcModule, int selectStatus));

int stcRegisterSocketExtension(STCBase *stcBase,
                               SocketExtension *socketExtension,
                               int moduleID);

int stcReleaseSocketExtension(STCBase *stcBase,
                              SocketExtension *socketExtension);

#ifdef __ZOWE_OS_ZOS
/* the userdata should provide the command receiver a way to address the base,
 * as well as any other context that will be needed to act on modify commands */
int stcLaunchConsoleTask(STCBase *base,
                         int (*commandReceiverMain)(RLETask *task),
                         void *userdata);

typedef enum STCConsoleCommandType_tag {
  STC_COMMAND_START,
  STC_COMMAND_STOP,
  STC_COMMAND_MODIFY,
  STC_COMMAND_UNKNOWN,
} STCConsoleCommandType;

ZOWE_PRAGMA_PACK

typedef struct CIB_tag{
  Addr31 cibnext;
  char   cibverb;   /* x4=start, 0x44=modify, x40=stop */
#define CID_COMMAND_VERB_START   0x04
#define CID_COMMAND_VERB_MODIFY  0x44
#define CID_COMMAND_VERB_STOP    0x40
  char   ciblen;    /* length in doublewords */
  unsigned short cibxoff; /* offset to extension */
  char   reserved08[2];
  unsigned short cibasid; /* ASID */
  char   reserved0C;
  char   cibversion;   /* probably 3 */
  unsigned short cibdatln; /* length in bytes of cibdata */
  char   cibdata;      /* first data bytes, probably longer */
} CIB;

typedef struct CIBX_tag{
  int    cibxutok;
  char   cibxauta;
  char   cibxautb;
  char   cibxdisp;
  char   cibxflg;
  char   cibxcnnm[8];
  char   cibxcart[8];
  int    cibxcnid;
  int    cibxptrc;
  int    cibxocid;
  char   cibxrsvd[12];
} CIBX;

ZOWE_PRAGMA_PACK_RESET

typedef int STCUserCommandHandler(STCBase *base, CIB *cib, STCConsoleCommandType commandType, const char *command, unsigned short commandLength, void *userData);

int stcLaunchConsoleTask2(STCBase *base, STCUserCommandHandler *userHandler, void *userData);

#endif

/* A main stc loop implementation that processes things in order of:
     - dispatching socket activity to modules
     - module heartbeats
     - dispatching work elements to modules
   ...and exits when the stopRequested flag is set.
   To be called after stcBaseInit, and
                after modules have been registered. */
int stcBaseMainLoop(STCBase *stcBase, int selectTimeoutMillis);

/* Values <= 0 are non-normal statuses.
   positive values are masks
*/

#define STC_BASE_SELECT_FAIL         (-1)
#define STC_BASE_SELECT_TIMEOUT      (-2)
#define STC_BASE_SELECT_NOTHING_OBVIOUS_HAPPENED 0 /* kinda weird to wake up with no events or timeout, but
                                               need this for completeness.  Applications probably *SHOULD* check
                                               their internal work Queue */
#define STC_BASE_SELECT_WORK_ELEMENTS_READY      1 /* mask test for WORK ready */
#define STC_BASE_SELECT_SOCKETS_READY            2 /* mask test for SOCKETS ready */


int stcBaseSelect(STCBase *stcBase,
                  int timeout,             /* in milliseconds */
                  int checkWrite, int checkRead, 
                  int *returnCode, int *reasonCode);

/* this will invoke the module's UDP or TCP handler if applicable */
int stcHandleReadySockets(STCBase *server,
                          int selectStatus);

void resetQueuedWorkEvent(STCBase *stcBase);

int stcBaseShutdown(STCBase *stcBase);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

