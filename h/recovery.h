

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __RECOVERY__
#define __RECOVERY__

#if defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)
#include "setjmp.h"
#include "signal.h"
#include "alloc.h"
#endif

#include "zowetypes.h"

#if !defined(__ZOWE_OS_ZOS) && !defined(__ZOWE_OS_AIX) && !defined(__ZOWE_OS_LINUX)
#error "unsupported platform"
#endif

#ifdef __ZOWE_OS_ZOS

#include "cellpool.h"

ZOWE_PRAGMA_PACK

typedef struct SDWAARC4_tag{
  int64       gprs[16];       /* 64 bit GPR's */
  char        reserved80[8];
  int         sdwag64h[16];   /* high havles */
  int64       crs[16];
  int64       sdwatrne;
  int64       sdwabea;
} SDWAARC4;

typedef struct SDWAPTRS_tag{
  int                sdwadsrp;    /* addr dump storage ranges - SDWANRC1 */
  int                sdwasrvp;    /* additional comp serv data - SDWARC1 */
  int                sdwaxiom;    /* IO Machine Check Area     - SDWARC2 */
  int                sdwaxspl;    /* Storage Subpools Area     - SDWANRC2 */
  int                sdwaxlck;    /* Additional FreLock Data   - SDWARC3  */
  int                sdwadspp;    /* dataspace storage ranges  - SDWANCR3 */ 
  SDWAARC4 * __ptr32 sdwaxeme;    /* Addr 64-bit info          - SDWARC4  */
  int                reserved;
} SDWAPTRS;

