

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

// The default for SYSOUT DCBs is now RECFM=VBA and LRECL=207.  These
// attributes will be overriden by any DD or preexisting dataset
// attributes when the DCB is opened.
//
// To change the default record format, if not overridden by any DD or
// preexisting dataset attributes, compile with the option 
// -DSYSOUT_RECFM=DCB_RECFM_x
//
// To change the default record length, if not overridden by any DD or
// preexisting dataset attributes, compile with the option 
// -DSYSOUT_LRECL=n
//
// To use the original record format, record length, and disable
// the ability to override the DCB attributes, compile with the
// option -DBASIC_SYSOUT_DCB
//
// The handling of \n in a format string has changed.  Only a \n at the
// end of a format string will be recognized and cause a write to occur
// (prior to this, imbedded \n's would cause a write to occur but would
// *not* cause any subsequent text in the format string to start at the
// beginning of the next line, as expected).  This \n will be removed from
// the data that is written (prior to this, the \n and the final \0 would
// be written in the fixed length output).  To go back to the prior
// behavior, compile with the option -DKEEP_NL_AND_NULL
//

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>
#include <metal/stdio.h>
#else
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#endif

#include "zowetypes.h"
#include "qsam.h"
#include "metalio.h"
#include "alloc.h"

static int isopen(void * dcbptr) {

  if (((dcbSAMwithPlist *)dcbptr)->dcb.Common.openFlags&DCB_OPENFLAGS_OPEN)
    return 1;
  else
    return 0;
}

/* Deleted static function isAllocated - never referenced         */

/* Deleted external function copySTCK - code nevered worked As
   written, the code would attempt to perform the STCK to address
   '1', which will always fail because it is a protected PSA
   address and also is not doubleword aligned as required by
   STCK.  Also, not defined in metalio.h for use elsewhere.       */

/* CSR Table CVTCSRT 

   IEACSTBL - is the real name

   x'220' on the CVT

   0x14 = 20  Name/Token Services (and other stuff)
           4  IEANTCR
           8  IEANTRT
           C  IEANTDL

          58  IEAVPSE
          5C  IEAVRLS 
          60  IEAVRLS

          7C  IEAN4CR
          80  IEAN4RT
          84  IEAN4DL
   0x18 = 24  CSR SLOTS FOR UNIX    
       BPX Routines
   0x1C = 28  ISG (Global Serialization Services)
           4  OBTAIN
           8  ISGLRELS
           C  ISGLOBTF
          10  ISGLRELF
          14  ISGLSLSR
          18  ISGLCRTG
          1C  ISGLOBTG
          20  ISGLRELG
          24  ISGLPRGG
          28  ISGLPBAG
          2C  ISGLIDS 
          
   0x20 = 32  ASRGLTAB
   0x24 = 36  TBCTS1-APPC/MVS[ CALLABLE SERVICE ROUTER
   0x28 = 40  RACF/SAF
   0x2C = 44  ATRRCSTB  RRS - Recovery Services
   0x30 = 48  CSRSI ???
   0x34 = 52  CSRUNH01 ??
   0x38 = 56  <NULL>
   0x3C = 60  Support for Unicode Using Unicode Services
       Interface description Decimal offset
       Character conversion 172
       Case conversion 180
       Normalization 212
       Collation 228
   0x40 = 64  EZBRECST - DNS resolution??  EZBRECST EZBRERSN EZBRESRV
   0x44 = 68  More IRR/SAF/RACF stuff
   0x48 = 72  XML Services (GXL)
   0x4C = 76  HWI - Hardware management console stuff HWICSTBL


   248 0xF8  
     saf vector table
   256 0x100 on the CVT
     AMCBS (VSAM routines)


   BPX1MPC decimal 408 (from 0x18 off CSR)
   
   GXL offsets (from 0x48 off of CSR)

   GXL1INI  31-bit parser initialization 10
   GXL1PRS  31-bit parse 14
   GXL1TRM  31-bit parser termination 18
   GXL1CTL  31-bit parser control operation 1C
   GXL1QXD  31-bit query XML document 20
   GXL4INI  64-bit parser initialization 28
   GXL4PRS  64-bit parse 30
   GXL4TRM  64-bit parser termination 38
   GXL4CTL  64-bit parser control operation 40
   GXL4QXD  64-bit query XML document 48
   
   */


