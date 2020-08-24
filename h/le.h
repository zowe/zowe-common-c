

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __LE__
#define __LE__ 1

#include "zowetypes.h"
#include "openprims.h"

#ifdef __ZOWE_OS_ZOS
#include "recovery.h"
#include "collections.h" /* needed for messageQueue in RLETask */
#endif

/* Very little of this makes any sense except on z/OS. */
#ifdef __ZOWE_OS_ZOS

#ifndef __LONGNAME__

#endif

#ifdef __ZOWE_OS_ZOS
ZOWE_PRAGMA_PACK
#endif

  /* important LE CA addresses to copy
     0x1F0 C/370 CGENE
     0x1F4 CEECAACRENT - writable static
     0x210 holds C/C++ RT routine vector see LE Vendor book
     0x218 holds CEECAACEDB
     */

typedef struct CAA_tag{
  char   ceecaaflag0;
  char   reserved001;
  char   ceecaalangp;  /* PL/I flags */
  char   reserved003[5];
  Addr31 ceecaabos;           /* start of current storage segment */
  Addr31 ceecaaeoc;           /* end of current storage segment */
  char   reserved010[0x034];
  short  reserved044;
  short  ceecaatorc;          /* thread return code */
  char   reserved048[0x02C];
  Addr31 ceecaatovf;          /* stack overflow routine */
  char   reserved078[0x0A8];
  char   unmapped120[0x0D0];  /* OFFSET 0x120 */
  Addr31 cgene;
  Addr31 writableStatic;
  char   convertFloatInit[8];
  Addr31 ceecaacprms;  /* 0x200 ??? */
  Addr31 ceecaac_rtl;
  Addr31 ceecaacthd;
  Addr31 ceecaacurrfecb;
  Addr31 runtimeLibraryVectorTable;
  Addr31 ceecaacpcb;
  Addr31 ceecaacedb;   /* important for something */
  char   reserved21C[3];
  char   ceecaaspcflag3; /* SPC flags, pretty obsolete */
  Addr31 ceecaacio;      /* 0x220 unknown */
  Addr31 ceecaafdsetfd;  /* 0x224 Used by FD_* macros  */
  char   ceecaafcbmutexok[2];  /* 0x228 what is this? */
  char   reserved22A[2];
  char   ceecaatc16[4];  /* 0x22C what is this? */
  int    ceecaatc17;     /* 0x230 what again */
  Addr31 ceecaaecov;     /* 0x234 routine vector, important */
  int    ceecaactofsv;   /* 0x238 what is this? */
  Addr31 ceecaatrtspcae; /* 0x23C Another routine vector ?? */
  char   reserved240[24];
  Addr31 ceecaaTCASrvUserWord; /* 0x258 */
  Addr31 ceecaaTCASrvWorkArea;
  Addr31 ceecaaTCASrvGetmain;
  Addr31 ceecaaTCASrvFreemain;
  Addr31 ceecaaTCASrvLoad;
  Addr31 ceecaaTCASrvDelete;
  Addr31 ceecaaTCASrvException; /* 0x270 */
  Addr31 ceecaaTCASrvAttention;
  Addr31 ceecaaTCASrvMessage;   /* 0x278 */
  char   reserved27C[4];
  Addr31 ceecaalws;     /* 0x280 PL/I LWS */
  Addr31 ceecaasavr;    /* register save?  fer what?? */
  char   reserved288[28];
  Addr31 loggingContext; /* see logging text */
  Addr31 rleTask;        /* normally reserved, but
                              we use this for a pointer to the
                              RLE Task */
  char   ceecaasystm;   /* 0x2AC */
  char   ceecaahrdwr;
  char   ceecaasbsys;
  char   ceecaaflag2;   /* various flags */
  /* OFFSET 0x2B0 */
  char   ceecaalevel;
  char   ceecaa_pm;
  unsigned short ceecaa_invar;  /* same fixed offset in
                                   31 or 64 mode */
  Addr31 ceecaagetls;    /* CEL Library stack mgr */
  Addr31 ceecaacelv;     /* CEL Library Vector */
  Addr31 ceecaagets;     /* CEL Library get storage */

  Addr31 ceecaalbos;     /* begin library storage stack */
  Addr31 ceecaaleos;     /* end library storage stack */
  Addr31 ceecaalnab;     /* next available byte of lib storage */
  Addr31 ceecaadmc;      /* ESPIE devil-may care routine */
  /* OFFSET 0x2D0 */
  int    ceecaaabcode;   /* abend completion code */
  int    ceecaarsncode;  /* abend reason code */
  Addr31 ceecaaerr;      /* Addr of current CIB */
  Addr31 ceecaagetsx;    /* Addr of CEL stack stg extender */
  /* OFFSET 0x2E0 */
  Addr31 ceecaaddsa;     /* Dummy DSA */
  int    ceecaasectsiz;  /* Vector Size */
  int    ceecaapartsum;  /* Vector partial sum */
  int    ceecaassexpnt;  /* Log of vector section size */
  /* OFFSET 0x2F0 */
  Addr31 ceecaaedb;      /* A(EDB) */
  Addr31 ceecaapcb;      /* A(PCB) */
  Addr31 ceecaaeyeptr;   /* Address (duh!) of Eyecatcher */
  Addr31 ceecaaptr;      /* Address of me (the CAA) */
  /* OFFSET 0x300 */
  Addr31 ceecaagets1;
  Addr31 ceecaashab;     /* Abend shunt */
  Addr31 ceecaaprgck;    /* Program Interrupt code for DMC */
  char   ceecaaflag1;
  char   ceecaashab_key; /* CEECAASHAB IPK key */
  char   reserved30E[2];
  /* OFFSET 0x310 */
  int    ceecaaurc;       /* thread level return code */
  Addr31 ceecaaess;       /* end of current user stack */
  Addr31 ceecaaless;      /* end of current library stack (hmmm....) */
  Addr31 ceecaaogets;     /* fastlink stuff */
  /* OFFSET 0x320 */
  Addr31 ceecaaogetls;    /* fastlink */
  Addr31 ceecaapicicb;    /* Pre Init Compatibility control block */
  Addr31 ceecaaogetsx;    /* fastlink */
  short  ceecaagosmr;
  short  unknown032E;
  /* OFFSET 0x330 */
} CAA;