typedef struct IHASDWA_tag{
  /* Here - pre built Queue */
  int     parms;      /* the 24-byte area for FRR's */
          /* sometimes address of purge IO request list */
  int     flagsAndCode;  /* flags and 3byte code */
  char           sdwactl1;  /* flags */
  char           sdwamwpa;  /* key and flags */
  unsigned short sdwainta;   /* interrupt code */
  int            ilcAndInstruction24; /* ancient crap */
  /* offset 0x10 */

  char           sdwacmkp;    /* channel interrupt masks */
  char           sdwamwpp;    /* PSW key and machine check-wait-sup */
  unsigned short sdwaintp;    /* interrupt code, or last two bytes of */
  int            ilcAndNextInstruction24;
  /* offset 0x18 */
  unsigned int   lowGPRs[16]; /* GPR's at time of error */
  /* offset 0x58 */
  int            sdwarbad;    /* rb address, of sup mode under RB */
  int            sdwa05C;     /* see book */
  /* offset 0x60 */
  int            sdwaepa;     /* entry point of abending program
                                 if not under an RB */
  int            sdwaiobr;    /* see book */
  long long      sdwaec1;     /* extended control PSW at abend */
  /* offset 0x70 */
  char           sdwa070;     /* reserved */
  char           sdwail1;     /* ILC */
  char           sdwainc1;    /* reserved for imprecise interrupts */
  char           sdwaicd1;    /* 8-bit interrupt check for program check */
  int            sdwatran;    /* virtual address causing xlate exception 
                                 can contain low byte of data exception
                                 on interrupt code 7 */
  long long      sdwaec2;     /* extended control PSW 
                                 For ESTAE rb level at create time
                                     ESTAI 0
                                     FRR   psw which gave FRR control */
  /* offset 0x80 */
  char           sdwa080;     /* reserved */
  char           sdwail2;     /* ILC */
  char           sdwainc2;    /* reserved for imprecise interrupts */
  char           sdwaicd2;    /* 8-bit interrupt check for program check */
  int            sdwatrn2;    /* virtual address causing xlate exception 
                                 can contain low byte of data exception
                                 on interrupt code 7 */
  int            retryRegisters[16];   /* see book, ugh */
  /* offset 0xC8 */
  int            subpoolAndLength;  /* low 3 bytes is length */
  char           sdwamch[28];           /* machine check data */
  /* offset 0xE8 */
  char           sdwaerra;       /* error type flags */
  char           sdwaerrb;       /* more error flags */
  char           sdwaerrc;     
  char           sdwaerrd;   
#define SDWAERRD_SDWACLUP 0x80
#define SDWAERRD_SDWANRBE 0x40
#define SDWAERRD_SDWASTAE 0x20
#define SDWAERRD_SDWACTS  0x10
#define SDWAERRD_SDWAMABD 0x08
#define SDWAERRD_SDWARPIV 0x04
#define SDWAERRD_SDWAMCIV 0x02
#define SDWAERRD_SDWAERFL 0x01
  unsigned short sdwafmid;         /* asid of memory for error */
  char           sdwaiofs;         /* current IO Status */
  char           sdwacpui;         /* CPU ID */
  /* Offset 0xF0 */
  int            sdwartya;         /* retry address */
  int            sdwareca;         /* address of var recording area in SDWA */
  unsigned short sdwaresF8;
  unsigned short sdwalcpu;         /* logcl addr of CPU holding resource */
  char           sdwarcde;         /* return code from recovery
                                      indicates retry or terminate */
  char           sdwaacf2;         /* additional processing request flags */
  char           sdwaacf3;         /* global locks to be freed */
  char           sdwaacf4;         /* more locks */
  /* OFFSET 0x100 */
  int            sdwalrsd;     /* lockword for RSM data space lock */
  int            sdwaiulw;     /* lockword for IOSUCH */
  int            sdwa108;      /* reserved */
  int            sdwaiplw;     /* lockword for iosynch lock */
  /* OFFSET 0x110 */
  int            sdwaaplw;     /* lockword for z40WPXH */
  int            sdwa114;      /* reserved */
  int            sdwa118;      /* reserved */
  int            sdwatalw;     /* lockword for z40WPXH */
  /* OFFSET 0x120 */
  unsigned short sdwaasid;     /* ASID for logrec debugging (HOME asid) */
  unsigned short sdwaseq;      /* seq number of error */ 
  char           sdwamodn[8];  /* load module name, given by rec routine */
  char           sdwacsct[8];  /* csect name,       ditto */
  char           sdwarexn[8];  /* recovery routine name, ditto */
  int            sdwadpla;     /* pointer to dump parameter list in SDWA */
  /* OFFSET 0x140 */
  char           sdwadpid;      /* dump ID */
  char           sdwadpfs;      /* dump flags */
  char           sdwadpf2;      /* dump flags 2 */
  char           sdwa143;       /* reserved */
  char           sdwssda0;       /* dump options */
  char           sdwasda1;
  char           sdwapdat;       /* P data options*/
  char           sdwa147;        /* reserved */
  int            sdwafrm1;       /* storage range 1 begin */
  int            sdwato1;        /* storage range 1 end */
  /* OFFSET 0x150 */
  int            sdwafrm2;       /* storage range 2 begin */
  int            sdwato2;        /* storage range 2 end */
  int            sdwafrm3;       /* storage range 3 begin */
  int            sdwato3;        /* storage range 3 end */
  /* OFFSET 0x160 */
  int            sdwafrm4;       /* storage range 4 begin */
  int            sdawto4;        /* storage range 4 end */
  int            sdwa168;        /* reserved */
  unsigned short sdwaverf;       /* FFFF means next valid */
  unsigned short sdwavid;        /* version indicator */
  /* OFFSET 0x170 */
  int            sdwaxpad;       /* addr of the extension pointers */
  int            sdwacr3;        /* control register 3 */
  int            sdwacr4;        /* control register 4 */
  int            sdwacmla;       /* address of CMLtobe freed */
  /* OFFSET 0x180 */
  char           sdwacomu[8];    /* frr to ESTAE Commctn buffer */
  int            sdwacomp;       /* component use word*/
  int            sdwaertm;       /* ERROR ID timestamp, not STCK */
  /* OFFSET 0x190 */
  unsigned short sdwavral;       /* var record area length */
  char           sdwadpva;       /* VRA flags */
  char           sdwaural;       /* user info length in VRA */
  char           sdwavra[255];
  char           sdwaid[5];      /* "SDWA " */
  /* OFFSET 0x298 - for basic area w/extension */
} SDWA;

