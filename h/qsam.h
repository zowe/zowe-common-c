

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __QSAM__
#define __QSAM__

#ifndef __LONGNAME__


#define hasVaryingRecordLength HASVRLEN
#define makeQSAMBuffer MKQSMBUF
#define freeQSAMBuffer FRQSMBUF
#define putlineV PUTLNEV
#define getlineV GETLNEV
#define getEODADBuffer GTEODBUF
#define freeEODADBuffer FREODBUF
#define getSAMLength GTSAMLN
#define getSAMBlksize GTSAMBS
#define getSAMLrecl GTSAMLR
#define getSAMCC GTSAMCC
#define getSAMRecfm GTSAMRF
#define bpamFind BPAMFIND
#define bpamRead BPAMREAD
#define bpamRead2 BPAMRD2
#define openEXCP OPENEXCP
#define closeEXCP CLOSEXCP

#endif

#ifndef STDOUT_DD
#define STDOUT_DD "SYSPRINT"
#endif

#define DCB_ORG_IS 0x8000                                                  
#define DCB_ORG_PS 0x4000                                                  
#define DCB_ORG_DA 0x2000                                                  
#define DCB_ORG_CX 0x1000  /* BTAM or QTAM Line Group */                   
#define DCB_ORG_PO 0x0200                                                  
#define DCB_ORG_U  0x0100                                                  
#define DCB_ORG_GS 0x0080  /* Graphics */                                  
#define DCB_ORG_TX 0x0040  /* TCAM Line Group */                           
#define DCB_ORG_TQ 0x0020  /* TCAM Message Queue */                        
#define DCB_ORG_ACB 0x0008 /* VSAM ACB */                                  
#define DCB_ORG_TR 0x0004  /* TCAM 3705 */                                 
                                                                           
