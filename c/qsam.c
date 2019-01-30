

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/


#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#else
#include <stdio.h>
#include <ctest.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "qsam.h"
#include "zos.h"
#include "alloc.h"
#include "metalio.h"

char *malloc24(int size){
  char *data;
  __asm(ASM_PREFIX
        " STORAGE OBTAIN,LENGTH=(%[length]),LOC=24\n"
        :[address]"=NR:r1"(data)
        :[length]"NR:r0"(size)
        :"r15");
  return data;
}

static char *malloc31(int size){
  char *data;
  __asm(ASM_PREFIX
        " STORAGE OBTAIN,LENGTH=(%[length]),LOC=31\n"
        :[address]"=NR:r1"(data)
        :[length]"NR:r0"(size)
        :"r15");
  return data;
}

char *free24(char *data, int size){
  /* printf("attempting to free, size: %d\n", size);*/
  __asm(ASM_PREFIX
        " STORAGE RELEASE,LENGTH=(%[length]),ADDR=(%[address]),CALLRKY=YES\n"  /* ,KEY=8 */
        :
        :[length]"NR:r0"(size), [address]"NR:r1"(data)
        :"r15");
  /* printf("succeed\n"); */
  return NULL;
}

static void free31(void *data, int size){
  /* printf("attempting to free, size: %d\n", size);*/
  __asm(ASM_PREFIX
        " STORAGE RELEASE,LENGTH=(%[length]),ADDR=(%[address]),CALLRKY=YES\n"  /* ,KEY=8 */
        :
        :[length]"NR:r0"(size), [address]"NR:r1"(data)
        :"r15");
  /* printf("succeed\n"); */
  return;
}


char *makeDCB(char *ddname,
              int dcbLen,
              int dsorg,
              int openFlags,
              int macroFormat,
              int optionCodes,
              int recfm,      /* zero for unspecified */
              int lrecl,      /* ditto */
              int blksize){   /* ditto */

  int ddnameLen = strlen(ddname);

  /* The maximum allowed block size (without using a DCBE) is
     32760, or X'7FF8'.  Negative blocksize values are being
     used to indicate that special processing is required for
     the DCB when it is opened.  The following values are defined-

       -1 - Open the DCB with Recfm and Lrecl not specified.  If
            DCB merge processing does not set these values then
            use the values specified when the DCB was built to
            complete open processing for the DCB.  The blocksize
            will be set to 0 to request a system determined
            blocksize.

       -2 - Same as -1, except if the DCB is a subsystem (ie,
            JES spooled) force the record format to be unblocked
            with the minimum allowed blocksize for the Lrecl.
  */

  /*
     It seems that when OPEN is called old-school mode, not only
     the dcb needs to be in 24-bit land, but the plist pointing
     to it as well.  MVS is a stern mistress.  So we prepend the
     the plist to the 4-bytes immediately preceding the dcb.
     */

  /* Note: The OPEN/CLOSE parameter list *can* reside in 31-bit
     storage if a MODE=31 format paramater list is built.  This
     parameter list has two words for each DCB.  The first word
     contains the flag byte in bits 0-7 of a MODE=24 parameter
     list in bits 0-7 and the rest of the word set to 0.  The
     second word contains the DCB address.  Then the OPEN/CLOSE
     SVC is issued the address of the MODE=31 parameter list
     must be in R0 and R1 must be set to 0.  An OPEN/CLOSE
     parameter list can not reside in 64-bit storage, though, so
     it is easiest to continue to allocate space for the
     OPEN/CLOSE parameter list prior to the DCB.                   */

  /* The area obtained for the DCB will be large enough to open and
     use it with QSAM, BSAM/BPAM, or EXCP processing.  The dcbLen
     value specified by the caller is ignored.

     A DCBE is also allocated for the DCB, to allow an EODAD
     routine with a 31 bit address to be specified.                 */

  /* We want the DCB to start on a double word boundary, but a 4
     byte open parameter list precedes it and adding reserved
     storage to the prefix will break lots of code that has
     hardcoded a length of 4 for the size of the prefix.  To cope
     with this, we add 4 bytes to the amount of storage that we
     obtain and then skip the first 4 bytes.  When we free the
     storage we must reverse this.                                  */

  dcbSAMwithPlist *newDcb = (dcbSAMwithPlist *)(malloc24(sizeof(dcbSAMwithPlist)+4)+4);


  newDcb->OpenCloseList = &newDcb->dcb;

  memset(&newDcb->dcb,0,sizeof(newDcb->dcb));
  memset(&newDcb->dcbe,0,sizeof(newDcb->dcbe));

  /* Build the common portion of the DCB */

  newDcb->dcb.Common.bufcb = 0x000001; /* implies no buffers */
  newDcb->dcb.Common.dsorg = dsorg;
  newDcb->dcb.Common.reserved1 = 0x00000001; /* because that's what macro does */
  newDcb->dcb.Common.eodad = 0x000001; /* impies no EndOfData routine */
  newDcb->dcb.Common.recfm = recfm & 0xff;
  for (int i=0; i<8; i++)
    newDcb->dcb.Common.ddname[i] = (i<ddnameLen) ? ddname[i] : ' ';
  newDcb->dcb.Common.openFlags = openFlags;
  newDcb->dcb.Common.macroFormat = (macroFormat&0xffff);
  newDcb->dcb.BBQCommon.optionCodes = optionCodes;
  newDcb->dcb.BBQCommon.check = 0x000001; /* implies no check */
  newDcb->dcb.BBQCommon.synad = 0x00000001; /* implies no synad */
  newDcb->dcb.BBQCommon.blksize = blksize & 0xffff;
  newDcb->dcb.BBQCommon.internalMethodUse = 0x00000001; /* see in macro output */

  /* Build the BSAM/BPAM/QSAM interface specific portion if needed */

  if (((dsorg == DCB_ORG_PS) || (dsorg == DCB_ORG_PO)) &&
      (macroFormat != DCB_MACRF_EXCP)){
    newDcb->dcb.BBQInterface.eobr = 0x000001;
    newDcb->dcb.BBQInterface.eobw = 0x00000001;
    newDcb->dcb.BBQInterface.lrecl = lrecl;
    newDcb->dcb.BBQInterface.cnp = 0x00000001;

    /* Build the DCB extension (aka, DCBE) and connect it to the DCB */

    memcpy(&newDcb->dcbe.id,"DCBE",sizeof(newDcb->dcbe.id));
    newDcb->dcbe.len = sizeof(newDcb->dcbe);
    newDcb->dcbe.eoda = getEODADBuffer();

    /* DCB named bits DCBH0 and DCBH1 set on indicates that a DCBE has been connected */

    newDcb->dcb.Device.dcbe = &newDcb->dcbe;
    newDcb->dcb.Common.dcbeIndicators = 0x84;  /* Show DCBE has been connected to the DCB */
  }

  return (char *) newDcb;
}

