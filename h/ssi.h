

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_SSI__
#define __ZOWE_SSI__  1

#ifndef __LONGNAME__


#endif /* longname */


#define SSRTOK 0
#define SSRTNSUP 0x4
#define SSRTNTUP 0x8
#define SSRTNOSS 0xC
#define SSRTDIST 0x10
#define SSRTLERR 0x14
#define SSRTNSSI 0x18

#pragma pack(packed)

typedef struct IEFJSSIB_tag{
  char          ssibid[4];   /* "SSIB" */
  uint16_t      ssiblen;     /* length of this struct/block */
  char          ssibflg1;
  char          ssibssid;    /* 0x00 unknown, 02 JES2, 03 JES3 */
  char          ssibssnm[4]; 
  char          ssibjbid[8]; /* Job ID or Subsyste Name to be verified */
  /* Offset 0x14 */
  char          ssibdest[8]; /* Default USERID of SYSOUT Destination */
  uint32_t      ssibrsv1;
  uint32_t      ssibsuse;    /* Reserved */
  /* Length == 0x24 */
} IEFJSSIB;

typedef struct IEFSSOBH_tag{
  char          ssobid[4];
  uint16_t      ssoblen;
  uint16_t      ssobfunc;  /* function code */
  IEFJSSIB *__ptr32 ssobssib;
  int32_t       ssobretn;  /* Return Code */
  /* Offset 0x10 */
  Addr31        ssobindv;
  /* optional fields start here */
  uint32_t      ssobeta;
  char          ssobflg1;   /* 0x80 is retry request */
  char          ssobrsv1[3];
} IEFSSOBH;


/* 
   SSPJ is placed in SSOBINDV for some SSI calls,
   particularly 
 */
typedef struct SSPJ_tag{
  uint16_t      sspjlen;
  char          sspjver;     /* Version should == 1 */
  char          sspjreq;     /* 1 register, 2 deregister, 3 restart, 4 query */
  char          sspjid[4];   /* ID "SSPJ" */
  uint32_t      sspjrsn;     /* Reason code */
  char          sspjgrp[8];  /* XCF Group Name */
  char          sspjnum[4];  /* */
  char          sspjjkey[4]; /* */
  /* Offset 0x1C */
  char          sspjstok[8];  /* SToken */
  char          sspjjbid[8];  /* JOBID (for Register Request only) */
  /* Offset 0x2C */
  char          sspjcaus;     /* Deregister cause */
  char          sspjrsv1[3];
  /* Total Length = 0x30 */
} SSPJ;


/* SSVI is the function-specific block for subsystem version information , function code == 54 (0x36) */
typedef struct SSVI_tag{
  uint16_t      ssvilen;  /* This must be at least SSVIMSIZ, but room must be made for variable length section */
  char          ssviver;  /* */
  char          ssvirsv1;
  char          ssviid[4];  /* 'SSVI' */
  uint16_t      ssvirlen;   /* Return or required length IN/OUT */
  char          ssvirver;
  uint16_t      ssviflen;   /* Length of fixed section */
  uint16_t      ssviasid;   /* V2 Address space assoc with Subsystem */
  /* Offset 0x10 */
  char          ssvivers[8];
  char          ssvifmid[8];
  char          ssvicnam[8];
  int           ssviudof;
  int           ssvisdof;
  /* end of v1 section */
  char          ssviplvl;
  char          ssvislvl;
  char          reserved[14];
  /* Variable data sections follow here */
} SSVI;

/* 
   Function Code 15 Verify Subsystem

   
   Always direct this at master (MSTR), and pass name of other system
   in ssibjbid left justified and padded with blanks */
typedef struct SSVS_tag{
  uint16_t      ssvslen;   /* VS extension length */
  char          ssvsflg1;  /* Flags */
#define SSVSUPSS 0x80
#define SSVSSTRT 0x40
  char          ssvsflg2;  /* reserved flag byte   */
  Addr31        svssctp;   /* ptr to ssct of the specified 
			    * subsystem-returned by the    
			    * master subsystem        */
  /* Offset 0x8 - short form length is 8 */
  uint16_t      ssvsnum;   /* subsystem's index for use with
			      subsystem affinity service 
			      on sscvt chain                */
  char          reserved[10];
  /* Offset 0x14, long form length is 20 */
} SSVS;

/* SSS2 is the function-specific block for SAPI */