#define DCB_MACRF_EXCP     0x8000 /* 0 for all real access methods */      
#define DCB_MACRF_FND_EXTN 0x4000 /* Foundation extension, EXCP */         
#define DCB_MACRF_GET      0x4000 /* QSAM, QISAM, TCAM */                  
#define DCB_MACRF_PUT_MSG  0x4000 /* QTAM, 0 for others */                 
#define DCB_MACRF_APND_REQ 0x2000 /* Appendages Req'd, EXCP */             
#define DCB_MACRF_READ     0x2000 /* BSAM, BPAM BISAM, BDAM BTAM */        
#define DCB_MACRF_WRT_LG   0x2000 /* Wrt Line Group QTAM */                
#define DCB_MACRF_CI       0x1000 /* Common Interface, EXCP */             
#define DCB_MACRF_MV_GET   0x1000 /* Move Mode of GET, QSAM QISAM */       
#define DCB_MACRF_READ_KEY 0x1000 /* Key Seg w/Read, BDAM */               
#define DCB_MACRF_LOC_GET  0x0800 /* Locate Mode of GET, Q(I)SAM */        
#define DCB_MACRF_READ_ID  0x0800 /* Id Arg w/Read, BDAM */                
#define DCB_MACRF_ABC      0x0400 /* PGM kps acurate bkl cnt, EXCP */      
#define DCB_MACRF_PT1      0x0400 /* Point, impls NOTE, BSAM BPAM */       
#define DCB_MACRF_SUB_GET  0x0400 /* Subst mode of get, QSAM */            
#define DCB_MACRF_DYN_BUF  0x0400 /* Dyn Buffering, BISAM, BDAM */         
#define DCB_MACRF_PG_FX_AP 0x0200 /* Page Fix apndge, EXCP */              
#define DCB_MACRF_CNTRL    0x0200 /* CNTRL, BSAM, QSAM */                  
#define DCB_MACRF_CHECK    0x0200 /* Check, BISAM */                       
#define DCB_MACRF_RD_EXCL  0x0200 /* Read Exclusive, BDAM */               
#define DCB_MACRF_DATA_GET 0x0100 /* Data Mode of GET, QSAM */             
#define DCB_MACRF_CHECK2   0x0100 /* Check, BDAM */                        
#define DCB_MACRF_SETL     0x0080 /* SETL, QISAM */                        
#define DCB_MACRF_PUT      0x0040 /* PUT (QSAM,TCAM) PUT(X) QISAM */       
#define DCB_MACRF_GTQ      0x0040 /* Get for Msg Grp, QTAM */              
#define DCB_MACRF_WRITE    0x0020 /* BSAM, BPAM, BISAM, BDAM, BTAM */      
#define DCB_MACRF_READ_LG  0x0020 /* Read LineGrp, QTAM */                 
#define DCB_MACRF_MV_PUT   0x0010 /* Move Mode of PUT, QSAM QISAM */       
#define DCB_MACRF_WRT_KEY  0x0010 /* Key Seg w/Write, BDAM */              
#define DCB_MACRF_5_WORD   0x0008 /* 5-Word Dev Intf, EXCP */              
#define DCB_MACRF_LD_MODE  0x0008 /* Load Mode (Crt BDAM) BSAM */          
#define DCB_MACRF_LOC_PUT  0x0008 /* Locate Mode of PUT, Q(I)SAM */        
#define DCB_MACRF_WRT_ID   0x0008 /* Id Arg w/Write, BDAM */               
#define DCB_MACRF_4_WORD   0x0004 /* 4-Word Dev Intf, EXCP */              
#define DCB_MACRF_PT2      0x0004 /* Point, impls NOTE, BSAM BPAM */       
#define DCB_MACRF_SUB_MODE 0x0004 /* Subst mode, QSAM */                   
#define DCB_MACRF_UIP      0x0004 /* Update in Place, QISAM */             
#define DCB_MACRF_3_WORD   0x0002 /* 3-Word Dev Intf, EXCP */              
#define DCB_MACRF_CNTRL2   0x0002 /* CNTRL, BSAM QSAM */                   
#define DCB_MACRF_SETL_KEY 0x0002 /* SETL by Key, QISAM */                 
#define DCB_MACRF_ADD_WRT  0x0002 /* Add Type of Write, BDAM */            
#define DCB_MACRF_1_WORD   0x0001 /* 1-Word Dev Intf, EXCP */              
#define DCB_MACRF_PGM_WA   0x0001 /* UsrPgm prvd WrkArea BSAM BDAM */      
#define DCB_MACRF_DATA_MOD 0x0001 /* Data Mode, QSAM */               
#define DCB_MACRF_SETL_ID  0x0001 /* SETL by ID, QISAM */             
                                                                      
/* DCB RECFM primary formats */
#define DCB_RECFM_FORMAT 0xE0
#define DCB_RECFM_F   0x80
#define DCB_RECFM_V   0x40
#define DCB_RECFM_U   0xC0
#define DCB_RECFM_D   0x20
/* DCB RECFM modifiers */
#define DCB_RECFM_B   0x10
#define DCB_RECFM_S   0x08
#define DCB_RECFM_A   0x04
#define DCB_RECFM_M   0x02
/* Combined DCB RECFM values */
#define DCB_RECFM_FBA 0x94                                            
#define DCB_RECFM_VBA 0x54                                            
#define DCB_RECFM_FB  0x90                                            
#define DCB_RECFM_VB  0x50                                            
#define DCB_RECFM_FBS 0x98                                            
#define DCB_RECFM_VBS 0x58                                            

typedef __packed struct dcbExtension{ /* Aka, the DCBE */
  char id[4];
  short int len;
  char rsvd006[2];
  void * __ptr32 dcb;
  unsigned int rela;
  __packed union {
    char flg1;       /* Set by OPEN processing */
    __packed struct {
      int open : 1;
      int md31 : 1;
      int slbi : 1;
      int xcapused : 1;
      int rsvd010_4 : 4;
    };
  };
  __packed union {
    char flg2;       /* Set by requestor prior to OPEN */
    __packed struct {
      int rmode31 : 1;
      int pasteod : 1;
      int rsvd011_2 : 1;
      int nover   : 1;
      int getsize : 1;
      int lbi     : 1;
      int xcapreq : 1;
      int rsvd011_7 : 1;
    };
  };
  short int nstr;
  __packed union {
    char flg3;       /* Set by requestor prior to OPEN */
    __packed struct {
      int large   : 1;
      int fixedbuf: 1;
      int eadscb  : 1;
      int rsvd014_3 : 2;
#define DCBE_SYNC_SYSTEM 1
#define DCBE_SYNC_NONE   7
      int sync    : 3;
    };
  };
  char rsvd015[7];
  int blksi;
  long long xsiz;
  void * __ptr32 eoda;
  void * __ptr32 syna;
  char rsvd030[6];
  char macc;
  char msdn;
} DCBExtension;

