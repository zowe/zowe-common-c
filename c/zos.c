

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/


/* If you want to use this file in 64 bit mode, it is *required* that you compile with roconst. */
/* Please see the build instructions below for recommended compile and link options */

#ifdef METTLE

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/string.h>
#include <metal/stdlib.h>
#include <metal/stdarg.h>
#include <metal/ctype.h>
#include "qsam.h"
#include "metalio.h"

#else
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#endif

#include "copyright.h"
#include "zowetypes.h"
#include <builtins.h>
#include "alloc.h"
#include "utils.h"
#include "zos.h"

#ifdef METTLE
#define fprintf(out,fmt,...) printf(fmt,__VA_ARGS__)
#define printf wtoPrintf
#define fflush(out)
#define dumpbuffer(area,len)
#endif

int testAuth(void)
{
// Perform TESTAUTH using __asm
  int rc;
  __asm(ASM_PREFIX
        " TESTAUTH FCTN=1 \n"
        " ST 15,%0 \n"
        :"=m"(rc)
        :
        :"r0","r1","r14","r15");
  return rc;
}

/* returns whether was in problem state */

#define PROBLEM_STATE 0x00010000

int extractPSW(void) {
  int highWord;
  __asm(ASM_PREFIX
        " EPSW 0,0 \n"
        " ST   0,%0 \n"
        :"=m"(highWord)
        :
        :"r0");
  return highWord;
}

/*
  returns "wasProblemState"
 */
int supervisorMode(int enable){
// Use MODESET macro for requests
  int currentPSW = extractPSW();
  if (enable){
    if (currentPSW & PROBLEM_STATE){
      __asm(ASM_PREFIX
            " MODESET MODE=SUP \n"
            :
            :
            :"r0","r1","r15");
      return TRUE;
    } else{
      return FALSE;  /* do nothing, tell caller no restore needed */
    }
  } else{
    if (currentPSW & PROBLEM_STATE){
      return TRUE;   /* do nothing, tell user was in problem state */
    } else{
      __asm(ASM_PREFIX
            " MODESET MODE=PROB \n"
            :
            :
            :"r0","r1","r15");
      return FALSE;
    }
  }
}

int setKey(int key){
  int oldKey;
  __asm(" XR   2,2 \n"
        " SLL  %1,4 \n"
        " MODESET KEYREG=(%1),SAVEKEY=(2) \n"
        " SRL  2,4 \n"
        " ST   2,%0 \n"
        :"=m"(oldKey)
        :"&r"(key)
        :"r2");
  return oldKey;
}

int64 getR12(void) {
  int64 value;
  __asm(ASM_PREFIX
#ifdef _LP64
        " STG 12,%0 \n"
#else
        " LLGTR 0,12 \n"
        " STG 0,%0 \n"
#endif
        :"=m"(value)
        :
        :"r0");
  return value;
}

int64 getR13(void) {
  int64 value;
  __asm(ASM_PREFIX
#ifdef _LP64
        " STG 13,%0 \n"
#else
        " LLGTR 0,13 \n"
        " STG 0,%0 \n"
#endif
        :"=m"(value)
        :
        :"r0");
  return value;
}

int ddnameExists(char *ddname){
// Use GETDSAB instead of DEVTYPE to check if a DD exists
// Build all GETDSAB data below 2G
  int rc;

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      DSAB * __ptr32 dsab;
      char ddname[8];
      char plist[16];
      char saveArea[72];
    )
  );

  memcpy(below2G->ddname,ddname,sizeof(below2G->ddname));

  __asm(ASM_PREFIX
#ifdef __XPLINK__
        " LA 13,%4 \n"
#endif
#ifdef _LP64
        " SAM31 , \n"
#endif
        " GETDSAB DDNAME=%3,DSABPTR=%1,LOC=ANY,MF=(E,%2,COMPLETE) \n"
#ifdef _LP64
        " SAM64 , \n"
#endif
        " ST 15,%0 \n"
        :"=m"(rc),"=m"(below2G->dsab)
        :"m"(below2G->plist),"m"(below2G->ddname),"m"(below2G->saveArea)
        :
#ifdef __XPLINK__
         "r13",
#endif
         "r0","r1","r14","r15");

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return rc;
}

/* atsadmin atsadm */


/* CVT Stuff */

CVT *getCVT(void) {
  int * __ptr32 mem = (int * __ptr32) 0;
  int * __ptr32 theCVT = (int * __ptr32)(*(mem+(0x10/4)));
  return (CVT*)theCVT;
}

Addr31 getATCVT(void) {
  int * __ptr32 mem = (int * __ptr32) 0;
  int * __ptr32 theATCVT = (int * __ptr32)(*(mem+(ATCVT_ADDRESS/4)));
  return theATCVT;
}

void *getIEACSTBL(void) {
  CVT *cvt = getCVT();
  return cvt->cvtcsrt;
}

cvtfix *getCVTPrefix(void) {
  char *theCVT = (char*)getCVT();
  return (cvtfix*)(theCVT-256);
}

ECVT *getECVT(void) {
  CVT *cvt = getCVT();
  return (ECVT*)(cvt->cvtecvt);
}

char *getSysplexName (void) {
  ECVT *ecvt = getECVT();
  return ecvt->ecvtsplx;
}

char *getSystemName (void) {
  char *ecvt = (char*)getECVT();
  return ecvt+0x158;
}

TCB *getTCB(void) {
  int *mem = (int*)0;
  return (TCB*)mem[CURRENT_TCB/sizeof(int)];
}

STCB *getSTCB(void) {
  TCB *tcb = getTCB();
  return (STCB*)(tcb->tcbstcb);
}

OTCB *getOTCB(void) {
  STCB *stcb = getSTCB();
  return (OTCB*)(stcb->stcbotcb);
}

ASCB *getASCB(void) {
  int *mem = (int*)0;
  return (ASCB*)(mem[CURRENT_ASCB/sizeof(int)]&0x7FFFFFFF);
}

