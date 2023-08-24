

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __METALIO__
#define __METALIO__ 1

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