static ntFunction *getNameTokenFunction(int selector){

  /* When calling in AMODE 31-
       31 bit pointer to CVT is at address 16
       Use IEANTxx routines
       Called in AMODE 31
       Routine addresses start at offset 4 in the function table
       Parameters must be below 2G
       Parameter list built with 31 bit pointers
     When calling in AMODE 64-
       64 bit pointer to CVT is at address 72
       Use IEAN4xx routines
       Called in AMODE 64
       Routine addresses start at offset 124 in the function table
       Parameters may be below or above 2G
       Parameter list built with 64 bit pointers

     Generation of correct code is based on if _LP64 is or is not
     defined, which causes a different CVT pointer offset and
     function table selection index to be used.
  */

#ifndef _LP64
#define CVTPTR 16
#define SELBASE
#else
#define CVTPTR 72
#define SELBASE 30 +
#endif

  /* Use structures rather than casting and addition of offsets to
     avoid errors in the lookup logic.
  */

  struct {
   char rsv0[0x220];
   void * __ptr32 csrt;
  } *cvt = (void*)(*((void**)CVTPTR));

  struct {
   char rsv0[0x14];
   void * __ptr32 ntft;
  } *csrt = cvt->csrt;

  struct {
   void * __ptr32 func[100];
  } *ntft = csrt->ntft;

  void *fptr = ntft->func[SELBASE selector];

  return (ntFunction*)fptr;

  /*  AMODE 31
    L     R15,X'04'(,R15)  CREATE
    L     R15,X'08'(,R15)  RETRIEVE     
    L     R15,X'0C'(,R15)  DELETE
      AMODE 64
    L     R15,X'7C'(,R15)  CREATE
    L     R15,X'80'(,R15)  RETRIEVE
    L     R15,X'84'(,R15)  DELETE
  */

#undef CVTPTR
#undef SELBASE
}



void wtoPrintf(char *formatString, ...);

int getNameTokenValue(int level, char *name, char *token){
  int persopt = NT_NOPERSIST; /* no persist */
  int retcode;

  ntFunction *ntFunction = getNameTokenFunction(NT_RETRIEVE);

  (ntFunction)(&level,name,token,&retcode);

  return retcode;
}

int createNameTokenPair(int level, char *name, char *token){
  int persopt = NT_NOPERSIST; /* no persist */
  int retcode;

  ntFunction *ntFunction = getNameTokenFunction(NT_CREATE);

  (ntFunction)(&level,name,token,&persopt,&retcode);

  return retcode;
}

/* External function deleteNameTokenPair - code never referenced in metalio.c
   and is not defined in metalio.h for use elsewhere.
*/
int deleteNameTokenPair(int level, char *name){
  int persopt = NT_NOPERSIST; /* no persist */
  int retcode;

  ntFunction *ntFunction = getNameTokenFunction(NT_DELETE);

  (ntFunction)(&level,name,&retcode);

  return retcode;
}

/* ddname padded to 8!!! */

#ifdef WRITSTAT
#define MAX_SYSOUT_COUNT 10
static sysoutCount = 0;
static SYSOUT *sysouts[MAX_SYSOUT_COUNT];

/* External function initSysouts - code never referenced in metalio.c
   and is not defined in metalio.h for use elsewhere.
*/
void initSysouts(){
  int i;
  for (i=0; i<MAX_SYSOUT_COUNT; i++){
    sysouts[i] = NULL;
  }
  sysoutCount = 0;
}
#endif