ASXB *getASXB(void) {
  ASCB *ascb = getASCB();
  return (ASXB*)ascb->ascbasxb;
}

ASSB *getASSB(ASCB *ascb){
  ASSB *assb = (ASSB*)ascb->ascbassb;
  return assb;
}

JSAB *getJSAB(ASCB *ascb){
  ASSB *assb = (ASSB*)ascb->ascbassb;
  JSAB *jsab = (JSAB*)assb->assbjsab;
  return jsab;
}

ACEE *getCurrentACEE(void) {
  TCB *tcb = getTCB();
  ASXB *asxb = getASXB();

  void *ptr = tcb->tcbsenv; /* The TCB's security environment shadows the ascb/axsb's. */

  if (ptr == NULL) {
    ptr = asxb->asxbsenv;
  }
  return (ACEE*)ptr;
}

TCB *getFirstChildTCB(TCB *tcb){
  return (TCB*)tcb->tcbltc;
}

TCB *getParentTCB(TCB *tcb){
  return (TCB*)tcb->tcbotc;
}

TCB *getNextSiblingTCB(TCB *tcb){
  return (TCB*)tcb->tcbntc;
}


/* SAF

   outer SAF arg list

   http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/ICHZC471/1.52?SHELF=EZ2ZO10I&DT=20060921115021

   return codes

   http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/ICHZA672/CCONTENTS?SHELF=EZ2ZO10I&DN=SA22-7686-10&DT=20070125204025

    Common Name: System authorization facility (SAF) router parameter list

    Macro ID: ICHSAFP

    DSECT Name: SAFP

Owning Component: Resource Access Control Facility (SC1BN)




   */

#define SAFP_FLAG_MSGR 0x80 /* message return requested */
#define SAFP_FLAG_R18  0x40 /* release 1.8 or higher requested */
#define SAFP_FLAG_SUPP 0x20 /* message suppression on */
#define SAFP_FLAG_DCPL 0x10 /* decouple ??? */
#define SAFP_FLAG_SYST 0x08 /* system ??? */

#define SAFP_STATUS_GCS  0x80 /* request came from GCS */
#define SAFP_STATUS_SFSU 0x40 /* SFS user accessing own file or  directory */

/* SAF function codes, aka request numbers */
#define SAFPAU  1 /* RACHECK - authorization function */
#define SAFPFAU 2 /* FRACHECK - Fast authorization function */
#define SAFPLIS 3 /* RACLIST - in-storage  list building function */
#define SAFPDEF 4 /* RACDEF - definition function */
#define SAFPVER 5 /* RACINIT - verification function */
#define SAFPEXT 6 /* RACXTRT - extract function */
#define SAFPDIR 7 /* RACDAUTH - directed authorization function */
#define SAFPTKMP 8 /* RACTKSRV - security   token services */
#define SAFPVERX 9 /* RACROUTE REQUEST=VERIFYX */
#define SAFPTKXT 10 /* RACTKSRV - extract token services */
#define SAFPTBLD 11 /* RACINIT - token build services */
#define SAFPEXTB 12 /* RACXTRT - branch entry */
#define SAFPAUD  13 /* RACAUDIT - audit service  */
#define SAFPSTAT 14 /* RACSTAT - status service */
#define SAFPSIGN 15 /* SIGNON request */
#define SAFPTXMP 16 /* Token map request for XMREQ=YES is  specified. */
#define SAFPTXXT 17 /* Token extract request for XMREQ=YES is specified. */

#define SAFPRL19 0 /* Indicates RACF 1.9.0 */
#define SAFPR192 2 /* Indicates RACF 1.9.2 */
#define SAFPRL21 3 /* Indicates RACF 2.1.0 */
#define SAFPRL22 4 /* Indicates RACF 2.2.0 */
#define SAFPRL23 5 /* Indicates RACF 2.3.0 */
#define SAFPRL24 6 /* Indicates RACF 2.4.0 */
#define SAFPRL26 7 /* Indicates RACF 2.6.0 */
#define SAFPRL28 8 /* Indicates RACF 2.6.8 */
#define SAFPRL7709 14 /* Indicates RACF 7709 */

#define SAFPVER_SIZE 108
#define SAFPAU_SIZE 92
#define SAFPSTAT_SIZE 24

typedef struct safp_tag{
  int safprret;  /* inner return */
  int safprrea; /* inner reason */
  short safppln;   /* length in bytes */
  char  safpvrrl;  /* version */
  char  reserved1;
  short safpreqt;  /* request number */
  char  safpflgs;
  char  safpmspl;  /* subppool */
  char  * __ptr32 safpreqr; /* requestor ptr to 8-byte string */
  char  * __ptr32 safpsubs; /* subsystem name: ptr to 8-byte string */
  void  * __ptr32 safpwa;    /* work area */
  void  * __ptr32 safpmsad;  /* message address */
  void  * __ptr32 reserved2;
  int   safpracp;   /* offset to racf related parms from base of this struct */
  int   safpsfrc;   /* SAF return code */
  int   safprfrs;   /* SAF reason code */
  short safpplnx;   /* SAFP extension length in bytes */
  short safpolen;   /* SAFP original plist length */
  void  * __ptr32 safpretd;  /* address of returned data */
  void  * __ptr32 safpflat;  /* address of flat parameter */
  void  * __ptr32 safpecb1;
  void  * __ptr32 safpecb2;
  void  * __ptr32 safpprev;  /* points to previous flat list */
  void  * __ptr32 safpnext;
  void  * __ptr32 safporig;
  int   safpflen;   /* flat param list length */
  int   safpusrw;   /* user word */
  void  * __ptr32 safppree;
  void  * __ptr32 safppost;
  void  * __ptr32 safpsync;
  char  safpskey;    /* requestor storage key */
  char  safpmode;    /* requestor address mode */
  char  safpsbyt;    /* status byte */
  char  reserved3;
} safp;

