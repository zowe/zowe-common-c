

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
#include "metalio.h"
#else
#include "stddef.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "idcams.h"
#include "logging.h"

#define TOSTR(number) #number
#define INT_TO_STR(number) TOSTR(number)

ZOWE_PRAGMA_PACK

/* System defined parameter lists, see "z/OS DFSMS Access Method Services
 * Commands - User I/O Routines" for details */

typedef struct IDCAMSIOList_tag {

  int groupCount;

  char * __ptr32 ddname1;
  void * __ptr32 ioRoutine1;
  void * __ptr32 userData1;

  char * __ptr32 ddname2;
  void * __ptr32 ioRoutine2;
  void * __ptr32 userData2;

} IDCAMSIOList;

typedef struct IDCAMSRecord_tag {
  void * __ptr32 recordAddress;
  int recordLength;
} IDCAMSRecord;

typedef struct ExitRoutineParms_tag {
  void * __ptr32 userData;
  char flag1;
  char flag2;
  short flag3;
  IDCAMSRecord * __ptr32 record;
} ExitRoutineParms;

/* Wrapper data structures */
struct IDCAMSCommand_tag {
  char eyecatcher[8];
#define IDCAMS_COMMAND_EYECATCHER "RSIDCCMD"
  struct IDCAMSCommandRecord_tag * __ptr32 firstRecord;
  struct IDCAMSCommandRecord_tag * __ptr32 lastRecord;
};

typedef struct IDCAMSCommandRecord_tag {
  char eyecatcher[8];
#define IDCAMS_COMMAND_RECORD_EYECATCHER "RSIDCCRC"
  struct IDCAMSCommandRecord_tag * __ptr32 next;
  char text[IDCAMS_SYSIN_EXIT_RECORD_SIZE];
} IDCAMSCommandRecord;

struct IDCAMSCommandOutput_tag {
  char eyecatcher[8];
#define IDCAMS_COMMAND_OUT_EYECATCHER "RSIDCOUT"
  struct IDCAMSOutputRecord_tag * __ptr32 firstRecord;
  struct IDCAMSOutputRecord_tag * __ptr32 lastRecord;
};

typedef struct IDCAMSOutputRecord_tag {
  char eyecatcher[8];
#define IDCAMS_OUTPUT_RECORD_EYECATCHER "RSIDCREC"
  unsigned int totalSize;
  struct IDCAMSOutputRecord_tag * __ptr32 next;
  unsigned int recordLength;
  char record[0];
} IDCAMSOutputRecord;

ZOWE_PRAGMA_PACK_RESET

static void *getSYSINUserExit() {

  void *exitAddress = NULL;

  __asm(
      "         LARL  %[exitAddress],SYSINEX                                   \n"
      "         J     SYSINRET                                                 \n"
      "SYSINEX  DS    0H                                                       \n"

      /*** EXIT ROUTINE START ***/

      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         BAKR  14,0                                                     \n"
      "         LARL  11,SYSINEX                                               \n"
      "         USING SYSINEX,11                                               \n"
      "         LR    2,1                                                      \n"
      "         USING UEXITPRM,2                                               \n"
      /* check control flags                                                    */
      "         L     10,IOFLGADR                                              \n"
      "         USING IOFLAGS,10                                               \n"
      "         CLI   IOFLAG1,IO@OPEN                                          \n"
      "         JE    UEXITOK              Open                                \n"
      "         CLI   IOFLAG1,IO@CLOS                                          \n"
      "         JE    UEXITOK              Close                               \n"
      "         CLI   IOFLAG1,IO@GET                                           \n"
      "         JE    RETRNREC             Get                                 \n"
      "         CLI   IOFLAG1,IO@PUT                                           \n"
      "         JE    UEXITERR             Put                                 \n"
      "         J     UEXITERR             Unknown request                     \n"
      /* return record                                                          */
      "RETRNREC DS    0H                                                       \n"
      "         L     3,USDATADR                                               \n"
      "         USING SYSINUDT,3                                               \n"
      /*     check if EODAT                                                     */
      "         ICM   4,B'1111',SIUDNRN    Load current record                 \n"
      "         JZ    UEXITEOD             Leave if NULL                       \n"
      /*     save the address of next record for future exit invocations        */
      "         USING CMDREC,4                                                 \n"
      "         MVC   SIUDNRN,CMDRNEXT                                         \n"
      /*     return record information to IDCAMS                                */
      "         L     5,IOINFADR                                               \n"
      "         USING IDCREC,5                                                 \n"
      /*     store record size                                                  */
      "         LA    6,"INT_TO_STR(IDCAMS_SYSIN_EXIT_RECORD_SIZE)"            \n"
      "         ST    6,RECSIZE                                                \n"
      /*     store record address                                               */
      "         LA    6,CMDRTEXT                                               \n"
      "         ST    6,RECADDR                                                \n"
      /* leave                                                                  */
      "UEXITOK  DS    0H                                                       \n"
      "         LA    15,0                 Success RC                          \n"
      "         J     UEXITRET                                                 \n"
      "UEXITEOD DS    0H                                                       \n"
      "         LA    15,4                 EODAT RC                            \n"
      "         J     UEXITRET                                                 \n"
      "UEXITERR DS    0H                                                       \n"
      "         LA    15,12                Error RC                            \n"
      "         J     UEXITRET                                                 \n"
      "UEXITRET EREG  0,1                                                      \n"
      "         PR                                                             \n"
      "UEXLTORG LTORG                                                          \n"
      "         POP   USING                                                    \n"

      /*** EXIT ROUTINE END ***/

      "SYSINRET DS    0H                                                       \n"
      : [exitAddress]"=r"(exitAddress)
      :
      :
  );

  return exitAddress;
}

