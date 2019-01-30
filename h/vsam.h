

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/



#ifndef __VSAM__
#define __VSAM__

/** \file
 *  \brief vsam.h is a set of utilities for handling VSAM datasets in C.
 * 
 *  This VSAM utility goes far beyond the half-way support of VSAM in the posix-ish
 *  interface in the XLC runtime library and works in METAL C equally well.
 *
 *  zOS Only (obviously). 
 */

#ifndef STDOUT_DD
#define STDOUT_DD "SYSPRINT"
#endif  
                                                                           
#define ACB_MACRF_KEY      0x8000       
#define ACB_MACRF_ADR      0x4000       /* Only way to search by RBA */
#define ACB_MACRF_CNV      0x2000     
#define ACB_MACRF_SEQ      0x1000     
#define ACB_MACRF_DIR      0x0800     
#define ACB_MACRF_IN       0x0400     
#define ACB_MACRF_OUT      0x0200     
#define ACB_MACRF_UBF      0x0100     
#define ACB_MACRF_CLR      0x0080       /* Keep LRECL Constant (only defined for OUT) */
#define ACB_MACRF_RPL      0x0040       /* Let RPL define CC type (not defined for OUT) */
#define ACB_MACRF_SKP2     0x0010       /* Yeah, IBM's manual claims this bit handles it, but experience shows otherwise... */
#define ACB_MACRF_LOG      0x0008       /* Logon indicator */
#define ACB_MACRF_RST      0x0004       
#define ACB_MACRF_DSN      0x0002       
#define ACB_MACRF_SKP      0x0001       
#define ACB_MACRF_AIX      0x0001       
#define ACB_MACRF_NLW      0x80         
#define ACB_MACRF_LSR      0x40         
#define ACB_MACRF_GSR      0x20         
#define ACB_MACRF_ICI      0x10         
#define ACB_MACRF_DFR      0x08         
#define ACB_MACRF_SIS      0x04         
#define ACB_MACRF_CFX      0x02         

#define RPL_OPTCD_LOC      0x80000000  /*    */
#define RPL_OPTCD_DIR      0x40000000  /*    */
#define RPL_OPTCD_SEQ      0x20000000  /*    */
#define RPL_OPTCD_SKP      0x10000000  /*    */
#define RPL_OPTCD_ASY      0x08000000  /*    */
#define RPL_OPTCD_KGE      0x04000000  /*    */
#define RPL_OPTCD_GEN      0x02000000  /*    */
#define RPL_OPTCD_EECB     0x01000000  /* External ECB */
#define RPL_OPTCD_KEY      0x00800000  /*    */
#define RPL_OPTCD_ADR      0x00400000  /*    */
#define RPL_OPTCD_CNV      0x00200000  /*    */
#define RPL_OPTCD_BWD      0x00100000  /*    */
#define RPL_OPTCD_LRD      0x00080000  /*    */
#define RPL_OPTCD_WAITX    0x00040000  /*    */
#define RPL_OPTCD_UPD      0x00020000  /*    */
#define RPL_OPTCD_NSP      0x00010000  /*    */
#define RPL_OPTCD_EOUS     0x00008000  /* End of User SYSOUT */
#define RPL_OPTCD_NRI      0x00004000  /*    */
#define RPL_OPTCD_CR       0x00002000  /*    */
#define RPL_OPTCD_VUF      0x00001000  /* Verify UCS/FCB Info */
#define RPL_OPTCD_LUB      0x00000800  /* Load UCS Buffer in Fold Mode */
#define RPL_OPTCD_UCS      0x00000400  /* UCS Load (requires FCB as well) */
#define RPL_OPTCD_FCB      0x00000200  /* FCB Load */
#define RPL_OPTCD_AFB      0x00000100  /* Align FCB Buffer Loading */
#define RPL_OPTCD_XRBA     0x00000080  /*    */
#define RPL_OPTCD_3MF      0x00000040  /* 3800 Mark Form */
#define RPL_OPTCD_NCR      0x00000020  /* No CI Reclaim*/
#define RPL_OPTCD_ACC      0x00000010  /* ANSI CC Type */
#define RPL_OPTCD_MCC      0x00000008  /* Machine CC Type */
#define RPL_OPTCD_XCC      0x00000004  /* Other CC Type */