#ifdef DCBDIAG
static void formatRecfm(int dcbRecfm, char *fmtRecfm){
  int  i = 1;
  if (dcbRecfm){
    switch (dcbRecfm&DCB_RECFM_FORMAT){
       case DCB_RECFM_V: fmtRecfm[0] = 'V';
         break;
       case DCB_RECFM_F: fmtRecfm[0] = 'F';
         break;
       case DCB_RECFM_U: fmtRecfm[0] = 'U';
         break;
       default:
         fmtRecfm[0] = 'X';
    }

    if (dcbRecfm&DCB_RECFM_B){
      fmtRecfm[1] = 'B';
      i++;
    }

    if (dcbRecfm&DCB_RECFM_S){
      fmtRecfm[i] = 'S';
      i++;
    }

    if (dcbRecfm&DCB_RECFM_M){
      fmtRecfm[i] = 'M';
      i++;
    } else if (dcbRecfm&DCB_RECFM_A){
      fmtRecfm[i] = 'A';
      i++;
    }
    fmtRecfm[i] = 0;
  } else
    strcpy(fmtRecfm,"NULL");
}

static void formatDsorg(int dcbDsorg,char *fmtDsorg){
  int  i = 0;
  if (dcbDsorg==DCB_ORG_PS)
    strcpy(fmtDsorg,"PS");
  else if (dcbDsorg==DCB_ORG_PO)
    strcpy(fmtDsorg,"PO");
  else
    snprintf(fmtDsorg,5,"%04.4X",dcbDsorg);
}

static void formatMacrf(int dcbMacrf,char *fmtMacrf){
  int i = 0;
  if (dcbMacrf&DCB_MACRF_EXCP){
    fmtMacrf[i]='E';
    i++;
  } else {
    if (dcbMacrf&DCB_MACRF_GET){
      if (dcbMacrf&DCB_MACRF_PUT){
        fmtMacrf[i]='(';
        i++;
      }
      fmtMacrf[i]='G';
      i++;
      /* Note: Data also sets Move, test for Data first */
      if (dcbMacrf&DCB_MACRF_DATA_GET){
        fmtMacrf[i]='D';
        i++;
      } else if (dcbMacrf&DCB_MACRF_MV_GET){
        fmtMacrf[i]='M';
        i++;
      } else if (dcbMacrf&DCB_MACRF_LOC_GET){
        fmtMacrf[i]='L';
        i++;
      }
      if (dcbMacrf&DCB_MACRF_CNTRL){
        fmtMacrf[i]='C';
        i++;
      }
    } else if (dcbMacrf&DCB_MACRF_READ){
      if (dcbMacrf&DCB_MACRF_WRITE){
        fmtMacrf[i]='(';
        i++;
      }
      fmtMacrf[i]='R';
      i++;
      if (dcbMacrf&DCB_MACRF_PT1){
        fmtMacrf[i]='P';
        i++;
      } else if (dcbMacrf&DCB_MACRF_CNTRL){
        fmtMacrf[i]='C';
        i++;
      }
    }
    if (dcbMacrf&DCB_MACRF_PUT){
      if (dcbMacrf&DCB_MACRF_GET){
        fmtMacrf[i]=',';
        i++;
      }
      fmtMacrf[i]='P';
      i++;
      /* Note: Data also sets Move, test for Data first */
      if (dcbMacrf&DCB_MACRF_DATA_MOD){
        fmtMacrf[i]='D';
        i++;
      } else if (dcbMacrf&DCB_MACRF_MV_PUT){
        fmtMacrf[i]='M';
        i++;
      } else if (dcbMacrf&DCB_MACRF_LOC_PUT){
        fmtMacrf[i]='L';
        i++;
      }
      if (dcbMacrf&DCB_MACRF_CNTRL2){
        fmtMacrf[i]='C';
        i++;
      }
    } else if (dcbMacrf&DCB_MACRF_WRITE){
      if (dcbMacrf&DCB_MACRF_READ){
        fmtMacrf[i]=',';
        i++;
      }
      fmtMacrf[i]='W';
      i++;
      if (dcbMacrf&DCB_MACRF_PT2){
        fmtMacrf[i]='P';
        i++;
      } else if (dcbMacrf&DCB_MACRF_CNTRL2){
        fmtMacrf[i]='C';
        i++;
      } else if (dcbMacrf&DCB_MACRF_LD_MODE){
        fmtMacrf[i]='L';
        i++;
      }
    }
    if (fmtMacrf[0]=='('){
      fmtMacrf[i]=')';
      i++;
    }
    fmtMacrf[i]=0;
  }
  if (i==0)
    snprintf(fmtMacrf,5,"%04.4X",dcbMacrf);
}
static int printDCBinfo(dcbSAMwithPlist *reqDcb, openexitData_t *openexitData,char *phase){
  char recfm[5];
  char dsorg[5];
  int dcbmacrf;
  char macrf[10];

  if (!(reqDcb->dcb.Common.openFlags&(DCB_OPENFLAGS_OPEN|DCB_OPENFLAGS_BUSY))){
    wtoPrintf("%s - DD name: %8.8s\n",phase,reqDcb->dcb.Common.ddname);
    dcbmacrf = reqDcb->dcb.Common.macroFormat;
  }
  else {
    wtoPrintf("%s\n",phase);
    dcbmacrf = reqDcb->dcb.Common.openmacroFormat;
  }

  if (openexitData){
    formatRecfm(openexitData->exitrecfm,recfm);
    wtoPrintf("Open data:  RECFM=%s, LRECL=%d, BLKSIZE=%d\n",recfm,openexitData->exitlrecl,openexitData->exitblksize);
  }

  formatRecfm(reqDcb->dcb.Common.recfm,recfm);
  formatDsorg(reqDcb->dcb.Common.dsorg,dsorg);
  formatMacrf(dcbmacrf,macrf);
  wtoPrintf("DCB data: RECFM=%s, LRECL=%d, BLKSIZE=%d, DSORG=%s, MACRF=%s\n",
            recfm,reqDcb->dcb.BBQInterface.lrecl,reqDcb->dcb.BBQCommon.blksize_s,
            dsorg,macrf);
}

