

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __METALIO__
#define __METALIO__ 1

struct WTO_PARM{
  unsigned short len;
  unsigned short code;
  char text[126]; /* Maximum allows single line WTO text length */
};

/* 
             0           Length of reply buffer, if for a 31-bit WTOR. Otherwise zero.
             1           Message length plus four if text is inline, fixed length if bytes
                         4-11 contain a pointer to a data area containing the message text.
             2           MCS flag byte, bit settings are:
                         1...      ....      Routing and descriptor codes are present.
                         .1..      ....      Reserved.
                         ..1.      ....      WTO is an immediate command response.
                         ...1      ....      Message type field exists.
                         ....      1...      WTO reply to a WTOR macro instruction.
                         ....      .1..      Message should be broadcast to all active
                                             consoles.
                         ....      ..1.      Message queued for hard copy only.
                         ....      ...1      Reserved.
             3           Second MCS flag byte: bit settings are:
                         1...      ....      Do not timestamp this message.
                         .1..      ....      Message is a multiline WTO.
                         ..1.      ....      Primary subsystem use only. JES3: Do not log
                                             minor WQEs if major WQE is not hardcopied. JES2:
                                             not used.
                         ...1      ....      Extended WPL format (WPX) exists.
                         ....      1...      Message is an operator command.
                         ....      .1..      Message should not be queued to hardcopy.
                         ....      ..1.      Message reissued via WQEBLK keyword.
                         ....      ...1      Reserved.
             4-n         The message text, normally the message ID, or a pointer to a data
                         area containing the message text. The message text can be of
                         variable length, but if a pointer is specified it will always occupy
                         4 bytes.
   */


#define WTO_HAS_ROUTE_AND_DESC 0x80
#define WTO_IS_IMMEDIATE_COMMAND_RESPONSE 0x20
#define WTO_MESSAGE_TYPE_FIELD_EXISTS 0x10
#define WTO_REPLY_TO_WTOR 0x08
#define WTO_BROADCAST 0x04
#define WTO_HARDCOPY  0x02

#define WTO_DO_NOT_TIMESTAMP 0x80
#define WTO_IS_MULTILINE     0x40
#define WTO_EXTENDED_WPL     0x10
#define WTO_IS_OPERATOR_COMMAND 0x08
#define WTO_NO_Q_TO_HARDCOPY 0x04
#define WTO_REISSUED_BY_WQEBLK 0x02

typedef struct WTORPrefix31_tag{
  char *replyBufferAddress; /* high bit must be set */
  char *replayECBAddress;
} WTORPrefix31;

#define WTO_ROUTE_CODE_OPERATOR_ACTION 1
#define WTO_ROUTE_CODE_OPERATOR_INFORMATION 2
#define WTO_ROUTE_CODE_TAPE_POOL 3
#define WTO_ROUTE_CODE_DIRECT_ACCESS_POOL 4
#define WTO_ROUTE_CODE_TAPE_LIBRARY 5
#define WTO_ROUTE_CODE_DISK_LIBRARY 6
#define WTO_ROUTE_CODE_UNIT_RECORD_POOL 7
#define WTO_ROUTE_CODE_TELEPROCESSING_CONTROL 8
#define WTO_ROUTE_CODE_SYSTEM_SECURITY 9
#define WTO_ROUTE_CODE_SYSTEM_ERROR 10
#define WTO_ROUTE_CODE_PROGRAMMER 11
#define WTO_ROUTE_CODE_EMULATOR 12

/* mutually exclusive? ask Steve Bice  */
#define WTO_DESC_CODE_SYSTEM_FAILURE 1
#define WTO_DESC_CODE_IMMEDIATE_ACTION 2
#define WTO_DESC_CODE_EVENTUAL_ACTION 3
#define WTO_DESC_CODE_SYSTEM_STATUS 4
#define WTO_DESC_CODE_IMMEDIATE_COMMAND_RESPONSE 5
#define WTO_DESC_CODE_JOB_STATUS 6

#define WTO_DESC_CODE_TASK_RELATED 7
#define WTO_DESC_CODE_OUT_OF_LINE  8
#define WTO_DESC_CODE_OPERATORS_REQUEST 9
#define WTO_DESC_CODE_NOT_DEFINED 10
#define WTO_DESC_CODE_CRITICAL_EVENTUAL_ACTION_REQUIRED 11
#define WTO_DESC_CODE_IMPORTANT_INFORMATION 12
#define WTO_DESC_CODE_AUTOMATION_INFORMATION 13

/* NON-WPX layout 

   COMMON
   messageData
   WPLFlags
   */

typedef struct WPLFlags_tag{
  unsigned short descriptorCode;
  unsigned short routingCode;
} WPLFlags;


typedef int ntFunction();

#define NT_CREATE   1
#define NT_RETRIEVE 2
#define NT_DELETE   3

#define NT_TASK_LEVEL    1
#define NT_HOME_LEVEL    2
#define NT_PRIMARY_LEVEL 3
#define NT_SYSTEM_LEVEL  4

#define NT_NOPERSIST 0
#define NT_PERSIST   1

#define NT_NOCHECKPOINT 0
#define NT_CHECKPOINTOK 2

#ifndef __LONGNAME__
#define getNameTokenFunctionTable GTNMTKFT
#define getNameTokenValue         GTNMTKVL
#define createNameTokenPair       CRNMTKPR
#define deleteNameTokenPair       DLNMTKPR
#define initSysouts              INITSYTS
#endif

ntFunction **getNameTokenFunctionTable();
int getNameTokenValue(int level, char *name, char *token);
int createNameTokenPair(int level, char *name, char *token);
int deleteNameTokenPair(int level, char *name);

typedef struct SYSOUT_struct{
  char * __ptr32 dcb;
  char * __ptr32 buffer;
  int  length;
  int  position;
  char ddname[8];
  char CC;
  char recfm;
} SYSOUT;

#define EBCDIC_NEWLINE 0x15

#ifndef __LONGNAME__
#define getSYSOUTStruct GTSYSOUT
#define wtoPrintf WTOPRNF
#define authWTOPrintf AWTOPRNF
#define sendWTO SENDWTO
#define qsamPrintf QSAMPRNF
#define printf PRINTF
#define fflush FFLUSH
#define setjmp SETJMP
#define longjmp LONGJMP
#endif

SYSOUT *getSYSOUTStruct(char *ddname, SYSOUT *existingSysout, char *buffer);

void sendWTO(int descriptorCode, int routingCode, char *message, int length);
void authWTOPrintf(char *formatString, ...);
void wtoPrintf(char *formatString, ...);
void qsamPrintf(char *formatString, ...);

#ifdef METTLE
#define EOF -1
void printf(char *formatString, ...);
void fflush(int fd);

#define stdout 1
#define stderr 2
#endif

typedef char jmp_buf[18*sizeof(void*)];

/*     ILP32                      LP64
   0    return address      0      return address
   4    return value        8      return value
   8-72 register saves      16-144 register saves
*/


int setjmp(jmp_buf env);

void longjmp(jmp_buf env, int value);


#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