/*  USED ONLY FOR VTAM ACBs  */
#define RPL_OPTCD2_DLGIN   0x80000000  /* Specific Terminal Mode */
#define RPL_OPTCD2_SSNIN   0x40000000  /* Continue Dialog with same Terminal */
#define RPL_OPTCD2_PSOPT   0x20000000  /* Pass Terminal to requesting application */
#define RPL_OPTCD2_NERAS   0x10000000  /* Write to 3270 w/o erasing current display */
#define RPL_OPTCD2_EAU     0x08000000  /* Write to 3270 and erase unprotected fields */
#define RPL_OPTCD2_ERACE   0x04000000  /* Write to 3270 and erase current display */
#define RPL_OPTCD2_NODE    0x02000000  /* Read from any Terminal */
#define RPL_OPTCD2_WROPT   0x01000000  /* Conversational Mode */
#define RPL_OPTCD2_EOB     0x00800000  /* Write a block of data */
#define RPL_OPTCD2_EOM     0x00400000  /* Write the last block of the message */
#define RPL_OPTCD2_EOT     0x00200000  /* Write the last block of the transmission */
#define RPL_OPTCD2_COND    0x00100000  /* Do not stop operation if started */
#define RPL_OPTCD2_NCOND   0x00080000  /* Stop operation immediately */
#define RPL_OPTCD2_LOCK    0x00040000  /* Reset Error Lock to Unlocked Status */
#define RPL_OPTCD2_CNALL   0x00008000  /* All Terminals in OPNDST List must be available before ANY are connected */
#define RPL_OPTCD2_CNANY   0x00004000  /* Connect any one Terminal in OPNDST List */
#define RPL_OPTCD2_QOPT    0x00001000  /* Queue the OPNDST request if can't be immediately satisfied */
#define RPL_OPTCD2_TPOST   0x00000800  /* RPL already under PSS */
#define RPL_OPTCD2_RLSOP   0x00000400  /* Schedule RELREQ exit of Terminal immediately */
#define RPL_OPTCD2_TCRNO   0x00000200  /* Close IN process for PO interface */
#define RPL_OPTCD2_ODACQ   0x00000080  /* App requires a specific Terminal */
#define RPL_OPTCD2_ODACP   0x00000040  /* App will accept any Terminal desiring LOGON */
#define RPL_OPTCD2_ODPRM   0x00000020  /* Specific Terminal is going to be preempted even if another app is holding it */
#define RPL_OPTCD2_PEND    0x00000010  /* Preempt the Terminal after all pending operations are completed */
#define RPL_OPTCD2_SESS    0x00000008  /* Preempt the Terminal after completion of the current dialog session */
#define RPL_OPTCD2_ACTV    0x00000004  /* Preempt the Terminal if connected, not busy */
#define RPL_OPTCD2_UNCON   0x00000002  /* Preempt the Terminal immediately */

#define ACB_MODE_DISP      0x00000000
#define ACB_MODE_LEAVE     0x30000000
#define ACB_MODE_REREAD    0x10000000
#define ACB_MODE_INPUT     0x00000000
#define ACB_MODE_OUTPUT    0x0F000000
#define ACB_MODE_UPDAT     0x04000000
#define ACB_MODE_OUTIN     0x07000000
#define ACB_MODE_INOUT     0x03000000
#define ACB_MODE_RDBACK    0x01000000
#define ACB_MODE_EXTEND    0x0E000000
#define ACB_MODE_OUTINX    0x06000000

#define SV99_VERB_ALLOC    0x01
#define SV99_VERB_UNALLOC  0x02
#define SV99_VERB_CONCAT   0x03
#define SV99_VERB_DECONCAT 0x04
#define SV99_VERB_REMOVE   0x05
#define SV99_VERB_DDALLOC  0x06
#define SV99_VERB_INFO     0x07

#define SV99_FLAG1_ONCNV   0x8000      /* Alloc Function - Do not use an existing allocation that doesn't have the convertible attribute to satisfy a request */
#define SV99_FLAG1_NOCNV   0x4000      /* Alloc Function - Do not use an existing allocation to satisfy the request */
#define SV99_FLAG1_NOMNT   0x2000      /* Alloc Function - Do not mount volumes or consider offline units (overrides MOUNT, OFFLN in FLAG2 below) */
#define SV99_FLAG1_JBSYS   0x1000      /* Alloc Function - Job-related SYSOUT */
#define SV99_FLAG1_CNENQ   0x0800      /* All Functions - Issue a conditional end in TIOT resource.  If not available, return an error code to user. */
#define SV99_FLAG1_GDGNT   0x0400      /* Alloc Function - Ignore the GDG name table and perform a locate for the GDG base level */
#define SV99_FLAG1_MSGL0   0x0200      /* All Functions - Ignore the MSGLEVEL parameter in the JCT and use MSGLEVEL-(,0) */
#define SV99_FLAG1_NOMIG   0x0100      /* Alloc Function - Do not recall migrated data sets */
#define SV99_FLAG1_NOSYM   0x0080      /* Allocate, Unallocate, Info Ret - Do not perform symbolic substitution */
#define SV99_FLAG1_ACUCB   0x0040      /* Alloc Function - Use actual UCB addresses */
#define SV99_FLAG1_DSABA   0x0020      /* Request that the DSAB for this allocation be placed above the 16MB line */
#define SV99_FLAG1_DXACU   0x0010      /* Request above-the-line DSABS, XTIOTs, and actual (uncaptured) UCB's for allocated devices */