#endif

typedef struct {           /* Holds information used by the DCB open exit */
  char exitlinkage[32];    /* Switch to AMODE 31 and branch */
  void * __ptr32 exitaddress;
  void * __ptr32 exitreturn;
  int  exithighhalves[16];
  unsigned int exitlist;
  char exitactive;
  char exitrecfm;
  char exitrsvd[2];
  int  exitblksize;
  int  exitlrecl;
} openexitData_t;

/* openCloseBBQ opens and closes BSAM, BPAM, and QSAM access methods */
static int openCloseBBQ(char *dcb, int mode, int svc){
  int rc;

  dcbSAMwithPlist *reqDcb = (dcbSAMwithPlist *)dcb;

  /* Set the mode flags in the Open/Close parameter list */

  reqDcb->OpenCloseListOptions |= (mode&0xFF000000) | 0x80000000;

  if (svc == 19){
    void * __ptr32 openexitAddr = NULL;
    int openexitDo = 0;
    openexitData_t *openexitData = NULL;

 /* When a SAM DCB is created the requestor can indicate that the
    record format and logical record length for the DCB are preferred,
    but not required, specifications by specifying a value of -1 or -2
    for the block size.  The specified values for the record format,
    logical record length, and block size for the dataset will be
    cleared from the DCB prior to issuing the OPEN and a DCB OPEN exit
    will be used to set values for any of these attributes which are
    not merged into the DCB from DD specifications or existing dataset
    attributes.

    Specification of either ANSI or machine carriage control, and only
    that attribute, for the record format is a preference for, but not a
    requirement of, the record format that will be used.  It will only
    be used if a record format is not set into the DCB prior to the
    DCB OPEN exit being given control.  The issuer of the OPEN request
    must be prepared to handle a dataset with or without a carriage
    control character, which may be in either ANSI or machine format.

    To determine if an ANSI or machine carriage control character
    exist as the first byte of the data for a DCB that was opened
    without any dataset attributes specified the requestor should
    use function getSAMCC or getSAMRecfm.  getSAMCC returns a value
    indicating none, ANSI, or machine carriage control for the DCB.
    getSAMRecfm returns the record format data from the DCB and must
    be broken down be the requestor.

    To determine the maximum record length for a DCB that was opened
    without any dataset attributes specified the requestor should
    use function getSAMLength or getSAMLrecl.  getSAMLength returns
    the maximum 'usable' record length (which excludes the length of
    a prefixed record descriptor word for variable record format).
    getSAMLrecl returns the logical record length from the DCB and
    the requestor must take into account if the length includes a
    prefixed record descriptor word for variable record format.

    When allocating or freeing a input/output record area for a DCB
    that is opened without any dataset attributes specified the
    requestor should always use functions makeQSAMBuffer and
    freeQSAMBuffer.  These functions will properly handle a DCB with
    a fixed, undefined, or variable record format.  The prefixed
    record descriptor word for a variable record format is allocated
    but is *not* externalized to the requestor in the address that
    is returned by or passed to the function.

    When writing data to a DCB that was opened without any dataset
    attributes specified the requestor should always use function
    putlineV.  This function will properly handle a DCB with a
    fixed, undefined, or variable record format.  When using
    variable record format the prefixed record descriptor word is
    not provided by the requestor.

    When reading data from a DCB that was opened without any dataset
    attributes specified the requestor should always use function
    getlineV.  This function will properly handle a DCB with a
    fixed, undefined, or variable record format.  When using
    variable record format the prefixed record descriptor word is
    not externalized to the requestor.

 */

    if (((reqDcb->dcb.Common.dsorg == DCB_ORG_PS) || (reqDcb->dcb.Common.dsorg == DCB_ORG_PO)) &&
       reqDcb->dcb.Common.macroFormat != DCB_MACRF_EXCP &&
       reqDcb->dcb.BBQCommon.blksize_s <= 0) {

      openexitData = (openexitData_t *)malloc24(sizeof(openexitData_t));
      memset(openexitData,0,sizeof(openexitData_t));

   /* If a block size of -2 is specified as the value for the block
      size, if a non-0 value is not merged into the DCB prior to the
      DCB OPEN exit getting control, then the block size to be used
      will depend on if the output is being written to the JES
      subsystem or a physical dataset.

      - For DDs allocated to the JES subsystem the use of record
        blocking does not make sense.  The exit will make the block
        size be the same as the record length, so that the record
        will be immediately passed to JES for processing. The record
        will then be visible in real time with tools like SDSF.

      - For DDs allocated to a physical dataset the use of record
        blocking makes sense.  The exit will leave the block size set
        to 0 so that a system determined blocksize value will be set
        by OPEN processing.

      We determine if the DD is allocated to the JES subsystem by
      getting the DSAB address for the DCB DD name, use it to locate
      the TIOT entry for the DD name, and check to see if bit
      TIOESYOT is on which indicates it is allocated as SYSOUT.
   */

      openexitData->exitrecfm = reqDcb->dcb.Common.recfm;  /* Save for the exit to use */
      reqDcb->dcb.Common.recfm = 0;   /* Always perform OPEN with a value of 0 */

      openexitData->exitlrecl = reqDcb->dcb.BBQInterface.lrecl; /* Save for the exit to use */
      if ((openexitData->exitrecfm&DCB_RECFM_FORMAT) == DCB_RECFM_V)
        openexitData->exitlrecl -= sizeof(RDW); /* Reduce by the RECFM=V RDW size */
      reqDcb->dcb.BBQInterface.lrecl = 0;   /* Always perform OPEN with a value of 0 */

      openexitData->exitblksize = reqDcb->dcb.BBQCommon.blksize_s; /* Save for the exit to use */
      reqDcb->dcb.BBQCommon.blksize = 0;   /* Always perform OPEN with a value of 0 */

      if (openexitData->exitblksize == -2) { /* If sysout block size processing requested */
        DSAB *dsabptr = getDSAB(&reqDcb->dcb.Common.ddname[0]);
        if (dsabptr) {
          TIOTEntry *tiotentry = (TIOTEntry *)dsabptr->dsabtiot;
          if (!tiotentry->tioelinl&0x02)  /* If not a subsystem dataset */
            openexitData->exitblksize = -1; /* Tell exit to not do SYSOUT block size processing */
        }
      }

      openexitData->exitlist = ((unsigned int)&openexitData->exitlinkage[0]) | 0x85000000;
      reqDcb->dcb.Common.exlst = (unsigned int)(&openexitData->exitlist);
#ifdef DCBDIAG
      printDCBinfo(reqDcb,openexitData,"Setup for OPEN exit completed");
#endif
    }


  /* Exit routines identified in a DCB exit list are entered in
     24-bit mode even if the rest of the program is executing in
     31-bit mode.  That means that a glue routine built in storage
     below 16M must be used to pass control to an exit routine
     which resides above 16M.

     The registers on entry to the DCB OPEN exit are-
      0    - Undefined (same as regs 2-13?).

      1    - The 3 low-order bytes contain the address of the DCB
             currently being processed.

      2-13 - Contents before execution of the macro.

      14   - Return address (must be preserved by the exit routine).

      15   - Address of exit routine entry point.

     The conventions for saving and restoring register contents are
     as follows:

      - The exit routine must preserve the contents of register 14.
        It need not preserve the contents of other registers. The
        control program restores the contents of registers 2 to 13
        before returning control to your program.

      - The exit routine must not use the save area whose address is
        in register 13, unless it saves and restores it's contents
        around any usage.

      - The exit routine must not issue an access method macro.

     What the above implies is that the DCB OPEN exit routine is
     being invoked with the SYNCH service, as an IRB, with registers
     2-13 having been restored to their contents at the time of the
     OPEN SVC.

     We can 'trick' the C compiler, allowing us to write the DCB
     OPEN exit in C, for both the Metal and LE environments,
     following the OPEN macro invocation.  The logic to do this is-

       If DCB OPEN exit is to be used,
         Build DCB exit list and connect to the DCB
         DoExit = 1
       Else
         DoExit = 0
       Using __asm,
         Set the address of the instruction following the OPEN macro
         as the exit list routine address
         Perform the OPEN request
       If Doexit = 1
         Save return address (probably points to CVTEXIT)
           Note: do not perform any function calls
         Do DCB OPEN exit processing
         DoExit = 0
         Cleanup DCB exit list
         Using __asm,
           Return using return address
       ...
  */
    __asm(ASM_PREFIX
          "&LX      SETA    &LX+1           Generate unique branch labels\n"
          "&L_DoOpen SETC   'TDoOpen&LX'     \n"
          "&L_OpenEnd SETC  'TOpenEnd&LX'    \n"
          "&L_Linkage SETC  'TLinkage&LX'    \n"
          "&L_LinkageLen SETC  'TLinkageLen&LX'    \n"
#ifdef _LP64
          " SAM31 , \n"
          " SYSSTATE AMODE64=NO \n"
#endif
          " LTR   %[exitdata],%[exitdata]  Using DCB OPEN exit? \n"
          " BZ    &L_DoOpen \n"
          " LARL  15,&L_Linkage \n"
          " MVC   %[exitlink](&L_LinkageLen,%[exitdata]),0(15)  \n"
          " LARL  0,&L_OpenEnd \n"
          " ST    0,%[exitaddr](,%[exitdata])   Yes, set address \n"
#ifdef _LP64
          " LLGTR 14,14    Clear high half prior to saving \n"
          " LLGTR 15,15  \n"
          " STMH  0,15,%[exithigh](%[exitdata]) \n"
#endif
          " B     &L_DoOpen \n"
          "&L_Linkage DS 0H   \n"
          " BSM   14,0         Save entry AMODE \n"
          " SAM31 ,            Switch to AMODE31 \n"
          " ST    14,%[exitret](,%[exitdata])   Save return address \n"
          " MVI   %[exitact](%[exitdata]),255   Indicate exit is active \n"
#ifdef _LP64
          " LMH   0,15,%[exithigh](%[exitdata]) Restore high halves \n"
#endif
          " L     15,%[exitaddr](,%[exitdata])   Get exit address \n"
          " BR    15           Go to the exit \n"
          "&L_Linkagelen EQU *-&L_Linkage \n"
          "&L_DoOpen DS  0H   \n"
          " OPEN  MODE=24,MF=(E,(%[plist])) \n"
          "&L_OpenEnd DS  0H   \n"
#ifdef _LP64
          " SAM64 , \n"
          " SYSSTATE AMODE64=YES \n"
#endif
          /* openexitDo listed as an output so compiler think it can be
             changed during the __asm code, as it will if the open exit
             processing that follows is actually performed.              */
          :[rc]"=NR:r15"(rc),"=m"(openexitDo)
          :[plist]"NR:r1"(&reqDcb->OpenCloseList),[exitdata]"r"(openexitData),
           [exitact]"i"(offsetof(openexitData_t,exitactive)),
           [exitlink]"i"(offsetof(openexitData_t,exitlinkage)),
           [exitaddr]"i"(offsetof(openexitData_t,exitaddress)),
           [exitret]"i"(offsetof(openexitData_t,exitreturn)),
           [exithigh]"i"(offsetof(openexitData_t,exithighhalves))
          :"r1","r0","r15");
    /* Note: Do not place any code between the prior __asm, that
             issues the OPEN, and the following test of exitactive.  */
    if (openexitData && openexitData->exitactive){ /* If open exit processing to be done */

      /* Make local copies of the current DCB dataset attributes */

      char recfm = reqDcb->dcb.Common.recfm;
      int blksize = reqDcb->dcb.BBQCommon.blksize;
      int lrecl = reqDcb->dcb.BBQInterface.lrecl;
      int fail = 0;

#ifdef DCBDIAG
      printDCBinfo(reqDcb,openexitData,"OPEN exit beginning");
#endif
      /* If a record format has not been set, set a value of V for
         SYSOUT and VB for all others.

      */
      if (recfm == 0){
        /* If RECFM=V/F/U is not specified */
        if (!(openexitData->exitrecfm&DCB_RECFM_FORMAT)) {
          if (openexitData->exitblksize == -1)
            recfm = DCB_RECFM_VB;
          else
            recfm = DCB_RECFM_V;
          recfm |= openexitData->exitrecfm &
                   (DCB_RECFM_A|DCB_RECFM_M);  /* Add any requested CC */
        } else {
          recfm = openexitData->exitrecfm;
          if (openexitData->exitblksize == -2) {
            recfm &= ~DCB_RECFM_B;
          }
        }
      } else if ((recfm&DCB_RECFM_FORMAT) == DCB_RECFM_D)
        fail = 1;               /* RECFM=D is not supported */

      /* Set the minimum acceptable logical record length */

      int minLrecl = 1;
      if (openexitData->exitlrecl)
        minLrecl = openexitData->exitlrecl;

      if ((recfm & DCB_RECFM_FORMAT) == DCB_RECFM_V)
        minLrecl+=4;
      if (recfm & (DCB_RECFM_A|DCB_RECFM_M) & !(openexitData->exitrecfm & (DCB_RECFM_A|DCB_RECFM_M)))
        minLrecl++;                    /* If DCB recfm indicates a CC and default recfm does not */

      /* If a block size has been set but not a logical record
         length, set a value based on the record format and block
         size.
      */

      if (blksize > 0 && lrecl == 0){

      /* - If the record format is not blocked, set the logical
           record length to the block size for fixed and undefined
           formats, and the block size minus 4 for variable format,
      */
        if ((recfm & DCB_RECFM_B) == 0){
          if ((recfm & DCB_RECFM_FORMAT) != DCB_RECFM_V)
            lrecl = blksize;
          else
            lrecl = blksize-sizeof(RDW);
        }

      /* - If the record format is fixed blocked, set the record
           length to the largest value which is evenly divisible
           into the block size, capping the length to a maximum of
           255 and a minimum of 132.  If none exist, leave the
           record length set to 0 (causing the OPEN to fail).
      */

        if ((recfm & (DCB_RECFM_FORMAT | DCB_RECFM_B)) == DCB_RECFM_FB){
          for (lrecl=255; lrecl>=minLrecl; lrecl--)
            if ((blksize%lrecl) == 0)    /* If evenly divisible */
              break;
        }

      /* - If the record format is variable blocked, set the logical
           record length to 255 or the block size minus 4, whichever
           is smallest.
           Note: The "spanned" attribute is ignored in this logic.
      */
        if ((recfm & (DCB_RECFM_FORMAT | DCB_RECFM_B)) == DCB_RECFM_VB){
          if ((blksize-sizeof(RDW)) > 164)
            lrecl = 164;
          else
            lrecl = blksize-sizeof(RDW);
        }

      }

      /* If a logical record length has not been set, set a value
         based on the record format (which is the minimum logical
         record length which was set earlier).
      */
      if (lrecl == 0)
        lrecl = minLrecl;

      /* If a block size has not been set, set a value based on the
         output destination and the record format.
      */
      if (blksize == 0) {
      /* - If being written to sysout with a record format of fixed
           or undefined, set the block size to the logical record
           length.
      */
      /* - If being written to sysout with a record format of
           variable, set the block size to the logical record length
           plus 4.
      */
        if (openexitData->exitblksize == -1){
          if ((recfm & DCB_RECFM_FORMAT) != DCB_RECFM_V)
            blksize = lrecl;
          else
            blksize = lrecl+sizeof(RDW);
      }

      /* - If being written to a physical dataset, leave the block
           size set to 0 so that a system determined block size will
           be set by OPEN.
      */
      else
        ;
      }

      /* If the usable logical record length < 132, fail the OPEN.  */

      if (lrecl < minLrecl)
        fail = 1;                            /* Force the OPEN to fail */

      /* Indicate that exit processing has been performed, and return. */

      if (!fail){
        reqDcb->dcb.Common.recfm = recfm;
        reqDcb->dcb.BBQCommon.blksize = blksize;
        reqDcb->dcb.BBQInterface.lrecl = lrecl;
      } else {
        reqDcb->dcb.Common.recfm = 0;
        reqDcb->dcb.BBQCommon.blksize = -1;
        reqDcb->dcb.BBQInterface.lrecl = 0;
      }

#ifdef DCBDIAG
      printDCBinfo(reqDcb,NULL,"OPEN exit completed");
#endif
      openexitData->exitactive = 0;
      __asm(ASM_PREFIX
            " BSM  0,14      Return to OPEN processing \n"
            :
            :"NR:r14"(openexitData->exitreturn));
    }

    if (openexitData){
      reqDcb->dcb.Common.exlst = (unsigned int)NULL;
      free24((char *)openexitData,sizeof(openexitData));
    }
  } else{
    __asm(ASM_PREFIX
#ifdef _LP64
          " SAM31 \n"
        " SYSSTATE AMODE64=NO \n"
#endif
          " CLOSE MODE=24,MF=(E,(%[plist])) \n"
          " LR %[rc],15  Save the return code \n"
          " CNOP 0,4 \n"
          " FREEPOOL (%[dcb]) \n"
#ifdef _LP64
          " SAM64 \n"
        " SYSSTATE AMODE64=YES \n"
#else
#ifndef METTLE
        " NOPR  0     Avoid LE assembly errors due to the way the C compiler works \n"
#endif
#endif
          :[rc]"=r"(rc)
          :[plist]"NR:r1"(&reqDcb->OpenCloseList),[dcb]"r"(&reqDcb->dcb)
          :"r0","r1","r14","r15");
  }
  return rc;
}