/* flag 0 defines */

#define SAF_VERIFY_BELOW   0x80
#define SAF_VERIFY_ANY     0x40
#define SAF_VERIFY_PRAL    0x20
#define SAF_VERIFY_VERIFYX 0x10
#define SAF_VERIFY_SYSN    0x08 /* PARAMETER SPECIFIED THAT IS NOT COMPATIBLE WITH SYSTEM=YES */
#define SAF_VERIFY_LOG_NONE 0x04

/* flag 1 defines */

#define SAF_VERIFY_ENIVR_MASK 0xC0
#define SAF_VERIFY_CREATE     0x00
#define SAF_VERIFY_CHANGE     0x40
#define SAF_VERIFY_DELETE     0x80
#define SAF_VERIFY_VERIFY     0xC0
#define SAF_VERIFY_NO_COMPLETE       0x20
#define SAF_VERIFY_SUBPOOL_SPECIFIED 0x10
#define SAF_VERIFY_NO_PWCHK          0x08
#define SAF_VERIFY_NO_STAT           0x04
#define SAF_VERIFY_LOG_ALL           0x02
#define SAF_VERIFY_NO_ENCRYPT        0x01

/* flag 2 defines */

#define SAF_VERIFY_TRUSTED_COMP  0x80
#define SAF_VERIFY_REMOTE_JOB    0x40
#define SAF_VERIFY_TRUSTED_KEYWORD 0x20
#define SAF_VERIFY_REMOVE_KEYWORD 0x10

/* flag 3 defines */
#define SAF_VERIFY_ERROPT_NOABEND 0x80
#define SAF_VERIFY_NESTED         0x40
#define SAF_VERIFY_NESTED_COPY    0x20
#define SAF_VERIFY_NO_MFA         0x10

typedef struct safVerifyRequest_tag{
  char initlen;
  char initsubp;
  char initflg0;
  char initflg1;
  void * __ptr32 userid;
  void * __ptr32 password;
  void * __ptr32 startProc;
  void * __ptr32 installInfo;
  void * __ptr32 groupName;
  void * __ptr32 newPassword;
  void * __ptr32 programmerName;
  void * __ptr32 accountNumber;
  void * __ptr32 magneticStripeCard;
  void * __ptr32 terminalId;
  void * __ptr32 jobname;
  void * __ptr32 applicationName;
  void * __ptr32 aceeAnchor;
  char sessionType;
  char initflg2;
  char initflg3;
  char reserved1;
  void * __ptr32 seclabel;
  void * __ptr32 exenode;
  void * __ptr32 suserid;
  void * __ptr32 snode;
  void * __ptr32 sgroup;
  void * __ptr32 poe;
  void * __ptr32 inputToken;
  void * __ptr32 stoken;
  void * __ptr32 logstr;
  void * __ptr32 outputToken;
  void * __ptr32 envirin;
  void * __ptr32 envirout;
  void * __ptr32 poeName;
  void * __ptr32 x500NamePair;
  void * __ptr32 reserved2;
  void * __ptr32 servauth;
  void * __ptr32 passphrase;
  void * __ptr32 newPassphrase;
  void * __ptr32 ictx;
  void * __ptr32 idid;
  void * __ptr32 icrx;
} safVerifyRequest;

/* first flag set */
#define SAF_AUTH_RACFIND_SPECIFIED 0x80
#define SAF_AUTH_RACFIND_YES       0x40
#define SAF_AUTH_ENTITYX_SPECIFIED 0x20
#define SAF_AUTH_DSTYPE_V          0x10
#define SAF_AUTH_31_BIT_ADDRS      0x08
#define SAF_AUTH_LOG_NOSTAT        0x06
#define SAF_AUTH_LOG_NOFAIL        0x04
#define SAF_AUTH_LOG_NONE          0x02
#define SAF_AUTH_ENTITY_CSA        0x01


/* third flag set */
#define SAF_AUTH_DSTYPE_T         0x80
#define SAF_AUTH_DSTYPE_M         0x40
#define SAF_AUTH_PROFILE_GIVEN    0x20
#define SAF_AUTH_VOLSER_SPECIFIED 0x08
#define SAF_AUTH_GENERIC          0x04
#define SAF_AUTH_PRIVATE          0x02

/* fourth flag set */
#define SAF_AUTH_STATUS_ERASE 0x80
#define SAF_AUTH_STATUS_EVERDOM 0x40
#define SAF_AUTH_STATUS_WRITEONLY 0x20
#define SAF_AUTH_STATUS_ACCESS 0x10

static int SAF(safp * __ptr32 safwrapper, int useSupervisorMode)
{
  int returnCode = 0;

  int supervisorState = 0;

  char * __ptr32 cvt = * (char * __ptr32 * __ptr32 ) 16;
  char * __ptr32 safVectorTable = (char * __ptr32) *(int *)(cvt + 248);
  char * __ptr32 safRouter = (char * __ptr32) *(int *)(safVectorTable + 12);

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      char saveArea[72];
    )
  );

  if (useSupervisorMode)
  {
    supervisorState = supervisorMode (1);
  }

  __asm (ASM_PREFIX
#ifdef __XPLINK__
          "         LA       13,%3 \n"
#endif
#ifdef _LP64
          "         SAM31 , \n"
#endif
          "         LR       1,%1 \n"
          "         LR       15,%2 \n"
          "         BASR     14,15 \n"
#ifdef _LP64
          "         SAM64 , \n"
#endif
          "         ST       15,%0 \n"
          :"=m"(returnCode)
          :"r"(safwrapper),"r"(safRouter),"m"(below2G->saveArea)
          :
#ifdef __XPLINK__
           "r13",
#endif
           "r1","r14","r15");

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  if (supervisorState)
  {
    supervisorState = supervisorMode (0);
  }
   return returnCode;
}

static int safTrace = 0;
static void *safTraceFile = 0;
void setSafTrace(int traceLevel, void *traceFile){
  safTrace = traceLevel;
  safTraceFile = traceFile;
}

