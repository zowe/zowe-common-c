

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/


#ifndef __VTAM__
#define __VTAM__

/** \file
 *  \brief vtam.h is a set of utilities for handling VTAM constructs in C.   
 * 
 * 
 *  zOS Only (obviously).   There are a view TSO and ISPF structures in this file, too.  
 */

#pragma pack(packed)

typedef struct IKJTSB_tag{
  int            tcbstatAndASCB;          /* ASCB in low 3 bytes */
#define TSBINUSE 0x80
#define TSBLWAIT 0x40 /* beyboad */
#define TSBDSPLY 0x20
#define TSBNOBUF 0x10
#define TSBITOFF 0x08
#define TSBDISC  0x04
#define TSB3270  0x02 /* this and TSBINUSE is a live terminal */
#define TSBATNLD 0x01
  int            tsbflagsAndTCBWaiting;   /* TCB waiting on ECB  */
#define TSBANSR  0x80
#define TSBOFLSH 0x40  /* to be flushed */
#define TSBOWIP  0x20  /* TPUT in progress */
#define TSBWOWIP 0x10  /* Waiting in OWAIT in progress */
#define TSBIFLSH 0x08  /* Input flush in progress */
#define TSBTJOW  0x04
#define TSBTJIP  0x02
#define TSBTJBF  0x01
  int            physicalLineSizeAndTrailerBuffer;
  int            numberOfOutputBuffersAndOutputBufferQueue;
  /* OFFSET 0x10 */
  int            tsbFlags2AndInputTrailerBuffers;
  int            numberOfInputBuffersAndInputBufferQueue;
  char           tsbflags3;
  char           tsbflags5;
  unsigned short tsbtermc; 
  int            tsbecb;
  /* OFFSET 0x20 */
  unsigned short tsbwtjid;
  unsigned short tsbstcc; 
  unsigned short tsbatnlc;
  unsigned short tsbatntc; 
  char           tsblnno;   /* number of lines */
  char           tsbflag4;
  unsigned short tsbasrce;
  char           tsbatncc[4];
  /* OFFSET 0x30 */
  int            tsbautos;
  int            tsbautoi;
  int            tsbersds;  /* Screen erasing chars -- It would be fun to change this */
  int            tsbctcb;   /* The TPUT-ing TCB */
  /* OFFSET 0x40 */
  char           tsbrecap[8];  /* TCAS APP LU NAME */
  char           tsbalunm[8];  /* VTAM APPL LU NAME */
  /* OFFSET 0x50 */
  char           crap[16]; 
  /* OFFSET 0x60 */
  Addr31         tsbextnt;  /* TSB Extension - very valuable */
  char           tsbprmr;   /* Primary rows */
  char           tsbprmc;   /* Primary columns */
  char           tsbaltr;   /* Alternate Rows  */
  char           tsbaltc;
  char           tsbtrmid[8]; /* symbolic name */
  /* OFFSET 0x70 */
  Addr31         tsbSAFGroup;  /* Really */
  int            unknown;
} IKJTSB;