char *openSAM(char *ddname, int mode, int isQSAM,
	      int recfm, int lrecl, int blksize){
  char *dcb = NULL;
  int status;
  int macrf = 0;

  /* MACRF for writing SYSPRINT,SYSOUT, etc ...                         
      BL2'0000000001010000' MACR (MACRO FORMAT)
      */
  if (mode == OPEN_CLOSE_OUTPUT){
    macrf = isQSAM ? (DCB_MACRF_PUT|DCB_MACRF_MV_PUT)
      : DCB_MACRF_WRITE;
  } else{
    macrf = isQSAM ? (DCB_MACRF_GET | DCB_MACRF_MV_GET)
      : DCB_MACRF_READ;
  }

  dcb = makeDCB(ddname,100,isQSAM ? DCB_ORG_PS : DCB_ORG_PO,0x02,
                macrf,
                0x00, /* option codes */
                recfm,lrecl,blksize);
  status = openCloseBBQ(dcb,mode,19);
  /* should do something with the status */
  return dcb;
}

int getSAMLength(char *dcb){
  if ((((dcbSAMwithPlist *)dcb)->dcb.Common.recfm & DCB_RECFM_FORMAT) == DCB_RECFM_V)
    return ((dcbSAMwithPlist *)dcb)->dcb.BBQInterface.lrecl-sizeof(RDW);
  else
    return ((dcbSAMwithPlist *)dcb)->dcb.BBQInterface.lrecl;
}