#define SV99_FLAG2_WTVOL   0x80000000  /* Alloc Function - Wait for volumes */ 
#define SV99_FLAG2_WTDSN   0x40000000  /* Alloc Function - Wait for dsname */ 
#define SV99_FLAG2_NORES   0x20000000  /* Alloc Function - Do not do data set reservation */ 
#define SV99_FLAG2_WTUNT   0x10000000  /* Alloc Function - Wait for units */ 
#define SV99_FLAG2_OFFLN   0x08000000  /* Alloc Function - Consider offline units */ 
#define SV99_FLAG2_TIONQ   0x04000000  /* All Functions - TIOT ENQ already done */ 
#define SV99_FLAG2_CATLG   0x02000000  /* Alloc Function - Set special catalog data set indicators */ 
#define SV99_FLAG2_MOUNT   0x01000000  /* Alloc Function - May mount volume */ 
#define SV99_FLAG2_UDEVT   0x00800000  /* Alloc Function - Unit name parameter is a device type */ 
#define SV99_FLAG2_PCINT   0x00400000  /* Alloc Function - Allocate private catalog to initiator */ 
#define SV99_FLAG2_DYNDI   0x00200000  /* Alloc Function - No JES3 DSN integrity process */ 
#define SV99_FLAG2_TIOEX   0x00100000  /* Alloc Function - XTIOT entry requested (for system program use only) */ 
#define SV99_FLAG2_ASERR   0x00080000  /* Unit Allocation/Unallocation Service (IEFAB4C1/IEFDB440) - Ignore coupling facility READ/WRITE fail when processing RELEASE func for Autoswitchable device (for system program use only) */ 
#define SV99_FLAG2_IGNCL   0x00040000  /* Alloc Function - ignore control limit (for system program use only) */ 
#define SV99_FLAG2_DASUP   0x00020000  /* Alloc Function - supress DD-level accounting in SMF Type 30 (EXCP section) and Type 40 records */ 

#define SV99_OPTS_IMSG     0x80        /* Issue message before returning to caller */
#define SV99_OPTS_RMSG     0x40        /* Return message to caller */
#define SV99_OPTS_LSTO     0x20        /* User storage should be below 16MB boundary */
#define SV99_OPTS_MKEY     0x10        /* User specified storage key for message blocks */
#define SV99_OPTS_MSUB     0x08        /* User specified subpool for message blocks */
#define SV99_OPTS_WTP      0x04        /* Use WTO for message output */

/* map acbCommon at 0 (0x00) size is 76 */                           
#define ACB_COMMON_OFFSET 0x0                                                              