typedef struct IKTTSBX_tag{
  int            flagAndFwdPointer;
  int            flagAndBwdPointer; 
  int            tsbxecb;      /* X-Mem sync */
  Addr31         tsbxnib;      /* NIB for logmode */
  char           tsbxlmod[8];
  char           tsbxuid[8];
  /* OFFSET 0x20 */
  char           tsbxflg1;
  char           tsbxflg2;
  char           tsbxflg3;
  char           tsbxlang;     /* Language */
  Addr31         tsbxtvwa;     /* TSO/TVWA work area A(IKTTVWA) */
  Addr31         tsbxtim;      /* proto manager, inbound */
  Addr31         tsbxtom;      /* proto manager, outbound */
  /* OFFSET 0x30 */
  char           tsbxgcid[4];  /* GCID received in RPQ reply */
  Addr31         tsbxsrbi;     /* TIM SRB */
  Addr31         tsbxsrb;      /* TOM SBR */
  Addr31         tsbxcsap;     /* POINTER TO THE CSA AREA FOR ASID TPUS */
  /* OFFSET 0x40 */
  Addr31         tsbxlbuf;     /* LOGON BUFFER */
  char           tsbxrszi;     /* Input RU Size */
  char           tsbxrszo;     /* Output RU Size */
  short          tsbxaind;     /* TSO/VT User APPL Index */
  char           tsbxtmtp;     /* Terminal Type */
  char           tsbxsubt;     /* Terminal Subtype */
  short          tsbxtmbf;     /* Buffer size */
  Addr31         tsbxrpl;      /* Pointer to RPL in TCAS */
  /* OFFSET 0x50 */
  char           tsbxbind[36]; /* Terminal Bind Image */
  Addr31         tsbxdmmi;     /* Mapping Manager, Inbound  */
  Addr31         tsbxdmmo;     /* Mapping Manager, Outbound */
  char           tsbxdcs[4];   /* DBCS if available   */
  /* OFFSET 0x80 */
  char           tsbxlngc[3];  /* Current Lanaguage   */
  char           tsbxflg4;
  char           tsbxurc[4];   /* URC Data (what is this?) */
  char           tsbxtrmr[8];  /* Real Name of Terminal */
  /* OFFSET 0x90 */
  char           tsbxnet[8];   /* NETID, of terminal, like ROCKNET1 */
  char           tsxbxipad[4]; /* IP Address, V4, if available */
  unsigned short tsbxport;     /* TCP Port */
  unsigned short tsbxdnsl;     /* length of DNS NAME */
  char           tsbxdns[131]; /* DNS Name */
  /* OFFSET 0x123 */
  char           tsbxflg5[4];  /* alignment, i doubt it */
  char           tsbxzolg;     /* IPv6 zone length      */
  /* OFFSET 0x128 */
  char           tsbxipv6[16]; /* IPv6 Addr */
  /* OFFSET 0x138 */
  char           tsbxzone[16]; /* IPv6 Zone */
  /* OFFSET 0x148 */
  char           tsbxpipl;     /* Printable IP Addr Length */
  char           tsbxpipc[39]; /* Printable IP Address     */
  /*
     HERE 
     1) Flesh this out
     3) services (standard fitlering REST?)
     4) Return to dispatch
     7) RPAC meetings
     9) Kohls, can get Bind image if negotiable
         but where does the NEW_ENVIRON go ??  

  */
} IKTTSBX;



typedef struct IKTTCAST_tag {
  char           eyecatcher[4];          /* "TCAS" */
  unsigned short tcasusec;               /* Number of Users                  */
  unsigned short tcasumax;               /* Max Users                        */
  char           tcasacbp[8];            /* ACB Password (probably obsolete) */
  /* OFFSET 0x010 */
  unsigned short tcasrcon;               /* RECONNECT TIME IN MINUTES        */
  unsigned short tcasclsz;               /* Cell Size (of what?)             */
  int            tcashbuf;               /* High Buffer Threshold            */
  int            tcaslbuf;               /* Low Buffer ....                  */
  unsigned short tcascrsz;               /* 3270 Screen size                 */
  char           tcaschnl;               /* Max Chain Length (of what?)      */
  char           reserved01F; 
  /* OFFSET 0x020 */
  char           tcasstid[8];            /* Symbolic Terminal Identifier     */
  int            tcasxecb;               /* Cross Memory Synch ECB           */
  Addr31         tcasdati;               /* Input Data Processor             */
  /* OFFSET 0x030 */
  Addr31         tcasdato;               /* OUtput Data Processor            */
  Addr31         tcasmsgs;               /* LPALIB Message Module            */
  Addr31         tcasfrr;                /* IO FRR Module                    */
  Addr31         tcasswa;                /* TCAS Work Area                   */
  /* OFFSET 0x040 */
  Addr31         tcasttl;                /* TIM/TOM List                     */
  Addr31         tcastsb;                /* First TSO/VTAM TSB               */
  Addr31         tcasiqm;                /* Input Queue Manager              */
  Addr31         tcasoqm;                /* Output Queue Manager             */
  /* OFFSET 0x050 */
  Addr31         tcasexit;               /* TIM/TOM Exit Routine             */
  Addr31         tcalte;                 /* LOWTERM Exit Module              */
  char           tcasflg1;               /* Flags: 
                                            1... ....           TCASBKMD       TERMINAL HAS BREAK MODE
                                            .1.. ....           TCASMDSW       BREAK MODE SWITCH ALLOWED
                                            ..1. ....           TCASABND       TCAS ABENDED
                                            ...1 ....           TCASVSD        VTAM SHUTTING DOWN
                                            .... 1...           TCASNACT       TCAS NOT ACTIVE
                                            .... .1..           TCASHAST       HALT ISSUED, ADDRESS SPACE
                                            TERMINATED
                                            .... ..1.           *              RESERVED
                                            .... ...1           TCASCONF       RESTRICTED BUFFERS
                                            */
  char           tcasflg2;               /* Flags: 
                                            1... ....           *              RESERVED
                                            .1.. ....           *              RESERVED
                                            ..11 1111           *              RESERVED
                                            */
  char           tcasflg3;
  char           tcasflg4;
  Addr31         tcasascb;               /* TCAS ASCB                        */
  Addr31         tcastgtf;               /* TCAS GTF Trace Routine           */
} IKTTCAST;