#ifndef BASIC_SYSOUT_DCB
#ifndef SYSOUT_RECFM
// Use RECFM=VBA - will be changed to VA when writing to JES
#define SYSOUT_RECFM DCB_RECFM_VBA
#endif
#ifndef SYSOUT_LRECL
// Allow 204 characters, plus one byte for CC and 4 bytes for RDW
#define SYSOUT_LRECL 207
#endif
// -2 allows for the DCB to be overridden, disabling blocking when writing to JES
#define SYSOUT_BLKSIZE -2
#else
#define SYSOUT_RECFM DCB_RECFM_FBA
#define SYSOUT_LRECL 133
#define SYSOUT_BLKSIZE 133
#endif

SYSOUT *getSYSOUTStruct(char *ddname, SYSOUT *existingSysout, char *buffer){
  SYSOUT *sysout = NULL;

#ifdef WRITSTAT
  int i;
  for (i=0; i<sysoutCount; i++){
    /* Check for sysouts[i] being non-0 should be unnecessary,
       as entries up to sysoutCount will always be set to a non-0 value. */
    if (sysouts[i] && !memcmp(sysouts[i]->ddname,ddname,8)){
      sysout = sysouts[i];
      break;
    }
  }
#else
  char name[16];
  union {
    char token[16];
    SYSOUT* sysouttoken;
  };

  memcpy(name,"MXXPRNTF",8);
  memcpy(name+8,ddname,8);
  if (!getNameTokenValue(NT_TASK_LEVEL,name,token)){
    sysout = sysouttoken;
  }
#endif

  if (!sysout) {

    /* When using writable static, there is no check for MAX_SYSOUT_COUNT having been reached */

    sysout = (SYSOUT*)safeMalloc31(sizeof(SYSOUT),"SYSOUT control block");
    memset(sysout,0,sizeof(SYSOUT));

    memcpy(sysout->ddname,ddname,8);
    char *dcb = openSAM(ddname,
                        OPEN_CLOSE_OUTPUT,
                        1,
                        SYSOUT_RECFM,
                        SYSOUT_LRECL,
                        SYSOUT_BLKSIZE);
      /* wtoPrintf("ZL printf INIT %s dcb = 0x%x",ddname,dcb);
         common = (struct dcbCommon *)
           (dcb+4+DCB_COMMON_OFFSET);
         wtoPrintf("ZL printf INIT SYSOUT %s DCB %x OFLAGS %x\n",
           ddname,common,common->openFlags); */

    sysout->dcb = dcb;

    if (isopen(dcb)){
      sysout->buffer = makeQSAMBuffer(dcb,&sysout->length); /* Buffer includes extra byte for a trailing null - not included in the length */

      int recfm = getSAMRecfm(dcb);

      /* If CC is x00, output will begin at offset 0.
         If CC is not null, it contains the proper CC value
         (ANSI or machine) to be placed at offset 0 and output will
         then begin at offset 1.                                     */

      if (recfm&DCB_RECFM_A)
        sysout->CC = ' ';    /* Use blank for ANSI CC */
      else if (recfm&DCB_RECFM_M)
        sysout->CC = 0x09;   /* Use X09 for machine CC */
      else
        sysout->CC = 0;      /* CC 0 for no CC 0 */

      if ((recfm&DCB_RECFM_FORMAT)==DCB_RECFM_V)
        sysout->recfm = 'V';
      else if ((recfm&DCB_RECFM_FORMAT)==DCB_RECFM_F)
        sysout->recfm = 'F';
      else
        sysout->recfm = 'U';
    }

#ifdef WRITSTAT
    sysouts[sysoutCount] = sysout;
    sysoutCount++;
#else
    sysouttoken = sysout;
    createNameTokenPair(NT_TASK_LEVEL,name,token);
#endif
  }

  return sysout;
}