ZOWE_PRAGMA_PACK_RESET

#endif /* __ZOWE_OS_ZOS */

#define RC_RCV_OK                 0
#define RC_RCV_ERROR              8
#define RC_RCV_SET_ESTAEX_FAILED  9
#define RC_RCV_DEL_ESTAEX_FAILED  10
#define RC_RCV_CONTEXT_NOT_FOUND  11
#define RC_RCV_CONTEXT_NOT_SET    12
#define RC_RCV_SIGACTION_FAILED   13
#define RC_RCV_LNKSTACK_ERROR     14
#define RC_RCV_ALLOC_FAILED       15
#define RC_RCV_LOCKED_ENV         16
#define RC_RCV_ABENDED            100

#ifdef __ZOWE_OS_ZOS

struct RecoveryContext_tag;

typedef void (AnalysisFunction)(struct RecoveryContext_tag * __ptr32 context, SDWA * __ptr32 sdwa, void * __ptr32 userData);
typedef void (CleanupFunction)(struct RecoveryContext_tag * __ptr32 context, SDWA * __ptr32 sdwa, void * __ptr32 userData);

ZOWE_PRAGMA_PACK

typedef struct RecoveryServiceInfo_tag {
  char loadModuleName[8];
  char csect[8];
  char recoveryRoutineName[8];
  char componentID[5];
  char subcomponentName[23];
  char buildDate[8];
  char version[8];
  char componentIDBaseNumber[4];
  short stateNameLength;
  char stateName[255];
  char padding0[7];
} RecoveryServiceInfo;

typedef struct RecoveryStateEntry_tag {

#define RCVR_STATE_VERSION        2

  char eyecatcher[8]; /* RSRSENTR */
  struct RecoveryStateEntry_tag * __ptr32 next;
  void * __ptr32 retryAddress;
  void * __ptr32 analysisFunction;
  void * __ptr32 analysisFunctionUserData;
  void * __ptr32 cleanupFunctionEntryPoint;
  void * __ptr32 cleanupFunctionUserData;
  uint64         analysisFunctionEnvironment;
  uint64         cleanupFunctionEnvironment;
  volatile int flags;
#define RCVR_FLAG_NONE                  0x00000000
#define RCVR_FLAG_PRODUCE_DUMP          0x01000000
#define RCVR_FLAG_RETRY                 0x02000000
#define RCVR_FLAG_DELETE_ON_RETRY       0x04000000
#define RCVR_FLAG_SDWA_TO_LOGREC        0x08000000
#define RCVR_FLAG_DISABLE               0x10000000
#define RCVR_FLAG_CPOOL_BASED           0x20000000
  volatile char state;
#define RECOVERY_STATE_DISABLED         0x00
#define RECOVERY_STATE_ENABLED          0x01
#define RECOVERY_STATE_ABENDED          0x02
#define RECOVERY_STATE_INFO_RECORDED    0x04
  int16_t linkageStackToken;
  int8_t key;
  uint8_t structVersion;
  char reserved[3];
  int sdumpxRC;
  long long retryGPRs[16];
  long long callerGRPs[16];   /* used for Metal 31/64 and LE 31 */
  char stackFrame[256];       /* used for all linkage conventions */
  char userFunctionParms[32]; /* used for passing parameters to non-XPLINK callees */

  RecoveryServiceInfo serviceInfo;

  struct {
    char length;
    char title[100];
  } dumpTitle;
  char padding[3]; /* becaue of the weird 101 byte thing above to get to 8 byte alignment */
  long long analysisStackPtr;
} RecoveryStateEntry;

