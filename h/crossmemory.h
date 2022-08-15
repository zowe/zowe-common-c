

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef H_CROSSMEMORY_H_
#define H_CROSSMEMORY_H_

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdlib.h>
#else
#include <stdlib.h>
#endif

#include "zowetypes.h"
#include "cellpool.h"
#include "utils.h"
#include "bpxnet.h"
#include "isgenq.h"
#include "lpa.h"
#include "nametoken.h"
#include "zos.h"
#include "socketmgmt.h"
#include "stcbase.h"

#ifndef CMS_COMP_ID
#define CMS_COMP_ID     "ZWE"
#endif

#ifndef CMS_SUBCOMP_ID
#define CMS_SUBCOMP_ID  "S"
#endif

#ifndef CMS_PROD_ID
#define CMS_PROD_ID CMS_COMP_ID CMS_SUBCOMP_ID
#endif

#ifndef CMS_MSG_SUBCOMP
#define CMS_MSG_SUBCOMP CMS_SUBCOMP_ID
#endif

#ifndef CMS_MSG_PRFX
#define CMS_MSG_PRFX  CMS_COMP_ID CMS_MSG_SUBCOMP
#endif

#define CROSS_MEMORY_SERVER_KEY     4
#define CROSS_MEMORY_SERVER_SUBPOOL 228

#define RC_CMS_OK                           0
#define RC_CMS_ERROR                        8
#define RC_CMS_PARM_NULL                    9
#define RC_CMS_PARM_BAD_EYECATCHER          10
#define RC_CMS_FUNCTION_ID_OUT_OF_RANGE     11
#define RC_CMS_GLOBAL_AREA_NULL             12
#define RC_CMS_GLOBAL_AREA_BAD_EYECATCHER   13
#define RC_CMS_SERVER_NULL                  14
#define RC_CMS_SERVER_BAD_EYECATCHER        15
#define RC_CMS_FUNCTION_NULL                16
#define RC_CMS_AXSET_FAILED                 17
#define RC_CMS_LXRES_FAILED                 18
#define RC_CMS_ETCRE_FAILED                 19
#define RC_CMS_ETCON_FAILED                 20
#define RC_CMS_DUPLICATE_SERVER             21
#define RC_CMS_ISGENQ_FAILED                22
#define RC_CMS_ECSA_ALLOC_FAILED            23
#define RC_CMS_NAME_TOKEN_CREATE_FAILED     24
#define RC_CMS_SERVER_NOT_AVAILABLE         25
#define RC_CMS_NAME_TOKEN_NOT_FOUND         26
#define RC_CMS_NAME_TOKEN_BAD_EYECATCHER    27
#define RC_CMS_LPA_ADD_FAILED               28
#define RC_CMS_LPA_DELETE_FAILED            29
#define RC_CMS_SERVER_NOT_READY             30
#define RC_CMS_NAME_TOKEN_DELETE_FAILED     31
#define RC_CMS_RACF_ROUTE_LIST_FAILED       32
#define RC_CMS_PERMISSION_DENIED            33
#define RC_CMS_PC_ENV_NOT_ESTABLISHED       34
#define RC_CMS_PC_ENV_NOT_TERMINATED        35
#define RC_CMS_PC_SERVICE_ABEND_DETECTED    36
#define RC_CMS_PC_RECOVERY_ENV_FAILED       37
#define RC_CMS_PC_NOT_IMPLEMENTED           38
#define RC_CMS_SERVER_ABENDED               39
#define RC_CMS_VSNPRINTF_FAILED             40
#define RC_CMS_MESSAGE_TOO_LONG             41
#define RC_CMS_LOGGING_CNTX_NOT_FOUND       42
#define RC_CMS_RECOVERY_CNTX_NOT_FOUND      43
#define RC_CMS_NAME_TOO_SHORT               44
#define RC_CMS_NAME_TOO_LONG                45
#define RC_CMS_SERVICE_NOT_INITIALIZED      46
#define RC_CMS_ZVT_NULL                     47
#define RC_CMS_ZVTE_NULL                    48
#define RC_CMS_ZVTE_NOT_ALLOCATED           49
#define RC_CMS_SERVICE_NOT_RELOCATABLE      50
#define RC_CMS_MAIN_LOOP_FAILED             51
#define RC_CMS_STACK_ALLOC_FAILED           52 /* keep in synch with handleProgramCall prolog */
#define RC_CMS_STACK_RELEASE_FAILED         53 /* keep in synch with handleProgramCall epilog */
#define RC_CMS_IMPROPER_SERVICE_AS          54
#define RC_CMS_BAD_SERVER_KEY               55
#define RC_CMS_RESMGR_NOT_ADDED             56
#define RC_CMS_RESMGR_NOT_REMOVED           57
#define RC_CMS_RESMGR_NOT_LOCKED            58
#define RC_CMS_RESMGR_NAMETOKEN_FAILED      59
#define RC_CMS_WRONG_SERVER_VERSION         60
#define RC_CMS_LATENT_PARM_NULL             61
#define RC_CMS_USER_PARM_NULL               62
#define RC_CMS_PC_HDLR_PARM_BAD_EYECATCHER  63
#define RC_CMS_ZERO_PC_NUMBER               64
#define RC_CMS_WRONG_CLIENT_VERSION         65
#define RC_CMS_ZVTE_CHAIN_LOOP              66
#define RC_CMS_ZVTE_CHAIN_NOT_LOCKED        67
#define RC_CMS_ZVTE_CHAIN_NOT_RELEASED      68
#define RC_CMS_SNPRINTF_FAILED              69
#define RC_CMS_SLH_NOT_CREATED              70
#define RC_CMS_MSG_QUEUE_NOT_CREATED        71
#define RC_CMS_CONFIG_HT_NOT_CREATED        72
#define RC_CMS_SLH_ALLOC_FAILED             73
#define RC_CMS_UNKNOWN_PARM_TYPE            74
#define RC_CMS_CONFIG_PARM_NAME_TOO_LONG    75
#define RC_CMS_CHAR_PARM_TOO_LONG           76
#define RC_CMS_CONFIG_PARM_NOT_FOUND        77
#define RC_CMS_STDSVC_PARM_NULL             78
#define RC_CMS_STDSVC_PARM_BAD_EYECATCHER   79
#define RC_CMS_NOT_APF_AUTHORIZED           80
#define RC_CMS_NO_STEPLIB                   81
#define RC_CMS_MODULE_NOT_IN_STEPLIB        82
#define RC_CMS_ZVT_NOT_ALLOCATED            83
#define RC_CMS_SERVER_NAME_NULL             84
#define RC_CMS_SERVICE_ENTRY_OCCUPIED       85
#define RC_CMS_NO_STORAGE_FOR_MSG           86
#define RC_CMS_ALLOC_FAILED                 87
#define RC_CMS_NON_PRIVATE_MODULE           88
#define RC_CMS_BAD_DUB_STATUS               89
#define RC_CMS_MODULE_QUERY_FAILED          90
#define RC_CMS_NO_ROOM_FOR_CMS_GETTER       91
#define RC_CMS_LANC_NOT_LOCKED              92
#define RC_CMS_LANC_NOT_RELEASED            93
#define RC_CMS_MAX_RC                       93

extern const char *CMS_RC_DESCRIPTION[];

ZOWE_PRAGMA_PACK

typedef struct CrossMemoryServerName_tag {
  char nameSpacePadded[16];
} CrossMemoryServerName;

#ifndef __LONGNAME__
#define CMS_DEFAULT_SERVER_NAME CMSDEFSN
#endif

extern const CrossMemoryServerName CMS_DEFAULT_SERVER_NAME;

typedef struct ELXLISTEntry_tag {
  int sequenceNumber;
  int pcNumber;
} ELXLISTEntry;

typedef struct ELXLIST_tag {
  int elxCount;
  ELXLISTEntry entries[32];
} ELXLIST;

#define CROSS_MEMORY_SERVER_PREFIX CMS_MSG_PRFX"."

#define CROSS_MEMORY_SERVER_MIN_SERVICE_ID    10
#define CROSS_MEMORY_SERVER_MAX_SERVICE_ID    127