/* External function message - is not defined in metalio.h for use
   elsewhere.
*/
void message(char *message){

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      WTOCommon31 common;
      char text[126];          /* Maximum length of WTO text is 126 - ABEND D23-xxxx0005 if longer than 126 */
    )
  );

  int len = strlen(message);
  if (len>sizeof(below2G->text))
    len=sizeof(below2G->text);

  below2G->common.length = len+sizeof(below2G->common); /* +4 for header */
  memcpy(below2G->text,message,len);

  __asm(ASM_PREFIX
        " WTO MF=(E,(%[wtobuf])) \n"
        :
        :[wtobuf]"NR:r1"(&below2G->common)
        :"r0","r1","r15");

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
}

/* this can only be called from authorized callers */
static void authWTOMessage(char *message){

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      WTOCommon31 common;
      char text[126];          /* Maximum length of WTO text is 126 - ABEND D23-xxxx0005 if longer than 126 */
    )
  );

  int len = strlen(message);
  if (len>sizeof(below2G->text))
    len=sizeof(below2G->text);

  below2G->common.length = len+sizeof(below2G->common); /* +4 for header */
  memcpy(below2G->text,message,len);

  __asm(ASM_PREFIX
#ifdef _LP64
        "         SAM31                                                          \n"
        "         SYSSTATE AMODE64=NO                                            \n"
#endif
        " WTO LINKAGE=BRANCH,MF=(E,(%[wtobuf])) \n"
#ifdef _LP64
        "         SAM64                                                          \n"
        "         SYSSTATE AMODE64=YES                                           \n"
#endif
        :
        :[wtobuf]"NR:r1"(&below2G->common)
        :"r0","r1","r15");

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
}

/* If descriptorCode and routingCode are both 0, then indicate to use system defaults */

void sendWTO(int descriptorCode, int routingCode, char *message, int length){
  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      WTOCommon31 common;
      char text[126];          /* Maximum length of WTO text is 126 - ABEND D23-xxxx0005 if longer than 126 */
      WPLFlags wplspace;       /* Allocate space, actual location depends on the text length */
    )
  );

  int len = strlen(message);
  if (len>sizeof(below2G->text))
    len=sizeof(below2G->text);

  /* below2G->common.replyBufferLength = 0 - storage was initialized to x'00' */
  below2G->common.length = len+sizeof(below2G->common); /* +4 for header */
  /* below2G->common.mcsFlags1 = 0 - storage was initialized to x'00' */
  /* below2G->common.mcsFlags2 = 0 - storage was initialized to x'00' */
  if ((descriptorCode != 0) || (routingCode != 0)){
    below2G->common.mcsFlags1 = WTO_HAS_ROUTE_AND_DESC;

    WPLFlags *wplFlags = (WPLFlags*)(&below2G->text+len);  /* WPL immediately follows the WTO text */
    wplFlags->descriptorCode = (unsigned short)(0x10000>>descriptorCode);
    wplFlags->routingCode = (unsigned short)(0x1000>>routingCode);
  }
  memcpy(below2G->text,message,len);
  /* printf("wto buffer (at 0x%x) is exactly (desc=%d, route=%d):\n",wtoBuffer,descriptorCode,routingCode);
  dumpbuffer(wtoBuffer,bufferLength);
     */
  /* Note: R0 only needs to be set prior to a multi-line WTO issued by an authorized program */
  __asm(ASM_PREFIX
        " WTO MF=(E,(%[wtobuf])) \n"
        :
        :[wtobuf]"NR:r1"(&below2G->common)
        :"r0","r1","r15");

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
}