/* map dcbDevice at 0 size is 20 */
typedef __packed struct dcbDevice{
  __packed struct dcbExtension * __ptr32 dcbe;
  char unmapped[16]; /* A complex, device type sensitive, portion of the DCB that we do not need */
} DCBDevice;
                                                                      
/* map dcbCommon at 20 (0x14) size is 32 */                           
#define DCB_COMMON_OFFSET 0x14                                        
#define VB_RECORD_OFFSET 4

typedef __packed struct dcbCommon{
  /* OFFSET 0x14 */
  char bufno;    /* 0 if no pool */                                   
  int  bufcb:24; /* buffer pool control block = 000001 if no pool */  
  /* OFFSET 0x18 */
  int  bufl:16;  /* buffer length */                                  
  int  dsorg:16; /* need #define's */                                 
  /* OFFSET 0x1C */
  int  reserved1; /* IOBAD or RESERVED FOUNDATION EXTENSION */        
  /* OFFSET 0x20 */
  char dcbeIndicators;  /* BFTEK, BFALN */                            
  int  eodad:24;        /* endofdata routine addr, =1 if not set*/    
  /* OFFSET 0x24 */
  char recfm;
  int  exlst:24;        /* exit list addr, 0 if not set */            
  /* OFFSET 0x28 */
  union {
    char ddname[8];       /* overwritten after open - mapped by */
    struct {
      int tiotOffset:16;
      int openmacroFormat:16;
      char openiosFlags;
      int debAddress:24;
    };
  };
  /* OFFSET 0x30 */
  char openFlags;  /* 0x10 - DCB_OPENFLAGS_OPEN - is considered success */
  char iosFlags;
  int  macroFormat:16;
} DCBCommon;                                                                 

/* The #define for DCBOFOPN has been deprecated, and will be removed
   in the future.  The function isOpen, declared in metalio.h, is the
   preferred way to test if the DCB created by qsam.c is open.        */

#define DCBOFOPN 0x10
#define DCB_OPENFLAGS_OPEN 0x10
#define DCB_OPENFLAGS_BUSY 0x01

/* map dcbBBQCommon at 52 (0x34) BSAM/BPAM/QSAM common - size 20 */
#define DCB_BBQ_COMMON_OFFSET 0x34
                                                                      
typedef __packed struct dcbBBQCommon {
  char optionCodes;  /* OPTCD */
  int  check:24;     /* or internal QSAM ERROR RTN */
  int  synad;        /* synchns err rtn addr, =1 if not set */
  int  internalFlags1:16;
  union{
    unsigned short int  blksize;
    short int blksize_s;
  };
  int  internalFlags2;
  int  internalMethodUse; /* seen "00000001" */
} DCBBBQCommon;

/* map dcbBsamBpamInterface at 72 (0x48) */
#define DCB_BSAM_BPAM_OFFSET 0x48

typedef __packed struct dcbBsamBpamInterface{
  char ncp;      /* MAX NUM OF OUTSTANDING READ/WRITES */   
  int  eobr:24;  /* internal AM use */                      
  int  eobw;     /* internal AM use */                      
  int  flags:16; /* flags */                                
  int  lrecl:16;                                            
  int  cnp;      /* control, note, point */                 
} DCBBBQInterface;

typedef __packed struct {
  DCBDevice Device;
  DCBCommon Common;
  DCBBBQCommon BBQCommon;
  DCBBBQInterface BBQInterface;
} DCBSAM;