#define CROSS_MEMEORY_SERVER_MAX_SERVICE_COUNT   (CROSS_MEMORY_SERVER_MAX_SERVICE_ID + 1)

#define CROSS_MEMORY_SERVER_MAX_CMD_TASK_NUM  30

struct CrossMemoryServerGlobalArea_tag;
struct CrossMemoryService_tag;

typedef int (CrossMemoryServiceFunction)(struct CrossMemoryServerGlobalArea_tag *globalArea, struct CrossMemoryService_tag *service, void *parm);

typedef struct CrossMemoryService_tag {
  CrossMemoryServiceFunction * __ptr32 function;
  unsigned int flags;
#define CROSS_MEMORY_SERVICE_FLAG_INITIALIZED   0x00000001
#define CROSS_MEMORY_SERVICE_FLAG_SPACE_SWITCH  0x00000002
#define CROSS_MEMORY_SERVICE_FLAG_LPA           0x00000004
  PAD_LONG(1, void *serviceData);
} CrossMemoryService;

#define CROSS_MEMORY_SERVER_VERSION             2
#define CROSS_MEMORY_SERVER_DISCARDED_VERSION   0xDEADDA7A

typedef struct CMSTimestamp_tag {
  char value[32];
} CMSBuildTimestamp;

typedef struct CrossMemoryServerGlobalArea_tag {

  char eyecatcher[8];
#define CMS_GLOBAL_AREA_EYECATCHER "RSCMSRVG"
  unsigned int version;
  unsigned char key;
  unsigned char subpool;
  unsigned short size;
  unsigned int flags;
  char reserved1[56];

  void * __ptr32 userServerAnchor;

  CrossMemoryServerName serverName;
  struct CrossMemoryServer_tag * __ptr32 localServerAddress;
  unsigned short serverASID;
  char padding1[2];
  unsigned int serverFlags;
  int ecsaBlockCount; /* must be on a 4-byte boundary for atomicIncrement */
#define CMS_MAX_ECSA_BLOCK_NUMBER 32
#define CMS_MAX_ECSA_BLOCK_SIZE   65536
  CMSBuildTimestamp lpaModuleTimestamp;
  char reserved2[16];

  int (* __ptr32 pccpHandler)();
  LPMEA lpaModuleInfo;

  struct {
    int pcssPCNumber;
    int pcssSequenceNumber;
    int pccpPCNumber;
    int pccpSequenceNumber;
  } pcInfo;

  int pcLogLevel;

  PAD_LONG(0, struct RecoveryStatePool_tag *pcssRecoveryPool);
  CPID pcssStackPool;

  char padding0[4];
  /* This is an opt-in feature for CMS Servers that want to offer Dynamic
   * Linkage to some of their routines is an MVS/Metal/ASM way. That is to
   * provide a well-known linkage vector with well-known offsets.
   *
   * The CMS server does not stipulate anything about what these services are.
   *
   * The ZIS (a CMS) server uses this for its plugins.
	 */
  PAD_LONG(1, void *userServerDynLinkVector);

  char reserved3[480];

  CrossMemoryService serviceTable[CROSS_MEMEORY_SERVER_MAX_SERVICE_COUNT];

  char reserved4[1320];

} CrossMemoryServerGlobalArea;

typedef struct CrossMemoryServerToken_tag {
  union {
    struct {
      char eyecatcher[8];
#define CMS_TOKEN_EYECATCHER "RSCMSTKN"
      CrossMemoryServerGlobalArea * __ptr32 globalArea;
      char reserved[4];
    };
    NameTokenUserToken ntToken;
  };
} CrossMemoryServerToken;

typedef uint64 CART;

typedef struct CMSWTORouteInfo_tag {
  CART cart;
  int consoleID;
  char padding[4];
} CMSWTORouteInfo;

typedef struct CMSModifyCommand_tag {
  CMSWTORouteInfo routeInfo;
  const char *commandVerb;
  const char *target;
  const char* const* args;
  unsigned int argCount;
} CMSModifyCommand;

typedef enum CMSModifyCommandStatus_tag {
  CMS_MODIFY_COMMAND_STATUS_UNKNOWN = 1,
  CMS_MODIFY_COMMAND_STATUS_PROCESSED = 2,
  CMS_MODIFY_COMMAND_STATUS_CONSUMED = 3,
  CMS_MODIFY_COMMAND_STATUS_REJECTED = 4,
} CMSModifyCommandStatus;

typedef int (CMSStarCallback)(CrossMemoryServerGlobalArea *globalArea,
                              void *userData);
typedef int (CMSStopCallback)(CrossMemoryServerGlobalArea *globalArea,
                              void *userData);
typedef int (CMSModifyCommandCallback)(CrossMemoryServerGlobalArea *globalArea,
                                       const CMSModifyCommand *command,
                                       CMSModifyCommandStatus *status,
                                       void *userData);

typedef struct CrossMemoryServer_tag {
  char eyecatcher[8];
#define CMS_EYECATCHER "RSCMSRV1"
  unsigned int flags;
#define CROSS_MEMORY_SERVER_FLAG_COLD_START   0x00000001
#define CROSS_MEMORY_SERVER_FLAG_READY        0x00000002
#define CROSS_MEMORY_SERVER_FLAG_TERM_STARTED 0x00000004
#define CROSS_MEMORY_SERVER_FLAG_TERM_ENDED   0x00000008
#define CROSS_MEMORY_SERVER_FLAG_CHECKAUTH    0x00000010
#define CROSS_MEMORY_SERVER_FLAG_CLEAN_LPA    0x00000020
  STCBase * __ptr32 base;
  CMSStarCallback * __ptr32 startCallback;
  CMSStopCallback * __ptr32 stopCallback;
  CMSModifyCommandCallback * __ptr32 commandCallback;
  int commandTaskCount;
  PAD_LONG(0, void *callbackData);
  CrossMemoryServerGlobalArea * __ptr32 globalArea;
  CMSWTORouteInfo startCommandInfo;
  CMSWTORouteInfo termCommandInfo;
  void * __ptr32 moduleAddressLocal;
  void * __ptr32 moduleAddressLPA;
  unsigned int moduleSize;
  CrossMemoryServerName name;
  EightCharString ddname;
  EightCharString dsname;
  ENQToken serverENQToken;
  ShortLivedHeap * __ptr32 slh;
  Queue * __ptr32 messageQueue;
  CPID messageQueueMainPool;
  CPID messageQueueFallbackPool;
  hashtable * __ptr32 configParms;
  LPMEA lpaCodeInfo;
  ELXLIST pcELXList;
  unsigned int pcssStackPoolSize;
  unsigned int pcssRecoveryPoolSize;
  CrossMemoryService serviceTable[CROSS_MEMEORY_SERVER_MAX_SERVICE_COUNT];
} CrossMemoryServer;

typedef struct CrossMemoryServerParmList_tag {
  char eyecatcher[8];
#define CMS_PARMLIST_EYECATCHER "RSCMSPRM"
  int version;
  CrossMemoryServerName serverName;
  int serviceID;
  int serviceRC;
  char reserved[2];
  uint16_t flags;
#define CMS_PARMLIST_FLAG_NONE            0x0000
#define CMS_PARMLIST_FLAG_NO_SAF_CHECK    0x0001
  PAD_LONG(1, void *callerData);
} CrossMemoryServerParmList;

typedef enum CrossMemoryServerParmType_tag {
  CMS_CONFIG_PARM_TYPE_CHAR
} CrossMemoryServerParmType;

#define CMS_CONFIG_PARM_MAX_NAME_LENGTH   72
#define CMS_CONFIG_PARM_MAX_VALUE_SIZE    128

typedef struct CrossMemoryServerConfigParm_tag {
  char eyecatcher[8];
#define CMS_PARM_EYECATCHER "RSCMSCFG"
  unsigned short valueLength;
  CrossMemoryServerParmType type;
  union {
    char charValueNullTerm[CMS_CONFIG_PARM_MAX_VALUE_SIZE];
  };
} CrossMemoryServerConfigParm;

typedef struct CrossMemoryServerStatus_tag {
  int cmsRC;
  char descriptionNullTerm[64];
} CrossMemoryServerStatus;

ZOWE_PRAGMA_PACK_RESET