static void *getSYSPRINTUserExit() {

  void *exitAddress = NULL;

  __asm(
      "         LARL  %[exitAddress],SYSPREX                                   \n"
      "         J     SYSPRRET                                                 \n"
      "SYSPREX  DS    0H                                                       \n"

      /*** EXIT ROUTINE START ***/

      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         BAKR  14,0                                                     \n"
      "         LARL  11,SYSPREX                                               \n"
      "         USING SYSPREX,11                                               \n"
      "         LR    2,1                                                      \n"
      "         USING UEXITPRM,2                                               \n"
      /* check control flags                                                    */
      "         L     10,IOFLGADR                                              \n"
      "         USING IOFLAGS,10                                               \n"
      "         CLI   IOFLAG1,IO@OPEN                                          \n"
      "         JE    SPEXITOK             Open                                \n"
      "         CLI   IOFLAG1,IO@CLOS                                          \n"
      "         JE    SPEXITOK             Close                               \n"
      "         CLI   IOFLAG1,IO@GET                                           \n"
      "         JE    SPEXTERR             Get                                 \n"
      "         CLI   IOFLAG1,IO@PUT                                           \n"
      "         JE    SAVEREC              Put                                 \n"
      "         J     SPEXTERR             Unknown request                     \n"
      /* return record                                                          */
      "SAVEREC  DS    0H                                                       \n"
      "         L     3,USDATADR                                               \n"
      "         USING CMDOUT,3                                                 \n"
      "         LTR   3,3                                                      \n"
      "         JZ    SPEXITOK                                                 \n"
      "         L     4,IOINFADR                                               \n"
      "         USING IDCREC,4                                                 \n"
      /*     allocate a new node                                                */
      "         LA    5,OREC@LEN                                               \n"
      "         A     5,RECSIZE                                                \n"
      "         STORAGE OBTAIN,LENGTH=(5),SP=132,LOC=31,CALLRKY=YES            \n"
      /*     init the new record                                                */
      "         USING OUTREC,1                                                 \n"
      "         MVC   ORECEYE(8),=C'"IDCAMS_OUTPUT_RECORD_EYECATCHER"'         \n"
      "         ST    5,ORECTLEN                                               \n"
      "         MVC   ORECNEXT,=F'0'                                           \n"
      "         MVC   ORECRLEN,RECSIZE                                         \n"
      "         L     5,RECSIZE                                                \n"
      "         BCTR  5,0                                                      \n"
      "         L     6,RECADDR                                                \n"
      "         EX    5,CPYVLREC                                               \n"
      "         J     ADDREC01                                                 \n"
      "CPYVLREC DS    0H                                                       \n"
      "         MVC   ORECVAL(0),0(6)                                          \n"
      /*     add the new record to the list                                     */
      "ADDREC01 DS    0H                                                       \n"
      "         ICM   5,15,CMDOFRST                                            \n"
      "         BNZ   ADDREC02                                                 \n"
      "         ST    1,CMDOFRST                                               \n"
      "         ST    1,CMDOLAST                                               \n"
      "         J     SPEXITOK                                                 \n"
      "ADDREC02 DS    0H                                                       \n"
      "         LR    5,1                                                      \n"
      "         L     1,CMDOLAST                                               \n"
      "         ST    5,CMDOLAST                                               \n"
      "         ST    5,ORECNEXT                                               \n"
      /* leave                                                                  */
      "SPEXITOK DS    0H                                                       \n"
      "         LA    15,0                 Success RC                          \n"
      "         J     SPEXTRET                                                 \n"
      "SPEXTEOD DS    0H                                                       \n"
      "         LA    15,4                 EODAT RC                            \n"
      "         J     SPEXTRET                                                 \n"
      "SPEXTERR DS    0H                                                       \n"
      "         LA    15,12                Error RC                            \n"
      "         J     SPEXTRET                                                 \n"
      "SPEXTRET EREG  0,1                                                      \n"
      "         PR                                                             \n"
      "SPLTORG  LTORG                                                          \n"
      "         POP   USING                                                    \n"

      /*** EXIT ROUTINE END ***/

      "SYSPRRET DS    0H                                                       \n"
      : [exitAddress]"=r"(exitAddress)
      :
      :
  );

  return exitAddress;
}