typedef struct ISPFTLD_tag{
  char           eyecatcher[4];
  Addr31         swapTLD;
  Addr31         thisTLD;
  Addr31         unknown00C;
  /* OFFSET 0x10 */
  Addr31         unknown010;
  Addr31         unknown014; /* TLD pointer */
  Addr31         tlf;        /* fields */
  Addr31         tla;        /* attributes */
  /* OFFSET 0x20 */
  Addr31         tct;        /* Char translation */
  Addr31         tds;
  Addr31         unknown028;
  Addr31         teg;
  /* OFFSET 0x30 */
  Addr31         unknown030;
  Addr31         isptsc0;
  Addr31         isptsi;
  Addr31         tsv;
  /* OFFSET 0x40 */
  Addr31         isptsp0;
  Addr31         unknown044;
  Addr31         unknown048;
  Addr31         dta;
  /* OFFSET 0x50 */
  Addr31         vcb;
  Addr31         unknown054;
  Addr31         unknown058;
  Addr31         unknown05C;
  /* OFFSET 0x60 */
  Addr31         screenImage;    /* logical window, w/o overlays */
  Addr31         unknown064;
  Addr31         unknown068;
  Addr31         unknown06C;
  /* OFFSET 0x70 */
  Addr31         unknown070;
  Addr31         unknown074;
  Addr31         unknown078;
  Addr31         tldEnvironment;  /* Environment VARS */
  /* OFFSET 0x80 */
  Addr31         wholeScreen;     /* with overlays */
  Addr31         unknown084;
  Addr31         unknown088;
  Addr31         unknown08C;
  /* OFFSET 0x90 */
  Addr31         unknown090;
  Addr31         unknown094;
  Addr31         unknown098;
  Addr31         unknown09C;
  /* OFFSET 0xA0 */
  Addr31         unknown0A0;
  Addr31         unknown0A4;
  unsigned short unknown0A8;
  unsigned short cursorOffset;  
  Addr31         unknown0AC;
  /* OFFSET 0xB0 */
  Addr31         unknown0B0;
  Addr31         unknown0B4;
  Addr31         unknown0B8;
  int            numberOfRows;
  /* OFFSET 0xC0 */
  int            screenWidth;
  char           unmapped[0x94];
  /* 0x100 Scroll Var
     0x108 Scroll Mode Value 
     */
  char           panelid[8];
  char           previousPanel[8];
  char           helpPanelid[8];   /* can be blank/null */
  /* Messages */
} ISPFTLD;


#pragma pack(reset)

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