#define LOG_COMP_ID_CMS       0x008F0001000C0001LLU
#define LOG_COMP_ID_CMSPC     0x008F0001000C0002LLU

#ifndef __LONGNAME__

#define cmsInitializeLogging CMINILOG
#define makeCrossMemoryServer CMMCMSRV
#define makeCrossMemoryServer2 CMMCMSR2
#define removeCrossMemoryServer CMMCRSRV
#define cmsSetPoolParameters CMCMSSPP
#define cmsRegisterService CMCMSRSR
#define cmsStartMainLoop CMCMAINL
#define cmsGetGlobalArea CMGETGA
#define cmsAddConfigParm CMADDPRM
#define cmsCallService CMCMSRCS
#define cmsCallService2 CMCALLS2
#define cmsCallService3 CMCALLS3
#define cmsPrintf CMCMSPRF
#define vcmsPrintf CMCMSVPF 
#define cmsGetConfigParm CMGETPRM
#define cmsGetConfigParmUnchecked CMGETPRU
#define cmsGetPCLogLevel CMGETLOG
#define cmsGetStatus CMGETSTS
#define cmsMakeServerName CMMKSNAM
#define cmsAllocateECSAStorage CMECSAAL
#define cmsFreeECSAStorage CMECSAFR
#define cmsAllocateECSAStorage2 CMECSAA2
#define cmsFreeECSAStorage2 CMECSAF2

#endif

#define CMS_SERVER_FLAG_NONE                  0x00000000
#define CMS_SERVER_FLAG_COLD_START            0x00000001
#define CMS_SERVER_FLAG_DEBUG                 0x00000002
#define CMS_SERVER_FLAG_CHECKAUTH             0x00000004
#define CMS_SERVER_FLAG_DEV_MODE              0x00000008
#define CMS_SERVER_FLAG_DEV_MODE_LPA          0x00000010

#define CMS_SERVICE_FLAG_NONE                 0x00000000
#define CMS_SERVICE_FLAG_SPACE_SWITCH         0x00000001
#define CMS_SERVICE_FLAG_RELOCATE_TO_COMMON   0x00000002
#define CMS_SERVICE_FLAG_CODE_IN_COMMON       0x00000004

/* server side functions (must be authorized and in supervisor mode) */
void cmsInitializeLogging();
CrossMemoryServer *makeCrossMemoryServer(STCBase *base, const CrossMemoryServerName *serverName, unsigned int flags, int *reasonCode);
CrossMemoryServer *makeCrossMemoryServer2(
    STCBase *base,
    const CrossMemoryServerName *serverName,
    unsigned int flags,
    CMSStarCallback *startCallback,
    CMSStopCallback *stopCallback,
    CMSModifyCommandCallback *commandCallback,
    void *callbackData,
    int *reasonCode
);
void removeCrossMemoryServer(CrossMemoryServer *server);
void cmsSetPoolParameters(CrossMemoryServer *server,
                          unsigned int pcssStackPoolSize,
                          unsigned int pcssRecoveryPoolSize);
int cmsRegisterService(CrossMemoryServer *server, int id, CrossMemoryServiceFunction *serviceFunction, void *serviceData, int flags);
int cmsStartMainLoop(CrossMemoryServer *server);
int cmsGetGlobalArea(const CrossMemoryServerName *serverName, CrossMemoryServerGlobalArea **globalAreaAddress);
int cmsAddConfigParm(CrossMemoryServer *server,
                     const char *name, const void *value,
                     CrossMemoryServerParmType type);

/**
 * Checks if the cross-memory caller has read access to the provided class
 * and entity.
 *
 * @param globalArea The global area of the server.
 * @param className The class to be checked.
 * @param entityName The entity to be checked.
 * @return True if the caller has read access to the class/entity, otherwise
 * false.
 */
bool cmsTestAuth(CrossMemoryServerGlobalArea *globalArea,
                 const char *className,
                 const char *entityName);


/* Use these inside your service functions if they need ECSA.
 * The number of allocated blocks is tracked in the CMS global area. */
void *cmsAllocateECSAStorage(CrossMemoryServerGlobalArea *globalArea, unsigned int size);
void cmsFreeECSAStorage(CrossMemoryServerGlobalArea *globalArea, void *data, unsigned int size);
void cmsAllocateECSAStorage2(CrossMemoryServerGlobalArea *globalArea,
                             unsigned int size, void **resultPtr);
void cmsFreeECSAStorage2(CrossMemoryServerGlobalArea *globalArea,
                         void **dataPtr, unsigned int size);

/* client side definitions */
#define CMS_CALL_FLAG_NONE                0x00000000
#define CMS_CALL_FLAG_NO_SAF_CHECK        0x00000001

/* client side functions */
int cmsCallService(const CrossMemoryServerName *serverName, int functionID, void *parmList, int *serviceRC);
int cmsCallService2(CrossMemoryServerGlobalArea *cmsGlobalArea,
                    int serviceID, void *parmList, int *serviceRC);
int cmsCallService3(CrossMemoryServerGlobalArea *cmsGlobalArea,
                    int serviceID, void *parmList, int flags, int *serviceRC);

/**
 * @brief Print a message to a cross-memory server's log
 * @param serverName Cross-memory server to whose log the message is to be
 * printed
 * @param formatString Format string of the message
 * @return RC_CMS_OK in case of success, and one of the RC_CMS_nn values in
 * case of failure
 */
int cmsPrintf(const CrossMemoryServerName *serverName, const char *formatString, ...);

/* 
   @brief the var-args version of cmsPrintf, see above
 */
int vcmsPrintf(const CrossMemoryServerName *serverName, const char *formatString, va_list argPointer);

/**
 * @brief Print the hex dump of the specified storage to a cross-memory server's
 * log
 * @param serverName Cross-memory server to whose log the dump is to be printed
 * @param data Data to be printed
 * @param size Size of the data (the maximum size is 512 bytes, the rest will
 * be truncated)
 * @param description Description of the data (the maximum length is 31
 * characters, the rest will be truncated)
 * @return RC_CMS_OK in case of success, and one of the RC_CMS_nn values in
 * case of failure
 */
int cmsHexDump(const CrossMemoryServerName *serverName,
               const void *data, unsigned size, const char *description);

/**
 * @brief Get a parameter from the cross-memory server's PARMLIB
 * @param serverName Cross-memory server whose parameter is to be read
 * @param name Name of the parameter
 * @param parm Result parameter entry
 * @return RC_CMS_OK in case of success, and one of the RC_CMS_nn values in
 * case of failure
 */
int cmsGetConfigParm(const CrossMemoryServerName *serverName, const char *name,
                     CrossMemoryServerConfigParm *parm);

/**
 * @brief Get a parameter from the cross-memory server's PARMLIB without the
 * authorization check (the caller must be SUP or system key)
 * @param serverName Cross-memory server whose parameter is to be read
 * @param name Name of the parameter
 * @param parm Result parameter entry
 * @return RC_CMS_OK in case of success, and one of the RC_CMS_nn values in
 * case of failure
 */
int cmsGetConfigParmUnchecked(const CrossMemoryServerName *serverName,
                              const char *name,
                              CrossMemoryServerConfigParm *parm);

int cmsGetPCLogLevel(const CrossMemoryServerName *serverName);
CrossMemoryServerStatus cmsGetStatus(const CrossMemoryServerName *serverName);
CrossMemoryServerName cmsMakeServerName(const char *nameNullTerm);

/* default message IDs (users of crossmemory.c can potentially redefine them) */

/* 001 - 099 should be used by the application that use the core cross memory server */
/* 100 - 199 are core server messages from the highest level (cmsStartMainLoop)  */
/* 200 - 399 are core server messages from lower level functions  */
/* 400 - 699 are messages from core services  */
/* 700 - 999 are messages from user services  */

#ifndef CMS_LOG_DEBUG_MSG_ID
#define CMS_LOG_DEBUG_MSG_ID                    CMS_MSG_PRFX"0100I"
#endif

#ifndef CMS_LOG_DUMP_MSG_ID
#define CMS_LOG_DUMP_MSG_ID                     CMS_MSG_PRFX"0101I"
#endif