typedef struct safAuthRequest_tag{
  char achkleng;
  char old_installationDataAddress[3];
  char achkflg1;
  char old_entityNameAddress[3];
  char achkflg2;
  char old_classNameAddress[3];
  char achkflg3;
  char old_volser3ByteAddress[3];
  void * __ptr32 old_volserAddress;
  void * __ptr32 applName;
  void * __ptr32 acee;
  void * __ptr32 owner; /* ? */
  void * __ptr32 installationData;
  void * __ptr32 entityName;
  void * __ptr32 className;
  void * __ptr32 volser;
  void * __ptr32 acclvl1;
  void * __ptr32 acclvl2;
  short fileSequenceNumber;
  char tapeFlags;
  char achkflg4;
  void * __ptr32 userid;
  void * __ptr32 groupName;
  void * __ptr32 ddname;
  void * __ptr32 reserved1;
  void * __ptr32 utoken;
  void * __ptr32 rtoken;
  void * __ptr32 logstr;
  void * __ptr32 recvr;
} safAuthRequest;

typedef struct safStatRequest_tag{
  void * __ptr32 className;
  void * __ptr32 CDTentry;
  short statLength;
  char reserved[2];
  void * __ptr32 classCopy;
  int classCopyLength;
  void * __ptr32 statNext;
} safStatRequest;

static safp *makeSAFCallData(int requestNumber,
                             int useSupervisorMode,
                             int *allocatedSize)
{
  safp *safWrapper;
  void *specificBlock;
  int specificDataSize = -1;
  int flags = 0;
  int version = 0;
  int blankingSize;

  version = SAFPR192;
  switch (requestNumber){
  case SAFPVER:
    specificDataSize = sizeof(safVerifyRequest);
    flags = SAFP_FLAG_R18;
    if (useSupervisorMode){
      flags |= SAFP_FLAG_SYST;
    }
    break;
  case SAFPAU:
    specificDataSize = sizeof(safAuthRequest);
    flags = SAFP_FLAG_R18;
    break;
  case SAFPSTAT:
    specificDataSize = sizeof(safStatRequest);
    flags = SAFP_FLAG_R18;
    version = SAFPRL7709;
    break;
  default:
    printf("unknown request number %d\n",requestNumber);
  }
  if (specificDataSize == -1){
    return NULL;
  }
  blankingSize = sizeof(safp)+specificDataSize;
  safWrapper = (safp *) safeMalloc31(blankingSize, "varlen safWrapper");
  *allocatedSize = blankingSize; /* so we can deallocate properly later */

  /* printf("blanking size 0x%x=%d, safp %d verify %d \n",blankingSize,blankingSize,sizeof(safp),specificDataSize); */
  memset(safWrapper,0,blankingSize);

  safWrapper->safppln = sizeof(safp);
  safWrapper->safpreqt = requestNumber;
  safWrapper->safpflgs = flags;
  safWrapper->safpvrrl = version;
  safWrapper->safpracp = sizeof(safp);
  safWrapper->safpplnx = 64; /* from MARS, seen examples of 64 in RACROUTE expansions */

  return safWrapper;
}

ACEE *getAddressSpaceAcee(void){
  return (ACEE *)(getASXB()->asxbsenv);
}

ACEE *getTaskAcee(void){
  return (ACEE *)(getTCB()->tcbsenv);
}

/* call this only when testAuth() == 0; cache the result of testAuth */
int setTaskAcee(ACEE *acee){
    ACEE * __ptr32 *taskAceePtr = (ACEE * __ptr32 *)&(getTCB()->tcbsenv);
    int userKey;
    // The original code issued MODESET MODE=SUP, saved the key, and then set it to 0
    __asm(ASM_PREFIX
          " MODESET MODE=SUP \n"
          " MODESET EXTKEY=ZERO,SAVEKEY=(2) \n"
          " ST 2,%0 \n"
          :"=m"(userKey)
          :
          :"r1","r2","r15");
    *taskAceePtr = acee;
    __asm(ASM_PREFIX
          " MODESET KEYREG=(%0) \n"
          " MODESET MODE=PROB \n"
          :
          :"r"(userKey)
          :"r1","r15");
    return 0;
}

/* modifications to TCBSENV (notes from the manual)

   For CREATE:

   If an ACEE is not specified, the address of the newly created ACEE
   is stored in the TCBSENV field of the task control block. If the
   ASXBSENV field contains binary zeros, the new ACEE address is also
   stored in the ASXBSENV field of the ASXB. If the ASXBSENV field is
   nonzero, it is not modified. The TCBSENV field is set
   unconditionally if ACEE= is not specified.

   For DELETE:

   If ACEE= is not specified or ACEE=0 is specified, and the TCBSENV
   field for the task using the RACROUTE REQUEST=VERIFY is nonzero,
   the ACEE pointed to by the TCBSENV is deleted, and TCBSENV is set
   to zero. If the TCBSENV and ASXBSENV fields both point to the same
   ACEE, ASXBSENV is also set to zero. If no ACEE address is passed,
   and TCBSENV is zero, the ACEE pointed to by ASXBSENV is deleted,
   and ASXBSENV is set to zero.



   */

static char *makeCountedString(char *name,
                               char *string,
                               int maxLength,
                               int isEntity,
                               int *allocatedSize)
{
  int allocSize = 0;
  int len = string ? strlen(string) : 0;
  if (len > maxLength){
    printf("%s too long\n", name);
    return 0;
  }
  int prefixLen = isEntity ? 4 : 1;
  allocSize = prefixLen+len+1;
  char *result = safeMalloc31(allocSize, "countedString");
  if (result == 0) {
    *allocatedSize = 0;
    return 0;
  }
  *allocatedSize = allocSize;
  if (prefixLen == 4) {
    result[0] = 0;
    result[1] = 0;
    result[2] = 0;
  }
  result[prefixLen-1] = len;
  if (len) memcpy(result+prefixLen, string, len);
  result[prefixLen+len] = 0;
  return result;
}