#define WTO_MAX_SIZE 126
void wtoPrintf(char *formatString, ...){
  char text[WTO_MAX_SIZE+1];       /* Allow for trailing null character */
  va_list argPointer;
  int cnt;

  for (int pass=0; pass<2; pass++){

    /* The resulting text string from vsnprintf is unpredictable if
       there is an error in the format string or arguments.  In that
       case we will set the output text area to null, repeat the
       vsnprintf, and then find the length of the null terminated
       string.  This avoids initializing the output text area prior
       to every successful request.
    */

    va_start(argPointer,formatString);
    cnt = vsnprintf(text,sizeof(text),formatString,argPointer);
    va_end(argPointer);

    if (cnt<0){
      if (pass==0)
        memset(text,0,sizeof(text));  /* Clear the text buffer before retrying the vsnprint request */
      else {
        text[WTO_MAX_SIZE] = 0;       /* Ensure strlen stops at the end of the text buffer */
        cnt = strlen(text);           /* Find the end of the text string */
      }
    } else
      break;                          /* vsnprintf did not return an error - cnt was set */
  }
  if (cnt>WTO_MAX_SIZE)               /* If more data to format than the text buffer length */
    cnt = WTO_MAX_SIZE;               /* Truncate the formatted length to the text buffer length */

  /* We never want to include a final \n character in the WTO text */

  if (cnt>0 && text[cnt-1] == '\n')   /* If text ends with \n */
    text[cnt-1] = 0;                  /* Change it into a null character */

  message(text);
}

void authWTOPrintf(char *formatString, ...){
  char text[WTO_MAX_SIZE+1];       /* Allow for trailing null character */
  va_list argPointer;
  int cnt;

  for (int pass=0; pass<2; pass++){

    /* The resulting text string from vsnprintf is unpredictable if
       there is an error in the format string or arguments.  In that
       case we will set the output text area to null, repeat the
       vsnprintf, and then find the length of the null terminated
       string.  This avoids initializing the output text area prior
       to every successful request.
    */

    va_start(argPointer,formatString);
    cnt = vsnprintf(text,sizeof(text),formatString,argPointer);
    va_end(argPointer);

    if (cnt<0){
      if (pass==0)
        memset(text,0,sizeof(text));  /* Clear the text buffer before retrying the vsnprint request */
      else {
        text[WTO_MAX_SIZE] = 0;       /* Ensure strlen stops at the end of the text buffer */
        cnt = strlen(text);           /* Find the end of the text string */
      }
    } else
      break;                          /* vsnprintf did not return an error - cnt was set */
  }
  if (cnt>WTO_MAX_SIZE)               /* If more data to format than the text buffer length */
    cnt = WTO_MAX_SIZE;               /* Truncate the formatted length to the text buffer length */

  /* We never want to include a final \n character in the WTO text */

  if (cnt>0 && text[cnt-1] == '\n')   /* If text ends with \n */
    text[cnt-1] = 0;                  /* Change it into a null character */

  authWTOMessage(text);
}