typedef struct RecoveryContext_tag {

#define RCVR_CONTEXT_VERSION      2

  char eyecatcher[8]; /* RSRCVCTX */
  int flags;
#define RCVR_ROUTER_FLAG_NONE                 0x00000000
#define RCVR_ROUTER_FLAG_NON_INTERRUPTIBLE    0x01000000
#define RCVR_ROUTER_FLAG_PC_CAPABLE           0x02000000
#define RCVR_ROUTER_FLAG_RUN_ON_TERM          0x04000000
#define RCVR_ROUTER_FLAG_USER_CONTEXT         0x08000000
#define RCVR_ROUTER_FLAG_USER_STATE_POOL      0x10000000
#define RCVR_ROUTER_FLAG_SRB                  0x20000000
#define RCVR_ROUTER_FLAG_LOCKED               0x40000000
#define RCVR_ROUTER_FLAG_FRR                  0x80000000
  int previousESPIEToken;
  unsigned char routerPSWKey;
  uint8_t structVersion;
  char reserved1[2];
  CPID stateCellPool;
  RecoveryStateEntry * __ptr32 recoveryStateChain;
  void * __ptr32 caa;
  RecoveryServiceInfo serviceInfo;
  char sdumpxParmList[200]; /* 184 is used by PLISTVER=3 */
  char reserved2[56];
  char sdumpxSaveArea[72];
} RecoveryContext;

typedef struct RecoveryStatePool_tag RecoveryStatePool;

ZOWE_PRAGMA_PACK_RESET

#elif defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)

struct RecoveryContext_tag;

typedef void (AnalysisFunction)(struct RecoveryContext_tag *context, int signal, siginfo_t *info, void *userData);
typedef void (CleanupFunction)(struct RecoveryContext_tag *context, int signal, siginfo_t *info, void *userData);

typedef struct RecoveryStateEntry_tag {
  char eyecatcher[8]; /* RSRSENTR */
  struct RecoveryStateEntry_tag *next;
  jmp_buf environment;
  AnalysisFunction *analysisFunction;
  void *analysisFunctionUserData;
  CleanupFunction *cleanupFunction;
  void *cleanupFunctionUserData;
  volatile int flags;
#define RCVR_FLAG_NONE                  0x00000000
#define RCVR_FLAG_PRODUCE_DUMP          0x01000000
#define RCVR_FLAG_RETRY                 0x02000000
#define RCVR_FLAG_DELETE_ON_RETRY       0x04000000
#define RCVR_FLAG_SDWA_TO_LOGREC        0x08000000
#define RCVR_FLAG_DISABLE               0x10000000
  volatile char state;
#define RECOVERY_STATE_DISABLED         0x00
#define RECOVERY_STATE_ENABLED          0x01
#define RECOVERY_STATE_ABENDED          0x02
  char dumpTitle[256];
} RecoveryStateEntry;

typedef struct RecoveryContext_tag {
  char eyecatcher[8]; /* RSRCVCTX */
  int flags;
#define RCVR_ROUTER_FLAG_NONE                 0x00000000
  RecoveryStateEntry *recoveryStateChain;
} RecoveryContext;

#endif /* __ZOWE_OS_ZOS */

#ifndef __LONGNAME__
#define recoveryEstablishRouter RCVRESRR
#define recoveryRemoveRouter RCVRRSRR
#define recoveryIsRouterEstablished RCVRTRES
#define recoveryPush RCVRPSFR
#define recoveryPop RCVRPPFR
#define recoverySetDumpTitle RCVRSDTL
#define recoverySetFlagValue RCVRSFLV
#define recoveryEnableCurrentState RCVRECST
#define recoveryDisableCurrentState RCVRDCST
#define recoveryUpdateRouterServiceInfo RCVRURSI
#define recoveryUpdateStateServiceInfo RCVRUSSI
#define recoveryGetABENDCode RCVRGACD
#define runFunctioninESTAE RCVRNFNE
#define showRecoveryState SHRCVRST
#endif

#ifdef __ZOWE_OS_ZOS