int safVerify(int options, char *userid, char *password,
              ACEE **aceeHandle,
              int *racfStatus, int *racfReason){
#ifdef DEBUG
  printf("in safVerify before safVerifyInternal\n");
#endif
  return (safVerifyInternal(options,
                            userid,
                            password,
                            NULL,
                            aceeHandle,
                            NULL,
                            0,
                            NULL,
                            0,
                            racfStatus,
                            racfReason));
#ifdef DEBUG
  printf("in safVerify after safVerifyInternal\n");
#endif
}

int safVerify2(int options, char *userid, char *password,
               ACEE **aceeHandle,
               void **messageAreaPtr, int subpool,
               int *racfStatus, int *racfReason){
  return (safVerifyInternal(options,
                            userid,
                            password,
                            NULL,
                            aceeHandle,
                            messageAreaPtr,
                            subpool,
                            NULL,
                            0,
                            racfStatus,
                            racfReason));
}

int safVerify3(int options,
               char *userid, char *password,
               ACEE **aceeHandle,
               void **messageAreaPtr, int subpool,
               char* applicationName,
               int *racfStatus, int *racfReason){
  return (safVerifyInternal(options,
                            userid,
                            password,
                            NULL,
                            aceeHandle,
                            messageAreaPtr,
                            subpool,
                            applicationName,
                            0,
                            racfStatus,
                            racfReason));
}

int safVerify4(int options,
               char *userid,
               char *password,
               char *newPassword,
               ACEE **aceeHandle,
               void **messageAreaPtr, int subpool,
               char* applicationName,
               int *racfStatus, int *racfReason){
  return (safVerifyInternal(options,
                            userid,
                            password,
                            newPassword,
                            aceeHandle,
                            messageAreaPtr,
                            subpool,
                            applicationName,
                            0,
                            racfStatus,
                            racfReason));
}

int safVerify5(int options,
               char *userid,
               char *password,
               char *newPassword,
               ACEE **aceeHandle,
               void **messageAreaPtr, int subpool,
               char *applicationName,
               int  sessionType,
               int  *racfStatus, int *racfReason){
  return (safVerifyInternal(options,
                            userid,
                            password,
                            newPassword,
                            aceeHandle,
                            messageAreaPtr,
                            subpool,
                            applicationName,
                            sessionType,
                            racfStatus,
                            racfReason));
}

