

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOS__
#define __ZOS__  1

#ifndef __LONGNAME__

#define supervisorMode SUPRMODE
#define ddnameExists DDEXISTS
#define atomicIncrement ATOMINCR

#define getCurrentACEE GTCRACEE
#define getFirstChildTCB GTFCHTCB
#define getParentTCB GTPRTTCB
#define getNextSiblingTCB GTNXSTCB

#define getCVTPrefix GETCVTPR
#define getIEACSTBL  GETCSTBL

#define getSysplexName GTSPLXNM
#define getSystemName GTSYSTNM

#define dsabIsOMVS DSABOMVS

#define getR13 GETR13
#define getR12 GETR12

#define getASCBJobname GETASCBJ

#endif


int extractPSW(void);
int supervisorMode(int enable);
int setKey(int key);
int ddnameExists(char *ddname);
int atomicIncrement(int *intPointer, int increment);

ZOWE_PRAGMA_PACK

typedef struct cvtfix_tag{
  char reserved[216];
  char cvtprodn[8];  /* product (MVS/OS390/ZOS) name and version */
  char cvtprodi[8];  /* the fmid */  
  char cvtverid[16]; /* user personalization of software */
  short reserved1;
  char cvtmdl[2]; /* product model number, signed packed decimal */
  char cvtnumb[2]; /* release number */
  char cvtlevl[2]; /* level number of release, e.g. YM2188 */
} cvtfix;