IDCAMSCommand *idcamsCreateCommand() {

  IDCAMSCommand *cmd =
      (IDCAMSCommand *)safeMalloc31(sizeof(IDCAMSCommand), "IDCAMSCommand");
  if (cmd == NULL) {
    return NULL;
  }

  memset(cmd, 0, sizeof(IDCAMSCommand));
  memcpy(cmd->eyecatcher, IDCAMS_COMMAND_EYECATCHER, sizeof(cmd->eyecatcher));

  return cmd;
}

int idcamsAddLineToCommand(IDCAMSCommand *cmd, const char *line) {

  int rc = RC_IDCAMS_OK;

  size_t lineSize = strlen(line);
  if (lineSize > IDCAMS_SYSIN_EXIT_RECORD_SIZE) {
    lineSize = IDCAMS_SYSIN_EXIT_RECORD_SIZE;
    rc = RC_IDCAMS_LINE_TOO_LONG;
  }

  IDCAMSCommandRecord *newRecord =
      (IDCAMSCommandRecord *)safeMalloc31(sizeof(IDCAMSCommandRecord),
                                          "IDCAMSCommandRecord");
  if (newRecord == NULL) {
    return RC_IDCAMS_ALLOC_FAILED;
  }

  memset(newRecord, 0, sizeof(IDCAMSCommandRecord));
  memcpy(newRecord->eyecatcher, IDCAMS_COMMAND_RECORD_EYECATCHER,
         sizeof(newRecord->eyecatcher));
  memset(newRecord->text, ' ', sizeof(newRecord->text));
  memcpy(newRecord->text, line, lineSize);

  if (cmd->firstRecord == NULL) {
    cmd->firstRecord = newRecord;
    cmd->lastRecord = newRecord;
  } else {
    cmd->lastRecord->next = newRecord;
    cmd->lastRecord = newRecord;
  }

  return rc;
}

static IDCAMSCommandOutput *idcamsCreateCommandOutput();

typedef __packed struct SYSINExitUserData_tag {
  char eyecatcher[8];
#define SYSIN_EXIT_CNTX_EYECATCHER "RSSIXUDT"
  const IDCAMSCommand * __ptr32 cmd;
  const IDCAMSCommandRecord * __ptr32 recordToReturn;
} SYSINExitUserData;

typedef IDCAMSCommandOutput SYSPRINTExitUserData;

static void initSYSINExitContext(SYSINExitUserData *cntx,
                                 const IDCAMSCommand *cmd) {

  memset(cntx, 0, sizeof(SYSINExitUserData));
  memcpy(cntx->eyecatcher, SYSIN_EXIT_CNTX_EYECATCHER, sizeof(cntx->eyecatcher));
  cntx->cmd = cmd;
  cntx->recordToReturn = cmd->firstRecord;

}

static void initIOList(IDCAMSIOList *ioList,
                       SYSINExitUserData *sysinUserData,
                       SYSPRINTExitUserData *sysprintUserData) {

  ioList->groupCount = 2;
  ioList->ddname1 = "DDSYSIN   ";
  ioList->ioRoutine1 = getSYSINUserExit();
  ioList->userData1 = sysinUserData;
  ioList->ddname2 = "DDSYSPRINT";
  ioList->ioRoutine2 = getSYSPRINTUserExit();
  ioList->userData2 = sysprintUserData;

}