/*****************************************************************************
* Establish a new recovery router.
*
* The function overrides an existing ESPIE (if any) and sets an ESTAE or FRR.
* The ESTAE/FRR recovery routine handles all present recovery states.
*
* Parameters:
*   flags                     - control flags
*     RCVR_ROUTER_FLAG_NONE               - no flags
*     RCVR_ROUTER_FLAG_NON_INTERRUPTIBLE  - cancel or detach will not
*                                           interrupted recovery process
*     RCVR_ROUTER_FLAG_PC_CAPABALE        - should be set when the recovery
*                                           needs to used in PC calls
*
* Return value:
*   When a router has successfully been established, the function returns
*   RC_RCV_OK. In case of an error, the function returns one
*   of the RC_RCV_xxxx error codes.
*****************************************************************************/
int recoveryEstablishRouter(int flags);

#ifdef RCVR_CPOOL_STATES

/*****************************************************************************
* Establish a new recovery router with the ability to pass user storage for
* the context and state pool.
*
* The function overrides an existing ESPIE (if any) and sets an ESTAE or FRR.
* The ESTAE/FRR recovery routine handles all present recovery states.
*
* Parameters:
*   userContext               - user storage for the recovery context
*   userStatePool             - user provided recovery state pool
*   flags                     - control flags
*     RCVR_ROUTER_FLAG_NONE               - no flags
*     RCVR_ROUTER_FLAG_NON_INTERRUPTIBLE  - cancel or detach will not
*                                           interrupted recovery process
*     RCVR_ROUTER_FLAG_PC_CAPABALE        - should be set when the recovery
*                                           needs to used in PC calls
*
* Return value:
*   When a router has successfully been established, the function returns
*   RC_RCV_OK. In case of an error, the function returns one
*   of the RC_RCV_xxxx error codes.
*****************************************************************************/
int recoveryEstablishRouter2(RecoveryContext *userContext,
                             RecoveryStatePool *userStatePool,
                             int flags);

/*****************************************************************************
* Make a user state pool.
*
* The function creates a pool for recovery states.
*
* Parameters:
*   stateCount                - the number of cells to be allocated
*
* Return value:
*   State pool structure on success or NULL on failure.
*****************************************************************************/
RecoveryStatePool *recoveryMakeStatePool(unsigned int stateCount);

/*****************************************************************************
* Remove a user state pool.
*
* The function removes a state pool.
*
* Parameters:
*   statePool                 - pool to be removed
*
* Return value:
*   N/A
*****************************************************************************/
void recoveryRemoveStatePool(RecoveryStatePool *statePool);

#endif /* RCVR_CPOOL_STATES */

/*****************************************************************************
* Remove the recovery router.
*
* The function removes the previously established router and restores the ESPIE.
* The function does nothing, if there is no router found.
*
* Return value:
*   Always RC_RCV_OK
*****************************************************************************/
int recoveryRemoveRouter();

/*****************************************************************************
* Check if there is a router.
*
* Parameters:
*   N/A
*
* Return value:
*   Return true if a router has been established.
*****************************************************************************/
bool recoveryIsRouterEstablished();