#ifndef CMS_LOG_INIT_STEP_FAILURE_MSG_ID
#define CMS_LOG_INIT_STEP_FAILURE_MSG_ID        CMS_MSG_PRFX"0102E"
#endif
#define CMS_LOG_INIT_STEP_FAILURE_MSG_TEXT      "Initialization step \'%s\' failed, RC = %d"
#define CMS_LOG_INIT_STEP_FAILURE_MSG           CMS_LOG_INIT_STEP_FAILURE_MSG_ID" "CMS_LOG_INIT_STEP_FAILURE_MSG_TEXT

#ifndef CMS_LOG_INIT_STEP_SUCCESS_MSG_ID
#define CMS_LOG_INIT_STEP_SUCCESS_MSG_ID        CMS_MSG_PRFX"0103I"
#endif
#define CMS_LOG_INIT_STEP_SUCCESS_MSG_TEXT      "Initialization step \'%s\' successfully completed"
#define CMS_LOG_INIT_STEP_SUCCESS_MSG           CMS_LOG_INIT_STEP_SUCCESS_MSG_ID" "CMS_LOG_INIT_STEP_SUCCESS_MSG_TEXT

#ifndef CMS_LOG_STARTING_CONSOLE_TASK_MSG_ID
#define CMS_LOG_STARTING_CONSOLE_TASK_MSG_ID    CMS_MSG_PRFX"0104I"
#endif
#define CMS_LOG_STARTING_CONSOLE_TASK_MSG_TEXT  "About to start console task"
#define CMS_LOG_STARTING_CONSOLE_TASK_MSG       CMS_LOG_STARTING_CONSOLE_TASK_MSG_ID" "CMS_LOG_STARTING_CONSOLE_TASK_MSG_TEXT

#ifndef CMS_LOG_INIT_STARTED_MSG_ID
#define CMS_LOG_INIT_STARTED_MSG_ID             CMS_MSG_PRFX"0105I"
#endif
#define CMS_LOG_INIT_STARTED_MSG_TEXT           "Core server initialization started"
#define CMS_LOG_INIT_STARTED_MSG                CMS_LOG_INIT_STARTED_MSG_ID" "CMS_LOG_INIT_STARTED_MSG_TEXT

#ifndef CMS_LOG_INIT_FAILED_MSG_ID
#define CMS_LOG_INIT_FAILED_MSG_ID              CMS_MSG_PRFX"0106E"
#endif
#define CMS_LOG_INIT_FAILED_MSG_TEXT            "Core server initialization failed, RC = %d"
#define CMS_LOG_INIT_FAILED_MSG                 CMS_LOG_INIT_FAILED_MSG_ID" "CMS_LOG_INIT_FAILED_MSG_TEXT

#ifndef CMS_LOG_COLD_START_MSG_ID
#define CMS_LOG_COLD_START_MSG_ID               CMS_MSG_PRFX"0107I"
#endif
#define CMS_LOG_COLD_START_MSG_TEXT             "Cold start initiated"
#define CMS_COLD_START_MSG                      CMS_LOG_COLD_START_MSG_ID" "CMS_LOG_COLD_START_MSG_TEXT

#ifndef CMS_LOG_GLB_CLEANUP_WARN_MSG_ID
#define CMS_LOG_GLB_CLEANUP_WARN_MSG_ID         CMS_MSG_PRFX"0108W"
#endif
#define CMS_LOG_GLB_CLEANUP_WARN_MSG_TEXT       "Global resources clean up RC = %d"
#define CMS_LOG_GLB_CLEANUP_WARN_MSG            CMS_LOG_GLB_CLEANUP_WARN_MSG_ID" "CMS_LOG_GLB_CLEANUP_WARN_MSG_TEXT

#ifndef CMS_LOG_SERVER_READY_MSG_ID
#define CMS_LOG_SERVER_READY_MSG_ID             CMS_MSG_PRFX"0109I"
#endif
#define CMS_LOG_SERVER_READY_MSG_TEXT           "Core server ready"
#define CMS_LOG_SERVER_READY_MSG                CMS_LOG_SERVER_READY_MSG_ID" "CMS_LOG_SERVER_READY_MSG_TEXT

#ifndef CMS_LOG_MAIN_LOOP_FAILURE_MSG_ID
#define CMS_LOG_MAIN_LOOP_FAILURE_MSG_ID        CMS_MSG_PRFX"0110E"
#endif
#define CMS_LOG_MAIN_LOOP_FAILURE_MSG_TEXT      "Main loop unexpectedly terminated"
#define CMS_LOG_MAIN_LOOP_FAILURE_MSG           CMS_LOG_MAIN_LOOP_FAILURE_MSG_ID" "CMS_LOG_MAIN_LOOP_FAILURE_MSG_TEXT

#ifndef CMS_LOG_MAIN_LOOP_TERM_MSG_ID
#define CMS_LOG_MAIN_LOOP_TERM_MSG_ID           CMS_MSG_PRFX"0111I"
#endif
#define CMS_LOG_MAIN_LOOP_TERM_MSG_TEXT         "Main loop terminated"
#define CMS_LOG_MAIN_LOOP_TERM_MSG              CMS_LOG_MAIN_LOOP_TERM_MSG_ID" "CMS_LOG_MAIN_LOOP_TERM_MSG_TEXT

#ifndef CMS_LOG_TERM_STEP_FAILURE_MSG_ID
#define CMS_LOG_TERM_STEP_FAILURE_MSG_ID        CMS_MSG_PRFX"0112E"
#endif
#define CMS_LOG_TERM_STEP_FAILURE_MSG_TEXT      "Termination step \'%s\' failed, RC = %d"
#define CMS_LOG_TERM_STEP_FAILURE_MSG           CMS_LOG_TERM_STEP_FAILURE_MSG_ID" "CMS_LOG_TERM_STEP_FAILURE_MSG_TEXT

#ifndef CMS_LOG_TERM_STEP_SUCCESS_MSG_ID
#define CMS_LOG_TERM_STEP_SUCCESS_MSG_ID        CMS_MSG_PRFX"0113I"
#endif
#define CMS_LOG_TERM_STEP_SUCCESS_MSG_TEXT      "Termination step \'%s\' successfully completed"
#define CMS_LOG_TERM_STEP_SUCCESS_MSG           CMS_LOG_TERM_STEP_SUCCESS_MSG_ID" "CMS_LOG_TERM_STEP_SUCCESS_MSG_TEXT

#ifndef CMS_LOG_SERVER_STOPPED_MSG_ID
#define CMS_LOG_SERVER_STOPPED_MSG_ID           CMS_MSG_PRFX"0114I"
#endif
#define CMS_LOG_SERVER_STOPPED_MSG_TEXT         "Core server stopped"
#define CMS_LOG_SERVER_STOPPED_MSG              CMS_LOG_SERVER_STOPPED_MSG_ID" "CMS_LOG_SERVER_STOPPED_MSG_TEXT

#ifndef CMS_LOG_SERVER_STOP_FAILURE_MSG_ID
#define CMS_LOG_SERVER_STOP_FAILURE_MSG_ID      CMS_MSG_PRFX"0115E"
#endif
#define CMS_LOG_SERVER_STOP_FAILURE_MSG_TEXT    "Core server stopped with an error, status = %d"
#define CMS_LOG_SERVER_STOP_FAILURE_MSG         CMS_LOG_SERVER_STOP_FAILURE_MSG_ID" "CMS_LOG_SERVER_STOP_FAILURE_MSG_TEXT

#ifndef CMS_LOG_SERVER_ABEND_MSG_ID
#define CMS_LOG_SERVER_ABEND_MSG_ID             CMS_MSG_PRFX"0116E"
#endif
#define CMS_LOG_SERVER_ABEND_MSG_TEXT           "Core server is abnormally terminating"
#define CMS_LOG_SERVER_ABEND_MSG                CMS_LOG_SERVER_ABEND_MSG_ID" "CMS_LOG_SERVER_ABEND_MSG_TEXT