int idcamsExecuteCommand(const IDCAMSCommand *cmd,
                         IDCAMSCommandOutput **output,
                         int *reasonCode) {

  IDCAMSCommandOutput *sysprintOutput = NULL;
  if (output != NULL) {
    sysprintOutput = idcamsCreateCommandOutput();
    if (sysprintOutput == NULL) {
      return RC_IDCAMS_ALLOC_FAILED;
    }
    *output = sysprintOutput;
  }

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      SYSINExitUserData sysinUserData;
      IDCAMSIOList ioList;
      char macroParmList[12];
      char idcamsParmList[16];
      char standardSaveArea[72];
    )
  );

  char *moduleName = "IDCAMS  ";
  int idcamsRC = 0;
  char callerRegs[128];

  SYSINExitUserData *sysinUserData = &below2G->sysinUserData;
  initSYSINExitContext(sysinUserData, cmd);
  SYSPRINTExitUserData *sysprintUserData = sysprintOutput;

  IDCAMSIOList *ioList = &below2G->ioList;
  initIOList(ioList, sysinUserData, sysprintUserData);

  __asm(
      ASM_PREFIX
      "         STMG  14,13,0(%[callerRegs])                                   \n"
      "         LA    13,0(%[saveArea])                                        \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         LINK  EPLOC=(%[moduleName])"
      ",PARAM=(=H'0',=H'0',=H'0',(%[ioList])),VL=1"
      ",MF=(E,(%[idcamsParmList]))"
      ",SF=(E,(%[macroParmList]))                                              \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         STG   15,8(%[callerRegs])                                      \n"
      "         LMG   14,13,0(%[callerRegs])                                   \n"
      "         ST    15,%[idcamsRC]                                           \n"
      : [idcamsRC]"=m"(idcamsRC)
      : [callerRegs]"r"(callerRegs),
        [saveArea]"r"(&below2G->standardSaveArea),
        [moduleName]"r"(moduleName),
        [ioList]"r"(ioList),
        [idcamsParmList]"r"(&below2G->idcamsParmList),
        [macroParmList]"r"(&below2G->macroParmList)
      : "r0", "r1", "r15"
  );

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
  below2G = NULL;

  if (idcamsRC > 0) {
    if (reasonCode != NULL) {
      *reasonCode = idcamsRC;
    }
    return RC_IDCAMS_CALL_FAILED;
  }

  return RC_IDCAMS_OK;
}

void idcamsDeleteCommand(IDCAMSCommand *cmd) {

  if (cmd == NULL) {
    return;
  }

  IDCAMSCommandRecord *currRecord = cmd->firstRecord;
  while (currRecord != NULL) {
    IDCAMSCommandRecord *nextRecord = currRecord->next;
    safeFree31((char *)currRecord, sizeof(IDCAMSCommandRecord));
    currRecord = nextRecord;
  }

  safeFree31((char *)cmd, sizeof(IDCAMSCommand));
  cmd = NULL;

}

void idcamsPrintCommandOutput(const IDCAMSCommandOutput *idcamsOutput) {

  if (idcamsOutput == NULL) {
    return;
  }

  IDCAMSOutputRecord *currentNode = idcamsOutput->firstRecord;
  while (currentNode != NULL) {
    zowelog(NULL, LOG_COMP_UTILS, ZOWE_LOG_INFO, "%.*s\n", currentNode->recordLength, currentNode->record);
    currentNode = currentNode->next;
  }

}

static IDCAMSCommandOutput *idcamsCreateCommandOutput() {

  IDCAMSCommandOutput *output =
      (IDCAMSCommandOutput *)safeMalloc31(sizeof(IDCAMSCommandOutput),
                                          "IDCAMSCommandOutput");
  if (output == NULL) {
    return NULL;
  }

  memset(output, 0, sizeof(IDCAMSCommandOutput));
  memcpy(output->eyecatcher, IDCAMS_COMMAND_OUT_EYECATCHER,
         sizeof(output->eyecatcher));

  return output;
}

/*
 * Free storage allocated in the SYSPRINT exit.
 *
 * Since the HLASM code may change in the exit (different subpool or key)
 * we can't rely on safeFree31 and have to use this ad hoc function.
 *
 */
static void freeSYSPRINTExitStorage(void *data, unsigned int size) {

  __asm(
      ASM_PREFIX
      "         STORAGE RELEASE"
      ",COND=NO"
      ",LENGTH=(%[size])"
      ",SP=132"
      ",ADDR=(%[storageAddr])"
      ",CALLRKY=YES"
      ",LINKAGE=SYSTEM                                                         \n"
      :
      : [size]"NR:r0"(size), [storageAddr]"r"(data)
      : "r0", "r1", "r14", "r15"
  );

}

void idcamsDeleteCommandOutput(IDCAMSCommandOutput *idcamsOutput) {

  if (idcamsOutput == NULL) {
    return;
  }

  IDCAMSOutputRecord *currentRecord = idcamsOutput->firstRecord;
  while (currentRecord != NULL) {
    IDCAMSOutputRecord *nextRecord = currentRecord->next;
    freeSYSPRINTExitStorage(currentRecord, currentRecord->totalSize);
    currentRecord = nextRecord;
  }

  safeFree31((char *)idcamsOutput, sizeof(IDCAMSCommandOutput));
  idcamsOutput = NULL;

}