/*****************************************************************************
* Push a new recovery state.
*
* The function creates a new recovery state and pushes it onto the state stack.
* When an ABEND occurs the states are processed in LIFO order by the router.
* Depending on the control flags, a state might be used as a retry point.
* You can pass NULL as a value if the corresponding parameter is a pointer.
*
* Parameters:
*   name                      - state name used in SDWAVRA
*   flags                     - control flags
*     RCVR_FLAG_NONE             - no action
*     RCVR_FLAG_PRODUCE_DUMP     - an SVC dump will be taken
*     RCVR_FLAG_RETRY            - router will retry to this state
*     RCVR_FLAG_DELETE_ON_RETRY  - state is removed on retry
*     RCVR_FLAG_SDWA_TO_LOGREC   - SDWA is written to LOGREC
*     RCVR_FLAG_DISABLE          - state is disabled
*   dumpTitle                 - SVC dump title
*   userAnalysisFunction      - function called in the very beginning of
*                               recovery
*   analysisFunctionUserData  - custom data for the analysis function
*   userCleanupFunction       - function called in the end of recovery, after
*                               the SVC dump has been taken
*   cleanupFunctionUserData   - custom data for the cleanup function
*
* Return value:
*   RC_RCV_OK is returned when a new state has been pushed.
*   RC_RCV_CONTEXT_NOT_FOUND is returned when no recovery context found,
*     in this case the state is not pushed.
*   RC_RCV_ABENDED is returned when a successful retry attempt has been made.
*****************************************************************************/
int recoveryPush(char *name, int flags, char *dumpTitle,
                 AnalysisFunction *userAnalysisFunction, void * __ptr32 analysisFunctionUserData,
                 CleanupFunction *userCleanupFunction, void * __ptr32 cleanupFunctionUserData);

/* recoveryPush2 is a variant that allows the analysisFunction to run on a clean stack and not
   overwrite the stack upon which the error/abend occurred.
   */
int recoveryPush2(char *name, int flags, char *dumpTitle, 
		  AnalysisFunction *userAnalysisFunction, void * __ptr32 analysisFunctionUserData,
		  char *analysisFunctionStack, int analysisFunctionStackSize,
		  CleanupFunction *userCleanupFunction, void * __ptr32 cleanupFunctionUserData);



/*****************************************************************************
* Pop the latest recovery state.
*
* The function removes the latest recovery state.
*
* Parameters:
*   N/A
*
* Return value:
*   N/A
*****************************************************************************/
void recoveryPop();

void showRecoveryState();

/*****************************************************************************
* Set the new dump title
*
* The function adds/updates the dump title. If NULL is passed the existing
* title is removed.
*
* Parameters:
*   title                     - new SVC dump title
*
* Return value:
*   N/A
*****************************************************************************/
void recoverySetDumpTitle(char *title);

/*****************************************************************************
* Set a flag value.
*
* Parameters:
*   flag                      - flag you want to change (see RCVR_FLAG_xxxx)
*   value                     - new value
*
* Return value:
*   N/A
*****************************************************************************/
void recoverySetFlagValue(int flag, bool value);

/*****************************************************************************
* Enable the current (latest) state.
*
* Parameters:
*   N/A
*
* Return value:
*   N/A
*****************************************************************************/
void recoveryEnableCurrentState();

/*****************************************************************************
* Disable the current (latest) state.
*
* Parameters:
*   N/A
*
* Return value:
*   N/A
*****************************************************************************/
void recoveryDisableCurrentState();

/*****************************************************************************
* Update router service information.
*
* All the input fields are used to update SDWA component and maintenance level
* information. A field value is ignored if it is NULL. If a value is too big,
* it gets truncated to the corresponding SDWA maximum field size. If a values
* is smaller that the field size, it gets space padded.
*
* The router info is always present in recovery regardless of the state. It has
* the lowest priority and will be overwritten by state service information if
* the state has it.
*
* Parameters:
*   loadModuleName            - load module name involved in the error
*                               (SDWAMODN, 8 bytes)
*   csect                     - CSECT name involved in the error
*                               (SDWACSCT, 8 bytes)
*   recoveryRoutineName       - recovery routine name handling the error
*                               (SDWAREXN, 8 bytes)
*   componentID               - component ID of the component involved
*                               in the error (SDWACID, 5 bytes)
*   subcomponentName          - name of the subcomponent and the module
*                               subfunction involved in the error
*                               (SDWASC, 23 bytes)
*   buildDate                 - build (assembly) date of the module involved
*                               in the error (SDWAMDAT, 8 bytes)
*   version                   - version of the module (PTF or product number)
*                               (SDWAMVRS, 8 bytes)
*   componentIDBaseNumber     - component ID base (prefix) number
*                               (SDWACIDB, 4 bytes)
*   stateName                 - state name, function name or any other useful
*                               information which will be written into SDWAVRA,
*                               values from several states might be accumulated
*                               (SDWAVRA, 255 bytes)
*
* Return value:
*   N/A
*****************************************************************************/
void recoveryUpdateRouterServiceInfo(char *loadModuleName, char *csect, char *recoveryRoutineName,
                                     char *componentID, char *subcomponentName,
                                     char *buildDate, char *version, char *componentIDBaseNumber,
                                     char *stateName);