typedef struct cvt_tag{               
  Addr31 cvttcbp;  /* PSATNEW address */
  Addr31 cvt0ef00; /* async exit */
  Addr31 cvtlink; /* dcb of SYS1.LINKLIB */
  Addr31 cvtauscb; /* addr of assign/unassign module */
  Addr31 cvtbuf;
  Addr31 cvtxapg; /* 0x14 supervisor appendage vector table */
  Addr31 cvt0vl00; /* address validity routine */
  Addr31 cvtpcnvt; /* rel-track TTR to MBBCCHHR routine */
  Addr31 cvtprltv; /* MBBCCHHR to TTR routine */
  Addr31 cvtllcb;  /* 024 what's an LLCB ? */
  Addr31 cvtlltrm;  /* LLA term manager */
  Addr31 cvtxtler; /* ERP (error recovery procedure) loader */
  Addr31 cvtsysad; /* UCB addr of sys residence vol MDCXXX */
  Addr31 cvtbterm; /* 0x34 addr of ABTERM routine */
  int  cvtdate;   /* current data in packed decimal */
  Addr31 cvtmslt;  /* master common area in master scheduler */
  Addr31 cvtzdtab; /* addr of device characteristic table */
  Addr31 cvtxitp;  /* 0x44 addr of error interpreter routine */
  Addr31 cvtef01;  
  short cvtvss;    /* vector section size */
  short cvtvpsm;   /* vector partial sum */
  short cvtexit;   /* SVC 3 instruction */
  short cvtbret;   /* BR14 instruction */
  Addr31 cvtsvdcb; /* 0x54 addr of DCB for SYS1.SVCLIB */
  Addr31 cvttpc;   /* addr of timer */
  char cvtflgc0;
  char cvtflgc1;
  short cvticpid;  /* IPL'ED CPU Phys ID */
  char cvtcvt[4];  /* CVT eyecatcher */
  Addr31 cvtcucb;  /* 0x64 addr of UCM (unit control module) */
  Addr31 cvtqte00; /* timer enqueue routine for interval timer */
  Addr31 cvtdqd00; /* timer dequeue for same */
  Addr31 cvtstb;   /* addr of device stats table */
  int  cvtdcb;     /* 0x74 high byte flags, low three bytes addr of DCB for SYS1.LOGREC */
  int  cvtsv76m;   /* SVC 7 message count field */
  Addr31 cvtixavl; /* supervisor's free list ptr */
  Addr31 cvtnucb;  /* reserved */
  Addr31 cvtfbosv; /* 0x84 address of program fetch routine */
  Addr31 cvt0ds;   /* addr of dispatcher */
  Addr31 cvtecvt;  /* addr of extended CVT , ECVT */
  Addr31 cvtdairx; /* addr of IKJDAIR TSO DYNAMIC ALLOC INTERFACE */
  Addr31 cvtmser;  /* 0x94 addr of data area of master scheduler res data area */
  Addr31 cvt0pt01; /* address of branch entry pt of POST routine */
  Addr31 cvttvt;   /* addr of TSO vector table */
  Addr31 cvt040id; /* WTO message ID, OBR */
  Addr31 cvtmz00;  /* 0xA4 highest addr in virt storage for machine */
  Addr31 cvt1ef00; /* addr of routine that creates IRB's for exits */
  Addr31 cvtqocr;  /* graphics GFX */
  Addr31 cvtqmwr;  /* addr of alloc communication area IEFZB432 */
  short cvtsnctr; /* 0xB4 serial number counter for unlabeled tapes */
  char cvtopta;   /* flags */
  char cvtoptb;   /* more flags */
  Addr31 cvtqcdsr; /* cde search routine addr */
  Addr31 cvtqlpaq; /* address of ptr most rec. entry on LPA */
  Addr31 cvtenfct; /* event notification control table */
  Addr31 cvtsmca;  /* 0xC4 system mgmt control area SMCA if SMF avail */
  Addr31 cvtabend;  /* addr of secondary CVT for abend in EOT */
  Addr31 cvtuser;  /* user word */
  Addr31 cvtmdlds; /* model dependent support */
  short cvtqabst; /* 0xD4 an SVC 13 - ABEND */
  short cvtlnksc; /* an SVC 6 - LINK */
  Addr31 cvttsce;  /* addr of first time slice control elem TSCE */
  Addr31 cvtpatch; /* addr of 200 byte FE patch area */
  Addr31 cvtrms;   /* addr of recov mgmt support comm vector */
  Addr31 cvtspdme; /* 0xE4 addr of processor damage monitor ECB */
  Addr31 cvt0scr1; /* addr of calc routine for rotational positoin sensing */
  int   cvtgtf; /* high byte flags, low 3 addr of main monitor call routing table */
  Addr31 cvtaqavt;  /* addr of TCAM dispatcher */
  Addr31 cvtrs0f4; /* 0xF4 reserved */
  Addr31 cvtsaf;   /* addr of SAF router vector table */
  Addr31 cvtext1;  /* addr of OS/VS COMMON extension ICB421 */
  Addr31 cvtcbsp;  /* addr of acc.meth. cb structure, MDC195 */
  Addr31 cvtpurg;  /* 0x104 addr of subsys purge routine ICB330, split 1/3 */
  int cvtamff;  /* acces method flags */
  Addr31 cvtqmsg; /* address of info to be printed by abend */
  Addr31 cvtdmsr; /* addr open/close/eov sup routine in nucleus, split 1/3 */
  Addr31 cvtsfr;  /* 0x114 address of SETFRR routine IEAVTSFR */
  Addr31 cvtgxl;  /* address of contents sup mem term routine */
  Addr31 cvtreal; /* address of virt storage byte following highest V=R */
  Addr31 cvtptrv; /* address of paging supervisor general routine 24real->virtual */
  Addr31 cvtihvp; /* 0x124 addr of IHV$COMM IHVSTRTM */
  Addr31 cvtjesct;/* JES control table, ICB342 */
  int  cvtrs12c;  /* reserved bit field */
  int  cvttz;     /* diff between local and GMT */
  Addr31 cvtmchpr; /* 0x134 machine check parm list */
  Addr31 cvteorm;  /* pre-Z arch potential real high storage addr, ECVTEORM */
  Addr31 cvtptrv3; /* 0xx13C */
  Addr31 cvtlkrm;
  Addr31 cvtapf;   /* 0x144 */
  Addr31 cvtext2;
  Addr31 cvthjes;  /* 0x14C */
  Addr31 cvtrstw2; /* stcp, trs */
  char cvtsname[8];
  Addr31 cvtgetl;  /* 0x15C */
  Addr31 cvtlpdsr; /* LPD ?? */
  Addr31 cvtpvtp;  /* 0x164 */
  Addr31 cvtlpdia; /* cvtdirst, cvtdicom, ctlpdir */
  Addr31 cvtrbcb;  /* 0x16C */
  Addr31 cvtrs170; /* 0x170, surprisingly */
  Addr31 cvtslida; 
  char cvtflags[4];
  Addr31 cvtrt03;
  char cvtrs180[8];
  char iAmTired[0x98];
  Addr31 cvtcsrt;  /* callable services */
  Addr31 cvtaqtop; /* pnter to allocation queue lock area */
  Addr31 cvtvvmdi; /* owned by (P)LPA search algorithm */
  Addr31 cvtasvt;  /* address space vector table */
  Addr31 cvtgda;   /* global data area */
  Addr31 cvtascbh; /* highest ASCB on dispatch queue */
  Addr31 cvtascbl; /* lowest ASCB on dispatch queue */
  Addr31 cvtrtmct; /* pointer recovery/termination control table */
  Addr31 cvtsv60;  /* "v(ieavstag)" - branch entry address for 24 or 31 bit addressing mode users of svc 60. @(dcr854) 
                       entry to a glue routine. */
  Addr31 cvtsdmp;  /* "v(ieavtsgl)" - address of svc dump branch entry point @(dcr664) */
  Addr31 cvtscbp;  /* "v(ieavtsbp)" - address of scb purge resource manager. */
  Addr31 cvtsdbf;  /* - address of 4k sqa buffer used by svc dump. high-order bit
                      of this cvt word is used as lock to indicate buffer is in use mdc035 */
  Addr31 cvtrtms;  /* - address of servicability level indicator processing (slip) header (mdc358) */
  Addr31 cvttpios; /* - address of the teleprocessing i/o supervisor routine (tpios) */
  Addr31 cvtsic;   /* - branch address of the routine to schedule system initialized cancel */
  Addr31 cvtopctp; /* "v(irarmcns)" - address of system resources manager (srm) control table mdc043 */
  Addr31 cvtexpro; /* "v(ieavexpr)" - address of exit prologue/type 1 exit mdc044 */
  Addr31 cvtgsmq;  /* "v(ieagsmq)" - address of global service manager queue mdc045 */
  Addr31 cvtlsmql; /* "v(iealsmq)" - address of local service manager queue mdc046 */
  int   cvtrs26c;  /*    - reserved. */
  Addr31 cvtvwait; /*  "v(ieavwait)" - address of wait routine mdc048  */
  Addr31 cvtparrl; /* "v(csvarmgr)" - address of partially loaded delete queue. */
  Addr31 cvtapftl; /*   - address of authorized program facility (apf) table. initialized by nip. */
  Addr31 cvtqcs01; /*  "v(ieaqcs01)" - branch entry address to program manager used by attach mdc051 */
  Addr31 cvtfqcb;  /*   - formerly used by enq/deq.  should always be zero. (mdc414) */
  Addr31 cvtlqcb;  /*      - formerly used by enq/deq.  should always be zero. (mdc414) */
  Addr31 cvtrenq;  /*  "v(ieavenq2)" - resource manager address for enq */
  Addr31 cvtrspie; /*  - resource manager for spie. @(pcc1076) */
  Addr31 cvtlkrma; /* "v(ieavelrm)" - resource manager address for lock manager. */
  Addr31 cvtcsd;   /*  - virtual address of common system data area (csd). initialized by nip. */
  Addr31 cvtdqiqe; /*  "V(IEADQIQE)" - RESOURCE MANAGER FOR EXIT EFFECTORS. */
  Addr31 cvtrpost; /*  "V(IEARPOST)" - RESOURCE MANAGER FOR POST. */
  Addr31 cvt062r1; /*  "V(IGC062R1)" - BRANCH ENTRY TO DETACH MDC060 */
  Addr31 cvtveac0; /*  "V(IEAVEAC0)" - ASCBCHAP BRANCH ENTRY MDC061 */
  Addr31 cvtglmn;  /*  "V(GLBRANCH)" - GLOBAL BRANCH ENTRY ADDRESS FOR GETMAIN/FREEMAIN MDC062 */
  Addr31 cvtspsa;  /*  "V(IEAVGWSA)" - POINTER TO GLOBAL WORK/SAVE AREA VECTOR TABLE (WSAG) MDC071 */
  Addr31 cvtwsal;  /*  "V(IEAVWSAL)" - ADDRESS OF TABLE OF LENGTHS OF LOCAL WORK/SAVE AREAS MDC072 */
  Addr31 cvtwsag;  /*  "V(IEAVWSAG)" - ADDRESS OF TABLE OF LENGTHS OF GLOBAL WORK/SAVE AREAS (MDC391) */
  Addr31 cvtwsac;  /*  "V(IEAVWSAC)" - ADDRESS OF TABLE OF LENGTHS OF CPU WORK/SAVE AREAS MDC074 */
  Addr31 cvtrecrq; /*  "V(IEAVTRGR)" - ADDRESS OF THE RECORDING REQUEST FACILITY (PART OF RTM1 - CALLED BY RTM2 AND RMS). @(DCR854) */
  Addr31 cvtasmvt; /*  "V(ASMVT)" - POINTER TO AUXILIARY STORAGE MANAGEMENT VECTOR TABLE (AMVT) (MDC340) */
  Addr31 cvtiobp;  /*  "V(IDA121CV)" - ADDRESS OF THE BLOCK PROCESSOR CVT (MDC079) YM0029 */
  Addr31 cvtspost; /*  "V(IEASPOST)" - POST RESOURCE MANAGER TERMINATION ROUTINE (RMTR) ENTRY POINT MDC085 */
/*int    cvtrstwd      overlay of next two fields
                       CVTRSTWD     - RESTART RESOURCE (0)          MANAGEMENT WORD. CONTAINS IDENTIFIER
                        OF USER IF RESTART IS IN USE.  OTHERWISE, ZERO. */
  short cvtrstci;  /*   CVTRSTCI     - CPU ID OF THE CPU HOLDING THE RESTART RESOURCE. */
  short cvtrstri;  /*   CVTRSTRI     - IDENTIFIER OF OWNING ROUTINE  */
  Addr31 cvtfetch; /*  "V(IEWMSEPT)" - ADDRESS OF ENTRY POINT FOR BASIC FETCH. */
  Addr31 cvt044r2; /*  "V(IGC044R2)" - ADDRESS OF IGC044R2 IN CHAP SERVICE ROUTINE MDC197 */
  Addr31 cvtperfm; /*  CVTPERFM     - ADDRESS OF THE PERFORMANCE WORK AREA.  SET BY IGX00018. MDC205 */
  Addr31 cvtdair;  /*  CVTDAIR      - ADDRESS OF IKJDAIR, TSO DYNAMIC ALLOCATION INTERFACE ROUTINE (MDC212) YM2225 */
  Addr31 cvtehdef; /*  CVTEHDEF     - ADDRESS OF IKJEHDEF, TSO DEFAULT SERVICE ROUTINE. @(PCC0919) */
  Addr31 cvtehcir; /*  CVTEHCIR     - ADDRESS OF IKJEHCIR, TSO CATALOG INFORMATION ROUTINE. @(PCC0919) */
  Addr31 cvtssap;  /*  CVTSSAP      - ADDRESS OF SYSTEM SAVE AREA */
  Addr31 cvtaidvt; /*  CVTAIDVT     - POINTER TO APPENDAGE ID VECTOR TABLE */
  Addr31 cvtipcds; /*  "V(IEAVEDR1)" - BRANCH ENTRY FOR DIRECT SIGNAL SERVICE ROUTINE */
  Addr31 cvtipcri; /*  "V(IEAVERI1)" - BRANCH ENTRY FOR REMOTE IMMEDIATE SIGNAL SERVICE ROUTINE */
  Addr31 cvtipcrp; /*  "V(IEAVERP1)" - BRANCH ENTRY FOR REMOTE PENDABLE SIGNAL SERVICE ROUTINE */
  Addr31 cvtpccat; /*   CVTPCCAT     - POINTER TO PHYSICAL CCA VECTOR TABLE */
  Addr31 cvtlccat; /*   CVTLCCAT     - POINTER TO LOGICAL CCA VECTOR TABLE */
  Addr31 cvtxsft;  /*  "V(IEAVXSFT)" - ADDRESS OF SYSTEM FUNCTION TABLE CONTAINING LINKAGE INDEX (LX) AND ENTRY
                        INDEX (EX) NUMBERS FOR SYSTEM ROUTINES. (MDC414) */
  Addr31 cvtxstks; /*  "V(IEAVXSTS)" - ADDRESS OF PCLINK STACK (SAVE=YES) ROUTINE. (MDC395) */
  Addr31 cvtxstkn; /*  "V(IEAVXSTN)" - ADDRESS OF PCLINK STACK (SAVE=NO) ROUTINE. (MDC395) */
  /* OFFSET 0x310 */
  Addr31 cvtxunss; /*  "V(IEAVXUNS)" - ADDRESS OF PCLINK UNSTACK (SAVE=YES) ROUTINE. (MDC395) */
  Addr31 cvtpwi;   /*   CVTPWI       - ADDRESS OF THE WINDOW INTERCEPT ROUTINE (MDC104) YM4043 */
  Addr31 cvtpvbp;  /*   CVTPVBP      - ADDRESS OF THE VIRTUAL BLOCK PROCESSOR (MDC105) YM4043 */
  Addr31 cvtmfctl; /*   CVTMFCTL     - POINTER TO MEASUREMENT FACILITY CONTROL BLOCK MDC100 */
  
  /* OFFSET 0x320 */
  Addr31 cvtmfrtr; /*    measurement control facility routine (or CVTBRET) */
  Addr31 cvtvpsib; /*   V(IARPSIV) branch entry to page services */
  Addr31 cvtvsi;   /*   V(IARXVIO) branch entry to VAM services */
  Addr31 cvtexcl;  /*   V(IECVEXCL)  addressof EXCP term routine */

  /* OFFSET 0x330 */
  Addr31 cvtxunsn; /*   V(IEAVXUNN) address of PCLINK unstack (save=NO) */
  Addr31 cvtisnbr; /*   V(ISNBRNCH) EP address of disabled processor interface mod*/
  Addr31 cvtxextr; /*   V(IEAVXEXT) address of PCLINK extract routine */
  Addr31 cvtmsfrm; /*   V(IEAVMFRM) address of the processor controller */
  
  /* OFFSET 0x340 */
  Addr31 cvtscpin; /*   address of IPL time SCPINFO block.  Mapped by IHASCCB */
                  /*   ECVTSCPIN has current */
  Addr31 cvtwsma;  /*   Wait State message area displayable by operator */
  Addr31 cvtrmbr;  /*   V(RMBRANCH) address of REGMAIN branch entry */
  Addr31 cvtlfrm;  /*   V(FMBRANCH) address of FREEMAIN branch entry */
  
  /* OFFSET 0x350 */
  Addr31 cvtgmbr;  /*   V(GMBRANCH) address of GETMAIN branch entry */
  Addr31 cvt0tc0a; /*   address of task close module IFG0TC0A */
  int   cvtrlstg; /*   address if real storage online at IPL in K */
  Addr31 cvtspfrr; /*   V(IEAVESPR) 'Super FRR' protects CP */
  
  /* OFFSET 0x360 */
  int   cvtrs360; /*   reserved */
  Addr31 cvtsvt;   /*   V(IEAVESVT) address pointer for fetch protected PSASVT */
  Addr31 cvtirecm; /*   addr of Initiator Resource Manager */
  Addr31 cvtdarcm; /*   addr of Device Allocator Resource Manager */
  
  /* OFFSET 0x370 */
  char  unmapped370[0x70];

  /*   OFFSET 0x3E0, RCVT */
  Addr31 cvtrac;   /*   addr of RACF communication vector table */
  Addr31 cvtcgl;   /*   Addr of routine used to change key of virt pages */
  Addr31 cvtsrm;   /*   Addr of entry table for SRM */
  Addr31 cvt0pt0E; /*   Addr of entry point to identify post exit routines */
  /* OFFSET 0x3F0 */
  Addr31 cvt0PT03; /*   Addr of reinvocation entry point from post exit routine */
  Addr31 cvttcasp; /*   Addr of the TSO/VTAM TCAS - Terminal Control Address Space */
  Addr31 cvtcttvt; /*   The CTT VT - what? */
  Addr31 cvtjterm; /*   Auxiliary storage management job termination resource manager */

  /* OFFSET 0x400 */
  char  unmapped400[0xB0]; 
  
  /* OFFSET 0x4B0 */
  Addr31 cvtnucmp; /*   address of nucleus map, array of 16-byte entries */
  /*
    many, many slots follow this point.
    They are not needed or mapped yet 
    */
} CVT;