void qsamPrintf(char *formatString, ...){
  va_list argPointer;

  SYSOUT *sysout = getSYSOUTStruct(STDOUT_DD,NULL,NULL);
  char *outBuffer = sysout->buffer;

  /* If DCB did not open then outBuffer will be null */

  if (outBuffer){

    int recLength = sysout->length;
    int fmtLength = recLength+1; /* Allow for a null character at the end */

    /* Prior to supporting the record format and logical record
       length being overridden the first character of formatted data
       was always supposed to be an ANSI CC value.

       With the record format override support the first character
       of formatted data passed by the requestor will remain
       unchanged.  If the machine CC attribute was merged into the
       DCB then the passed CC character will *not* be a valid
       machine CC value (I am not going to try to translate from
       ANSI to machine CC).  If neither the ANSI or machine CC
       attribute was merged into the DCB then the passed CC
       character will just be the first character of the output
       record (I am not going to strip the CC character off of the
       passed data).
    */

    int cnt;

    for (int pass=0; pass<2; pass++){

      /* The resulting text string from vsnprintf is unpredictable if
         there is an error in the format string or arguments.  In that
         case we will set the output text area to null, repeat the
         vsnprintf, and then find the length of the null terminated
         string.  This avoids initializing the output text area prior
         to every successful request.
      */

      va_start(argPointer,formatString);
      int cnt = vsnprintf(outBuffer,fmtLength,formatString,argPointer);
      va_end(argPointer);

      if (cnt<0){
        if (pass==0)
          memset(outBuffer,0,fmtLength); /* Clear the text buffer before retrying the vsnprint request */
        else {
          outBuffer[recLength] = 0;      /* Ensure strlen stops at the end of the record */
          cnt = strlen(outBuffer);       /* Find the end of the text string */
        }
      } else
        break;                           /* vsnprintf did not return an error - cnt was set */
    }

    if (cnt>recLength)                   /* If more data to format than the record length */
      cnt = recLength;                   /* Truncate the formatted length to the record length */
#ifndef KEEP_NL_AND_NULL
    /* We don't want to include a final \n character in the formatted text */

    if (cnt>0 && outBuffer[cnt-1] == '\n')  /* If text ends with \n */
      cnt--;                           /* Decrement character count */
#else
    if (cnt<recLength)
      cnt++;                           /* Increment, so null is written out */
#endif

    if (cnt<recLength && sysout->recfm=='F')
      memset(outBuffer+cnt,' ',recLength-cnt); /* Pad fixed length record with blanks */

    putlineV(sysout->dcb,sysout->buffer,cnt); /* putlineV ignores the length value when writing fixed length records */
  }
}

#ifdef WRITSTAT
  static int killPrintf = 0;

/* External function killprnf - code never referenced in metalio.c
   and is not defined in metalio.h for use elsewhere.
*/
void killprnf(){
  killPrintf = 1;
}
#endif

ZOWE_PRAGMA_PACK

typedef struct ENQParms_tag{
  char    flags;    /* always 192 (x'C0') - End-of-list, ignore flag bits 2-4 and 6-7 */
  char    minorLength;
  char    options;  /* EXCLUSIVE,STEP,RET=NONE is 0 */
  /*
             0...      ....    Exclusive request.
             1...      ....    Shared request.
             .0..      0...    Scope of the minor name is STEP.
             .0..      1...    RESERVE type. The resource is known across systems and UCB=
                               was specified. The last word of the parameter list is the
                               address of a word containing the UCB address.
             .1..      0...    Scope of the minor name is SYSTEM.
             .1..      1...    Scope of the minor name is SYSTEMS.
             ..1.      ....    Obsolete.
             ...1      ....    Set "must complete" equal to STEP.
             ....      .000    RET=NONE.
             ....      .001    RET=HAVE.
             ....      .010    RET=CHNG.
             ....      .011    RET=USE.
             ....      .100    "ECB=addr". The ECB address is contained in the parameter list
                               prefix.
             ....      .111    RET=TEST.
             */
  char    fieldCodes;
  Addr31  major;
  Addr31  minor;
} ENQParms; 

ZOWE_PRAGMA_PACK_RESET

static int enqueueSYSOUT(SYSOUT *sysout){

  int status;

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      ENQParms enq;
    )
  );

  below2G->enq.flags = 0xC0;
  below2G->enq.minorLength = 8;
  below2G->enq.major = MSGPREFIX "TRACE";
  below2G->enq.minor = sysout->ddname;

  __asm(ASM_PREFIX
        " ENQ MF=(E,(%[plist])) \n"
        :[rc]"=NR:r15"(status)
        :[plist]"NR:r1"(&below2G->enq)
        :"r0","r1","r15");

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return status;
}

static int dequeueSYSOUT(SYSOUT *sysout){

  int status;

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      ENQParms deq;
    )
  );

  below2G->deq.flags = 0xC0;
  below2G->deq.minorLength = 8;
  below2G->deq.major = MSGPREFIX "TRACE";
  below2G->deq.minor = sysout->ddname;

  __asm(ASM_PREFIX
        " DEQ MF=(E,(%[plist])) \n"
        :[rc]"=NR:r15"(status)
        :[plist]"NR:r1"(&below2G->deq)
        :"r0","r1","r15");

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return status;
}