static int safVerifyInternal(int options,
                             char *userid,
                             char *password,
                             char *newPassword,
                             ACEE **aceeHandle,
                             void **messageAreaPtr,
                             int  subpool,
                             char *applicationName,
                             int  sessionType,
                             int *racfStatus, int *racfReason)
{
  int useSupervisorMode = 1; /* verify create/delete demands this 0 options & VERIFY_SUPERVISOR; */
  int safWrapperSize = 0;
  int countedUserSize = 0;
  int countedPasswordSize = 0;
  int countedPassphraseSize = 0;
  int countedNewPasswordSize = 0;
  int countedNewPassphraseSize = 0;
  char *countedPassword = NULL;
  char *countedPassphrase = NULL;
  char *countedNewPassword = NULL;
  char *countedNewPassphrase = NULL;
  char *application = NULL;
  safp *safWrapper = makeSAFCallData(SAFPVER,useSupervisorMode,&safWrapperSize);
  char *workArea = safeMalloc31(512, "safVerifyInternal workArea");
  ACEE * __ptr32  * __ptr32 ACEEPtr =  (ACEE * __ptr32 * __ptr32) safeMalloc31(4, "ACEE ptr");
  safVerifyRequest *verifyRequest = (safVerifyRequest*)(((char*)safWrapper)+ sizeof(safp));
  char *countedUserid = makeCountedString("userid", userid, 8, FALSE, &countedUserSize);
  if (strlen (password) <= 8) {
    countedPassword = makeCountedString("password", password, 8, FALSE, &countedPasswordSize);
  }
  else {
    countedPassphrase = makeCountedString("passphrase", password, 100, FALSE, &countedPassphraseSize);
  }
  if (newPassword) {
    if (strlen (newPassword) <= 8) {
      countedNewPassword = makeCountedString("newPassword", newPassword, 8, FALSE, &countedNewPasswordSize);
    }
    else {
      countedNewPassphrase = makeCountedString("newPassphrase", newPassword, 100, FALSE, &countedNewPassphraseSize);
    }
  }
  if (applicationName) {
    if (strlen (applicationName) <= 8) {
      application = safeMalloc31 (8, "Application Name workArea");
      memset (application, ' ', 8);
      memcpy (application, applicationName, strlen (applicationName));
      verifyRequest->applicationName = application;
    }
  }
  if (sessionType) {
    verifyRequest->sessionType = sessionType;
  }
  int safStatus = 0x96; /* no memory or too long */
  if (safWrapper && workArea && countedUserid && (countedPassword || countedPassphrase)) {
    char verifyFlags0 = 0;
    char verifyFlags1 = 0;
    char verifyFlags3 = 0;
    safWrapper->safpwa = workArea;
    if (messageAreaPtr != NULL)
    {
      safWrapper->safpflgs |= 0x80;
      safWrapper->safpmspl = subpool;
    }

    if (options & (VERIFY_CREATE | VERIFY_DELETE)) {
      if (options & VERIFY_WITHOUT_LOG) {
        verifyFlags0 |= SAF_VERIFY_LOG_NONE;
      }
    }

    verifyRequest->initlen = sizeof(safVerifyRequest);
    if (options & VERIFY_CREATE){
      verifyFlags1 |= SAF_VERIFY_CREATE;
    } else if (options & VERIFY_DELETE){
      verifyFlags1 |= SAF_VERIFY_DELETE;
    } else if (options & VERIFY_CHANGE){
      verifyFlags1 |= SAF_VERIFY_CHANGE;
    } else {
      verifyFlags1 |= SAF_VERIFY_VERIFY;
    }

    if (!(options & VERIFY_DELETE)){
      verifyRequest->userid = countedUserid;
      if (options & VERIFY_WITHOUT_PASSWORD){
        verifyFlags1 |= SAF_VERIFY_NO_PWCHK;
      } else{
        if (options & VERIFY_WITHOUT_MFA){
          verifyFlags3 |= SAF_VERIFY_NO_MFA;
        }
        if (countedPassword) {
          verifyRequest->password = countedPassword;
        }
        else {
          verifyRequest->passphrase = countedPassphrase;
        }
        if (countedNewPassword) {
          verifyRequest->newPassword = countedNewPassword;
        }
        else {
          verifyRequest->newPassphrase = countedNewPassphrase;
        }
      }
      if (options & VERIFY_WITHOUT_STAT_UPDATE)
        verifyFlags1 |= SAF_VERIFY_NO_STAT;
    }

    if (aceeHandle != NULL){
      /* flags 0 is zero
         flags 1 is zero */
      *ACEEPtr = *aceeHandle;
      if (safTrace){
        fprintf(safTraceFile,"setting acee anchor to %0.8X', %0.8X'\n",aceeHandle, *aceeHandle);
        fprintf(safTraceFile,"setting real acee anchor to %0.8X', %0.8X'\n",ACEEPtr, *ACEEPtr);
      }
      verifyRequest->aceeAnchor = ACEEPtr; /* placeholder for system-created ACEE */
    }
    verifyRequest->initflg0 = verifyFlags0;
    verifyRequest->initflg1 = verifyFlags1;
    verifyRequest->initflg3 = verifyFlags3;

    if (safTrace){
      fprintf(safTraceFile,"countedUserid %0.8X' password %0.8X' passphrase %0.8X'\n",
              countedUserid, countedPassword, countedPassphrase);
      fprintf(safTraceFile,"about to go call saf wrapper at %0.8X' verify block at %0.8X' acee %0.8X'\n",safWrapper,verifyRequest,
              (aceeHandle != NULL ? *aceeHandle : NULL));
      dumpbuffer((char*)safWrapper,sizeof(safp));
      fprintf(safTraceFile,"verify:\n",0);
      dumpbuffer((char*)verifyRequest,sizeof(safVerifyRequest));
    }
    safStatus = SAF(safWrapper,useSupervisorMode);
    if (messageAreaPtr != NULL && safWrapper->safpmsad != NULL)
    {
      *messageAreaPtr = safWrapper->safpmsad;
    }
    if (aceeHandle != NULL)
    {
      *aceeHandle = *ACEEPtr;
    }
    *racfStatus = safWrapper->safprret;
    *racfReason = safWrapper->safprrea;
  }
  safeFree((char *) ACEEPtr, 4);
  if (countedUserid) {
    safeFree(countedUserid, countedUserSize);
    countedUserid = NULL;
  }
  if (countedPassword) {
    safeFree(countedPassword, countedPasswordSize);
    countedPassword = NULL;
  }
  if (countedPassphrase) {
    safeFree(countedPassphrase, countedPassphraseSize);
    countedPassphrase = NULL;
  }
  if (countedNewPassword) {
    safeFree(countedNewPassword, countedNewPasswordSize);
    countedNewPassword = NULL;
  }
  if (countedNewPassphrase) {
    safeFree(countedNewPassphrase, countedNewPassphraseSize);
    countedNewPassphrase = NULL;
  }
  if (application)
  {
    safeFree (application, 8);
    application = NULL;
  }
  safeFree((char *) safWrapper, safWrapperSize);
  safeFree(workArea, 512);

  return safStatus;
}

static int safAuth_internal(int options, char *safClass, char *entity, int accessLevel, int authstatus, ACEE *acee,
                            int *racfStatus, int *racfReason)
{
  int isDataset = !strcmp(safClass,"DATASET");
  char firstVolser[7];
  int volserCount;
  if (isDataset){
    int locateStatus = locate(entity,&volserCount,firstVolser);
    firstVolser[6] = 0;
    if (locateStatus || !strcmp(firstVolser,"MIGRAT") || !strcmp(firstVolser,"ARCIVE")){
      printf("could not locate dataset or dataset is migrated\n");
      return 0x96;
    }
  }
  int useSupervisorMode = options & AUTH_SUPERVISOR;
  int safWrapperSize, countedClassSize, countedEntitySize;
  safp *safWrapper = makeSAFCallData(SAFPAU,useSupervisorMode,&safWrapperSize);
  char *workArea = safeMalloc31(512, "safAuth_internal workArea");
  safAuthRequest *authRequest = (safAuthRequest*)(((char*)safWrapper) + sizeof(safp));
  char *countedClass = makeCountedString("class", safClass, 8, FALSE, &countedClassSize);
  char *countedEntity = makeCountedString("entity", entity, 255, TRUE, &countedEntitySize);
  int safStatus = 0x96;
  if (safWrapper && workArea && countedClass && countedEntity) {
    safWrapper->safpwa = workArea;
    char authFlags0 = 0;
    char authFlags1 = 0;

    if (safTrace){
      dumpbuffer((char*)countedEntity,50);
    }

    authRequest->achkleng = sizeof(safAuthRequest);
    authRequest->achkflg1 = SAF_AUTH_31_BIT_ADDRS | SAF_AUTH_ENTITYX_SPECIFIED;
    authRequest->achkflg2 = accessLevel;
    authRequest->achkflg3 = 0;
    authRequest->achkflg4 = authstatus;
    authRequest->entityName = countedEntity;
    authRequest->className = countedClass;
    if (acee){
      authRequest->acee = acee;
    }
    if (isDataset){
      authRequest->volser = firstVolser;
      if (options & AUTH_DATASET_VSAM){
        authRequest->achkflg1 |= SAF_AUTH_DSTYPE_V;
      }
      if (options & AUTH_DATASET_TAPE){
        authRequest->achkflg3 |= SAF_AUTH_DSTYPE_T;
      }
    }

    if (safTrace){
      fprintf(safTraceFile,"before calling SAF, wrapper=%0.8X':\n",safWrapper);
      dumpbuffer((char*)safWrapper,sizeof(safp));
      fprintf(safTraceFile,"before calling SAF, auth=%0.8X':\n", authRequest);
      dumpbuffer((char*)authRequest,sizeof(safAuthRequest));
      fprintf(safTraceFile,"before calling SAF, class=%0.8X':\n", countedClass);
      dumpbuffer((char*)countedClass,countedClass[0]+1);
      fprintf(safTraceFile,"before calling SAF, entity=%0.8X':\n", countedEntity);
      dumpbuffer((char*)countedEntity,countedEntity[3]+4);
    }
    safStatus = SAF(safWrapper, useSupervisorMode);
    if (safTrace){
      fprintf(safTraceFile,"after calling SAF, safStatus=%d\n",safStatus);
      fprintf(safTraceFile,"after calling SAF, wrapper=%0.8X':\n",safWrapper);
      dumpbuffer((char*)safWrapper,sizeof(safp));
      fprintf(safTraceFile,"after calling SAF, auth=%0.8X':\n", authRequest);
      dumpbuffer((char*)authRequest,sizeof(safAuthRequest));
    }
    *racfStatus = safWrapper->safprret;
    *racfReason = safWrapper->safprrea;
  }
  safeFree(countedClass, countedClassSize);
  safeFree(countedEntity, countedEntitySize);
  safeFree((char *) safWrapper, safWrapperSize);
  safeFree(workArea, 512);
  return safStatus;
}