/*****************************************************************************
* Update recovery state service information.
*
* All the input fields are used to update SDWA component and maintenance level
* information. A field value is ignored if it is NULL. If a value is too big,
* it gets truncated to the corresponding SDWA maximum field size. If a values
* is smaller that the field size, it gets space padded.
*
* Parameters:
*   same as for rcvrUpdateRouterServiceInfo()
*
* Return value:
*   N/A
*****************************************************************************/
void recoveryUpdateStateServiceInfo(char *loadModuleName, char *csect, char *recoveryRoutineName,
                                    char *componentID, char *subcomponentName,
                                    char *buildDate, char *version, char *componentIDBaseNumber,
                                    char *stateName);

/*****************************************************************************
* Extract ABEND completion and reason code from SDWA.
*
* Parameters:
*   sdwa                      - SDWA
*   completionCode            - ABEND completion code
*   reasonCode                - ABEND reason code
*
* Return value:
*   N/A
*****************************************************************************/
void recoveryGetABENDCode(SDWA *sdwa, int *completionCode, int *reasonCode);

#elif defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_LINUX)

/*****************************************************************************
* Establish a new recovery router.
*
* The function creates a new recovery context and sets the corresponding signal
* handlers.
*
* Parameters:
*   flags                     - control flags
*     RCVR_ROUTER_FLAG_NONE               - no flags
*
* Return value:
*   When a router has successfully been established, the function returns
*   RC_RCV_OK. In case of an error, the function returns one
*   of the RC_RCV_xxxx error codes and errno is set to indicate the error.
*****************************************************************************/
int recoveryEstablishRouter(int flags);

/*****************************************************************************
* Remove the recovery router.
*
* The function restores the previous handlers and removes the recovery context.
*
* Return value:
*   RC_RCV_OK if the router has been successfully removed, otherwise one of the
*   RC_RCV_xxxx error codes is returned and errno is set to indicate the error.
*****************************************************************************/
int recoveryRemoveRouter();

/*****************************************************************************
* Check if there is a router.
*
* Parameters:
*   N/A
*
* Return value:
*   Return true if a router has been established.
*****************************************************************************/
bool recoveryIsRouterEstablished();

/*****************************************************************************
* The following function declarations should not be used directly,
* they are part of the pushReovery macro.
*****************************************************************************/
RecoveryStateEntry *addRecoveryStateEntry(RecoveryContext *context, char *name, int flags, char *dumpTitle,
                                          AnalysisFunction *userAnalysisFunction, void *analysisFunctionUserData,
                                          CleanupFunction *userCleanupFunction, void *cleanupFunctionUserData);
RecoveryContext *getRecoveryContext();

