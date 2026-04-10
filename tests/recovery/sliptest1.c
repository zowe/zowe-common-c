

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  sliptest1 — Display SLIP trap summary from CVT -> CVTRTMS -> SHDR.

  The SCE (individual trap) entries are fetch-protected, so we can only
  read the SHDR header: trap count, chain pointers, PER state, etc.
*/

#include <stdio.h>
#include <string.h>

#include "zowetypes.h"
#include "zos.h"
#include "utils.h"
#include "mvscmd.h"

#pragma pack(packed)

struct shdr {
  unsigned char    shdrcbid[4];       /* 0x00 eyecatcher "SHDR"          */
  void * __ptr32   shdrpfc;           /* 0x04 PFC pointer                */
  union {                             /* 0x08                            */
    double           _shdrctfw;
    struct {
      void * __ptr32   _shdrctr;      /* 0x08 CTR                        */
      void * __ptr32   _shdrfwd;      /* 0x0C fwd ptr PER SCE chain      */
    } _shdr_struct1;
  } _shdr_union1;
  void * __ptr32   shdrbkwd;          /* 0x10 bkwd ptr PER SCE chain     */
  union {                             /* 0x14                            */
    int              _shdrflcs;
    struct {
      unsigned char    _shdrflgs;     /* 0x14 flags byte 1               */
      unsigned char    _shdrflg2;     /* 0x15 flags byte 2               */
      short int        _shdridct;     /* 0x16 total trap count           */
    } _shdr_struct2;
  } _shdr_union2;
  void * __ptr32   shdrproc;          /* 0x18 SLIP processor routine     */
  void * __ptr32   shdrper;           /* 0x1C enabled non-ignore PER trap*/
  void * __ptr32   shdrsrtn;          /* 0x20 search routine             */
  void * __ptr32   shdrperr;          /* 0x24 PER routine                */
  void * __ptr32   shderpera;         /* 0x28 PER activation chain       */
  void * __ptr32   shdrperj;          /* 0x2C PER jobstep chain          */
  union {                             /* 0x30                            */
    int              _shdrseq;
    struct {
      unsigned char    _filler1[2];
      unsigned char    _shdrseqa[2];  /* 0x32 sequence number            */
    } _shdr_struct3;
  } _shdr_union3;
  void * __ptr32   shdrsrb;           /* 0x34 SRB                        */
  void * __ptr32   shdrpost;          /* 0x38 POST                       */
  unsigned char    shdrcpid[4];       /* 0x3C cell pool ID               */
  unsigned char    shdrecb[4];        /* 0x40 ECB                        */
  unsigned char    shdrgecb[4];       /* 0x44 global ECB                 */
  unsigned char    shdrlecb[4];       /* 0x48 local ECB                  */
  union {                             /* 0x4C                            */
    unsigned char    _shdrcreg[12];
    unsigned char    _shdrcr9[4];
    struct {
      unsigned char    _shdrc9;
      unsigned char    _shdrc91;
      unsigned char    _filler2[2];
      struct {
        unsigned char    _shdrcr10[4];
        unsigned char    _shdrcr11[4];
      } _shdrcrs;
    } _shdr_struct4;
  } _shdr_union4;
  void * __ptr32   shdrcms1;          /* 0x58 CMS1                       */
  void * __ptr32   shdrcmr1;          /* 0x5C CMR1                       */
  void * __ptr32   shdrcmr2;          /* 0x60 CMR2                       */
  void * __ptr32   shdrsshp;          /* 0x64 subsystem header           */
  unsigned char    shdrsstm[8];       /* 0x68 subsystem timestamp        */
  void * __ptr32   shdrss1p;          /* 0x70 subsystem 1 pointer        */
  void * __ptr32   shdrpcde;          /* 0x74 PCDE                       */
  void * __ptr32   shdrpvl1;          /* 0x78 PVL1                       */
  void * __ptr32   shdrpvd1;          /* 0x7C PVD1                       */
  unsigned char    shdrpvmn[8];       /* 0x80 PV module name             */
  unsigned char    shdrpvas[2];       /* 0x88 PV ASID                    */
  unsigned char    shdrpvfl;          /* 0x8A PV flags                   */
  unsigned char    shdrflag;          /* 0x8B general flag               */
  void * __ptr32   shdrpasc;          /* 0x8C PV ASCB                    */
  void * __ptr32   shdrptcb;          /* 0x90 PV TCB                     */
  void * __ptr32   shdrpvr1;          /* 0x94 PVR1                       */
  unsigned char    shdrstid[4];       /* 0x98 ST ID                      */
  void * __ptr32   shdrmlc;           /* 0x9C MLC                        */
  short int        shdrmlt;           /* 0xA0 MLT                        */
  short int        _filler3;          /* 0xA2                            */
  int              _filler4;          /* 0xA4                            */
  union {                             /* 0xA8                            */
    double           _shdrctf2;
    struct {
      void * __ptr32   _shdrctr2;     /* 0xA8 CTR2                       */
      void * __ptr32   _shdrfwd2;     /* 0xAC fwd ptr non-PER SCE chain  */
    } _shdr_struct5;
  } _shdr_union5;
  void * __ptr32   shdrbwd2;          /* 0xB0 bkwd ptr non-PER SCE chain */
  void * __ptr32   shdridq;           /* 0xB4 ID queue head              */
  void * __ptr32   shdridql;          /* 0xB8 ID queue tail              */
  void * __ptr32   shdrwmsg;          /* 0xBC WTO message buffer         */
  void * __ptr32   shdrxadr;          /* 0xC0 extended address           */
  void * __ptr32   shdrcms2;          /* 0xC4 CMS2                       */
  void * __ptr32   shdrcmr3;          /* 0xC8 CMR3                       */
  void * __ptr32   shdrcmr4;          /* 0xCC CMR4                       */
};