int safAuth(int options, char *safClass, char *entity, int accessLevel, ACEE *acee, int *racfStatus, int *racfReason){
  return safAuth_internal(options,
                          safClass,
                          entity,
                          accessLevel,
                          0,
                          acee,
                          racfStatus,
                          racfReason);
}

int safAuthStatus(int options, char *safClass, char *entity, int *accessLevel, ACEE *acee, int *racfStatus, int *racfReason){
  int safStatus = 0;
  safStatus = safAuth_internal(options,
                               safClass,
                               entity,
                               SAF_AUTH_ATTR_READ,
                               SAF_AUTH_STATUS_ACCESS,
                               acee,
                               racfStatus,
                               racfReason);
  *accessLevel = *racfReason;
  return safStatus;
}

int safStat(int options, char *safClass, char *copy, int copyLength, int *racfStatus, int *racfReason){
  int useSupervisorMode = options & STAT_SUPERVISOR;
  int safWrapperSize;
  safp *safWrapper = makeSAFCallData(SAFPSTAT,useSupervisorMode,&safWrapperSize);
  char *workArea = safeMalloc31(512, "safStat workArea");
  safStatRequest *statRequest = (safStatRequest*)(((char*)safWrapper)+ sizeof(safp));
  char *classBuffer = safeMalloc31(12, "safStat classBuffer");
  int safStatus = 0x96;
  int classLength = strlen(safClass);
  if (safWrapper && workArea && statRequest && classBuffer && (classLength <= 8)){
    safWrapper->safpwa = workArea;

    memset(classBuffer, ' ', sizeof(classBuffer));
    memcpy(classBuffer, safClass, classLength);
    statRequest->className = classBuffer;
    statRequest->classCopy = copy;
    statRequest->classCopyLength = copyLength;
    statRequest->statLength = sizeof(safStatRequest);

    if (safTrace){
      fprintf(safTraceFile,"about to call saf, wrapper %0.8X:\n",safWrapper);
      dumpbuffer((char*)safWrapper,sizeof(safp));
      fprintf(safTraceFile,"stat %0.8X:\n",statRequest);
      dumpbuffer((char*)statRequest,sizeof(safStatRequest));
      fprintf(safTraceFile,"className %0.8X:\n",classBuffer);
      dumpbuffer((char*)classBuffer,12);
      fprintf(safTraceFile,"classCopy %0.8X:\n",copy);
      dumpbuffer((char*)copy,copyLength);
      fprintf(safTraceFile,"workArea %0.8X\n",workArea);
      fflush(safTraceFile);
    }
    safStatus = SAF(safWrapper, useSupervisorMode);
    *racfStatus = safWrapper->safprret;
    *racfReason = safWrapper->safprrea;
  }
  safeFree(classBuffer, 12);
  safeFree((char *) safWrapper, safWrapperSize);
  safeFree(workArea, 512);
  return safStatus;
}

/* SAFPAU
   here's the data structure:
   http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/ICHZC471/1.2?SHELF=EZ2ZO10I&DT=20060921115021

   */

/* SAFPSTAT
   here's the data structure:
   http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/ICHZC471/1.58?SHELF=EZ2ZO10I&DT=20060921115021

   */

/* SAFPVER
   here's the data structure
   http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/ICHZC471/1.43?SHELF=EZ2ZO10I&DT=20060921115021

   */


/* HERE
   CVT SAF functions

   get current TCB AC
   */


/* LOCATE/CAMLIST */