typedef struct ASVT_tag{
  char reserved1[472];
  Addr31 asvtreua;
  Addr31 asvtravl;
  int   asvtaav;
  int   asvtast;
  int   asvtanr;
  int   asvtstrt;
  int   asvtnonr;
  int   asvtmaxi; 
  char  reserved2[8]; /* 500 */
  char  asvtasvt[4]; /* eyecatcher */
  int   asvtmaxu; /* max addr spaces */
  int   asvtmdsc;
  int   asvtfrst; /* first free */
  int   asvtenty; /* first entry */
  /* variable number of ASCB pointers */
} ASVT;

#define CURRENT_TCB      0x21C                
#define CURRENT_ASCB     0x224               
#define ATCVT_ADDRESS    0x408
#define ASCB_CSCB_OFFSET  0x38            
#define CSCB_ACTIVITY_FLAGS_OFFSET 0x7   

typedef struct PSA_tag{
  int a;
  int b;
  int c;
  int d;
  CVT *__ptr32 cvtptr;
  unsigned char reserved2[0x21C-0x14];
  struct tcb_tag * __ptr32 psatold;
  unsigned char reserved3[0x224-0x220];
  struct ascb_tag * __ptr32 psaaold;
  unsigned char reserved4[0x49C-0x228];
  unsigned int psamodew;
} PSA;