#define CAA_SIZE 0x318 /* from an LE vendor manual - somewhat in conflict with above structure !!!! */

#define SRBCLEAN ((void*)8)

typedef struct LibraryFunction_tag{
  char *name;
  void *pointer;
  void *replacement31;
  void *replacement64;
} LibraryFunction;

#define SDWA_COPY_MAX 0x2000

/* Don't change this struct
   there are hard-coded assembly language offsets that depend on this */

/* The RTL for RLE is either 64 bit or not; XPLINK or not
   Rarely do we see anything but
     31 bit standard linkage
     64 bit XPLINK linkage
 */
#define RLE_RTL_64      0x0001
#define RLE_RTL_XPLINK  0x0002

typedef struct RLEAnchor_tag{
  char   eyecatcher[8]; /* RLEANCHR */
  int64  flags;
  CAA   *mainTaskCAA;
  void **masterRTLVector; /* copied from CAA */
} RLEAnchor;

/* An RLE Task can run in SRB mode or TCB Mode or switchably depending on run
   state conditions */

#define RLE_TASK_TCB_CAPABLE 0x0001
#define RLE_TASK_SRB_CAPABLE 0x0002
#define RLE_TASK_SRB_START   0x0004  /* should be started in SRB */
#define RLE_TASK_NON_ZOS     0x0008  /* really */
#define RLE_TASK_RECOVERABLE 0x0010
#define RLE_TASK_DISPOSABLE  0x0020  /* task is removed after execution */

#define RLE_TASK_STATUS_THREAD_CREATED    0x00000001

/* for sanity, RLETasks always live in 31-bit space in ZOS
   They might run code in AMODE 64,but this kind of root data structure
   are better off in 31-bit land
   */

/* Warning: must be kept in sync with RLETASK in scheduling.c */
typedef struct RLETask_tag{
  char         eyecatcher[4]; /* "RTSK"; */

  int          statusIndicator;
  int          flags;

  int          stackSize;
  PAD_LONG(0, char *stack);

  PAD_LONG(1, void *userPointer);
  PAD_LONG(2, int (*userFunction)(struct RLETask_tag *));
  PAD_LONG(3, int (*trampolineFunction)(struct RLETask_tag *));
  PAD_LONG(4, char *selfParmList);

  RLEAnchor    *__ptr32 anchor;       /* PTR31 */
  char         *__ptr32 fakeCAA;
  RecoveryContext *__ptr32 recoveryContext;

  Queue        *__ptr32 messageQueue; /* PTR 31 this a queue of stuff to deal with that is pending during SRB execution */
  struct PauseElement_tag *__ptr32 pauseElement;  /* PTR31 */
  int           attachECB;
  union {
    OSThread threadData;
    char threadDataFiller[32];
  };

  int           sdwaBaseAddress;
  char          sdwaCopy[SDWA_COPY_MAX];
} RLETask;