#ifdef METTLE
void vfprintf(void *target, char *formatString, va_list argPointer){
  int i,len;
  int containsNewline = 0;

#ifdef WRITSTAT
  if (killPrintf){
    return;
  }
#endif
  SYSOUT *sysout = getSYSOUTStruct(STDOUT_DD,NULL,NULL);
  char *outBuffer = sysout->buffer;

  /* If DCB did not open then outBuffer will be null */

  if (!outBuffer)
    return;

  int pos = sysout->position;
  int recLength = sysout->length;
  int fmtLength = recLength+1; /* Allow for a null character at the end */

  if (pos==0 && sysout->CC!=0){  /* Start of line - set CC for the line if one exists */
    outBuffer[0] = sysout->CC;
    pos++;
  }

  int cnt;

  for (int pass=0; pass<2; pass++){

    /* The resulting text string from vsnprintf is unpredictable if
       there is an error in the format string or arguments.  In that
       case we will set the output text area to null, repeat the
       vsnprintf, and then find the length of the null terminated
       string.  This avoids initializing the output text area prior
       to every successful request.
    */

    cnt = vsnprintf(outBuffer+pos,fmtLength-pos,formatString,argPointer);

    if (cnt<0){
      if (pass==0)
        memset(outBuffer+pos,0,fmtLength-pos); /* Clear the text buffer before retrying the vsnprint request */
      else {
        outBuffer[recLength] = 0;      /* Ensure strlen stops at the end of the record */
        cnt = strlen(&outBuffer[pos]); /* Find the end of the text string */
      }
    } else
      break;                           /* vsnprintf did not return an error - cnt was set */
  }

  pos += cnt;

  if (pos>recLength)                   /* If more data to format than the record length */
    pos = recLength;                   /* Truncate the formatted length to the record length */

#ifndef KEEP_NL_AND_NULL
  /* We don't want to include a final \n character in the formatted text */

  if (pos>0 && outBuffer[pos-1] == '\n'){ /* If text ends with \n */
    pos--;                           /* Decrement character count */
    containsNewline = 1;
  }
#else
  for (i=0; i<pos; i++){
    if (sysout->buffer[i] == EBCDIC_NEWLINE){
      containsNewline = 1;
      break;
    }
  }
#endif
  if (containsNewline || (pos == recLength)){
#ifdef KEEP_NL_AND_NULL
    if (pos<recLength)
      pos++;                           /* Increment, so null is written out */
#endif
/*$*$*$ How do we get a 'sysout' block with no dcb ? */
    if (sysout->dcb){
      if (enqueueSYSOUT(sysout) == 0){
        if (pos<recLength && sysout->recfm=='F')
          memset(outBuffer+pos,' ',recLength-pos); /* Pad fixed length record with blanks */
        putlineV(sysout->dcb,outBuffer,pos);
        dequeueSYSOUT(sysout);
      }
    } else{
      message(outBuffer);
    }
    pos = 0;
  }
  sysout->position = pos;
}

void printf(char *formatString, ...){
  va_list argPointer;
  va_start(argPointer,formatString);
  vfprintf(NULL,formatString,argPointer);
  va_end(argPointer);
}


void fflush(int fd){

/* There might be a need to have fflush for stdout to
perform a CLOSE request (while holding an ENQ) against
the DCB, so that any buffered data will be written out,
and then perform an OPEN with EXTEND so additional data
can be written.  This will not work if the dataset was
allocated with FREE=CLOSE (DSABUNAL) or has a record
format of FBS.  In those cases the fflush would have to
be ignored.  By having output to JES be written without
blocking the need for this has been minimized.
*/
}
#endif

/* setjmp buffer is
   slot 0  0  0  handler address
   slot 1  4  8  retval
   slot 2  8 16  R14 save
   slot 3 12 24  R15 save
   slot 4 16 32  R0  save
   slot 5 20 40  R1  save
   slot 6 24 48  R2  save
   ....
   */