#define shdrfwd   _shdr_union1._shdr_struct1._shdrfwd
#define shdridct  _shdr_union2._shdr_struct2._shdridct
#define shdrflgs  _shdr_union2._shdr_struct2._shdrflgs
#define shdrflg2  _shdr_union2._shdr_struct2._shdrflg2
#define shdrfwd2  _shdr_union5._shdr_struct5._shdrfwd2

#pragma pack(reset)

int main(void) {
  CVT *cvt = getCVT();
  void *rtms = (void *)(unsigned long)cvt->cvtrtms;

  if (!rtms) {
    printf("SLIP: no SHDR (CVTRTMS is NULL) — SLIP has never been initialized\n");
    return 0;
  }

  struct shdr *hdr = (struct shdr *)rtms;

  /* Validate eyecatcher */
  if (memcmp(hdr->shdrcbid, "\xe2\xc8\xc4\xd9", 4) != 0) {  /* EBCDIC "SHDR" */
    printf("SLIP: SHDR at %p has bad eyecatcher: %02x%02x%02x%02x\n",
           rtms,
           hdr->shdrcbid[0], hdr->shdrcbid[1],
           hdr->shdrcbid[2], hdr->shdrcbid[3]);
    return 1;
  }

  int trapCount = (int)hdr->shdridct;
  int hasPER    = (hdr->shdrfwd != 0);
  int hasNonPER = (hdr->shdrfwd2 != 0);

  printf("SLIP Trap Summary (SHDR at 0x%08X)\n", (unsigned int)(unsigned long)rtms);
  printf("  Total traps defined:  %d\n", trapCount);
  printf("  PER trap chain:       %s\n",
         hasPER ? "active" : "empty");
  printf("  Non-PER trap chain:   %s\n",
         hasNonPER ? "active" : "empty");
  printf("  Flags:                0x%02x 0x%02x\n",
         hdr->shdrflgs, hdr->shdrflg2);

  if (hasPER) {
    printf("  PER chain fwd:        0x%08X\n",
           (unsigned int)(unsigned long)hdr->shdrfwd);
    printf("  Active PER trap:      0x%08X\n",
           (unsigned int)(unsigned long)hdr->shdrper);
  }

  if (hasNonPER) {
    printf("  Non-PER chain fwd:    0x%08X\n",
           (unsigned int)(unsigned long)hdr->shdrfwd2);
    printf("  Non-PER chain bkwd:   0x%08X\n",
           (unsigned int)(unsigned long)hdr->shdrbwd2);
  }

  if (hdr->shdridq) {
    printf("  ID queue:             0x%08X\n",
           (unsigned int)(unsigned long)hdr->shdridq);
  }

  /* Get trap details via MVS command */
  if (trapCount > 0) {
    printf("\nTrap details (via D SLIP,ID=ALL):\n");
    MvsCmdResult cmdResult;
    int cmdRC = mvsCmdIssue("D SLIP,ID=ALL", &cmdResult);
    if (cmdRC == 0) {
      for (int i = 0; i < cmdResult.lineCount; i++) {
        printf("  %s\n", cmdResult.lines[i]);
      }
    } else {
      printf("  (D SLIP,ID=ALL failed rc=%d — may need CONSOLE authority)\n",
             cmdRC);
    }
    mvsCmdFree(&cmdResult);
  }

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