typedef struct ecvt_tag{
  char  eyecatcher[4];
  Addr31 ecvtcplx;   /* address IXCCPLX */
  char  ecvtsplx[8];/* Sysplex name used for debugging */
  int   ecvtsple;   /* sysplex partitioning ECB */
  Addr31 ecvtsplq;   /* sysplex partitioning queue */
  Addr31 ecvtstc1;   /*  V(IEATSTC1) */
  Addr31 ecvtstc2;   /*  V(IEATSTC2) */
  /* offset 0x20 here */
  Addr31 ecvtstc3;   /*  V(IEATSTC3) */
  Addr31 ecvtstc4;   /*  V(IEATSTC4) */

  Addr31 ecvtappc;   /*  Anchor for APPC structures */
  Addr31 ecvtsch;    /*  Anchor for APPC scheduler data structures */
  /* offset 0x30 here */
  int   ecvtiosf;   /*  IOS Flags  --- offset 0x30*/
  Addr31 ecvtomda;   /* operations measurement data - console services */
  short ecvtr038;   /* reserved */
  char  ecvtcnz;
  char  ecvtaloc;   /* flags */
  Addr31 ecvtbpms;   /* BELOW 16M, pageable device support start addr */
  Addr31 ecvtbpme;   /* BELOW 16M, pageable device support end   addr - offset x40 */
  Addr31 ecvtapms;   /* ABOVE 16M, pageable device support start addr */
  Addr31 ecvtapme;   /* ABOVE 16M, pageable device support end   addr  */
  Addr31 ecvtqucb;   /* XCF data area IXCTQUCB anchor - offset 0x4C */
  Addr31 ecvtssdf;   /* free SSD queue */
  int   ecvtssds;   /* sequence number for previous and compare double swap */
  int   ecvtr058;   /* reserved */
  Addr31 ecvtsrbt;   /* SSD resource manager */
  Addr31 ecvtdpqh;   /* Queue of DU-ALpools for PC Auth - offset 0x60 */
  Addr31 ecvttcre;   /* V(IEAVTCRE) entry */
  char  ecvtxcfg[16];  /* Sysplex Configuration requirements bitstring */
  int   ecvtr078;   /* reserved */
  int   ecvtr07C;   /* reserved */
  /* Offset 0x80 Here */
  Addr31 ecvtscha;   /* V(IEAVSCHA) schedule */
  int   ecvtr084;   /* reserved */
  Addr31 ecvtdlcb;   /* CSVDLCB DLCB for the Current LNKLST set */
  Addr31 ecvtnttp;   /* address of system level name/token header */
  /* Offset 0x90 Here */
  Addr31 ecvtsrbj;   /* V(IEAVJOIN) - see book */
  Addr31 ecvtsrbl;   /* V(IEAVLEAV) - see book */
  Addr31 ecvtmsch;   /* address of SLM (XES) subchannel list */
  Addr31 ecvtcal;    /* address of Common Area List (CAL) of System Lock Manager (SLM) */
  /* Offset 0xA0 Here */
  char ecvtload[8]; /* edited MVS load parameter */
  char ecvtmlpr[8]; /* load parameter used for this IPL */
  /* offset 0xB0 Here */
  Addr31 ecvttcp;    /* Token used by TCPIP */
  int   ecvtr0B4;   /* reserved */
  Addr31 ecvtnvdm;   /* Netview DM TCP ID Block Pointer */
  int   ecvtr0BC;   /* reserved */
  /* offset 0xC0 Here */
  Addr31 ecvtgrmp;   /* Graphics Resource Monitor GRM data block */
  Addr31 ecvtwlm;    /* WLM Vector table */
  Addr31 ecvtcsm;    /* Communication Storage Manager (VTAM) */
  Addr31 ecvtctbl;   /* Customer Anchor Table - RVT somewhere in here */
  /* offset 0xD0 Here */
  Addr31 ecvtpmcs;   /* V(IEAVPMCS)  process service routine */
  Addr31 ecvtpmcr;   /* V(IEAVPMCR)  ditto-ish */
  Addr31 ecvtstx1;   /* V(IEAVAX01) STAX defer yes */
  Addr31 ecvtstx2;   /* V(IEAVAX02) STAX defer no */
  /* Offset 0xE0 Here */
  int   ecvtslid;   /* Slip Trap ID or 0x00000000 */
  Addr31 ecvtcsvt;   /* CSV Table - Callable Services Vector - Important and Common */
  Addr31 ecvtasa;    /* ASA Table */
  Addr31 ecvtexpm;   /* V(IEAVEXPM) GETXSB service routine */
  /* Offset 0xF0 Here */
  Addr31 ecvtocvt;   /* OpenMVS Anchor, its CVT */
  Addr31 ecvtoext;   /* OpenMVS  external data - what is this? */
  Addr31 ecvtcmps;   /* V(CSRCMPSS) - compression services routine */
  Addr31 ecvtnucp;   /* nucleus dataset name */
  /* Offset 0x100 Here */
  Addr31 ecvtxrat;   /* XES Anchor Table for branch entry routine addresses */
  Addr31 ecvtpwvt;   /* Processor Workunit Queue Vector Table */
  char  ecvtclon[2];/* System.within Sysplex 2-char ID */
  char  ecvtgmod;   /* GRS operation mode, NONE, 1 - Ring , 2 - Star */
  char  ecvtr10B[5]; /* reserved */
  /* Offset 0x110 */
  char  ecvtr110[10]; /* reserved */
  short ecvtptim;     /* time value for parallel detach (RTM) */
  Addr31 ecvtjcct;     /* JES Communication Control Table */
  /* Offset 0x120 */
  char junk[0xBC]; 
  int  ecvtpseq;
} ECVT;

typedef struct gda_tag{
  char   gdaid[4];  /* eyecatcher "GDA " */
  char   stuff[0x60];
  void  *gdafbqcf;   /* chain head of CSA FBQE */ 
  void  *gdafbqcl;   /* chain tail */
  void  *gdacsa;     /* base of CSA */
  int    gdacsasz;   /* CSA size */
  void  *gdaefbcf;   /* first ECSA FBQE */
  void  *gdaefbcl;   /* last  ECSA FBQE */
  void  *gdaecsa;    /* base of ECSA */
  /* OFFSET 0x80 */
  int    gdaecsas;   /* size of ECSA */
  int    gdacsare;   /* amount of low common (CSA + SQA) left over */
  void  *gdaspt;     /* CSA subpool table */
  int    gdacsacv;   /* amount of CSA converted to SQA */
  /* OFFSET 0x90 */
  void  *gdasqa;     /* start of SQA */
  int    gdasqasz;   /* size of SQA */
  void  *gdaesqa;    /* start of ESQA */
  int    gdaesqas;   /* size of ESQA */
  /* OFFSET 0xA0 */
  void  *gdapvt;     /* start of low private */
  int    gdapvtsz;   /* low private size.  Why 0? */
  void  *gdaepvt;    /* start of high private */
  int    gdaepvts;   /* size of high private */
  /* OFFSET 0xB0 */
  void  *gdacpanc;   /* VSM's SQA Cell pool */
  int    gdacpcnt;   /* Free Cell Count in above */
  void  *gdafcadr;   /* First free cell in above */
  void  *gdacpab;    /* Address of Permanent CPAB (what is this?) table */
  /* OFFSET 0xC0 */
  void  *gdavr;      /* address of global V=R area (PSPI) */
  int    gdavrsz;    /* size of above */
  int    gdavregs;   /* default V=R region size */
  void  *gdawrka;    /* global work area in nucleus */
  /* OFFSET 0xD0 */
  char   unmapped[0x310-0xD0];
} GDA;

typedef struct rcvt_tag {
  char   unmapped0[0x279];

  /* OFFSET 0x279 */
  char   rcvtflg3;
#define RCVTFLG3_BIT_RCVTDCDT 0x80 /* dynamic CDT is active */
#define RCVTFLG3_BIT_RCVTPLC  0x40 /* allow lower-case passwords */
#define RCVTFLG3_BIT_RCVTCFLD 0x20 /* custom fields in effect */
#define RCVTFLG3_BIT_RCVTAUTU 0x10 /* authority avail to auth exits */

  /* OFFSET 0x27A */
  char   unmapped27A[0x904-0x27A];
} RCVT;

CVT *getCVT(void);
Addr31 getATCVT(void);
void *getIEACSTBL(void);
cvtfix *getCVTPrefix(void);
ECVT *getECVT(void);

typedef struct ocvt_tag{  /* see SYS1.MACLIB(BPXZOCVT) */
  char eyecatcher[4]; /* "OCVT" */
  char flags;
  char hiLength;
  unsigned short ocvtLength;
  Addr31 ocvtExtension;       /* aka OCVE */
  Addr31 publicOCVTExtension; /* aka OEXT */
  Addr31 bpxjc; /* This field cannot move, it must be 
                   offset 10 hex. This field is used
                   by BPXJCSR.
                */
  Addr31 ascbOfKernel; 
  unsigned short asidOfKernel;
  unsigned short reserved1;
  Addr31 tcbOfKernelJobstep;
  char tcbTokenOfKernelJobstep[16];
  Addr31 bpxjcns; /* PC1 (BPXJCNS) number                 */
  Addr31 bpxjcss; /* PC2 (BPXJCSS) number                 */
  Addr31 bpxjcsa; /* PC3 (BPXJCSA) number    https://www-304.ibm.com/support/docview.wss?uid=isg1OA33552 for clues       */
} OCVT;