typedef struct SSS2_tag{
  uint16_t      sss2len;     /* Length of this block */
  char          sss2ver;     /* Version, 1==Inital, 2==Supports Client Print, 3==Supports Job Correlator */
  char          sss2reas;    /* Reason Code (Output Field) */
  char          sss2eye[4];  /* "SSS2" */
  char          sss2appl[8]; /* zeros or EBCDIC */
  /* Offset 0x10 */
  char          sss2apl1[20];
  /* Offset 0x24 */
  char          sss2type;
#define SSS2PUGE 1           /* Put/Get */
#define SSS2COUN 2           /* Count */
#define SSS2BULK 3           /* Bulk Modify */
  char          reserved1[3]; 
  /* Not Done, many fields follow */
} SSS2;


/* Function Code 82 JES Property Query 
   Mapped by IZASSJP

   There are several request codes for different pieces of JES info

   Each request code is associated with a specific data area to map results.

   NJE            - IAZJPNJN
   Spool          - IAZJPSPL
   Initiator      - IAZJPITD
   JESPlex        - IAZJPLEX
   JobClass       - IAZJPCLS
   PROCLIB Concat - IAZJPROC

   */

#define SSJPNJOD 4   /* nje node info obtain     */
#define SSJPNJRS 8   /* nje node storage return  */
#define SSJPSPOD 12  /* spool info obtain        */
#define SSJPSPRS 16  /* spool storage return     */
#define SSJPITOD 20  /* initiator info obtain    */
#define SSJPITRS 24  /* initiator storage return */
#define SSJPJXOD 28  /* jesplex info obtain      */
#define SSJPJXRS 32  /* jesplex storage return   */
#define SSJPJCOD 36  /* job class info obtain    */
#define SSJPJCRS 40  /* job class storage return */
#define SSJPPROD 44  /* proclib concat obtain    */
#define SSJPPRRS 48  /* proclib storage return   */
#define SSJPCKOD 52  /* ckpt information obtain  */
#define SSJPCKRS 56  /* ckpt storage return      */

/*------------------------------------------------------------*
 *        Values of SSJPRETN when SSOBRETN is SSJPERRU (8)    *
 *        for all functions (values of SSJPFREQ)              *
 *------------------------------------------------------------*/
#define SSJPUNSF 4   /* Function code passed in
			SSJPFREQ is not supported   */
#define SSJPNTDS 8   /* SSJPUSER pointer is zero    */
#define SSJPUNSD 12  /* SSJPUSER CB version number           
			is not correct              */
#define SSJPSMLE 16  /* SSJPUSER CB length is too small         */
#define SSJPEYEE 20  /* SSJPUSER CB eyecatcher is not correct   */
#define SSJPINVA 136 /* Invalid filter arguments.               */
#define SSJPGLBL 140 /* Function not supported on global (JES3) */
#define SSJPSMAP 144 /* Error with storage addressed         
			by storage management              
			anchor pointer - e.g. not          
			key 1, fetch protected,            
			incorrect eyecatcher                */
/*------------------------------------------------------------*
 *        Values of SSJPRETN when SSOBRETN is SSJPSTOR (20)   *
 *        for all functions (values of SSJPFREQ)              *
 *------------------------------------------------------------*/
#define SSJPGETM 128 /* $GETMAIN failed       */
#define SSJPSTGO 132 /* STORAGE OBTAIN failed */

typedef struct IAZSSJP_tag{
  char          ssjpid[4];   /* "SSJP" */
  uint16_t      ssjplen;     /* length of this block */
  uint16_t      ssjpver;     /* version number of ssob, current is 1 */
  char          ssjpfreq;    /* function request byte */
  char          ssjprsv1[3]; 
  uint32_t      ssjpretn;    /* More of a reason code, for when SSOBTERN is 8 or 20 */
  /* Offset 0x10 */
  Addr31        ssjpuser;    /* Pointer at user paramter area */
  char          reserved[12]; 
  /* Length 0x20 */
} IAZSSJP;

/* JPRC Filter Constants */
#define JPRCFNAM 0x80  /* Filter on PROCLIB DD name */
#define JPRCFJBC 0x40  /* Return PROCLIB for JOBCLASS (JES2 only) */
#define JPRCFSTC 0x20  /* Return started task PROCLIB */
#define JPRCFTSO 0x10  /* Return TSO logon PROCLIB    */
#define JPRCFINT 0x08  /* Return internal reader PROCLIB (JES3 only)       */
#define JPRCFLOC 0x04  /* Return PROCLIB for address space calling the SSI */