#ifndef CMS_LOG_NOT_APF_AUTHORIZED_MSG_ID
#define CMS_LOG_NOT_APF_AUTHORIZED_MSG_ID       CMS_MSG_PRFX"0117E"
#endif
#define CMS_LOG_NOT_APF_AUTHORIZED_MSG_TEXT     "Not APF-authorized (%d)"
#define CMS_LOG_NOT_APF_AUTHORIZED_MSG          CMS_LOG_NOT_APF_AUTHORIZED_MSG_ID" "CMS_LOG_NOT_APF_AUTHORIZED_MSG_TEXT

#ifndef CMS_LOG_BAD_SERVER_KEY_MSG_ID
#define CMS_LOG_BAD_SERVER_KEY_MSG_ID           CMS_MSG_PRFX"0118E"
#endif
#define CMS_LOG_BAD_SERVER_KEY_MSG_TEXT         "Core server started in wrong key %d"
#define CMS_LOG_BAD_SERVER_KEY_MSG              CMS_LOG_BAD_SERVER_KEY_MSG_ID" "CMS_LOG_BAD_SERVER_KEY_MSG_TEXT

#ifndef CMS_LOG_MODIFY_CMD_INFO_MSG_ID
#define CMS_LOG_MODIFY_CMD_INFO_MSG_ID          CMS_MSG_PRFX"0200I"
#endif
#define CMS_LOG_MODIFY_CMD_INFO_MSG_TEXT        ""
#define CMS_LOG_MODIFY_CMD_INFO_MSG             CMS_LOG_MODIFY_CMD_INFO_MSG_ID" "CMS_LOG_MODIFY_CMD_INFO_MSG_TEXT

#ifndef CMS_LOG_BAD_SERVICE_ID_MSG_ID
#define CMS_LOG_BAD_SERVICE_ID_MSG_ID           CMS_MSG_PRFX"0201E"
#endif
#define CMS_LOG_BAD_SERVICE_ID_MSG_TEXT         "Service ID %d is out of range"
#define CMS_LOG_BAD_SERVICE_ID_MSG              CMS_LOG_BAD_SERVICE_ID_MSG_ID" "CMS_LOG_BAD_SERVICE_ID_MSG_TEXT

#ifndef CMS_LOG_DUP_SERVER_MSG_ID
#define CMS_LOG_DUP_SERVER_MSG_ID               CMS_MSG_PRFX"0202E"
#endif
#define CMS_LOG_DUP_SERVER_MSG_TEXT             "A duplicate server is running"
#define CMS_LOG_DUP_SERVER_MSG                  CMS_LOG_DUP_SERVER_MSG_ID" "CMS_LOG_DUP_SERVER_MSG_TEXT

#ifndef CMS_LOG_SERVER_NOT_LOCKED_MSG_ID
#define CMS_LOG_SERVER_NOT_LOCKED_MSG_ID        CMS_MSG_PRFX"0203E"
#endif
#define CMS_LOG_SERVER_NOT_LOCKED_MSG_TEXT      "Server not locked, ISGENQ RC = %d, RSN = 0x%08X"
#define CMS_LOG_SERVER_NOT_LOCKED_MSG           CMS_LOG_SERVER_NOT_LOCKED_MSG_ID" "CMS_LOG_SERVER_NOT_LOCKED_MSG_TEXT

#ifndef CMS_LOG_GLB_AREA_NULL_MSG_ID
#define CMS_LOG_GLB_AREA_NULL_MSG_ID            CMS_MSG_PRFX"0204E"
#endif
#define CMS_LOG_GLB_AREA_NULL_MSG_TEXT          "Global area address in NULL"
#define CMS_LOG_GLB_AREA_NULL_MSG               CMS_LOG_GLB_AREA_NULL_MSG_ID" "CMS_LOG_GLB_AREA_NULL_MSG_TEXT

#ifndef CMS_LOG_RELOC_FAILURE_MSG_ID
#define CMS_LOG_RELOC_FAILURE_MSG_ID            CMS_MSG_PRFX"0205E"
#endif
#define CMS_LOG_RELOC_FAILURE_MSG_TEXT          "Relocation failed for %d (0x%p not in [0x%p, 0x%p])"
#define CMS_LOG_RELOC_FAILURE_MSG               CMS_LOG_RELOC_FAILURE_MSG_ID" "CMS_LOG_RELOC_FAILURE_MSG_TEXT

#ifndef CMS_LOG_INVALID_EYECATCHER_MSG_ID
#define CMS_LOG_INVALID_EYECATCHER_MSG_ID       CMS_MSG_PRFX"0206E"
#endif
#define CMS_LOG_INVALID_EYECATCHER_MSG_TEXT     "%s (0x%p) has invalid eyecatcher"
#define CMS_LOG_INVALID_EYECATCHER_MSG          CMS_LOG_INVALID_EYECATCHER_MSG_ID" "CMS_LOG_INVALID_EYECATCHER_MSG_TEXT

#ifndef CMS_LOG_ALLOC_FAILURE_MSG_ID
#define CMS_LOG_ALLOC_FAILURE_MSG_ID            CMS_MSG_PRFX"0207E"
#endif
#define CMS_LOG_ALLOC_FAILURE_MSG_TEXT          "%s (%zu) not allocated"
#define CMS_LOG_ALLOC_FAILURE_MSG               CMS_LOG_ALLOC_FAILURE_MSG_ID" "CMS_LOG_ALLOC_FAILURE_MSG_TEXT

#ifndef CMS_LOG_LPA_LOAD_FAILURE_MSG_ID
#define CMS_LOG_LPA_LOAD_FAILURE_MSG_ID         CMS_MSG_PRFX"0208E"
#endif
#define CMS_LOG_LPA_LOAD_FAILURE_MSG_TEXT       "Module not loaded into LPA, RC = %d, RSN = %d"
#define CMS_LOG_LPA_LOAD_FAILURE_MSG            CMS_LOG_LPA_LOAD_FAILURE_MSG_ID" "CMS_LOG_LPA_LOAD_FAILURE_MSG_TEXT

#ifndef CMS_LOG_LPA_DELETE_FAILURE_MSG_ID
#define CMS_LOG_LPA_DELETE_FAILURE_MSG_ID       CMS_MSG_PRFX"0209E"
#endif
#define CMS_LOG_LPA_DELETE_FAILURE_MSG_TEXT     "Module not deleted from LPA, RC = %d, RSN = %d"
#define CMS_LOG_LPA_DELETE_FAILURE_MSG          CMS_LOG_LPA_DELETE_FAILURE_MSG_ID" "CMS_LOG_LPA_DELETE_FAILURE_MSG_TEXT

#ifndef CMS_LOG_LPMEA_INVALID_MSG_ID
#define CMS_LOG_LPMEA_INVALID_MSG_ID            CMS_MSG_PRFX"0210W"
#endif
#define CMS_LOG_LPMEA_INVALID_MSG_TEXT          "No valid LPMEA in global area"
#define CMS_LOG_LPMEA_INVALID_MSG               CMS_LOG_LPMEA_INVALID_MSG_ID" "CMS_LOG_LPMEA_INVALID_MSG_TEXT

#ifndef CMS_LOG_NTP_DELETE_FAILURE_MSG_ID
#define CMS_LOG_NTP_DELETE_FAILURE_MSG_ID       CMS_MSG_PRFX"0211E"
#endif
#define CMS_LOG_NTP_DELETE_FAILURE_MSG_TEXT     "Name/Token delete failed, RC = %d"
#define CMS_LOG_NTP_DELETE_FAILURE_MSG          CMS_LOG_NTP_DELETE_FAILURE_MSG_ID" "CMS_LOG_NTP_DELETE_FAILURE_MSG_TEXT

#ifndef CMS_LOG_RACROUTE_LIST_FAILURE_MSG_ID
#define CMS_LOG_RACROUTE_LIST_FAILURE_MSG_ID    CMS_MSG_PRFX"0212E"
#endif
#define CMS_LOG_RACROUTE_LIST_FAILURE_MSG_TEXT  "RACROUTE LIST failed (%d, %d, %d)"
#define CMS_LOG_RACROUTE_LIST_FAILURE_MSG       CMS_LOG_RACROUTE_LIST_FAILURE_MSG_ID" "CMS_LOG_RACROUTE_LIST_FAILURE_MSG_TEXT