#ifdef __ZOWE_OS_ZOS
ZOWE_PRAGMA_PACK_RESET
#endif

#define LIBRARY_FUNCTION_COUNT 32

#ifndef __LONGNAME__

#define getCAA LEGETCAA
#define showRTL LESHWRTL
#define makeRLEAnchor LEMKRLEA
#define deleteRLEAnchor LEDLRLEA
#define makeRLETask LEMKRLET
#define deleteRLETask LEDLRLET
#define initRLEEnvironment LEINRLEE
#define termRLEEnvironment LETMRLEE
#define makeFakeCAA LEMKFCAA
#define abortIfUnsupportedCAA LEARTCAA

#endif

char *getCAA();
void showRTL();

#else /*  not __ZOWE_OS_ZOS - */

#define RLE_TASK_TCB_CAPABLE 0x0001
#define RLE_TASK_RECOVERABLE 0x0010
#define RLE_TASK_DISPOSABLE  0x0020


typedef struct RLEAnchor_tag{
  char   eyecatcher[8]; /* RLEANCHR */
} RLEAnchor;

typedef struct RLETask_tag{
  char         eyecatcher[4]; /* "RTSK"; */

  /* TBD: I don't yet know what's going to be needed, so
     just dike it all out and then bring members in as needed. */

  void         * userPointer;
  RLEAnchor    * anchor;
  int           flags;

#if 0
  char         *__ptr32 stack;               /* PTR 31 make it big, so never gets extended  */
  int         (*__ptr32 epa)(struct RLETask_tag *);   /* PTR 31 this is the routine that gets things going in a new DU */
  Queue        *__ptr32 messageQueue;                 /* PTR 31 this a queue of stuff to deal with that is pending during SRB execution */
  /* OFFSET 0x10 */
  int           statusIndicator;
  char         *__ptr32 fakeCAA;
  int           reserved018;
  int           sdwaBaseAddress;
  /* OFFSET 0x20 */
  struct PauseElement_tag *__ptr32 pauseElement;  /* PTR31 */
  /* offset 0x30 */
  int           attachECB;
  int           reserved034;
  int           selfParmList64;
  char         *__ptr32 selfParmList;
  /* OFFSET 0x40 */
  char          sdwaCopy[SDWA_COPY_MAX];
#endif
} RLETask;


#endif  /* not __ZOWE_OS_ZOS */

/*
  These functions are defined for all platforms (though they
  may not do very much...).
 */

#ifdef METTLE

#ifdef _LP64
#define establishGlobalEnvironment(anchor) __asm(" LG 12,%0 " : : "m"(anchor->mainTaskCAA) : "r15")
#else /* not _LP64 */
#define establishGlobalEnvironment(anchor) __asm(" L 12,%0" : : "m"(anchor->mainTaskCAA) : "r15")
#endif

#ifdef _LP64
#define returnGlobalEnvironment() __asm(" LG 15,128(,13) \n" " STG 12,120(,15)" : : : "r15")
#else /* not _LP64 */
#define returnGlobalEnvironment() __asm(" L 15,4(,13) \n" " ST 12,68(,15)" : : : "r15")
#endif

#else /* not METTLE */

void establishGlobalEnvironment(RLEAnchor *anchor);
void returnGlobalEnvironment(void);

#endif /* METTLE */

RLEAnchor *makeRLEAnchor();
void deleteRLEAnchor(RLEAnchor *anchor);

/* implemented for zOS in scheduling.c, for others in le.c */
RLETask *makeRLETask(RLEAnchor *anchor,
                     int taskFlags,
                     int functionPointer(RLETask *task));

void deleteRLETask(RLETask *task);

int initRLEEnvironment();
void termRLEEnvironment();

char *makeFakeCAA(char *stackArea, int stackSize);

void abortIfUnsupportedCAA();

#endif /* __LE__ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