typedef struct IAZJPROC_tag{
  char          jprcid[8];   /* "JESPROCI" */
  uint16_t      jprclen;     /*  AL2(JPRCSZE) - Length of JPROC parameter  */
  char          jprcverl;    /* Version Level, must be 1 */
  char          jprcverm;    /* Version Mod, must be 0 */
  uint16_t      jprcvero;    /* Subsystem Version/Modifier */
  char          reserved1[2];  
 /**************************************************************
  *                                                            *
  *        JPRCSTRP is an anchor for use by the subsystem      *
  *        that responds to this request.  It is expected that *
  *        the caller will set this to zero the FIRST time an  *
  *        SSOB extension is used and from that point on it    *
  *        will be managed by the subsystem.                   *
  *                                                            *
  **************************************************************/
  Addr31        jprcstrp;
 /**************************************************************
  *                                                            *
  *        Begin input-only fields.                            *
  *                                                            *
  *        NOTES: - Many of the filters only apply to JES2 or  *
  *                 JES3.  Each filter is denoted with what    *
  *                 applies.                                   *
  *                                                            *
  *        The filters in JPRCFLTR are a list of PROCLIBs to   *
  *        include in the output area.  For example, setting   *
  *        JPRCFNAM with JPRCPNAM set to PROC00 and setting    *
  *        JPRCFTSO will return PROC00 AND the PROCLIB used    *
  *        for TSO logon.  A particular PROCLIB concatenation  *
  *        will only be returned once even if it matches       *
  *        multiple filters.                                   *
  *                                                            *
  *        If the class specified in JPRCJCLS does not exist,  *
  *        this is considered an input error and no data will  *
  *        be returned.                                        *
  *                                                            *
  **************************************************************/
  char          jprcfltr;
  char          reserved2[7];
 /**************************************************************     
  *       NOTE: Each filter below is enabled via one           *
  *             of the bit settings above.                     *
  **************************************************************/
  /* Offset 0x1C */ 
  char          jprcpnam[8]; 
  char          jprcjcls[8];
  /* Offset 0x2C */
  int           reserved3[10];
 /**************************************************************
  *                                                            *
  *        Begin output-only fields.                           *
  *                                                            *
  **************************************************************/ 
  /* 0ffset 0x54 */
  Addr31        jprclptr;   /* Pointer to first PROCLIB (JPRHDR) data buffer. */
  Addr31        jprcnmbr;   /* Number of PROCLIB (JPRHDR) data buffers returned */
  int           reserved4[11];
} IAZJPROC;

/**************************************************************
 *                                                            *
 *        The following structures define output data for     *
 *        this SSI.  The basic layout is a chain of PROCLIB   *
 *        DD name Information sections.  Each PROCLIB section *
 *        has a common section and a data set section (with   *
 *        the list of data sets in the concatenation).  See   *
 *        diagram below for a high level view of the data     *
 *        structure layout.                                   *
 *                                                            *
 *          PROCLIB info section                              *
 *          +----------------+                                *
 *   JPRHDR | JPRNXTP   =-----------> Pointer to the next     *
 *          |                |        JPRHDR in the chain.    *
 *          |                |        Zero if end of chain.   *
 *          +----------------+                                *
 *  JPRPREF | Prefix Section |                                *
 *          | . . .          |                                *
 *          +----------------+                                *
 *  JPRGENI | General        |                                *
 *          | PROCLIB Data   |                                *
 *          | Section        |                                *
 *          | . . .          |                                *
 *          +----------------+                                *
 *  JPRDSNM | Data Set Data  |                                *
 *          | Section        |                                *
 *          | . . .          |                                *
 *          +----------------+                                *
 *                                                            *
 **************************************************************/

typedef struct JPRHDR_tag{
  char          jpreye[8];   /* "JPRHDR  " */
  uint16_t      jproprf;     /* Offset to prefix section */
  uint16_t      reserved1;
  struct JPRHDR_tag *jprnxtp;  /* linked list link */
  
} JPRHDR;

typedef struct JPRPREF_tag{
  uint16_t      jprprlen;
} JPRPREF;

#pragma pack(reset)

void initSSIB(IEFJSSIB *__ptr32 ssib);

/* Many SSI Functions can use the job's life-of-job SSIB instead 
   of an SSIB built by the requestor for that specific request. 
   */
IEFJSSIB *__ptr32 getJobSSIB();

void initSSOB(IEFSSOBH *__ptr32 ssob, 
	      int functionCode,
	      IEFJSSIB *__ptr32 ssib,
	      Addr31 functionDataBlock);

/* The specific function data block for JES Properties (code == 82)
   is the SSJP.   There is another level underneath this structure 
   and this convenience function helps initialize the request.
   */

void initSSJP(IAZSSJP *__ptr32 ssjp, char requestType, Addr31 specificBlock);

int callSSI(IEFSSOBH *__ptr32 ssob);

#endif




/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