ZOWE_PRAGMA_PACK
typedef struct acbCommon{
  /* OFFSET 0x00 */
  char acbId;           /* always 0xA0 */
  char acbSubtype;      /* always 0x10 */
  int  acbLen:16;       /* should always be 0x004C */
  int  ambListPointer;  /* filled by macro */
  int  interRoutPoint;  /* interface routine pointer (filled by macro) */
  /* OFFSET 0x0C */
  int  macrf12:16;      /* see flag values above (ACB_MACRF_xxx)*/
  char numConcurAIX;    /* number of concurrent strings for AIX patch, 0 if none */
  char numStr;          /* number of strings, always 01 here */
  int  numDataBuff:16;  /* # of CI buffers dedicated to data */
  int  numIndexBuff:16; /* # of CI buffers dedicated to indices */
  /* OFFSET 0x14 */     
  char macrf3;          /* see flag values above (ACB_MACRF_xxx) */
  char srpID;           /* shared resource pool ID, 00 if not set */
  int  jesBuff:16;      /* JES Buffer Pool / # of Journal Buffers */
  /* OFFSET 0x18 */     
  char recfm;           
  char readIntOp;       /* read integrity options, 0 if none */
  int  dsorg:16;        /* DataSet Organization type, 0x0008 for VSAM ACB */
  int  msgArea;         /* pointer to Message Area, 0 if not set */
  int  pass;            /* pointer to password, 0 if not set */
  int  exitList;        /* Exit List addr, 0 if not set */
  /* OFFSET 0x28 */     
  char ddname[8];       /* overriden after open, reestablished after close */
  /* OFFSET 0x30 */     
  char openFlags;       /* open flags, 0x1x is open, 0x0x is close */
  char erFlags;         /* error flags */
  int  inFlags:16;      /* have not seen these non-zero yet */
  int  openjJFCB;       /* pointer to OPENJ JFCB, 0 if not set */
  int  buffSpace;       /* pointer to buffer space, 0 if not set */
  /* OFFSET 0x3C */
  int  blockSize:16;    /* seems to defer to RPL value. We set it anyway */
  int  recordSize:16;   /* seems to defer to RPL value. We set it anyway */
  int  userWorkArea;    /* pointer to User Work Area, 0 if not set */
  int  cbManip;         /* pointer to Control Block Manipulation, 0 if not set */
  int  appName;         /* pointer to Application Name, 0 if not set */
} ACBCommon;      

/* map rplCommon at 76 (0x4C) size is 76 */
#define RPL_COMMON_OFFSET 0x4C


typedef struct rplCommon{       
  /* OFFSET 0x4C */
  char rplId;           /* always 0x00 here */
  char rplSubtype;      /* always 0x10 here */
  char rplType;         /* always 0x00 here */
  char rplLen;          /* should always be 0x4C here */
  int  placeholder;     /* pointer to Placeholder, 0 if not set */
  int  ecb;             /* pointer to Event Control Block, 0 if not set */
  char status;          /* status byte */
  int  feedback:24;     /* feedback codes */
  int  keyLen:16;       /* key length, used for GET/POINT to read arg */
  int  transId:16;      /* TransID */
  int  cc;              /* pointer to Control Character */
  /* OFFSET 0x64 */
  int  acb;             /* pointer to ACB, in theory, should always be (rplCommon - 76) here */
  int  tcb;             /* pointer to TCB, 0 if not set */
  int  workArea;        /* pointer to Record Area */
  int  arg;             /* pointer to Search Argument */
  /* OFFSET 0x74 */
  int  optcd1;          /* see flag values above (RPL_OPTCD_xxx) */
  int  nextRPL;         /* pointer to next RPL, 0 if none */
  int  recLen;          /* Record Length */
  int  bufLen;          /* Buffer Length, must be large enough for records.  Suggest multiples of recLen
                            However, any value large enough to hold an entire record will be valid. */
  /* OFFSET 0x84 */
  int  optcd2;          /* see flag values above (RPL_OPTCD_xxx) */
  char rba[8];          /* Relative Byte Address of most recent request, seemingly ignored */
  char exitDefn;        /* Exit Definitions, always 0x40 here */
  char activeInd;       /* Active Indicator, always 0x00 here */
  int  maxErrLen:16;    /* maximum error message length, 0 for no maximum */
  int  messageArea;     /* pointer to message area, 0 if not used */
} RPLCommon;                                                     

/* map acbWithVTAM at 0 (0x0) size is 108 */
#define ACB_VTAM_OFFSET 0x0

typedef struct acbWithVTAM{
  int temp;
/* NEEDS TO BE DEFINED */
} ACBWithVTAM;

/* map rplWithVTAM at 108 (0x6C) size is 112 */
#define RPL_VTAM_OFFSET 0x6C

typedef struct rplWithVTAM{
  int temp;
/* NEEDS TO BE DEFINED */
} RPLWithVTAM;

/* map sv99RB at 0 (0x00) size is 20 */
#define SV99_COMMON_OFFSET 0x00
///SV99 Request Block
typedef struct sv99RB{
  char rbLen;           /* should always be 0x14, unless using extended format, in which case it will be 0x38 (Length of extension stored in S99RBXLN) */
  char verbCode;        /* verb code (SV99_VERB_xxx) */
  int flag1:16;         /* flags (SV99_FLAG1_xxx) */
  int error:16;         /* error reason code */
  int info:16;          /* information reason code */
  int textUnit;         /* pointer to Text Unit Pointer List.  List is one fullword long per TU.  Last TU address HO bit must be set. */
  int reqBlkExt;        /* pointer to Request Block extention */
  int flag2;            /* flags (SV99_FLAG2_xxx) */
} SV99RB;