/*****************************************************************************
* Push a new recovery state.
*
* The macro creates a new recovery state and pushes it onto the state stack.
* When an error occurs the states are processed in LIFO order by the router.
* Depending on the control flags, a state might be used as a retry point.
* You can pass NULL as a value if the corresponding parameter is a pointer.
*
* Parameters:
*   name                      - state name
*   flagValue                 - control flags
*     RCVR_FLAG_NONE             - no action
*     RCVR_FLAG_PRODUCE_DUMP     - a core dump will be taken
*     RCVR_FLAG_RETRY            - router will retry to this state
*     RCVR_FLAG_DELETE_ON_RETRY  - state is removed on retry
*     RCVR_FLAG_DISABLE          - state is disabled
*   dumpTitle                 - core dump title
*   userAnalysisFunction      - function called in the very beginning of
*                               recovery
*   analysisFunctionUserData  - custom data for the analysis function
*   userCleanupFunction       - function called in the end of recovery, after
*                               the core dump has been taken
*   cleanupFunctionUserData   - custom data for the cleanup function
*
* Return value:
*   RC_RCV_OK is returned when a new state has been pushed.
*   RC_RCV_CONTEXT_NOT_FOUND is returned when no recovery context found,
*     in this case the state is not pushed.
*   RC_RCV_ABENDED is returned when a successful retry attempt has been made.
*
* Warning:
*   The macro uses the statement expressions feature, it is currently supported
*   by the XL C/C++, GCC and Clang compilers.
*****************************************************************************/
#define recoveryPush(name, flagValue, dumpTitle, \
                     userAnalysisFunction, analysisFunctionUserData, \
                     userCleanupFunction, cleanupFunctionUserData) \
({\
  volatile int status = RC_RCV_OK; \
  RecoveryContext *context = getRecoveryContext(); \
  if (context != NULL) { \
    RecoveryStateEntry *newEntry = \
        addRecoveryStateEntry(context, name, flagValue, dumpTitle, \
                              userAnalysisFunction, analysisFunctionUserData, \
                              userCleanupFunction, cleanupFunctionUserData); \
    int setjumpRC = sigsetjmp(newEntry->environment, 1); \
    if (setjumpRC != 0) { \
        RecoveryStateEntry *currentState = context->recoveryStateChain; \
        RecoveryStateEntry *nextState = currentState->next; \
        if (currentState->flags & RCVR_FLAG_DELETE_ON_RETRY) { \
          safeFree((char *)currentState, sizeof(RecoveryStateEntry)); \
          currentState = NULL; \
          context->recoveryStateChain = nextState; \
        } \
        status = RC_RCV_ABENDED; \
    } else { \
      status = RC_RCV_OK; \
    } \
  } else { \
    status = RC_RCV_CONTEXT_NOT_FOUND; \
  } \
  status; \
})

/*****************************************************************************
* Pop the latest recovery state.
*
* The function removes the latest recovery state.
*
* Parameters:
*   N/A
*
* Return value:
*   N/A
*****************************************************************************/
void recoveryPop();

/*****************************************************************************
* TODO needs to be research, we might not be able to affect the core dump in any way
*
* Set the new dump title
*
* The function adds/updates the dump title. If NULL is passed the existing
* title is removed.
*
* Parameters:
*   title                     - new core dump title
*
* Return value:
*   N/A
*****************************************************************************/
void recoverySetDumpTitle(char *title);

/*****************************************************************************
* Set a flag value.
*
* Parameters:
*   flag                      - flag you want to change (see RCVR_FLAG_xxxx)
*   value                     - new value
*
* Return value:
*   N/A
*****************************************************************************/
void recoverySetFlagValue(int flag, bool value);

/*****************************************************************************
* Enable the current (latest) state.
*
* Parameters:
*   N/A
*
* Return value:
*   N/A
*****************************************************************************/
void recoveryEnableCurrentState();

/*****************************************************************************
* Disable the current (latest) state.
*
* Parameters:
*   N/A
*
* Return value:
*   N/A
*****************************************************************************/
void recoveryDisableCurrentState();

#endif /* __ZOWE_OS_ZOS */

/*****************************************************************************
* External context setter function.
*
* Parameters:
*   context                   - recovery context to be set
*
* Return value:
*   0 if the context has successfully been set and a non-zero return code if
*   the function failed to save the context.
*****************************************************************************/
extern int rcvrscxt(RecoveryContext *context);

/*****************************************************************************
* External context getter function.
*
* Parameters:
*   N/A
*
* Return value:
*   Recovery context is returned in a case of success. NULL is returned if
*   the function failed.
*****************************************************************************/
extern RecoveryContext *rcvrgcxt();

#endif 


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