/* Request blocks are octopus-chameleon hybrids.  The documentation
   is paleolithic and confusing.

   
 */

typedef struct IKJRB_tag{
  char     stuff[0x0C];
  Addr31   rbcde;
  char     moreStuff[0x20]; /* at least */

} IKJRB;

typedef struct tcb_tag{
  Addr31 tcbrbp; /* request block pointer */
  Addr31 tcbpie; /* PIE/EPIE , error handler */
  Addr31 tcbdeb; /* DEB queue */
  Addr31 tcbtio;  /* TIOT (Task IO Table) */
  int  tcbcmp;   /* 0x10 completion flags, and codes in 8-12-12 format */
  int  tcbabfAndTcbrnb; /* flags and testran stuff */
  int  tcbmss;          /* last SPQE on MSS queue, etc */
  char tcbpkf;   /* storage protection key */
  char tcbflgs1;
  char tcbflgs2;
  char tcbflgs3; 
  char tcbflgs4; /* 0x20 */
  char tcbflgs5;
  char tcblmp;   /* task limiting priority */
  char tcbdsp;   /* task dispatching priority */
  Addr31 tcblls;  /* addr of last elt (LLE) in Load List */
  Addr31 tcbjlb;  /* addr of joblib DCB */
  int  tcbjpq;  /* last cde in job pack areas 1+31 or 8+24 format */
  int  tcbgrs[16]; /* 0x30 General Register Save Area */
  Addr31 tcbfsa; /* 0x70 first prob program save area */
  struct tcb_tag *__ptr32 tcbtcb;  /* tcb chain in address space */
  Addr31 tcbtme;
  Addr31 tcbjstcb; /* first job step TCB or this TCB is key 0 */
  Addr31 tcbntc;  /* 0x80 attach stuff */
  Addr31 tcbotc;  /* originating task */
  Addr31 tcbltc;
  Addr31 tcbiqe;  /* interrupt queue element IQE */
  Addr31 tcbecb; /* 0x90 ECB to be posted for task term */
  char tcbtsflg; /* time-sharing flags */
  char tcbstpct; /* number of set task starts */
  char tcbtslp; /* limit prty of TS task */
  char tcbtsdp; /* disp prty of TS task */
  Addr31 tcbrd; /* DPQE-8 for job step serialization, aka TCBPQE */
  Addr31 tcbae; /* list origin of AQE's for this task serialization */
  char tcbnstae; /* 0xA0 STAE flags */
  char tcbstabb[3]; /* current stae control block */
  Addr31 tcbtct;  /* timing control table - SMF */
  Addr31 tcbuser;  /* user word, shouldn't be used!! */
  int tcbscndy; /* secondary nondispatchability bits */
  int tcbmdids; /* 0xB0 reserved for model-dep support */
  Addr31 tcbjscb; /* JSCB (job step control block), high-8 also abend recursion bits, TCBRECDE */
  Addr31 tcbssat; /* subsystem affinity table */
  Addr31 tcbiobrc; /* IOB restore chain */
  Addr31 tcbexcpd; /* 0xC0 EXCP debug area */
  Addr31 tcbext1; /* os-vs common extension ICB311 */
  char tcbdsp4;
  char tcbdsp5;
  char tcbflgs6;
  char tcbflgs7;
  char tcbdar;
  char tcbsv37; /* rsvd for user */
  char tcbsysct; /* # of outstanding system must-complete requests ICB497 */
  char tcbstmct; /* # of outstanding step must-complete requests ICB497 */
  Addr31 tcbext2; /* 0xD0 OS-VS common extension */
  char undifferentiated1[0x2C]; 
  char undifferentiated2[0x38]; /* 0x100 */
  Addr31 tcbstcb;
  char undifferentiated3[0x18]; /* 0x100 */
  Addr31 tcbsenv; /* securirt environment, 0 or ACEE, 0 means use ASCB/ASXB env */
  
  
} TCB;

typedef struct stcb_tag{
  char undifferentiated1[0xD8];
  Addr31 stcbotcb;
  Addr31 stcbdcxh; /* address of the job pack queue cde extensions hash table.  
                      ownership: contents supervisor (csv)
                      serialization: local lock. */
  char undifferentiated2[0x120]; /* gets us to 0x200 */
  Addr31 stcbotca; /* Address of OTCB Alternate Anchor For Cleanup 
                      Ownership: USS
                      Serialization: run under this task. */
  Addr31 stcblaa;  /* Address of LE Library Anchor Area */
  Addr31 stcbpie;  /* Address of PIE control block */
} STCB;

/* See for OMVS structs:

   http://publib.boulder.ibm.com/infocenter/zos/v1r9/index.jsp?topic=/com.ibm.zos.r9.bpxb100/zotcb.htm

   https://www.ibm.com/support/knowledgecenter/SSLTBW_2.2.0/com.ibm.zos.v2r1.bpxb100/zotcb.htm

   List of all OMVS/USS things 
   http://publib.boulder.ibm.com/infocenter/zos/v1r9/index.jsp?topic=/com.ibm.zos.r9.bpxb100/zotcb.htm

   http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/bpxzb1c0/B.0?SHELF=all13be9.bks&DT=20110609191818

   The Map

      TCB -> STCB

      STCB -> OTCB  (thru STCBOTCA at offset 0x200)  

      OAPB is the Process thing pointed to by OtcbOapb, PprpOapb
      


   */

typedef struct otcb_tag{
  char           otcbid[4];  /* eyecatcher */
  char           otcbsp;  /* subpool */
  char           zero1[1]; /* should be zero */
  short          otcblen; /* length of me */
  Addr31         otcbptxl; /* pthread parms */
  Addr31         otcbkser; /* KSER ???? */
  /* OFFSET 0x10 */
  char           otcbflagsb1; /* flag set 1 */
  char           otcbflagsb2; /* flag set 2 */
  char           otcbflagsb3; /* flag set 3 - ACEE/RACF stuff */
  char           otcbflagsb4; /* flag set 4 */
  int            otcbpprt; /* thread id part */
  int            otcbseqno; /* ditto */
  int            otcbsigglags; /* */
  /* OFFSET 0x20 */
  char           undifferentiated1[0x40];
  /* OFFSET 0x60 */
  Addr31         otcbotim;
  Addr31         otcboabp;
  char           stuff[8];
  /* OFFSET 0x70 */
  Addr31         otcbptlcppsdptr;      /* Ptrace local Ppsd pointer */
  Addr31         otcbmvsspauseecblist; /* Pointer to the BPXZECBL - System copy of user ECB addresses passed to MVSpauseInit */
  Addr31         otcbsavedscb;         /* Saved SCB addr of STAI on entry to Local Child Process */
  Addr31         otcbuecblist;         /* Pointer to the BPXZECBL - System copy of user and system ECBs address for the BPXLUKW
                                          - User KernWait service */
  /* 0x80 */
  int            otcbruid;  /* OTCBRUID DS    F         Real Uid */
  int            otcbeuid;  /* OTCBEUID DS    F         Effective Uid */
  int            otcbsuid;  /* OTCBSUID DS    F         Saved Uid      */
  Addr31         otcbsavedacee;
  /* 0x90 */
  Addr31         otcbpprx; /*  Address of the Pprx, an extension of the Pprt */
  int            otcbmrpwduid; /* TCBMRPWDUID DS F        Password verified UID */
  int            otcbpswbyt03; /* OTCBPSWBYT03 DS F        Caller's PSW bytes 0-4 */
  char           otcbmprwdusername[8]; /* DS CL8 Password verified userid */
  /* OFFSET 0xA4 */
  Addr31         otcbsavedsecenv; /* DS A     Pointer to ACEE saved by BPX1ENV for a toggle req */
  char          *otcbmvsuseridptr; /* DS A    Pointer to userid of this thread, points to 
                                     either OtcbLoginNInfo or OasbLoginNInfo */
  int            otcbloginnlen; /* DS F       Task userid length */
  /* B0 */
  char           otcbloginname[9]; /*  DS CL9     Tasks userid, must be '00'x (null)
                                       terminated. Preceding length does not include X        
                                       ' terminating null */
  char           otcbprin2flags; 
  char           reserved1[2];
  Addr31         otcbthli; 
  /* OFFSET 0xC0 */
  Addr31         otcbactsctbnodeptr; 
  Addr31         otcbtoggledsctbnodeptr;
  int            otcbpag;
  int            otcbrgid;
  /* OFFSET 0xD0 */
  int            otcbegid;
  int            otcbsgid;
  Addr31         otcbracgidsptr;
  /* DC */
  char           otcbwlmetoken[8];
  int            otcbsavedgid; /* Gid set by getpwname, used by setgid   */
  int            otcbaliasnlen;
  /* EC */
  char           otcbaliasname[12]; /* only really filled to 8+1 */
  int            otcbosenvcellptr; /* OSENV Cell ptr */
  int            otcbosenvseqn; /* 2nd half of OSENV token, the sequence number */
} OTCB;

