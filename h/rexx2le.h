#ifndef __ZOWE_REXX2LE__
#define __ZOWE_REXX2LE__ 1

#include "zowetypes.h"

ZOWE_PRAGMA_PACK

/*********************************************************************
  ATTENION, PLEASE!
  ATENCION POR FAVOR!
  AUFMERKSAMKEIT, BITTE!
  VNIMANIYE POZHALUYSTA!
  YATRI KRIPYA DHYAN DE!

  These data structures are shared between 31 and 64 bit programs and
  can have no size/offset differences when compiled under 31 or 64 bit
  settings.  That means Addr31 is the common pointer type and there
  are ABSOLUTELY NO 64 bit data stuctures in here.

  **********

  IF YOU DIDN"T READ THIS, GO BACK AND READ IT!!

  ********************************************************************/

typedef struct REXXArg_tag {
  Addr31 address;  /* 0 if unsupplied positional argument */
  int    length;   /* empty string is a real address with a length of 0 */
} REXXArg;

#define REXX_LAST_ARG 0xFFFFFFFF
#define REXX_NO_RESULT 0x80000000

typedef struct IRXEVALB_tag{
  char      evpad1[4]; /* reserved MBZ */
  int       evsize;    /* size of EvalBlock in doublewords (uint64_t, 8 bytes) */
  int       evlen;     /* len in bytes of evdata, set to REXX_NO_RESULT on entry */
  char      evpad2[4];
  char      evdata[0]; /* return data starts here */
} IRXEVALB;

typedef struct IRXENVB_tag{
  char      id[8];
  char      version[4];
  int       length;
  Addr31    parmblock;
  Addr31    userfield;   
  Addr31    irxexte; 
  /* etc... */
} IRXENVB;

#define EXCOM_DROP_VARIABLE      'D'
#define EXCOM_DROP_SYMBOLIC_NAME 'd'
#define EXCOM_COPY_SHARED_VAR    'F'
#define EXCOM_RETRIEVE_SYMBOLIC_NAME 'f'
#define EXCOM_FETCH_NEXT_VARIABLE 'N'
#define EXCOM_FETCH_PRIVATE_INFO  'P'
#define EXCOM_STORE_VARIABLE_VALUE 'S'
#define EXCOM_SET_SYMBOLIC_NAME 's'

/*
  Private info names "ARG", "SOURCE", "VERSION"
 */

#define SHVCLEAN 0x00 /* Execution was OK */
#define SHVNEWV  0X01 /* Variable did not exist */
#define SHVLVAR  0x02 /* Last variable transferred (for "N") */
#define SHVTRUNC 0x04 /* Truncation occurred during "Fetch" */
#define SHVBADN  0X08 /*  Invalid variable name */
#define SHVBADV  0x10 /*  Value too long */
#define SHVBADF  0x80 /* Invalid function code (SHVCODE) */

typedef struct SHVBLOCK_tag{
  Addr31 shvnext;
  int    shvuse;
  char   shvcode;
  char   shvret;    /* see SHV.. above */
  char   reserved[2];
  int    shvbufl; /* length of fetch value buffer */
  Addr31 shvnama; /* address of var name */
  int    shvnaml; /* var name len */
  Addr31 shvvala; /* address of data values */
  int    shvvall; /* data length */
} SHVBLOCK;

#define IRXEXCOM_INSUFFICIENT_STORAGE (-1)
#define IRXEXCOM_ENTRY_CONDITIONS (-2)
#define IRXEXCOM_NO_ENV 28
#define IRXEXCOM_INVALID_PARMLIST 32
/* and others */

/* https://books.google.com/books?id=gVflBwAAQBAJ&pg=PA266&lpg=PA266&dq=IRXSHVB&source=bl&ots=G0EtLoj8cA&sig=ACfU3U0nchvu1G-zzdIwxgTh3HUWpdQaqw&hl=en&sa=X&ved=2ahUKEwj21r-L2a34AhW_j4kEHW-gBFYQ6AF6BAgFEAM#v=onepage&q=IRXSHVB&f=false
 */

typedef struct IRXEFPL_tag{
  Addr31 efplcom; /* what is this */
  Addr31 efplbarg; 
  Addr31 efplearg; 
  Addr31 efplfb;
  Addr31 rexxArgs;
  Addr31 *__ptr32 evalBlockHandle;
} IRXEFPL;

typedef struct IRXEXCOMParms_tag{
  char     *__ptr32 eyecatcher;
  int      parm2; /* MBZ */
  int      parm3; /* MBZ */
  SHVBLOCK *__ptr32 shvblock;
  Addr31   envblock;  /* if non-zero supersedes R0 */
  int      retcodePtr;
  int      retcode;
} IRXEXCOMParms;

typedef struct REXXEnv_tag {
  /* first time though call main and return the "servicePoint"
     safeMalloc31 the metal stack
   */
  char     eyecatcher[8];
  char     le64Name[8];
  uint32_t stackSize;
  Addr31   metalStack;
  Addr31   pipiContext;
  struct REXXInvocation_tag *__ptr32 invocation;
  /* addresses for calling metal 64 */
  Addr31   metal64Init;
  Addr31   metal64Call;
  Addr31   metal64Term;
  /* Address for calling back to REXX */
  Addr31   irxexcom; 
} REXXEnv;

typedef struct REXXInvocation_tag {
  REXXEnv *__ptr32 env;
  IRXEFPL *__ptr32 efpl;
  IRXENVB *__ptr32 envBlock;
  int    traceLevel;
  Addr31 evalBlock;
  char  *__ptr32 functionName;
  int    functionNameLength;
  int    argCount;
} REXXInvocation; 



ZOWE_PRAGMA_PACK_RESET



#endif 