#ifndef CMS_LOG_ZVT_NOT_POPULATED_MSG_ID
#define CMS_LOG_ZVT_NOT_POPULATED_MSG_ID        CMS_MSG_PRFX"0213E"
#endif
#define CMS_LOG_ZVT_NOT_POPULATED_MSG_TEXT      "ZVT not populated, RC = %d"
#define CMS_LOG_ZVT_NOT_POPULATED_MSG           CMS_LOG_ZVT_NOT_POPULATED_MSG_ID" "CMS_LOG_ZVT_NOT_POPULATED_MSG_TEXT

#ifndef CMS_LOG_GLB_AREA_NOT_SET_MSG_ID
#define CMS_LOG_GLB_AREA_NOT_SET_MSG_ID         CMS_MSG_PRFX"0214E"
#endif
#define CMS_LOG_GLB_AREA_NOT_SET_MSG_TEXT       "Global area not set, RC = %d"
#define CMS_LOG_GLB_AREA_NOT_SET_MSG            CMS_LOG_GLB_AREA_NOT_SET_MSG_ID" "CMS_LOG_GLB_AREA_NOT_SET_MSG_TEXT

#ifndef CMS_LOG_GLB_AREA_NOT_RET_MSG_ID
#define CMS_LOG_GLB_AREA_NOT_RET_MSG_ID         CMS_MSG_PRFX"0215E"
#endif
#define CMS_LOG_GLB_AREA_NOT_RET_MSG_TEXT       "Global area not retrieved, RC = %d"
#define CMS_LOG_GLB_AREA_NOT_RET_MSG            CMS_LOG_GLB_AREA_NOT_RET_MSG_ID" "CMS_LOG_GLB_AREA_NOT_RET_MSG_TEXT

#ifndef CMS_LOG_PC_SET_FAILURE_MSG_ID
#define CMS_LOG_PC_SET_FAILURE_MSG_ID           CMS_MSG_PRFX"0216E"
#endif
#define CMS_LOG_PC_SET_FAILURE_MSG_TEXT         "PC-%s not set, step = %s (%d %d)"
#define CMS_LOG_PC_SET_FAILURE_MSG              CMS_LOG_PC_SET_FAILURE_MSG_ID" "CMS_LOG_PC_SET_FAILURE_MSG_TEXT

#ifndef CMS_LOG_TOO_MANY_CMD_TOKENS_MSG_ID
#define CMS_LOG_TOO_MANY_CMD_TOKENS_MSG_ID      CMS_MSG_PRFX"0217E"
#endif
#define CMS_LOG_TOO_MANY_CMD_TOKENS_MSG_TEXT    "Too many tokens in command"
#define CMS_LOG_TOO_MANY_CMD_TOKENS_MSG         CMS_LOG_TOO_MANY_CMD_TOKENS_MSG_ID" "CMS_LOG_TOO_MANY_CMD_TOKENS_MSG_TEXT

#ifndef CMS_LOG_CMD_TOO_LONG_MSG_ID
#define CMS_LOG_CMD_TOO_LONG_MSG_ID             CMS_MSG_PRFX"0218E"
#endif
#define CMS_LOG_CMD_TOO_LONG_MSG_TEXT           "Command too long (%u)"
#define CMS_LOG_CMD_TOO_LONG_MSG                CMS_LOG_CMD_TOO_LONG_MSG_ID" "CMS_LOG_CMD_TOO_LONG_MSG_TEXT

#ifndef CMS_LOG_CMD_TKNZ_FAILURE_MSG_ID
#define CMS_LOG_CMD_TKNZ_FAILURE_MSG_ID         CMS_MSG_PRFX"0219E"
#endif
#define CMS_LOG_CMD_TKNZ_FAILURE_MSG_TEXT       "Command not tokenized"
#define CMS_LOG_CMD_TKNZ_FAILURE_MSG            CMS_LOG_CMD_TKNZ_FAILURE_MSG_ID" "CMS_LOG_CMD_TKNZ_FAILURE_MSG_TEXT

#ifndef CMS_LOG_CMD_RECEIVED_MSG_ID
#define CMS_LOG_CMD_RECEIVED_MSG_ID             CMS_MSG_PRFX"0220I"
#endif
#define CMS_LOG_CMD_RECEIVED_MSG_TEXT           "Modify %s command received"
#define CMS_LOG_CMD_RECEIVED_MSG                CMS_LOG_CMD_RECEIVED_MSG_ID" "CMS_LOG_CMD_RECEIVED_MSG_TEXT

#ifndef CMS_LOG_CMD_ACCEPTED_MSG_ID
#define CMS_LOG_CMD_ACCEPTED_MSG_ID             CMS_MSG_PRFX"0221I"
#endif
#define CMS_LOG_CMD_ACCEPTED_MSG_TEXT           "Modify %s command accepted"
#define CMS_LOG_CMD_ACCEPTED_MSG                CMS_LOG_CMD_ACCEPTED_MSG_ID" "CMS_LOG_CMD_ACCEPTED_MSG_TEXT

#ifndef CMS_LOG_DISP_CMD_RESULT_MSG_ID
#define CMS_LOG_DISP_CMD_RESULT_MSG_ID          CMS_MSG_PRFX"0222I"
#endif
#define CMS_LOG_DISP_CMD_RESULT_MSG_TEXT        ""
#define CMS_LOG_DISP_CMD_RESULT_MSG             CMS_LOG_DISP_CMD_RESULT_MSG_ID" "CMS_LOG_DISP_CMD_RESULT_MSG_TEXT

#ifndef CMS_LOG_TERM_CMD_MSG_ID
#define CMS_LOG_TERM_CMD_MSG_ID                 CMS_MSG_PRFX"0223I"
#endif
#define CMS_LOG_TERM_CMD_MSG_TEXT               "Termination command received"
#define CMS_LOG_TERM_CMD_MSG                    CMS_LOG_TERM_CMD_MSG_ID" "CMS_LOG_TERM_CMD_MSG_TEXT

#ifndef CMS_LOG_INVALID_CMD_ARGS_MSG_ID
#define CMS_LOG_INVALID_CMD_ARGS_MSG_ID         CMS_MSG_PRFX"0224W"
#endif
#define CMS_LOG_INVALID_CMD_ARGS_MSG_TEXT       "%s expects %u args, %u provided, command ignored"
#define CMS_LOG_INVALID_CMD_ARGS_MSG            CMS_LOG_INVALID_CMD_ARGS_MSG_ID" "CMS_LOG_INVALID_CMD_ARGS_MSG_TEXT

#ifndef CMS_LOG_BAD_LOG_COMP_MSG_ID
#define CMS_LOG_BAD_LOG_COMP_MSG_ID             CMS_MSG_PRFX"0225W"
#endif
#define CMS_LOG_BAD_LOG_COMP_MSG_TEXT           "Log component \'%s\' not recognized, command ignored"
#define CMS_LOG_BAD_LOG_COMP_MSG                CMS_LOG_BAD_LOG_COMP_MSG_ID" "CMS_LOG_BAD_LOG_COMP_MSG_TEXT

#ifndef CMS_LOG_BAD_LOG_LEVEL_MSG_ID
#define CMS_LOG_BAD_LOG_LEVEL_MSG_ID            CMS_MSG_PRFX"0226W"
#endif
#define CMS_LOG_BAD_LOG_LEVEL_MSG_TEXT          "Log level \'%s\' not recognized, command ignored"
#define CMS_LOG_BAD_LOG_LEVEL_MSG               CMS_LOG_BAD_LOG_LEVEL_MSG_ID" "CMS_LOG_BAD_LOG_LEVEL_MSG_TEXT

#ifndef CMS_LOG_BAD_CMD_MSG_ID
#define CMS_LOG_BAD_CMD_MSG_ID                  CMS_MSG_PRFX"0227W"
#endif
#define CMS_LOG_BAD_CMD_MSG_TEXT                "Modify %s command not recognized"
#define CMS_LOG_BAD_CMD_MSG                     CMS_LOG_BAD_CMD_MSG_ID" "CMS_LOG_BAD_CMD_MSG_TEXT