typedef struct IHACDE_tag{
  struct IHACDE_tag *__ptr32 next;
  Addr31                     cdrrbp;    /* address of RB that had module, or not, kinda complex */
  char                       cdname[8];  
  /* OFFSET 0x10 */
  unsigned int               cdentpt;   /* Entry point with high bit settable for 31, low bit for 64 */
  Addr31                     cdxlmjp;   /* Extent list address or major CDE address if this is a minor */
  unsigned short             cduse;     /* usage count */
  char                       cdattrb;   /* flags */
  char                       cdsp;      /* Module subpoolID */
  char                       cdattr;    /* flag byte */
  char                       cdattr2; 
  char                       cdattr3; 
  char                       cdattr4;   /* reserved for future flags */
} IHACDE;

typedef struct acee_tag{
  char aceeacee[4]; /* eyecatcher */
  char aceesp; /* subpool */
  char aceelenhigh;
  short aceelen; /* length */
  char aceevrsn;
  char reserved1[3]; 
  int  aceeiep;
  Addr31 aceeinst; /* 0x10 User data address */
  char aceeuser[9]; /* username with pre-count */
  char aceegrp[9];  /* group with pre-count */
  char aceeflg1;
  char aceeflg2; 
  char aceeflg3; /* 0x28 */
} ACEE;

typedef struct assb_tag{  /* ADDRESS SPACE SECONDARY BLOCK */
  char   assbassb[4]; /* eyecatcher */
  int    assbvafn; /* count of tasksin addrspace reg Vector Control sup */
  char   vectorStuff1[16]; /* unimportant guck */
  char   assbxmf1; /* cross mem flags */
  char   assbxmf2; /* cross mem flags */
  char   assbxmcc[2]; /* cross mem count 20 */
  Addr31 assbcbtp; /*  pointer to ihaacbt */
  int    assbvsc; /*      vio slot allocated count. */
  int    assbnvsc; /*     non-vio slot allocated count. */
  int    assbasrr; /*     address space re-reads to be reported in the type 30 smf record. */
  Addr31 assbdexp; /* pointer to ihadexp */
  char   assbstkn[8]; /*     stoken.  ownership: (0)          supervisor control. */
  /* OFFSET 0x38 */
  Addr31 assbbpsa; /* IBM presentation manager (!) - it would be SO cool if this were free */
  Addr31 assbcsct; /* Cache Facility Stop Count */
  /* OFFSET 0x40 */
  Addr31 assbbalv; /* Basic Access List */
  Addr31 assbbald; /* Basic Access List Designator */
  Addr31 assbxmse; /* XMSE for this address space */
  int    assbtsqn; /* Next TToken Sequence Number */
  /* OFFSET 0x50 */
  int    assbvcnt; /* Count of Current Tasks with Vector Affinity */
  Addr31 assbpalv; /* PASN Access List */
  Addr31 assbasei; /* Address Space Related information */
  Addr31 assbrma;  /* Address Space Resource Manager Control Structure */
  /* OFFSET 0x60 */
  int64  assbhst;  /* RSM Service Time for Hiperspace Read/Write */
  int64  assbiipt; /* IOS I/O interrupt processing time */
  /* OFFSET 0x70 */
  int    assbanec; /* ALESERV ADD with no EAX count */
  Addr31 assbsdov; /* Shared Data Object Manager Vector Table */
  int    assbmcso; /* NUmber of Console IDs activated by the address space 
                      High bit set indicates at least one activated */
  Addr31 assbdfas; /* Address of DFP=SMSX for the address space */
  /* OFFSET 0x80 */
  char   assbflgs; /* flags */
  char   assbflg1;
  char   assbflg2; 
  char   assbflg3;
  Addr31 assbascb; /* ASCB */
  Addr31 assbasfr; /* Forward Pointer */
  Addr31 assbasrb; /* Backward Poitner */
  /* OFFSET 0x90 */
  Addr31 assbssd;  /* Suspended SRB Descriptor Queue */
  Addr31 assbmqma; /* Control block anchor for MQM MVS/ESA (hmm, MQ) */
  char   assblasb[8]; /* Token if/for MVS/APPC resources associated with this
                         address space */
  /* OFFSET 0xA0 */
  Addr31 assbsch;  /* APPC Scheduler Address Space Block */
  Addr31 assbfsc;  /* Count accumulated by IEAMFCNT for this Addr Space */
  Addr31 assbjsab; /* JSAB */
  Addr31 assbrctw; /* RCT's WEB - what is this ?? */
  /* OFFSET 0xB0 */
  char   rsvd0B0[8];
  Addr31 assbtlmi; /* Tailored Lock Manager Info Block */
  Addr31 assbsdas; /* Working storage obtained by the dump task */
  /* OFFSET 0xC0 */
  int    assbtpin; /* Count of UCB pin requests issued in task mode outstanding */
  int    assbspin; /* Count of UCB pin requests issued in SRB mode */
  int    assbect1; /* Count of Allocation requests on EDT#1 */
  int    assbect2; /* Count of Allocation requests on EDT#2 */
  /* OFFSET 0xD0 */
  char   stuff[0x80];
  /* OFFSET 0x150 */
  /* Both of these can be and often are filled with 0's */
  char   assbjbni[8]; /* Jobname of Initiated Program for this A-space */
  char   assbjbns[8]; /* Jobname of the START/MOUNT/Login for this A-Space */
  /* OFFSET 0x160 */
  /* cool stats and other stuff */
} ASSB;

typedef struct jsab_tag{
  char   jsabjsab[4]; /* eyecatcher */
  Addr31 jsabnext;
  int    jsablen;
  char   jsabvers;
  char   jsabflg1;
  char   jsabflg2;
  char   jsabclr;
  char   jsabscid[4];
  char   jsabjbid[8];
  char   jsabjbnm[8];
} JSAB;

