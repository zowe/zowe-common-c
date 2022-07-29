#ifndef __ZOWE_LEPIPI__
#define __ZOWE_LEPIPI__ 1

#define PIPI_INIT_SUB 3
#define PIPI_CALL_SUB 4
#define PIPI_TERM    5
#define PIPI_ADD_ENTRY 6
#define init_main 1
#define init_sub_dp 9
#define call_sub_addr_nochk 12
#define call_sub_addr_nochk2 14

typedef union{
  uint32_t _32;
  uint64_t _64;
} PIPIToken;

typedef int pipiINITSUB(int  *functionCode,
			void **pipiTable,            /* double indirect */
			void *serviceRoutineVector, /* can be null, what does this do */
			char **runtimeOptions,       /* like CEE RUNOPTS */
			PIPIToken  *token);

typedef int pipiCALLSUB(int  *functionCode,
			int  *ptabIndex,    
			PIPIToken  *token,                /* from init */
			void *parmptr,              /* for target sub routine */
			int  *returnCode,
			int  *reasonCode,
			void *feedbackToken);        /* 31: 3 WORDS == 12 bytes , 64: 16 bytes */

typedef int pipiTERM(int *functionCode,
		     int *token,
		     int *envRC);


ZOWE_PRAGMA_PACK  

typedef struct PIPI31Entry_tag{
  char     name[8];       /* 8 chars, old-skool upcased */
  Addr31   address;
  int      mbz;           /* as far as I know */
} PIPI31Entry;

typedef struct PIPI31Header_tag{
  char        eyecatcher[8]; /* "CEEXPTBL" */
  int         entryCount;
  int         entrySize;     /* 16 for PIPI31 Entry */
  int         version;       /* should be 2 */
  char        flags;         /* NOSTOR(80) CICS(40), STACKPROTECT(20) */
  char        mbz[3];         
  PIPI31Entry entries[0];
} PIPI31Header;

typedef struct PIPI64Entry_tag{
  char     name[8];       /* 8 chars, old-skool upcased */
  uint64_t address;       /* made this an integer so it would compile well in 31 bit */
} PIPI64Entry;

typedef struct PIPI64Header_tag{
  char        eyecatcher[8]; /* "CELQPTBL" */
  int         entryCount;
  int         entrySize;     /* 16 for PIPI64 Entry */
  int         version;       /* should be 100 (0x64) */
  char        flags;         /* NOSTOR(80) CICS(40), STACKPROTECT(20) */
  char        mbz[3];         
  PIPI64Entry entries[0];
} PIPI64Header;

typedef struct PIPIContext_tag{
  char eyecatcher[4]; /* PIPI */
  int  flags;
  int  traceLevel;
  void *pipiEP;
  PIPIToken token;
  char *runtimeOptions;
  union{
    PIPI31Header _31;
    PIPI64Header _64;
  } header;
} PIPIContext;


ZOWE_PRAGMA_PACK_RESET

#define PIPI_ENTRY_SIZE 16

#define PIPI_CONTEXT_64 0x0001

#endif