#define idcamsDSECTs IDCDSECT
void idcamsDSECTs() {

  /* SYSPRINT specific and common DSECTs for the SYSIN and SYSPRINT exits */

  __asm(

      "         GBLC   &CSECTNM            Global variable for current CSECT   \n"
      "         MACRO                                                          \n"
      "         CSECTNM ,                  Macro to set current CSECT          \n"
      "         GBLC   &CSECTNM                                                \n"
      "&CSECTNM SETC   '&SYSECT'                                               \n"
      "         MEND                                                           \n"
      "         CSECTNM ,                  Get current CSECT                   \n"

      /*** System-defined DSECTs ***/

      /* Input/output record, area is provided by IDCAMS (IDCAMSRecord) */
      "IDCREC   DSECT ,                                                        \n"
      "RECADDR  DS    A                                                        \n"
      "RECSIZE  DS    F                                                        \n"
      "REC@LEN  EQU   *-IDCREC                                                 \n"
      "         EJECT ,                                                        \n"

      /* Input parameters passed to user exit (ExitRoutineParms) */
      "UEXITPRM DSECT ,                                                        \n"
      "USDATADR DS    A                                                        \n"
      "IOFLGADR DS    A                                                        \n"
      "IOINFADR DS    A                                                        \n"
      "PRM@LEN  EQU   *-UEXITPRM                                               \n"
      "         EJECT ,                                                        \n"

      /* Control flags passed to exits */
      "IOFLAGS  DSECT ,                                                        \n"
      "IOFLAG1  DS    X                                                        \n"
      "IO@OPEN  EQU   X'00'                                                    \n"
      "IO@CLOS  EQU   X'04'                                                    \n"
      "IO@GET   EQU   X'08'                                                    \n"
      "IO@PUT   EQU   X'0C'                                                    \n"
      "IOFLAG2  DS    X                                                        \n"
      "IO@INPT  EQU   X'80'                                                    \n"
      "IO@OUTP  EQU   X'40'                                                    \n"
      "IO@DDNA  EQU   X'20'                                                    \n"
      "IO@DSNA  EQU   X'10'                                                    \n"
      "IOMSGSN  DS    H                                                        \n"
      "FLG@LEN  EQU   *-IOFLAGS                                                \n"
      "         EJECT ,                                                        \n"

      /*** Wrapper DSECTs ***/

      /* User data passed to SYSIN exit (SYSINExitUserData) */
      "SYSINUDT DSECT ,                                                        \n"
      "SIUDEYE  DS    CL8                                                      \n"
      "SIUDCMD  DS    A                                                        \n"
      "SIUDNRN  DS    A                                                        \n"
      "SIUD@LEN EQU   *-SYSINUDT                                               \n"
      "         EJECT ,                                                        \n"

      /* Command results (IDCAMSCommandOutput aka SYSPRINTExitUserData) */
      "CMDOUT   DSECT ,                                                        \n"
      "CMDOEYE  DS    CL8                                                      \n"
      "CMDOFRST DS    A                                                        \n"
      "CMDOLAST DS    A                                                        \n"
      "DL@LEN   EQU   *-CMDOUT                                                 \n"
      "         EJECT ,                                                        \n"

      /* Command result record (IDCAMSOutputRecord) */
      "OUTREC   DSECT ,                                                        \n"
      "ORECEYE  DS    CL8                                                      \n"
      "ORECTLEN DS    F                                                        \n"
      "ORECNEXT DS    A                                                        \n"
      "ORECRLEN DS    F                                                        \n"
      "ORECVAL  DS    0C                                                       \n"
      "OREC@LEN EQU   *-OUTREC                                                 \n"
      "         EJECT ,                                                        \n"

      /* IDCAMS command record (IDCAMSCommandRecord) */
      "CMDREC   DSECT ,                                                        \n"
      "CMDREYE  DS    CL8                                                      \n"
      "CMDRNEXT DS    A                                                        \n"
      "CMDRTEXT DS    CL"INT_TO_STR(IDCAMS_SYSIN_EXIT_RECORD_SIZE)"            \n"
      "CMDR@LEN EQU   *-CMDREC                                                 \n"
      "         EJECT ,                                                        \n"

#ifdef METTLE
      "&CSECTNM CSECT ,                                                        \n"
#endif

  );

}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