typedef struct asxb_tag{
  char  asxbasxb[4]; /* eyecatcher */
  Addr31 asxbftcb;    /* first TCB */
  Addr31 asxbltcb;    /* last  TCB */
  short asxbtcbs;    /* TCB count */
  char  asxbflg1;
  char  asxbschd;
  Addr31 asxbmpst;  /* 0x10 VTAM stuff */
  Addr31 asxblwa;  
  Addr31 asxbvfvt;
  Addr31 asxbsaf;   /* ROUTER RRCB ADDRESS */
  char undiffereniated1[0xA0]; /* 0x20 */
  char   asxbuser[8];  /* usually only 7 chars */
  Addr31 asxbsenv;  /* ACEE pointer */
  char   stuff[0xF8-0xCC];
  Addr31 asxbitcb;
} ASXB;

typedef struct ascb_tab{
  char ascbascb[4]; /* eyecatcher */
  Addr31 ascbfwdp; /* address of next ascb on ascb ready queue */
  Addr31 ascbbwdp; /* - address of previous ascb on ascb ready queue */
  Addr31 ascbltcs; /* - tcb and preemptable-class srb local lock suspend service queue.  
                      serialization: ascb cml promotion web lock. */
  Addr31 ascbsvrb; /* 0x10 - svrb pool address. this offset fixed by architecture. (mdc310) */
  Addr31 ascbsync; /*  - count used to synchronize svrb pool.  this offset fixed by architecture. (mdc311) */
  Addr31 ascbiosp; /*  - pointer to ios purge interface control block (ipib) (mdc308) */
  int   ascbwqlk;  /* web queue lock word (0)         
                      serialization: compare and swap
                      ownership: supervisor control */
  Addr31 ascbsawq; /* - address of address space srb web queue
                      serialization: web   queue lock
                      ownership: supervisor control */
  unsigned short ascbasid;    /*  - address space identifier for the ascb */
  char ascbr026;   /*  - reserved */
  char ascbsrmflags; /* 0x27 - srm flags 
                      ownership: srm
                      serialization: srmlock*/
  char ascbll5;      /* 0x28 Flags: 0x20 STAGE II 
                        EXIT EFECTOR HAS
                        SCHEDULED AN RQE OR
                        IQE AND STAGE III
                        EXIT EFFECTOR SHOULD
                        BE INVOKED */
  char   ascbhlhi;     /* 0x29 Indication of suspend locks hled at task suspension */
  short  ascbdph;     /* 0x2A dispatching priority */
  int    ascbtcbe;    /* 0x2C Count of ready TCB's in the space that are in an enclave */
  Addr31 ascblda;     /* 0x30 Pointer to local data area part of LSQA for VSM */
  char   ascbrsmf;    /* RSM Addr Space flags */
  char   ascbflg3;
  short  ascbr036;
  /* OFFSET 0x38 */
  Addr31 ascbcscb;
  Addr31 ascbtsb;     /* TSO TSB thing */
  /* OFFSET 0x40 */
  uint64 ascbejst;    /* elapsed job step timing */
  uint64 ascbewst;
  /* OFFSET 0x50 */
  int    ascbjstl;
  int    ascbecb;
  int    ascbubet;
  Addr31 ascbtlch;    /* Chain Field for Time Limit Exceeded Queue */
  /* OFFSET 0x60 */
  Addr31 ascbdump;    /* SVC dump task TCB */
  int    ascbfw1;     /* Full word label for compare and swap of subfields */
  Addr31 ascbtmch;    /* Termination Queue Chain */
  ASXB *__ptr32 ascbasxb; /* the asxb  offset 0x6C */
  short ascbswct; 
  char ascbdsp1; /* dispatch flags */
  char ascbflg2; /* flags */
  short reserved1; /* mbz */
  short ascbsrbs; /* count of srbs suspended in this AS */
  Addr31 ascbllwq; /* local lock web queue */
  Addr31 ascbrctp; /* region control task */
  int ascblock; /* THE local lock */
  Addr31 ascblswq; /* local lock web suspend queue */
  int ascbqecb; /* quiesce ECB */
  int ascbmecb; /* mem cre/del ECB */
  Addr31 ascboucb; /* SRM user control block pointer */
  Addr31 ascbouxb; /* SRM user control extension pointer */
  short ascbfmct; /* reserved, used to have frame count */
  char ascblevl; /* 0->3 */
  char ascbfla2;  /* flag byte */
  Addr31 ascbr09c; /* after z1.11 reserved, former local lock requestor address */
  Addr31 ascbiqea; /* ptr to IQE for atcam ascync processing */
  Addr31 ascbrtmc; /* anchor for sqa sdwa queue */
  int ascbmcc; /* memory termination completion code */
  Addr31 ascbjbni; /* ptr to jobname for initiated programs or zero */
  Addr31 ascbjbns; /* ptr to jobname for start/mount/logon programs or zero */
  char ascbsrq1;
  char ascbsrq2;
  char ascbsrq3;
  char ascbsrq4;
  Addr31 ascbvgtt; /* VSAM global term task */
  Addr31 ascbpctt; /* private catalog termination table */
  /* offset C0 here */
  char undifferentiated2[0x5C]; 
  TCB  *__ptr32 ascbxtcb;               /* top of the TCB tree for address space */
  char ascbcs1;   /* C/S flags 1 and 2 */
  char ascbcs2;
  char ascbr122_1; /* reserved fullword padding */
  char ascbr122_2;
  Addr31 ascbgxl; /*  offset 0x124     - ADDRESS OF GLOBALLY LOADED MODULE EXTENT INFO CONTROL BLOCK */
  char ascbeatt[8];  /* expended and accounted task time. */ 
  char ascbints[8]; /* job selection time stamp */
  char ascbll1; /* flags/ serialization local lock */
  char ascbll2;
  char ascbll3;
  char ascbll4;
  Addr31 ascbrcms; /*     address of the requested cms class lock for which the local lock holder is suspended */
  int  ascbiosc; /*    - i/o service measure, updated by jes2, jes3, smf */
  short ascbpkml; /*     - pkm of last task dispatched in this address space. */
  short ascbxcnt; /* EXCP count field */
  Addr31 ascbnsqa; /* address of the sqa resident nssa chain. */
  Addr31 ascbasm;  /* address of the asm header */
  Addr31 ascbassb;  /* offset 0x150 */
  /* more stuff */
} ASCB;

typedef struct SSIB_tag{ /* length 0x24 */                
  unsigned int eyecatcher; /* SSIB */                      
  unsigned short length;                                   
  unsigned char flags;                                     
  unsigned char ssid;                                      
  char subsystem_name[4];                                  
  char ssibjbid[8];
  char ssibdest[8];
  int  ssibrsv1;
  int  ssibssuse;
} SSIB;

typedef struct JESPEXT_tag{
  unsigned char reserved1[0x54];
  SSIB * __ptr32 jessmsib; /* the storage management subsystem ssib */
} JESPEXT;

typedef struct SSCT_tag{
  char id[4]; /* SSCT */
  struct SSCT_tag *__ptr32 scta;  /* chain pointer */
  char sname[4];          /* 4 letter name */
  unsigned char flags;    /* equates */
  unsigned char ssid;     /* JES2 or 3 */
  unsigned char reserved1[2];
  Addr31 ssvt;              /* SUBSYSTEM VECTOR TABLE POINTER */
  int   ssctsuse;          /* USED By SUBSYSTEM */
  Addr31 ssctsyn;           /* HASH TABLE SYN POINTER */
  int   ssctsus2;          /* SUBSYSTEM USE 2 */
  int   reserved2;         
} SSCT;

typedef struct JESCT_tag{
  char eyecatcher[4]; /* JEST */
  Addr31 jesunits;     /* SYSRES UCB */
  Addr31 jeswaa;
  Addr31 jesqmgr;  /* Address of SWA Mgr */
  Addr31 jesresqm; /* Entry Point intf to QMNGRIO */
  Addr31 jesssreq; /* IEFSSREQ routine address */
  SSCT *__ptr32 jesssct;
  char jespjesn[4]; /* primary JES name */
  unsigned char reserved2[0x44]; /* offset 0x20 */
  JESPEXT *__ptr32 jesctext; /* offset 0x64 */
  Addr31 jesppt; /* POINTER TO THE PROGRAM PROPERTIES TABLE */
  Addr31 jesrstrt;  /*   pointer to restart code table */
  Addr31 jesparse; /* pointer to the parser routine */
  Addr31 jesxb603; /* pointer to restart component message module (iefxb603) */
} JESCT;