//
//  The following does not work with XPLINK, fix it when it is needed
//

/* return 1 if longjmp called with value 0, else return value */
#ifdef _LP64
int setjmp(jmp_buf env){ 
  char *saves = (char*)env;
  int value = 0;
  __asm(ASM_PREFIX
        "         LG   14,128(13)\n"      /* back chain */
	"         LG   14,8(14)\n"        /* retaddr of caller */
	"         STMG 14,13,16(%1)\n"   /* store everything */
	"         LARL 14,NONLOCAL\n"   /* get handler address */
	"         STG  14,0(%1)\n"      /* and put in save block */
        "         J    NORMAL\n"
	/* long jmp finds a address, set 15 to it, 1 pts to block */
	"NONLOCAL LG  15,8(1)\n"        /* retval */
	"         LG  14,16(1)\n"       /* retaddr */
	"         LMG 2,13,48(1)\n"     /* restore R2-R13 - use 0,1,14,15 for playing */
	"         LG  1,128(13)\n"      /* find prev save area, and clobber R1 (done with plist) */
	"         STG 14,8(1)\n"       /* push good addr back in to prev save area */
	"         ST  15,%0\n"         /* ST/STG, I am pointing at a 4-byte value even in AMODE 64 */
	"NORMAL   DS  0H"
	:
	"=m"(value)
	:
	"r"(saves)
	:
	"r0", "r1", "r14", "r15");
  return value;
}
#else
int setjmp(jmp_buf env){ 
  char *saves = (char*)env;
  int value = 0;
  __asm(ASM_PREFIX
        "         L    14,4(13)\n"      /* back chain */
	"         L    14,12(14)\n"     /* retaddr of caller */
	"         STM  14,13,8(%1)\n"   /* store everything */
	"         LARL 14,NONLOCAL\n"   /* get handler address */
	"         ST   14,0(%1)\n"      /* and put in save block */
        "         J    NORMAL\n"
	/* long jmp finds a address, set 15 to it, 1 pts to block */
	"NONLOCAL L   15,4(1)\n"
	"         L   14,8(1)\n"
	"         LM  2,13,24(1)\n"     /* restore R2-R13 - use 0,1,14,15 for playing */
	"         L   1,4(13)\n"        /* find prev save area, and clobber R1 (done with plist) */
	"         ST  14,12(1)\n"       /* push good addr back in to prev save area */
	"         ST  15,%0\n"
	"NORMAL   DS  0H"
	:
	"=m"(value)
	:
	"r"(saves)
	:
	"r0", "r1", "r14", "r15");
  return value;
}
#endif

/* 
  Restores the context of the environment buffer env that was saved
  by invocation of the setjmp routine[1] in the same invocation of the
  program. Invoking longjmp from a nested signal handler is
  undefined. The value specified by value is passed from longjmp to
  setjmp. After longjmp is completed, program execution continues as
  if the corresponding invocation of setjmp had just returned. If the
  value passed to longjmp is 0, setjmp will behave as if it had
  returned 1; otherwise, it will behave as if it had returned value.
  */

void longjmp(jmp_buf env, int value){
  char *saves = (char*)env;

  __asm(ASM_PREFIX
#ifdef _LP64
        "         LG  14,0(%[plist])    Get NONLOCAL handler addr \n"
        "         STG 15,8(%[plist])    Save return value into jump buffer \n"
#else
        "         L  14,0(%[plist])    Get NONLOCAL handler addr \n"
        "         ST 15,4(%[plist])    Save return value into jump buffer \n"
#endif
	"         LA  5,273\n"     /* what's this? */
        "         BR  14"
	:
	:[plist]"NR:r1"(saves),[value]"NR:r15"(value)
	:"r1", "r5", "r14", "r15");
}




/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