#ifndef CMS_LOG_EMPTY_CMD_MSG_ID
#define CMS_LOG_EMPTY_CMD_MSG_ID                CMS_MSG_PRFX"0228W"
#endif
#define CMS_LOG_EMPTY_CMD_MSG_TEXT              "Empty modify command received, command ignored"
#define CMS_LOG_EMPTY_CMD_MSG                   CMS_LOG_EMPTY_CMD_MSG_ID" "CMS_LOG_EMPTY_CMD_MSG_TEXT

#ifndef CMS_LOG_NOT_READY_FOR_CMD_MSG_ID
#define CMS_LOG_NOT_READY_FOR_CMD_MSG_ID        CMS_MSG_PRFX"0229W"
#endif
#define CMS_LOG_NOT_READY_FOR_CMD_MSG_TEXT      "Server not ready for command %s"
#define CMS_LOG_NOT_READY_FOR_CMD_MSG           CMS_LOG_NOT_READY_FOR_CMD_MSG_ID" "CMS_LOG_NOT_READY_FOR_CMD_MSG_TEXT

#ifndef CMS_LOG_BAD_DISPLAY_OPTION_MSG_ID
#define CMS_LOG_BAD_DISPLAY_OPTION_MSG_ID       CMS_MSG_PRFX"0230W"
#endif
#define CMS_LOG_BAD_DISPLAY_OPTION_MSG_TEXT     "Display option \'%s\' not recognized, command ignored"
#define CMS_LOG_BAD_DISPLAY_OPTION_MSG          CMS_LOG_BAD_DISPLAY_OPTION_MSG_ID" "CMS_LOG_BAD_DISPLAY_OPTION_MSG_TEXT

#ifndef CMS_LOG_RESMGR_NOT_LOCKED_MSG_ID
#define CMS_LOG_RESMGR_NOT_LOCKED_MSG_ID        CMS_MSG_PRFX"0231E"
#endif
#define CMS_LOG_RESMGR_NOT_LOCKED_MSG_TEXT      "RESMGR version %d not locked, ISGENQ RC = %d, RSN = 0x%08X"
#define CMS_LOG_RESMGR_NOT_LOCKED_MSG           CMS_LOG_RESMGR_NOT_LOCKED_MSG_ID" "CMS_LOG_RESMGR_NOT_LOCKED_MSG_TEXT

#ifndef CMS_LOG_RESMGR_NOT_RELEASED_MSG_ID
#define CMS_LOG_RESMGR_NOT_RELEASED_MSG_ID      CMS_MSG_PRFX"0232E"
#endif
#define CMS_LOG_RESMGR_NOT_RELEASED_MSG_TEXT    "RESMGR version %d not released, ISGENQ RC = %d, RSN = 0x%08X"
#define CMS_LOG_RESMGR_NOT_RELEASED_MSG         CMS_LOG_RESMGR_NOT_RELEASED_MSG_ID" "CMS_LOG_RESMGR_NOT_RELEASED_MSG_TEXT

#ifndef CMS_LOG_RESMGR_ECSA_FAILURE_MSG_ID
#define CMS_LOG_RESMGR_ECSA_FAILURE_MSG_ID      CMS_MSG_PRFX"0233E"
#endif
#define CMS_LOG_RESMGR_ECSA_FAILURE_MSG_TEXT    "RESMGR ECSA storage not allocated, size = %u"
#define CMS_LOG_RESMGR_ECSA_FAILURE_MSG         CMS_LOG_RESMGR_ECSA_FAILURE_MSG_ID" "CMS_LOG_RESMGR_ECSA_FAILURE_MSG_TEXT

#ifndef CMS_LOG_RESMGR_NT_NOT_CREATED_MSG_ID
#define CMS_LOG_RESMGR_NT_NOT_CREATED_MSG_ID    CMS_MSG_PRFX"0234E"
#endif
#define CMS_LOG_RESMGR_NT_NOT_CREATED_MSG_TEXT  "RESMGR NAME/TOKEN not created,  RC = %d"
#define CMS_LOG_RESMGR_NT_NOT_CREATED_MSG       CMS_LOG_RESMGR_NT_NOT_CREATED_MSG_ID" "CMS_LOG_RESMGR_NT_NOT_CREATED_MSG_TEXT

#ifndef CMS_LOG_RESMGR_NT_NOT_RETR_MSG_ID
#define CMS_LOG_RESMGR_NT_NOT_RETR_MSG_ID       CMS_MSG_PRFX"0235E"
#endif
#define CMS_LOG_RESMGR_NT_NOT_RETR_MSG_TEXT     "RESMGR NAME/TOKEN not retrieved, RC = %d"
#define CMS_LOG_RESMGR_NT_NOT_RETR_MSG          CMS_LOG_RESMGR_NT_NOT_RETR_MSG_ID" "CMS_LOG_RESMGR_NT_NOT_RETR_MSG_TEXT

#ifndef CMS_LOG_RESMGR_NOT_ADDED_MSG_ID
#define CMS_LOG_RESMGR_NOT_ADDED_MSG_ID         CMS_MSG_PRFX"0236E"
#endif
#define CMS_LOG_RESMGR_NOT_ADDED_MSG_TEXT       "RESMGR not added for ASID = %04X, RC = %d, manager RC = %d"
#define CMS_LOG_RESMGR_NOT_ADDED_MSG            CMS_LOG_RESMGR_NOT_ADDED_MSG_ID" "CMS_LOG_RESMGR_NOT_ADDED_MSG_TEXT

#ifndef CMS_LOG_RESMGR_NOT_REMOVED_MSG_ID
#define CMS_LOG_RESMGR_NOT_REMOVED_MSG_ID       CMS_MSG_PRFX"0237E"
#endif
#define CMS_LOG_RESMGR_NOT_REMOVED_MSG_TEXT     "RESMGR not removed for ASID = %04X, RC = %d, manager RC = %d"
#define CMS_LOG_RESMGR_NOT_REMOVED_MSG          CMS_LOG_RESMGR_NOT_REMOVED_MSG_ID" "CMS_LOG_RESMGR_NOT_REMOVED_MSG_TEXT

#ifndef CMS_LOG_RNAME_FAILURE_MSG_ID
#define CMS_LOG_RNAME_FAILURE_MSG_ID            CMS_MSG_PRFX"0238E"
#endif
#define CMS_LOG_RNAME_FAILURE_MSG_TEXT          "%s RNAME not created, %s"
#define CMS_LOG_RNAME_FAILURE_MSG               CMS_LOG_RNAME_FAILURE_MSG_ID" "CMS_LOG_RNAME_FAILURE_MSG_TEXT

#ifndef CMS_LOG_NT_NAME_FAILURE_MSG_ID
#define CMS_LOG_NT_NAME_FAILURE_MSG_ID          CMS_MSG_PRFX"0239E"
#endif
#define CMS_LOG_NT_NAME_FAILURE_MSG_TEXT        "%s NAME (NT) not created, %s"
#define CMS_LOG_NT_NAME_FAILURE_MSG             CMS_LOG_NT_NAME_FAILURE_MSG_ID" "CMS_LOG_NT_NAME_FAILURE_MSG_TEXT

#ifndef CMS_LOG_BUILD_TIME_MISMATCH_MSG_ID
#define CMS_LOG_BUILD_TIME_MISMATCH_MSG_ID      CMS_MSG_PRFX"0240W"
#endif
#define CMS_LOG_BUILD_TIME_MISMATCH_MSG_TEXT    "Discarding outdated LPA module at %p (%26.26s - %26.26s)"
#define CMS_LOG_BUILD_TIME_MISMATCH_MSG          CMS_LOG_BUILD_TIME_MISMATCH_MSG_ID" "CMS_LOG_BUILD_TIME_MISMATCH_MSG_TEXT

#ifndef CMS_LOG_BAD_SERVICE_ADDR_ID_MSG_ID
#define CMS_LOG_BAD_SERVICE_ADDR_ID_MSG_ID      CMS_MSG_PRFX"0241E"
#endif
#define CMS_LOG_BAD_SERVICE_ADDR_ID_MSG_TEXT    "Service with ID %d not relocated, 0x%p not in range [0x%p, 0x%p]"
#define CMS_LOG_BAD_SERVICE_ADDR_ID_MSG         CMS_LOG_BAD_SERVICE_ADDR_ID_MSG_ID" "CMS_LOG_BAD_SERVICE_ADDR_ID_MSG_TEXT