/* map sv99RBx at 20 (0x14) size is 36.  Typically extended for error recording */
#define SV99_EXTENSION_OFFSET 0x14
///SV99 Request Block Extension
typedef struct sv99RBx{
  char id[6];           /* "S99RBX" */
  char ver;             /* Version # (0x01) */
  char options;         /* Processing Options (SV99_OPTS_xxx) */
  char subpool;         /* Subpool for message blocks */
  char key;             /* Storage key for message blocks */
  char sev;             /* Severity Level for messages processing (0,4,8) */
  char numMsgs;         /* Number of message blocks returned */
  int cppl;             /* address of CPPL */
  int msgReturn;        /* Message service return code.  First two bytes RESERVED.  Third byte is message output return code.  Fourth byte is message block storage return code */
  int wtoReturn;        /* Putline/WTO return code */
  int msgBlock;         /* pointer to Message Block */
  int infoReturn;       /* Info Retrieval Return Code for SJF keys.  First two bytes are Error Reason Code.  Second two bytes are Information Reason Code */
  int smsFeedback;      /* SMS Reason Code */
} SV99RBX;

/* map sv99TU at 0 (0x00) size is variable at 6 + size of parameter */
///SV99 Text Unit
typedef struct sv99TU{
  int key:16;           /* Lookup Tables by Function: http://pic.dhe.ibm.com/infocenter/zos/v1r12/index.jsp?topic=%2Fcom.ibm.zos.r12.ieaa800%2Fbyfunc.htm */
  int qty:16;           /* Usually 0x01.  Quantity of Parameters within this Text Unit */
  int len:16;           /* Length of the Parameter.  NOTE: Remember to allocate space for the parameter itself and place it following this field. */
} SV99TU;

/* map linkddNames at 0 (0x00) size is 50 */
///DDNames List
typedef struct linkDDNames{
  int len:16;           /* Length of the list.  Always 48 (0x30) */
  char buffer[32];      /* 32 bytes of zeroes.  Values here are ignored. */
  char sysin[8];        /* DD name of SYSIN dataset, spaces for filler (0x40). If default, leaves as 0's. */
  char sysprint[8];     /* DD name of SYSPRINT dataset, spaces for filler (0x40). If default, leave as 0's. */
} LINKDDNames;



ZOWE_PRAGMA_PACK_RESET


#define opencloseACB VOPCLACB
#define putRecord VPUTREC
#define getRecord VGETREC
#define makeDataBuffer VMKDBUFF
#define freeDataBuffer VFRDBUFF
#define pointByRBA VPNBYRBA 
#define pointByKey VPNBYKEY
#define pointByRecord VPNBYREC
#define pointByCI VPNBYCI
#define allocateDataset VALLOCDS
#define defineAIX VDEFAIX
#define deleteCluster DELCLUST

char *makeACB(char *ddname,int acbLen,int macrf1,int macrf2,int rplSubint,int rplType,int rplLen,int keyLen,int opCode1,int opCode2,int recLen,int bufLen);
int opencloseACB(char *acb, int mode, int svc);

char *openACB(char *ddname, int mode,int macrf1,int macrf2,int opCode1,int recLen,int bufLen);
int closeACB(char *acb, int mode);

void modRPL(char *acb,int reqType,int keyLen,int workArea,int searchArg,int optcd1,int optcd2,int nextRPL,int recLen,int bufLen);
int point(char *acb, char *arg);
int putRecord(char *acb, char *line, int length);
int getRecord(char *acb, char *line, int *lengthRead);

char *makeDataBuffer(char *acb, int *bufferSize);
void freeDataBuffer(char *acb, char *buffer);

int deleteCluster(char *dsName);

int pointByRBA(char *acb, int *rba);
int pointByKey(char *acb, char *key, int keyLength);
int pointByRecord(char *acb, int *record);
int pointByCI(char *acb, int *rba);

int allocateDataset(char *ddname, char *dsn, int mode, char *space, int macrf1, int macrf2, int opCode1, int avgRecLen, int maxRecLen, int bufLen, int keyLen, int keyOff, char *dataclass);
int defineAIX(char *clusterdsn, char *aixdsn, char *pathdsn, char *space, int recLen, int keyLen, int keyOff, int uniqueKey, int upgrade, int debug);

#endif

int putlineV(char *dcb, char *line, int len);
int getlineV(char *dcb, char *line, int *lengthRead);


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