unsigned short getSAMBlksize(char *dcb){
  return ((dcbSAMwithPlist *)dcb)->dcb.BBQCommon.blksize;
}

int getSAMLrecl(char *dcb){
  return ((dcbSAMwithPlist *)dcb)->dcb.BBQInterface.lrecl;
}

int getSAMCC(char *dcb){
  if (((dcbSAMwithPlist *)dcb)->dcb.Common.recfm & DCB_RECFM_A)
    return DCB_RECFM_A;
  else if (((dcbSAMwithPlist *)dcb)->dcb.Common.recfm & DCB_RECFM_M)
    return DCB_RECFM_M;
  else
    return 0;
}

int getSAMRecfm(char *dcb){
  return ((dcbSAMwithPlist *)dcb)->dcb.Common.recfm;
}

char *openEXCP(char *ddname){
  char *dcb = NULL;
  int status;
  int macrf = DCB_MACRF_EXCP;
  int mode = OPEN_CLOSE_INPUT;

  dcb = makeDCB(ddname,
                100,
                DCB_ORG_PS,
                0x02, /* OFLGS , because I copied the listing of the macro expansion, sad but true */
                macrf,
                0x02, /* option code */
                0,
                0,
                0);
  status = openCloseBBQ(dcb,mode,19);
  /* should do something with the status */
  return dcb;
}