#ifndef CMS_LOG_CMD_REJECTED_MSG_ID
#define CMS_LOG_CMD_REJECTED_MSG_ID             CMS_MSG_PRFX"0242W"
#endif
#define CMS_LOG_CMD_REJECTED_MSG_TEXT           "Modify %s command rejected"
#define CMS_LOG_CMD_REJECTED_MSG                CMS_LOG_CMD_REJECTED_MSG_ID" "CMS_LOG_CMD_REJECTED_MSG_TEXT

#ifndef CMS_LOG_CMD_REJECTED_BUSY_MSG_ID
#define CMS_LOG_CMD_REJECTED_BUSY_MSG_ID        CMS_MSG_PRFX"0243W"
#endif
#define CMS_LOG_CMD_REJECTED_BUSY_MSG_TEXT      "server busy, modify commands are rejected"
#define CMS_LOG_CMD_REJECTED_BUSY_MSG           CMS_LOG_CMD_REJECTED_BUSY_MSG_ID" "CMS_LOG_CMD_REJECTED_BUSY_MSG_TEXT

#ifndef CMS_LOG_RES_NOT_CREATED_MSG_ID
#define CMS_LOG_RES_NOT_CREATED_MSG_ID          CMS_MSG_PRFX"0244E"
#endif
#define CMS_LOG_RES_NOT_CREATED_MSG_TEXT        "Resource '%s' not created, RC = %d"
#define CMS_LOG_RES_NOT_CREATED_MSG             CMS_LOG_RES_NOT_CREATED_MSG_ID" "CMS_LOG_RES_NOT_CREATED_MSG_TEXT

#ifndef CMS_LOG_STEP_ABEND_MSG_ID
#define CMS_LOG_STEP_ABEND_MSG_ID               CMS_MSG_PRFX"0245E"
#endif
#define CMS_LOG_STEP_ABEND_MSG_TEXT             "ABEND S%03X-%02X averted in step '%s' (recovery RC = %d)"
#define CMS_LOG_STEP_ABEND_MSG                  CMS_LOG_STEP_ABEND_MSG_ID" "CMS_LOG_STEP_ABEND_MSG_TEXT

#ifndef CMS_LOG_SRVC_ENTRY_OCCUPIED_MSG_ID
#define CMS_LOG_SRVC_ENTRY_OCCUPIED_MSG_ID      CMS_MSG_PRFX"0246E"
#endif
#define CMS_LOG_SRVC_ENTRY_OCCUPIED_MSG_TEXT    "Service entry %d is occupied"
#define CMS_LOG_SRVC_ENTRY_OCCUPIED_MSG         CMS_LOG_SRVC_ENTRY_OCCUPIED_MSG_ID" "CMS_LOG_SRVC_ENTRY_OCCUPIED_MSG_TEXT

#ifndef CMS_LOG_DEV_MODE_ON_MSG_ID
#define CMS_LOG_DEV_MODE_ON_MSG_ID              CMS_MSG_PRFX"0247W"
#endif
#define CMS_LOG_DEV_MODE_ON_MSG_TEXT            "Development mode is enabled"
#define CMS_LOG_DEV_MODE_ON_MSG                 CMS_LOG_DEV_MODE_ON_MSG_ID" "CMS_LOG_DEV_MODE_ON_MSG_TEXT

#ifndef CMS_LOG_REUSASID_NO_MSG_ID
#define CMS_LOG_REUSASID_NO_MSG_ID              CMS_MSG_PRFX"0248W"
#endif
#define CMS_LOG_REUSASID_NO_MSG_TEXT            "Address space is not reusable, start with REUSASID=YES to prevent an ASID shortage"
#define CMS_LOG_REUSASID_NO_MSG                 CMS_LOG_REUSASID_NO_MSG_ID" "CMS_LOG_REUSASID_NO_MSG_TEXT

#ifndef CMS_LOG_NON_PRIVATE_MODULE_MSG_ID
#define CMS_LOG_NON_PRIVATE_MODULE_MSG_ID       CMS_MSG_PRFX"0249E"
#endif
#define CMS_LOG_NON_PRIVATE_MODULE_MSG_TEXT     "Module %8.8s is loaded from common storage, ensure %8.8s is valid in the STEPLIB"
#define CMS_LOG_NON_PRIVATE_MODULE_MSG          CMS_LOG_NON_PRIVATE_MODULE_MSG_ID" "CMS_LOG_NON_PRIVATE_MODULE_MSG_TEXT

#ifndef CMS_LOG_DUB_ERROR_MSG_ID
#define CMS_LOG_DUB_ERROR_MSG_ID                CMS_MSG_PRFX"0250E"
#endif
#define CMS_LOG_DUB_ERROR_MSG_TEXT              "Bad dub status %d (%d,0x%04X), verify that the started task user has an OMVS segment"
#define CMS_LOG_DUB_ERROR_MSG                   CMS_LOG_DUB_ERROR_MSG_ID" "CMS_LOG_DUB_ERROR_MSG_TEXT

#ifndef CMS_LOG_LOOKUP_ANC_CREATED_MSG_ID
#define CMS_LOG_LOOKUP_ANC_CREATED_MSG_ID       CMS_MSG_PRFX"0251I"
#endif
#define CMS_LOG_LOOKUP_ANC_CREATED_MSG_TEXT     "Look-up routine anchor has been created at %p"
#define CMS_LOG_LOOKUP_ANC_CREATED_MSG          CMS_LOG_LOOKUP_ANC_CREATED_MSG_ID" "CMS_LOG_LOOKUP_ANC_CREATED_MSG_TEXT

#ifndef CMS_LOG_LOOKUP_ANC_REUSED_MSG_ID
#define CMS_LOG_LOOKUP_ANC_REUSED_MSG_ID        CMS_MSG_PRFX"0252I"
#endif
#define CMS_LOG_LOOKUP_ANC_REUSED_MSG_TEXT      "Look-up routine anchor at % p has been reused"
#define CMS_LOG_LOOKUP_ANC_REUSED_MSG           CMS_LOG_LOOKUP_ANC_REUSED_MSG_ID" "CMS_LOG_LOOKUP_ANC_REUSED_MSG_TEXT

#ifndef CMS_LOG_LOOKUP_ANC_DELETED_MSG_ID
#define CMS_LOG_LOOKUP_ANC_DELETED_MSG_ID       CMS_MSG_PRFX"0253I"
#endif
#define CMS_LOG_LOOKUP_ANC_DELETED_MSG_TEXT     "Look-up routine anchor at % p has been deleted"
#define CMS_LOG_LOOKUP_ANC_DELETED_MSG          CMS_LOG_LOOKUP_ANC_DELETED_MSG_ID" "CMS_LOG_LOOKUP_ANC_DELETED_MSG_TEXT

#ifndef CMS_LOG_LOOKUP_ANC_DISCARDED_MSG_ID
#define CMS_LOG_LOOKUP_ANC_DISCARDED_MSG_ID     CMS_MSG_PRFX"0254W"
#endif
#define CMS_LOG_LOOKUP_ANC_DISCARDED_MSG_TEXT   "Look-up routine anchor at %p has been discarded due to %s:"
#define CMS_LOG_LOOKUP_ANC_DISCARDED_MSG        CMS_LOG_LOOKUP_ANC_DISCARDED_MSG_ID" "CMS_LOG_LOOKUP_ANC_DISCARDED_MSG_TEXT

#ifndef CMS_LOG_LOOKUP_ANC_ALLOC_ERROR_MSG_ID
#define CMS_LOG_LOOKUP_ANC_ALLOC_ERROR_MSG_ID   CMS_MSG_PRFX"0255E"
#endif
#define CMS_LOG_LOOKUP_ANC_ALLOC_ERROR_MSG_TEXT "Look-up routine anchor has not been created"
#define CMS_LOG_LOOKUP_ANC_ALLOC_ERROR_MSG      CMS_LOG_LOOKUP_ANC_ALLOC_ERROR_MSG_ID" "CMS_LOG_LOOKUP_ANC_ALLOC_ERROR_MSG_TEXT

#endif /* H_CROSSMEMORY_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