TCB *getTCB(void);
STCB *getSTCB(void);
OTCB *getOTCB(void);
ASCB *getASCB(void);
ASXB *getASXB(void);
ASSB *getASSB(ASCB *ascb);
JSAB *getJSAB(ASCB *ascb);


char *getSystemName (void);
char *getSysplexName(void);

ACEE *getCurrentACEE(void);
TCB *getFirstChildTCB(TCB *tcb);
TCB *getParentTCB(TCB *tcb);
TCB *getNextSiblingTCB(TCB *tcb);

/* TCB Tree

   

 */

/* DSAB Chain
 
   DD's w/o tcb tree

   JSCB TCBJSCB  in top TCB

   JSCB QMPI
   DSABQ  - QDB (mapped in IHAQDB)

    QDBLELMP DS    A -            POINTER TO LAST ELEMENT               

   DSAB  IHADSAB

   ->TIOT  

   IEFTIOT1 - 

   getDSAB(DDName)

 */

#define GETDSAB_PLIST_LENGTH 16

#define DSABFLG4_DSABHIER 0x10

typedef struct DSAB_tag{
  char   eyecatcher[4];
  Addr31 dsabfchn;
  Addr31 dsabbchn;
  short  dsablnth; /* length */
  short  dsabopct; /* OPEN DCB COUNT FOR TIOT DD ENTRY */
  /* OFFSET 0x10 */
  Addr31 dsabtiot; 
  int    dsabssva; /* only low 24bits used SWA VIRTUAL ADDRESS OF SIOT    */
  int    dsabxsva; /* only low 24 - SVA of XSIOT                */
  int    dsabanmp; /* &NAME OR GDG-ALL DSNAME PTR, 0 IF NONE */
  char   dsaborg1;
  char   dsaborg2;
  char   dsabflg1;
  char   dsabflg2;
  char   dsabflg3;
  char   dsabflg4;
  short  dsabdext; /*     INDEX TO DEXT TABLE   */
  int    dsabtcbp; /* tcb under which set in-use */
  int    dsabpttr; /* relative TTR of data set password */
  char   dsabssnm[4]; /* subsystem name */
  int    dsabsscm;    /* subsystem communication area */
  char   dsabdcbm[6]; /* bit map of DCB fields */
  short  dsabtctl;   /* offset of lookup entry from beginning of TCTIOT 
                       If DSABATCT is on, use DSABTCT2 instead               */
  Addr31 dsabsiot;    /* SIOT incore address */
  
  int dsabtokn;    /* DD Token */
  int dsabtct2;    /* offset of lookup entry from beg of TCTIOT, always valid */
  int dsabsiox;    /* virtual address of SIOTX */
  int dsabfcha;
  int dsabbcha;
  char dsabflg5;
  char reserved[7];

} DSAB;

/* JFCB 
   The JFCB is a prehistory nightmare 
 */

typedef struct JFCB_tag{
  char           jfcbdsnm[44];
  char           otherName[8];   /* can be many things */
  int            flagsAndTTR;
  int            fcbAndOtherStuff;
  unsigned short vsamBuffers;
  unsigned short vsamLRECL;
  /* 0x40 */
  char           tdsiByte1;
  char           tdsiByte2;
  char           labelType;
  char           otherThing;
  unsigned short dataSetSequenceNumber;
  unsigned short volumeSequenceNumber;
  char           dataManagementSwitches[8];
  /* OFFSET 0x50 */
  char           creationDate[3];
  char           expirationDate[3];
  char           indicatorByte1;
  char           indicatorByte2;
  char           moreStuff[8];       /* at least !! */
} JFCB;

/* TIOT Task Input/Output Table 
   
   This DSECT seems ancient, weird, and bad.
 */

typedef struct TIOTHeader_tag{
  char      tiocnjob[8];
  char      tiocstpn[8]; 
  char      tiocjstn[8];    /* Job Step Name for Procs */
} TIOTHeader; 

typedef struct TIOTEntry_tag{
  char      tioelngh;
  char      tioestta;
  char      tioerloc;
  char      tioelinl;
  char      tioeddnm[8];
  int       tioejfcb;      /* this evil thing is 24 high bits of SWA index, and last byte of status bits */
} TIOTEntry;

ZOWE_PRAGMA_PACK_RESET

DSAB *getDSAB(char *ddname);


int dsabIsOMVS(DSAB *dsab);

int locate(char *dsn, int *volserCount, char *firstVolser);

#define VERIFY_WITHOUT_LOG         0x400
#define VERIFY_WITHOUT_STAT_UPDATE 0x200
#define VERIFY_WITHOUT_MFA 0x100
#define VERIFY_WITHOUT_PASSWORD 0x80
#define VERIFY_CREATE 0x40  /* if not, modify ACEE of TCB */
#define VERIFY_DELETE 0x20
#define VERIFY_CHANGE 0x10
#define VERIFY_SUPERVISOR 0x01 /* perform check in supervisor mode */

#define getAddressSpaceAcee GADSACEE
#define getTaskAcee GTSKACEE
#define setTaskAcee STSKACEE

#define safVerify SAFVRIFY
#define safVerify2 SAFVRFY2
#define safVerify3 SAFVRFY3
#define safVerify4 SAFVRFY4
#define safVerify5 SAFVRFY5

ACEE *getAddressSpaceAcee(void);
ACEE *getTaskAcee(void);
int setTaskAcee(ACEE *acee);

int safVerify(int options, char *userid, char *password,
              ACEE **aceeHandle,
              int *racfStatus, int *racfReason);

int safVerify2(int options, char *userid, char *password,
               ACEE **aceeHandle,
               void **messageAreaPtr, int subpool,
               int *racfStatus, int *racfReason);

int safVerify3(int options, char *userid, char *password,
               ACEE **aceeHandle,
               void **messageAreaPtr, int subpool,
               char  *applicationName,
               int *racfStatus, int *racfReason);

int safVerify4(int options,
               char *userid,
               char *password,
               char *newPassword,    /* NULL if not changing password or passphrase */
               ACEE **aceeHandle,
               void **messageAreaPtr, int subpool,
               char  *applicationName,
               int *racfStatus, int *racfReason);

int safVerify5(int options,
               char *userid,
               char *password,
               char *newPassword,    /* NULL if not changing password or passphrase */
               ACEE **aceeHandle,
               void **messageAreaPtr, int subpool,
               char *applicationName,
               int  sessionType,
               int  *racfStatus, int *racfReason);

/* second flag set */
#define SAF_AUTH_ATTR_ALTER       0x80 
#define SAF_AUTH_ATTR_CONTROL     0x08
#define SAF_AUTH_ATTR_UPDATE      0x04
#define SAF_AUTH_ATTR_READ        0x02

#define AUTH_DATASET_VSAM 0x80
#define AUTH_DATASET_TAPE 0x40
#define AUTH_SUPERVISOR   0x01

#define STAT_SUPERVISOR   0x01

int safAuth(int options, char *safClass, char *entity, int accessLevel, ACEE *acee, int *racfStatus, int *racfReason);

int safAuthStatus(int options, char *safClass, char *entity, int *accessLevel, ACEE *acee, int *racfStatus, int *racfReason);

int safStat(int options, char *safClass, char *copy, int copyLen, int *racfStatus, int *racfReason);

int getSafProfileMaxLen(char *safClass, int trace);

int64 getR12(void);
int64 getR13(void);

void *loadByName(char *moduleName, int *statusPtr);

char *getASCBJobname(ASCB *ascb);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