void closeEXCP(char *dcb){
  int mode = OPEN_CLOSE_INPUT;
  closeSAM(dcb,mode);
}

void closeSAM(char *dcb, int mode){
  openCloseBBQ(dcb,mode,20);
  free24(dcb-4,sizeof(dcbSAMwithPlist)+4);
}

int putline(char *dcb, char *line){
  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      char savearea[72];
    )
  );

  int status = 0;

  __asm(ASM_PREFIX
#ifdef __XPLINK__
        " LA 13,%[savearea] \n"
#endif
#ifdef _LP64
        " SAM31 \n"
        " SYSSTATE AMODE64=NO \n"
#endif
        " PUT (%[dcb]),(%[data])\n"
#ifdef _LP64
        " SAM64 \n"
        " SYSSTATE AMODE64=YES \n"
#endif
        :[rc]"=NR:r15"(status)
        :[dcb]"NR:r1"(&((dcbSAMwithPlist *)dcb)->dcb),[data]"NR:r0"(line),[savearea]"m"(below2G->savearea)
        :"r0","r1","r14","r15"
#ifdef __XPLINK__
        ,"r13"
#endif
        );
  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
  return status;
}

/* Rather than build the routine as static data or to allocate storage
   and copy static data into it, imbed the EODAD routine in the
   executable code for getEODADBuffer.  This will be a 31-bit address,
   so the DCBE must be used to specify the EODAD routine.             */