int locate(char *dsn, int *volserCount, char *firstVolser){

  int dsnLength = strlen(dsn);

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      void * __ptr32 svc26Arg[4];
      char dsn44[44];
      char reserved[4];     // SVC 26 return area must be doubleword aligned
      struct {
        short int volserCount;
        struct {
          char devtype[4];
          char volser[6];
          short int sequence;
        } volumeEntry[21];
        char reserved[11];
      } svc26ReturnArea;
    )
  );

  int status;

  below2G->svc26Arg[0] = (void*)68; /* indicates call type */
  below2G->svc26Arg[1] = below2G->dsn44;
  below2G->svc26Arg[3] = &below2G->svc26ReturnArea;

  memset(below2G->dsn44,' ',sizeof(below2G->dsn44));
  memcpy(below2G->dsn44,dsn,dsnLength);

  /*
    printf("dsn44 %x len %d\n",dsn44,dsnLength);
    dumpbuffer(dsn44,44);
    printf("arg %x:\n",svc26Arg);
    dumpbuffer(svc26Arg,16);
    */

  __asm(ASM_PREFIX
        " LOCATE (%1) \n"
        " ST     15,%0 \n"
        :"=m"(status)
        :"r"(&below2G->svc26Arg)
        :"r0","r1","r15");

  /*
     printf("locate1 status %d\n",status);
     dumpbuffer(svc26Arg,16);
     dumpbuffer(svc26ReturnArea,265);
     */

  if (status == 0){
    *volserCount = below2G->svc26ReturnArea.volserCount;
    memcpy(firstVolser,below2G->svc26ReturnArea.volumeEntry[0].volser,
           sizeof(below2G->svc26ReturnArea.volumeEntry[0].volser));
  }
  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return status;
}

int atomicIncrement(int *intPointer, int increment){
  int newValue;
  // Let C set the increment address into a register
  // Let C set the increment value into a register
  // Increment the value in the assembler code rather having C do it
  // Loop in the assembler code on a compare failure rather than having C do it
  // Set the new value into a register to return to C
  // Generate a label value to avoid inlining issues using Metal C
  // Use LAA if the architecture level allows it
  __asm(ASM_PREFIX
  #if (__ARCH__ >=9)   /* If LAA is supported */
        "         LAA   %0,%1,0(%2)  Increment and update current value \n"
        "         AR    %0,%1        Set the new value to return \n"
  #else                /* If LAA is *not* supported */
        "         LCLA  &ATI \n"
        "&ATI     SETA  &ATI+1 \n"
        "&ATILP   SETC  'ATINCR&ATI' \n"
        "         L     0,0(,%2)     Get current value \n"
        "&ATILP   LR    %0,0 \n"
        "         AR    %0,%1        Set the new value to update and return \n"
        "         CS    0,%0,0(%2)   Update current value \n"
        "         JNZ   &ATILP       Repeat if compare failure \n"
  #endif
        :"=r"(newValue)      /* newValue returned in a register */
        :"r"(increment),     /* increment contained in a register */
         "a"(intPointer)     /* intPointer contained in a register */
        :"r0");
  return newValue;
}

typedef struct LOADARGS_tag{
  char * __ptr32 epname;  /* ptr to 8 char string */
  int dcbAddress; /* let it be 0 for most uses */
  char parameterListFormatNumber;
  char reserved;
  char options1;
  char options2;
  int  moreOptions;         /* EXPLICIT LOAD, LOADPT, EXTINFO */
} LOADARGS;

void *loadByName(char *moduleName, int *statusPtr){
  int  entryPoint;
  int  status;

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      LOADARGS loadargs;
    )
  );

  below2G->loadargs.epname = moduleName;

  __asm(ASM_PREFIX
        " LOAD SF=(E,%2) \n"
        " ST 15,%0 \n"
        " ST 0,%1 \n"
        :"=m"(status),"=m"(entryPoint)
        :"m"(below2G->loadargs)
        :"r0","r1","r15");
  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
  /* printf("LOAD status = 0x%x\n",status); */
  *statusPtr = status;
  if (status){
    return 0; /* entry point not found */
  } else{
    return (void*)entryPoint;
  }

}



/* DSAB Stuff

     DDNAME

     PDS YES, MEMBER=YES
     PDS YES, MEMBER=NO

     DS YES, MEMBER=NOT_SPECIFIED
     DS_NO,  MEMBER=NOT_SPECIFIED

     dsab->tiot
   */


DSAB *getDSAB(char *ddname){
  int rc;
  DSAB *dsab;
  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      DSAB * __ptr32 dsab;
      char ddname[8];
      char plist[16];
      char saveArea[72];
    )
  );

  memcpy(below2G->ddname,ddname,sizeof(below2G->ddname));

  __asm(ASM_PREFIX
#ifdef __XPLINK__
        " LA 13,%4 \n"
#endif
#ifdef _LP64
        " SAM31 \n"
#endif
        " GETDSAB DDNAME=%3,DSABPTR=%1,LOC=ANY,MF=(E,%2,COMPLETE) \n"
#ifdef _LP64
        " SAM64 \n"
#endif
        " ST 15,%0 \n"
        :"=m"(rc),"=m"(below2G->dsab)
        :"m"(below2G->plist),"m"(below2G->ddname),"m"(below2G->saveArea)
        :
#ifdef __XPLINK__
         "r13",
#endif
         "r0","r1","r14","r15");

  dsab = below2G->dsab;

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  if (rc == 0){
    return dsab;
  } else{
    return NULL;
  }
}

int dsabIsOMVS(DSAB *dsab){
  char flag3 = dsab->dsabflg3;
  char flag4 = dsab->dsabflg4;

  return (flag4 & DSABFLG4_DSABHIER);
}

char *getASCBJobname(ASCB *ascb){
  JSAB *jsab = getJSAB(ascb);
  if (jsab){
    return (char*)&(jsab->jsabjbnm[0]);
  } else{
    ASSB *assb = (ASSB*)ascb->ascbassb;
    if (assb->assbjbns[0]&0xff){
      return (char*)&(assb->assbjbns[0]);
    } else if (assb->assbjbni[0]&0xff){
      return (char*)&(assb->assbjbni[0]);
    } else{
      return NULL;
    }
  }
}

#ifdef METTLE
#pragma insert_asm(" CVT DSECT=YES,LIST=NO ")
#pragma insert_asm(" IEFJESCT ")
#else
void gen_dsects_only_os_c(void) { // Required for __asm invoked macros to be able to compile
  __asm(" CVT DSECT=YES,LIST=NO \n"
        " IEFJESCT \n"
        );
}
#endif



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