/* There is lots of code that adds 4 bytes to skip past the DCB
   open list that preceeds the DCB.  Do not declare additional
   variables prior to the DCB.                                 */
typedef __packed struct {
  union {
    DCBSAM * __ptr32 OpenCloseList;
    unsigned int OpenCloseListOptions;
  };
  DCBSAM dcb;
  DCBExtension dcbe;
} dcbSAMwithPlist;

#define OPEN_CLOSE_DISP    0x00000000
#define OPEN_CLOSE_REWIND  0x40000000
#define OPEN_CLOSE_LEAVE   0x30000000
#define OPEN_CLOSE_FREE    0x20000000
#define OPEN_CLOSE_REREAD  0x10000000
#define OPEN_CLOSE_INPUT   0x00000000
#define OPEN_CLOSE_OUTPUT  0x0F000000
#define OPEN_CLOSE_UPDAT   0x04000000
#define OPEN_CLOSE_OUTIN   0x07000000
#define OPEN_CLOSE_INOUT   0x03000000
#define OPEN_CLOSE_RDBACK  0x01000000
#define OPEN_CLOSE_EXTEND  0x0E000000
#define OPEN_CLOSE_OUTINX  0x06000000

typedef __packed struct {
  char flag1;
  char flag2;
  char sens0;
  char sens1;
  void * __ptr32 ecb;
  char status[6];    /* Status was not fully mapped */
  short int residual;
  void * __ptr32 start;
  void * __ptr32 dcb;
  void * __ptr32 restart;
  short int incam;
  short int errct;
  char seek[8];
} IOB;

typedef __packed struct DECB_tag{
  int ecb;
  char type1;  /* 0x80 - "S" coded for length */
  char type2;  /* 0x80 is a suggestion */
  short int length;
  void * __ptr32 dcb;
  void * __ptr32 data;
  IOB  * __ptr32 iob;
  int   rsvd;
  // Note: The DECB for BISAM is 8 bytes longer
  // Note: The DECB for BDAM is 12 bytes longer
} DECB;

typedef struct {
  unsigned short length;
  unsigned short rsvd;
} RDW;

char *malloc24(int size);

char *openSAM(char *ddname, int mode, int isQSAM,
	      int recfm, int lrecl, int blksize);

int getSAMLength(char *dcb);     /* Note - Record length does *not* include the RDW for variable format records */

unsigned short getSAMBlksize(char *dcb);

int getSAMLrecl(char *dcb);      /* Note - Record length includes the RDW for variable format records */

int getSAMCC(char *dcb);         /* Returns DCB_RECFM_A, DCB_RECFM_M, or 0 */

int getSAMRecfm(char *dcb);      /* Returns DCB_RECFM_* value */

int putline(char *dcb, char *line);
int getline(char *dcb, char *line, int *lengthRead);

/* 
   getline allocs an instruction buffer which is static. This is not so great
   for restricted environments <cough>ITM<cough>. getline2 allows you to manage
   a non-static var (via get/freeEODADBuffer). 
 */
char *getEODADBuffer();
void freeEODADBuffer(char *EODAD);
int getline2(char *EODAD, char *dcb, char *line, int *lengthRead);

/* The following two functions are a convenience for making appropriate sized io buffers */
char *makeQSAMBuffer(char *dcb, int *bufferSize);
void freeQSAMBuffer(char *dcb, char *buffer);

int putlineV(char *dcb, char *line, int len);
int getlineV(char *dcb, char *line, int *lengthRead);

void closeSAM(char *dcb, int mode);

char *openEXCP(char *ddname);
void closeEXCP(char *dcb);

int hasVaryingRecordLength(char *dcb);

int bpamFind(char * __ptr32 dcb, char * __ptr32 memberName, int *reasonCode);
int bpamRead(void * __ptr32 dcb, void * __ptr32 buffer);
int bpamRead2(void * __ptr32 dcb, void * __ptr32 buffer, int *lengthRead);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