char *getEODADBuffer() {
  char *eodad;
  __asm(ASM_PREFIX
        "&LX      SETA    &LX+1           Generate unique branch labels\n"
        "&L1      SETC    'TEXIT&LX'      \n"
        " BRAS %[eodad],&L1    Set EODAD address \n"
        "* EODAD routine \n"
        "         LHI     10,1     Set RC=1 \n"
        "         BR      14       Return \n"
        "&L1      DS      0H \n"
        : [eodad]"=r"(eodad));

  return eodad;
}

void freeEODADBuffer(char *EODAD) {
  /* No longer needed */
}

int getline(char *dcb, char *line, int *lengthRead) {
  return getline2((char *)0, dcb, line, lengthRead);
}

/* It is not explicitly documented, but R15 on return after a
   successful GET/CHECK request will contain 0.  The EODAD routine
   will now set R15 to 1 and return using R14 to the instruction
   following the BALR generated by the GET macro.  R15 can then be
   used to determine if end-of-data occurred or not.

   Update: unfortunately, it's been observed that R15 has the GET/CHECK
   return address when a dataset has more than one line/block. R15 has been
   changed to R10, which is not reserved or used by GET/CHECK.

   Note: Also not explicitly documented, R1 on entry to the EODAD
   routine will contain the address of the DCB specified for the
   GET/CHECK request that caused entry into the routine and R15 will
   contain the address of the EODAD routine.                         */

// This routine knows how to handle RECFM F, V, and U format processing.
//   RECFM F - Return the DCB lrecl for the record length
//   RECFM V - Return the RDW record length, minus the length of the RDW,
//             for the record length
//   RECFM U - Return the DCB lrecl for the record length
int getline2(char *EODAD, char *dcb, char *line, int *lengthRead){
  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      char savearea[72];
    )
  );

  int status = 0;

  __asm(ASM_PREFIX
#ifdef __XPLINK__
        " LA 13,%[savearea] \n"
#endif
#ifdef _LP64
        " SAM31 \n"
	" SYSSTATE AMODE64=NO \n"
#endif
        " GET (%[dcb]),(%[data])\n"
#ifdef _LP64
        " SAM64 \n"
	       " SYSSTATE AMODE64=YES \n"
#endif
        :[rc]"+NR:r10"(status) /* must be same GPR as the one in the EODAD exit */
        :[dcb]"NR:r1"(&((dcbSAMwithPlist *)dcb)->dcb),[data]"NR:r0"(line),[savearea]"m"(below2G->savearea)
        :"r0", "r1", "r10", "r14", "r15"
#ifdef __XPLINK__
        ,"r13"
#endif
        );
  if ((((dcbSAMwithPlist *)dcb)->dcb.Common.recfm & DCB_RECFM_FORMAT) != DCB_RECFM_V)
    *lengthRead = ((dcbSAMwithPlist *)dcb)->dcb.BBQInterface.lrecl;
  else
    *lengthRead = ((RDW *)line)->length-sizeof(RDW);
  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
  return status;
}

int hasVaryingRecordLength(char *dcb){
  return ((((dcbSAMwithPlist *)dcb)->dcb.Common.recfm & DCB_RECFM_FORMAT) == DCB_RECFM_V);
}

char *makeQSAMBuffer(char *dcb, int *bufferSize){
  int size = ((dcbSAMwithPlist *)dcb)->dcb.BBQInterface.lrecl;

  char *buffer = malloc31(size+4); /* +4 for null termination convenience */

  if ((((dcbSAMwithPlist *)dcb)->dcb.Common.recfm & DCB_RECFM_FORMAT) == DCB_RECFM_V){
    *bufferSize = size-sizeof(RDW);  /* Exclude RDW from returned size */
    buffer += sizeof(RDW);           /* Exclude RDW from returned address */
  } else
    *bufferSize = size;

  return buffer;
}

void freeQSAMBuffer(char *dcb, char *buffer){
  int size = ((dcbSAMwithPlist *)dcb)->dcb.BBQInterface.lrecl;

  char *truebuffer = buffer;

  if ((((dcbSAMwithPlist *)dcb)->dcb.Common.recfm & DCB_RECFM_FORMAT) == DCB_RECFM_V)
    truebuffer -= sizeof(RDW);

  free31(truebuffer,size+4);

  return;
}

// This routine knows how to handle RECFM F, V, and U format processing.
//   RECFM F - No special processing
//   RECFM V - Adjust the data address to include the preallocated RDW,
//             and set the length into it
//   RECFM U - Set the length into the DCB block size field
int putlineV(char *dcb, char *line, int len){
  char *data = line;
  if ((((dcbSAMwithPlist *)dcb)->dcb.Common.recfm & DCB_RECFM_FORMAT) == DCB_RECFM_F)
    ;                       /* Do nothing for RECFM=F */
  else if ((((dcbSAMwithPlist *)dcb)->dcb.Common.recfm & DCB_RECFM_FORMAT) == DCB_RECFM_V){
    data = (char *)((int)data-sizeof(RDW));
    ((dcbSAMwithPlist *)dcb)->dcb.BBQInterface.lrecl = len+sizeof(RDW);
    ((RDW *)data)->length = len+sizeof(RDW);
    ((RDW *)data)->rsvd = 0;
  } else
    ((dcbSAMwithPlist *)dcb)->dcb.BBQInterface.lrecl = len;

  return putline(dcb,data);
}

// This routine knows how to handle RECFM F, V, and U format processing.
//   RECFM F - No special processing
//   RECFM V - Adjust the data address to include the preallocated RDW
//   RECFM U - No special processing
int getlineV(char *dcb, char *line, int *lengthRead){
  char *data = line;
  if ((((dcbSAMwithPlist *)dcb)->dcb.Common.recfm & DCB_RECFM_FORMAT) == DCB_RECFM_V)
    data = (char *)((int)data-sizeof(RDW));

  return getline(dcb,data,lengthRead);
}

/***** BPAM STUFF ****/

int bpamFind(char * __ptr32 dcb, char * __ptr32 memberName, int *reasonCode){

  int rc;
  int rsn;

  __asm(ASM_PREFIX
#ifdef _LP64
      " SAM31                 \n"
      " SYSSTATE AMODE64=NO   \n"
#endif
      " FIND  (%[dcb]),(%[name]),D     \n"
#ifdef _LP64
      " SAM64                 \n"
      " SYSSTATE AMODE64=YES  \n"
#else
#ifndef METTLE
        " NOPR  0     Avoid LE assembly errors due to the way the C compiler works \n"
#endif
#endif
      :[rc]"=NR:r15"(rc), [rsn]"=NR:r0"(rsn)
      :[dcb]"NR:r1"(&((dcbSAMwithPlist *)dcb)->dcb), [name]"NR:r0"(memberName)
      :"r0", "r1", "r14", "r15");

  *reasonCode = rsn;
  return rc;
}

int bpamRead(void * __ptr32 dcb, void * __ptr32 buffer){
  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      char savearea[72];
    )
  );

  char *decbData = malloc24(sizeof(DECB));
  memset(decbData,0,sizeof(DECB));
  DECB *decb = (DECB*)decbData;
  decb->type2 = 0x80;

  int rc = 0;
  __asm(ASM_PREFIX
#ifdef __XPLINK__
      " LA 13,%[savearea] \n"
#endif
#ifdef _LP64
      " SAM31                             \n"
      " SYSSTATE AMODE64=NO               \n"
#endif
      " READ  (%[decb]),SF,(%[dcb]),(%[data]),'S',MF=E  \n"
      " CHECK (%[decb])                   \n"
#ifdef _LP64
      " SAM64                             \n"
      " SYSSTATE AMODE64=YES              \n"
#endif
      :[rc]"+NR:r10"(rc) /* must be same GPR as the one in the EODAD exit */
      :[decb]"NR:r2"(decb), [dcb]"NR:r6"(&((dcbSAMwithPlist *)dcb)->dcb), [data]"NR:r9"(buffer),[savearea]"m"(below2G->savearea)
      :"r0", "r1", "r2", "r6", "r9", "r10", "r14", "r15"
#ifdef __XPLINK__
       ,"r13"
#endif
      );

  free24(decbData, sizeof(DECB));
  decb = NULL;

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
  return rc;
}

int bpamRead2(void * __ptr32 dcb, void * __ptr32 buffer, int *lengthRead){
  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      char savearea[72];
    )
  );

  char *decbData = malloc24(sizeof(DECB));
  memset(decbData,0,sizeof(DECB));
  DECB *decb = (DECB*)decbData;
  decb->type2 = 0x80;

  int rc = 0;
  __asm(ASM_PREFIX
#ifdef __XPLINK__
      " LA 13,%4 \n"
#endif
#ifdef _LP64
      " SAM31                             \n"
      " SYSSTATE AMODE64=NO               \n"
#endif
      " READ  (%[decb]),SF,(%[dcb]),(%[data]),'S',MF=E  \n"
      " CHECK (%[decb])                   \n"
#ifdef _LP64
      " SAM64                             \n"
      " SYSSTATE AMODE64=YES              \n"
#endif
      :[rc]"+NR:r10"(rc) /* must be same GPR as the one in the EODAD exit */
      :[decb]"NR:r2"(decb), [dcb]"NR:r6"(&((dcbSAMwithPlist *)dcb)->dcb), [data]"NR:r9"(buffer),[savearea]"m"(below2G->savearea)
      :"r0", "r1", "r2", "r6", "r9", "r10", "r14", "r15"
#ifdef __XPLINK__
       ,"r13"
#endif
      );

  int maxReadSize = ((dcbSAMwithPlist *)dcb)->dcb.BBQCommon.blksize;
  *lengthRead = maxReadSize - decb->iob->residual;

  free24(decbData, sizeof(DECB));
  decb = NULL;

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
  return rc;
}



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

