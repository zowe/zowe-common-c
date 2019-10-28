

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __DYNALLOC__
#define __DYNALLOC__  1

#ifdef METTLE
#include <metal/stdint.h>
#else
#include <stdint.h>
#endif

#include "zowetypes.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS targets are supported only
#endif

#define turn_on_HOB(x) x = (TextUnit* __ptr32) ((int) x | 0x80000000)
#define turn_off_HOB(x) x = (TextUnit* __ptr32) ((int) x & 0x7FFFFFFF)

#define S99VRBAL 0x01     /* Allocation                      */
#define S99VRBUN 0x02     /* Unallocation                    */
#define S99VRBCC 0x03     /* Concatenation                   */
#define S99VRBDC 0x04     /* Deconcatenation                 */
#define S99VRBRI 0x05     /* Remove in-use                   */
#define S99VRBDN 0x06     /* DDNAME Allocation               */
#define S99VRBIN 0x07     /* Information retrieval           */

#define DALDDNAM  0x0001  /*  DDNAME                         */
#define DALDSNAM  0x0002  /*  DSNAME                         */
#define DALMEMBR  0x0003  /*  MEMBER NAME                    */
#define DALSTATS  0x0004  /*  DATA SET STATUS                */
#define DALNDISP  0x0005  /*  DATA SET NORMAL DISPOSITION    */
#define DALCDISP  0x0006  /*  DATA SET CONDITIONAL DISP      */
#define DALTRK    0x0007  /*  TRACK SPACE TYPE               */
#define DALCYL    0x0008  /*  CYLINDER SPACE TYPE            */
#define DALBLKLN  0x0009  /*  AVERAGE DATA BLOCK LENGTH      */
#define DALPRIME  0x000A  /*  PRIMARY SPACE QUANTITY         */
#define DALSECND  0x000B  /*  SECONDARY SPACE QUANTITY       */
#define DALDIR    0x000C  /*  DIRECTORY SPACE QUANTITY       */
#define DALRLSE   0x000D  /*  UNUSED SPACE RELEASE           */
#define DALSPFRM  0x000E  /*  CONTIG,MXIG,ALX SPACE FORMATB   */
#define DALROUND  0x000F  /*  WHOLE CYLINDER (ROUND) SPACE   */
#define DALVLSER  0x0010  /*  VOLUME SERIAL                  */
#define DALPRIVT  0x0011  /*  PRIVATE VOLUME                 */
#define DALVLSEQ  0x0012  /*  VOL SEQUENCE NUMBER            */
#define DALVLCNT  0x0013  /*  VOLUME COUNT                   */
#define DALVLRDS  0x0014  /*  VOLUME REFERENCE TO DSNAME     */
#define DALUNIT   0x0015  /*  UNIT DESCRIPTION               */
#define DALUNCNT  0x0016  /*  UNIT COUNT                     */
#define DALPARAL  0x0017  /*  PARALLEL MOUNT                 */
#define DALSYSOU  0x0018  /*  SYSOUT                         */
#define DALSPGNM  0x0019  /*  SYSOUT PROGRAM NAME            */
#define DALSFMNO  0x001A  /*  SYSOUT FORM NUMBER             */
#define DALOUTLM  0x001B  /*  OUTPUT LIMIT                   */
#define DALCLOSE  0x001C  /*  UNALLOCATE AT CLOSE            */
#define DALCOPYS  0x001D  /*  SYSOUT COPIES                  */
#define DALLABEL  0x001E  /*  LABEL TYPE                     */
#define DALDSSEQ  0x001F  /*  DATA SET SEQUENCE NUMBER       */
#define DALPASPR  0x0020  /*  PASSWORD PROTECTION            */
#define DALINOUT  0x0021  /*  INPUT ONLY OR OUTPUT ONLY      */
#define DALEXPDT  0x0022  /*  2 DIGIT YEAR EXPIRATION DATE   */
#define DALRETPD  0x0023  /*  RETENTION PERIOD               */
#define DALDUMMY  0x0024  /*  DUMMY ALLOCATION               */
#define DALFCBIM  0x0025  /*  FCB IMAGE-ID                   */
#define DALFCBAV  0x0026  /*  FCB FORM ALIGNMENT,IMAGE VERIFY*/
#define DALQNAME  0x0027  /*  QNAME ALLOCATION               */
#define DALTERM   0x0028  /*  TERMINAL ALLOCATION            */
#define DALUCS    0x0029  /*  UNIVERSAL CHARACTER SET        */
#define DALUFOLD  0x002A  /*  UCS FOLD MODE                  */
#define DALUVRFY  0x002B  /*  UCS VERIFY CHARACTER SET       */
#define DALDCBDS  0x002C  /*  DCB DSNAME REFERENCE           */
#define DALDCBDD  0x002D  /*  DCB DDNAME REFERENCE           */
#define DALBFALN  0x002E  /*  BUFFER ALIGNMENT               */
#define DALBFTEK  0x002F  /*  BUFFERING TECHNIQUE            */

#define DALOPTCD  0x0045  /*  http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/IEA2A860/26.4.6?SHELF=&DT=20050714101051&CASE=                      
                              if set to J put 0x01 in text unit

                           */
#define DALRECFM  0x0049  /*  RECFM */

#define DALRECFM_M      0x02
#define DALRECFM_A      0x04
#define DALRECFM_S      0x08
#define DALRECFM_B      0x10
#define DALRECFM_D      0x20
#define DALRECFM_V      0x40
#define DALRECFM_F      0x80
#define DALRECFM_U      0xc0
#define DALRECFM_FB     0x90
#define DALRECFM_VB     0x50
#define DALRECFM_FBS    0x98
#define DALRECFM_VBS    0x58

#define DALPASSW  0x0050  /*  PASSWORD FOR DATA SET          */
#define DALPERMA  0x0052  /*  PERMANENTLY ALLOCATED ATTR     */
#define DALCNVRT  0x0053  /*  CONVERTIBLE ATTR               */
#define DALRTDDN  0x0055  /*  REQUEST RTN OF ASSOC DDNAME    */
#define DALRTDSN  0x0056  /*  REQUEST RTN OF ALLOC'D DSN     */
#define DALRTORG  0x0057  /*  REQUEST RTN OF DS ORG          */
#define DALSUSER  0x0058  /*  DEST TO ROUTE SYSOUT (JCL DEST)*/           
#define DALSHOLD  0x0059  /*  ROUTE SYSOUT TO HOLD QUEUE     */           
#define DALSSREQ  0x005C  /*  REQ ALLOC OF SUBSYS DATA SET   */           
#define DALRTVAL  0x005D  /*  REQ RTN OF VOLSER              */           
#define DALSSNM   0x005F  /*  REQ ALLOC OF SUBSYS DATA SET   */           
#define DALSSPRM  0x0060  /*  SPECS SUBSYS-DEF"D PARMS W/DALSSNM */       
#define DALPROT   0x0061  /*  REQS DA DS BE RACF PROTECTED   */           
#define DALSSATT  0x0062  /*  ALLOC SUBSYS DATA SET TO SYSIN */           
#define DALUSRID  0x0063  /*  DEST TO ROUTE SYSOUT (WITH DALSUSER) */     
#define DALBURST  0x0064  /*  STACKER OF 3800 FOR PAPER OUTPUT */         
#define DALCHARS  0x0065  /*  NAME(S) OF CHAR TABLES FOR 3800 */          
#define DALCOPYG  0x0066  /*  COPY GROUPING FOR 3800         */           
#define DALFFORM  0x0067  /*  FORM OVERLAY FOR 3800          */           
#define DALFCNT   0x0068  /*  NUMBER OF COPIES ON WHICH FORM OVLY USED */ 
#define DALMMOD   0x0069  /*  COPY MOD MODULE FOR 3800       */           
#define DALMTRC   0x006A  /*  TABLE REF CHAR FOR CHAR TABLE  */           
#define DALDEFER  0x006C  /*  DEFER MOUNTING VOL UNTIL OPEN  */           
#define DALEXPDL  0x006D  /*  EXP. DATE WITH 4-DIGIT YEAR    */           
#define DALBRTKN  0x006E  /*  SPOOL DS BROWSE TOKEN          */           
#define DALINCHG  0x006F  /*  TAPE RECDG TECHNIQUE AND MEDIA TYPE */      
#define DALOVAFF  0x0070  /*  OVERRIDE SYSAFF FOR INTERNAL READER */      
#define DALRTCTK  0x0071  /*  REQ RTN OF JES CTOKEN          */           
#define DALACODE  0x8001  /*  ACCESS CODE FOR TAPE DATA SET  */           
#define DALOUTPT  0x8002  /*  REF TO JCL OUTPUT STMT OR DYN OUTPUT DESC */
#define DALCNTL   0x8003  /*  REF TO JCL CNTL STMT           */           
#define DALSTCL   0x8004  /* STORCLAS FOR NEW SMS DATA SET   */           
#define DALMGCL   0x8005  /* MGMTCLAS FOR NEW SMS DATA SET   */           
#define DALDACL   0x8006  /* DATACLAS FOR NEW SMS DATA SET   */           
#define DALRECO   0x800B  /* RCD ORG OF VSAM DATA SET        */           
#define DALKEYO   0x800C  /* KEY OFFSET OF VSAM DATA SET     */           
#define DALREFD   0x800D  /* NAME OF JCL DD STATMENT TO COPY ATTRS FROM */
#define DALSECM   0x800E  /* NAME OF RACF PROGILE TO COPY FROM */         
#define DALLIKE   0x800F  /* MODEL OF SMS DS TO COPY ATTRS FROM */        
/* MORE DALXXXX PARMS EXIST BUT ARE NOT YET CODED !! */

/* DALSPIN specifies whether the output for the SYSOUT data set is to be printed immediately upon unallocation of the data set, or at the end of the job.
When you code DALSPIN, # and LEN must be 1; PARM must contain one of the following:
X'80' Data set available for printing when it is unallocated
X'40' Data set available for printing at the end of the job.
*/
#define DALSPIN   0x8013

#define DALPATH   0x8017  /* uss pathname char * < 256 bytes */

/* NOTE:  All of the parameter values like "__PATH_OCREATE are specified in dynit.h */

#define DALPOPT   0x8018  /* int: bunch of options, how to code
			     __PATH_OCREAT, __PATH_OAPPEND, 
			     __PATH_OEXCL, __PATH_ONOCTTY, 
			     __PATH_OTRUNC, __PATH_ONONBLOCK, 
			     __PATH_ORDONLY, __PATH_OWRONLY, __PATH_ORDWR */

#define DALPOPT_OCREAT      0x00000080
#define DALPOPT_OEXCL       0x00000040
#define DALPOPT_ONOCTTY     0x00000020
#define DALPOPT_OTRUNC      0x00000010
#define DALPOPT_OAPPEND     0x00000008
#define DALPOPT_ONONBLOCK   0x00000004
#define DALPOPT_ORDWR       0x00000003
#define DALPOPT_ORDONLY     0x00000002
#define DALPOPT_OWRONLY     0x00000001

#define DALPMDE   0x8019  /* int: Specifies the file access attributes for the HFS file. 
			     Values are: __PATH_SIRUSR, 
			     __PATH_SIWUSR, __PATH_SIXUSR, 
			     __PATH_SIRWXU, __PATH_SIRGRP, 
			     __PATH_SIWGRP, __PATH_SIXGRP, 
			     __PATH_SIRWXG, __PATH_SIROTH, 
			     __PATH_SIWOTH, __PATH_SIXOTH, 
			     __PATH_SIRWXO, __PATH_SISUID, __PATH_SISGID. */

#define DALPMDE_SISUID       0x00000800
#define DALPMDE_SISGID       0x00000400
#define DALPMDE_SIRUSR       0x00000100
#define DALPMDE_SIWUSR       0x00000080
#define DALPMDE_SIXUSR       0x00000040
#define DALPMDE_SIRWXU       0x000001C0
#define DALPMDE_SIRGRP       0x00000020
#define DALPMDE_SIWGRP       0x00000010
#define DALPMDE_SIXGRP       0x00000008
#define DALPMDE_SIRWXG       0x00000038
#define DALPMDE_SIROTH       0x00000004
#define DALPMDE_SIWOTH       0x00000002
#define DALPMDE_SIXOTH       0x00000001
#define DALPMDE_SIRWXO       0x00000007

#define DALPNDS   0x801A  /* char Specifies the normal HFS file disposition desired. 
			     It is either __DISP_KEEP or __DISP_DELETE
			     */
#define DALPCDS   0x801B  /* char Specifies the abnormal HFS file disposition desired. 
			     It is either __DISP_KEEP or __DISP_DELETE
			     */
#define DALFDAT   0x801D  /* one char, 0x80, binary, 0x40 text, 0x20 counted records with lenght in prefix:
			     details at 
                             http://publib.boulder.ibm.com/infocenter/zos/v1r12/index.jsp?topic=%2Fcom.ibm.zos.r12.ieaa800%2Fsyssym.htm */
/* MORE DALXXXX PARMS EXIST BUT ARE NOT YET CODED !! */                   

#define DALVSER  0x0010
#define DALVSEQ  0x0012
#define DALDSORG 0x003C
#define DALDSORG_PS 0x4000
#define DALBLKSZ 0x0030
#define DALLRECL 0x0042 
#define DALBUFNO 0x0034
#define DALNCP   0x0044
#define DALAVGR  0x8010
#define DALDIAGN 0x0054
#define DALBRTKN 0x006E
/* dynamic unallocation */
#define DUNDDNAM 0x0001
#define DUNOVDSP 0x0005
#define DUNOVCLS 0x0018
#define DUNOVSUS 0x0058
#define DUNOVUID 0x0063

typedef struct _text_unit {
  short key;
  short number_of_parameters;
  short first_parameter_length;
  char first_value[0];
} TextUnit;

#pragma map(createSimpleTextUnit, "DYNASTXU")
#pragma map(createSimpleTextUnit2, "DYNASTX2")
#pragma map(createCharTextUnit, "DYNACTXU")
#pragma map(createCompoundTextUnit, "DYNACMTU")
#pragma map(createIntTextUnit, "DYNAITXU")
#pragma map(createInt8TextUnit, "DYNAI1TU")
#pragma map(createInt16TextUnit, "DYNAI2TU")
#pragma map(freeTextUnit, "DYNAFTXU")
#pragma map(AllocIntReader, "DYNAAIRD")
#pragma map(dynallocSharedLibrary, "DYNASLIB")
#pragma map(dynallocUSSDirectory, "DYNAUSSD")
#pragma map(dynallocUSSOutput, "DYNAUSSO")
#pragma map(AllocForSAPI, "DYNASAPI")
#pragma map(AllocForDynamicOutput, "DYNADOUT")
#pragma map(DeallocDDName, "DYNADDDN")

TextUnit *createSimpleTextUnit(int key, char *value);
TextUnit *createSimpleTextUnit2(int key, char *value, int firstParameterLength);
TextUnit *createCharTextUnit(int key, char value);
TextUnit *createCompoundTextUnit(int key, char **values, int valueCount);
TextUnit *createIntTextUnit(int key, int value);
TextUnit *createInt8TextUnit(int key, int8_t value);
TextUnit *createInt16TextUnit(int key, int16_t value);
void freeTextUnit(TextUnit * text_unit);

/* open a stream to the internal reader */

int AllocIntReader(char *datasetOutputClass, char *ddnameResult,
                   char *errorBuffer);

int dynallocSharedLibrary(char *ddname, char *dsn, char *errorBuffer);

int dynallocUSSDirectory(char *ddname, char *ussPath, char *errorBuffer);

int dynallocUSSOutput(char *ddname, char *ussPath, char *errorBuffer);

int AllocForSAPI(char *dsn,
                 char *ddnameResult,
                 char *ssname,
                 void *browseToken,
                 char *errorBuffer);

int AllocForDynamicOutput(char *outDescName,
                          char *ddnameResult,
                          char *ssname,
                          char *error_buffer);

int DeallocDDName(char *ddname);

#define DOADDRES 0x0027
/* ADDRESS FieldCount: 4 FieldLength 0-60
   EBCDIC text characters
   Specifies the delivery address for the SYSOUT data set. */

#define DOAFPPRM 0x0051
/* AFPPARMS FieldCount: 1 FieldLength 1-54
   Cataloged data set name
   Identifies a data set containing control parameters for the AFP Print Distributor feature of PSF. */
#define DOAFPST  0x0048
/* Maximum number of value fields: 1
   Length of value field: 1
   Value field:  X'40' for YES, X'80' for NO  
   Specifies to Print Services Facility (PSF) that an AFP Statistics report is to be generated while printing this SYSOUT data set. */

#define DOBUILD 0x0028
/* BUILDING FieldCount: 1 FieldLength 1-60
   EBCDIC text characters
   Specifies the building location associated with the SYSOUT data set. */

#define DOBURST 0x0001
/* BURST FieldCount: 1 FieldLength 1
   X02 for YES X04 for NO
   Directs output to a stacker on a 3800 Printing Subsystem. */

#define DOCHARS 0x0002
/* CHARS FieldCount: 4 FieldLength 1-4 (see note 1)
   Alphanumeric or national (@, $, #) characters
   Names character-arrangement tables for printing on a 3800 Printing Subsystem. */

#define DOCKPTLI 0x0003
/* CKPTLINE FieldCount: 1 FieldLength 2
   binary number from 0 to 32767 decimal
   Specifies the maximum lines in a logical page. */

#define DOCKPTPA 0x0004
/* CKPTPAGE FieldCount: 1 FieldLength 2
   binary number from 1 to 32767 decimal
   Specifies the number of logical pages to be printed or transmitted before JES takes a checkpoint. */

#define DOCKPTSE 0x0005
/* CKPTSEC FieldCount: 1 FieldLength 2
   binary number from 1 to 32767 decimal
   Specifies how many seconds of printing are to elapse between each checkpoint of this SYSOUT data set. */

#define DOCLASS 0x0006
/* CLASS FieldCount: 1 FieldLength 1
   alphanumeric character or *
   Assigns the system data set to an output class. */

#define DOCOLORM 0x003A
/* COLORMAP FieldCount: 1 FieldLength 1-8
   alphanumeric or national (@,$,#) characters
   Specifies the AFP resource (object) for the data set that contains color translation information. */

#define DOCOMPAC 0x0007
/* COMPACT FieldCount: 1 FieldLength 1-8
   alphanumeric characters
   Specifies a compaction table for sending this SYSOUT data set to a SNA remote terminal. */

#define DOCOMSET 0x0032
/* COMSETUP FieldCount: 1 FieldLength 1-8
   alphanumeric characters, $, #, @
   Specifies the name of a microfile setup resource. */

#define DOCONTRO 0x0008
/* CONTROL FieldCount: 1 FieldLength 1
   X80 for SINGLE X40 for DOUBLE X20 for TRIPLE X10 for PROGRAM
   Specifies that all the data records begin with carriage control characters or specifies line spacing. */

#define DOCOPIE9 0x0009
/* COPIES (dataset count) FieldCount: 1 FieldLength 1
   v For JES2: binary number from 1 to 255 decimal v For JES3: binary number from 0 to 255 decimal
   Specifies number of copies printed. */

#define DOCOPIEA 0x000A
/* COPIES (group values) FieldCount: 8 FieldLength 1
   v For JES2: binary number from 1 to 255 decimal v For JES3: binary number from 1 to 254 decimal
   Specifies number of copies printed before next page. */

#define DODATACK 0x2022
/* DATACK FieldCount: 1 FieldLength 1
   X00 for BLOCK X80 for UNBLOCK X81 for BLKCHAR X82 for BLKPOS
   Specifies how errors in printers accessed through the functional subsystem Print Services Facility (PSF) are to be reported.  */

#define DODEFAUL 0x000B
/* DEFAULT FieldCount: 1 FieldLength 1
   X40 for YES X80 for NO
   Specifies that this is a default output descriptor. */

#define DODEPT 0x0029
/* DEPT FieldCount: 1 FieldLength 1-60
   EBCDIC text characters
   Specifies the department identification associated with the SYSOUT data set. */

#define DODEST 0x000C
/* DEST FieldCount: 1 FieldLength 1-127
   See z/OS MVS JCL Reference.
   Sends a SYSOUT data set to the specified destination. */

#define DODPAGEL 0x0023
/* DPAGELBL FieldCount: 1 FieldLength 1
   X40 for YES X80 for NO
   Indicates whether the system should place a security label on each output page. YES means the system should place a label on each page. NO means the system should not place a label on each page. */

#define DODUPLEX 0x003D
/* DUPLEX FieldCount: 1 FieldLength 1
   X80 for NO X40 for NORMAL X20 for TUMBLE
   Specifies whether the job is to be printed on one or both sides of the paper. Overrides comparable FORMDEF specification. */

#define DOFCB 0x000D
/* FCB FieldCount: 1 FieldLength 1-4
   alphanumeric or national (@, $, #) characters
   Specifies FCB image, carriage control tape for 1403 Printer, or data-protection image for 3525 Card Punch. */

#define DOFLASE 0x000E
/* FLASH (overlay name) FieldCount: 1 FieldLength 1-4
   alphanumeric or national (@, $, #) characters
   For printing on a 3800 Printing Subsystem, indicates that the data set is to be printed with forms overlay. */

#define DOFLASF 0x000F
/* FLASH (count) FieldCount: 1 FieldLength 1
   binary number from 0 to 255 decimal
   For printing on a 3800 Printing Subsystem, specifies how many copies are to be printed with forms overlay. */

#define DOFORMD 0x001D
/* FORMDEF FieldCount: 1 FieldLength 1-6
   alphanumeric or national (@, $, #) characters
   Names a library member that PSF uses in printing the SYSOUT data set on a 3800 Printing Subsystem Model 3. */

#define DOFORMLN 0x003B
/* FORMLEN FieldCount: 1 FieldLength 1-10
   See z/OS MVS JCL Reference.
   Specifies the form length to be used for a print data set when it is not specified in the DORMDEF parameter. */

#define DOFORMS 0x0010
/* FORMS FieldCount: 1 FieldLength 1-8
   alphanumeric or national (@, $, #) characters
   Identifies forms on which the SYSOUT data set is to be printed or punched. */

#define DOFSSDAT 0x0047
/* FSSDATA FieldCount: 1 FieldLength 1-127
   EBCDIC text characters
   Data that JES ignores but passes to a functional subsystem application. */

#define DOGROUPI 0x0011
/* GROUPID FieldCount: 1 FieldLength 1-8
   alphanumeric characters
   Specifies that this SYSOUT data set belongs to a user-named output group. (JES2 only) */

#define DOINDEX 0x0012
/* INDEX FieldCount: 1 FieldLength 1
   binary number from 1 to 31 decimal.
   Specifies how many print positions the left margin is to be indented for a SYSOUT data set printed on a 3211 Printer with the indexing feature. (JES2 only)  */

#define DOINTRAY 0x003E
/* INTRAY FieldCount: 1 FieldLength 1-3
   binary number from 1 to 255 decimal
   Specifies the printer input tray from which to take paper for the print job. Overrides comparable FORMDEF specification. */

#define DOLINDEX 0x0014
/* LINDEX FieldCount: 1 FieldLength 1
   binary number from 1 to 31 decimal.
   Specifies how many print positions the right margin is to be moved in from the full page width for a SYSOUT data set printed on a 3211 Printer with the indexing feature. (JES2 only) */

#define DOLINECT 0x0015
/* LINECT FieldCount: 1 FieldLength 1
   binary number from 0 to 255 decimal
   Specifies the maximum lines JES2 is to print on each page. (JES2 only) */

#define DOMAILBC 0x0049
/* MAILBCC FieldCount: 32 FieldLength 1-60
   EBCDIC text characters
   Specifies one or more e-mail addresses of the recipients on the blind copy list. */

#define DOMAILCC 0x004A
/* MAILCC FieldCount: 32 FieldLength 1-60
   EBCDIC text characters
   Specifies one or more e-mail addresses of the recipients on the copy list. */

#define DOMAILFI 0x004B
/* MAILFILE FieldCount: 1 FieldLength 1-60
   EBCDIC text characters
   Specifies the file name of the attachment to an e-mail. */

#define DOMAILFR 0x004C
/* MAILFROM FieldCount: 1 FieldLength 1-60
   EBCDIC text characters
   Specifies the descriptive name or other identifier of the sender of an e-mail. */

#define DOMAILTO 0x004D
/* MAILTO FieldCount: 32 FieldLength 1-60
   EBCDIC text characters
   Specifies one or more e-mail addresses of the e-mail recipients. */

#define DOMODIF6 0x0016
/* MODIFY (module name) FieldCount: 1 FieldLength 1-4
   alphanumeric or national (@, $, #) characters
   Specifies a copy-modification module in SYS1.IMAGELIB to be used by JES to print the data set on a 3800 Printing Subsystem. */

#define DOMODIF7 0x0017
/* MODIFY (trc) FieldCount: 1 FieldLength 1
   binary number from 0 to 3
   Specifies which character arrangement table is to be used. Related to the CHARS key. */

#define DONAME 0x002D
/* NAME FieldCount: 1 FieldLength 1-60
   EBCDIC text characters
   Specifies the preferred name of the owner of the SYSOUT data set. */

#define DONOTIFY 0x002F
/* NOTIFY FieldCount: 4 FieldLength 1-17
   node (optional) and userid
   Sends a print complete message to the specified destination. */

#define DOXOFSTB 0x0043
/* OFFSETXB FieldCount: 1-10 FieldLength 1-13
   See z/OS MVS JCL Reference.
   Specifies the X offset of the logical page origin from the physical page origin for the back side of each page. Overrides comparable FORMDEF specification. */

#define DOXOFSTF 0x0041
/* OFFSETXF FieldCount: 1-10 FieldLength 1-13
   See z/OS MVS JCL Reference.
   Specifies the X offset of the logical page origin from the physical page origin for the front side of each page. Overrides comparable FORMDEF specification.  */

#define DOYOFSTB 0x0044
/* OFFSETYB FieldCount: 1-10 FieldLength 1-13
   See z/OS MVS JCL Reference.
   Specifies the Y offset of the logical page origin from the physical page origin for the back side of each page. Overrides comparable FORMDEF specification. */

#define DOYOFSTF 0x0042
/* OFFSETYF FieldCount: 1-10 FieldLength 1-13
   See z/OS MVS JCL Reference.
   Specifies the Y offset of the logical page origin file the front side of each page. Overrides comparable FORMDEF specification. */

#define DOOUTBIN 0x2023
/* OUTBIN FieldCount: 1 FieldLength 4
   binary number from 1 to 65535 decimal
   Specifies the printer output bin ID. */

#define DOOUTDB 0x002B
/* OUTDISP (normal job completion) FieldCount: 1 FieldLength 1
   X'80' for WRITE X'40' for HOLD X'20' for KEEP X'10' for LEAVE X'08' for PURGE
   Specifies the SYSOUT data set disposition for normal job completion. (JES2 only) */

#define DOOUTDC 0x002C
/* OUTDISP (abnormal job completion) FieldCount: 1 FieldLength 1
   X'80' for WRITE X'40' for HOLD X'20' for KEEP X'10' for LEAVE X'08' for PURGE
   Specifies the SYSOUT data set disposition for abnormal job completion. (JES2 only) */

#define DOOVRLYB 0x0040
/* OVERLAYB FieldCount: 1 FieldLength 1-8
   alphanumeric or national ($, #, @) characters
   Specifies that the named medium overlay is to be placed on the back side of each sheet to be printed. The overlay is printed in addition to overlays specified in the FORMDEF. */

#define DOOVRLYF 0x003F
/* OVERLAYF FieldCount: 1 FieldLength 1-8
   alphanumeric or national ($, #, @) characters
   Specifies that the named medium overlay is to be placed on the front side of each sheet to be printed. The overlay is printed in addition to overlays specified in the FORMDEF. */

#define DOOVFL 0x0033
/* OVFL FieldCount: 1 FieldLength 1
   X'80' for ON X'40' for OFF
   Specifies whether or not JES3 should test for page overflow on an output printer. (JES3 only) */

#define DOPAGEDE 0x001F
/* PAGEDEF FieldCount: 1 FieldLength 1-6
   alphanumeric or national (@, $, #) characters
   Names a library member that PSF uses in printing the SYSOUT data set on a 3800 Printing Subsystem Model 3. */

#define DOPIMSG 0x0021
/* PIMSG FieldCount: 2 FieldLength 1
   X80 for NO X40 for YES The second value field is a two-byte number from 0 through 999 decimal, having a length field of 2.
   Indicates that messages from a functional subsystem should or should not be printed in the listing following the SYSOUT data set. Printing terminates if the number of printing errors exceeds the second value field.  */

#define DOPORTNO 0x0045
/* PORTNO FieldCount: 1 FieldLength 2
   binary number from 1 to 65535 decimal
   Specifies the TCP port number at which the FSS (for example, Infoprint Server) connects to the printer rather than connecting to LPD on the printer. Specify either PORTNO or PRTQUEUE, but not both. PRTQUEUE indicates the queue used when connecting to LPD on the printer. */

#define DOPRMODE 0x0018
/* PRMODE FieldCount: 1 FieldLength 1-8
   alphanumeric characters
   Identifies the process mode required to print the SYSOUT data set. */

#define DOPRTATT 0x0050
/* PRTATTRS FieldCount: 1 FieldLength 1-127
   EBCDIC text characters
   Specifies an Infoprint Server job attribute. The z/OS Infoprint Server Users Guide documents job attribute names and syntax for acceptable values. */

#define DOPROPTN 0x0039
/* PRTOPTNS FieldCount: 1 FieldLength 1-16
   see z/OS MVS JCL Reference.
   Named entity that can specify additional print options for FSS use. */

#define DOPRTERR 0x003C
/* PRTERROR FieldCount: 1 FieldLength 1
   X'80' for QUIT X'40' for HOLD X'20' for DEFAULT
   Specifies the action to be taken on a SYSOUT data set while being printed by PSF/MVS for a terminating error. */

#define DOPRTQUE 0x0038
/* PRTQUEUE FieldCount: 1 FieldLength 1-127
   see z/OS MVS JCL Reference.
   Identifies the target print queue for use by the FSS. */

#define DOPRTY 0x0019
/* PRTY FieldCount: 1 FieldLength 1
   binary number from 0 to 255 decimal
   Specifies initial priority at which the SYSOUT data set enters the output queue. */

#define DOREPLYT 0x004E
/* REPLYTO FieldCount: 1 FieldLength 1-60
   EBCDIC text characters
   Specifies the e-mail address to which recipients of the e-mail can respond. */

#define DORESFMT 0x0046
/* RESFMT FieldCount: 1 FieldLength 1
   X'80' for P249 X'40' for P300
   Specifies the resolution used to format the print data set. */

#define DORETANF 0x0037
/* RETAINF FieldCount: 1 FieldLength 1-10
   see z/OS MVS JCL Reference.
   Specifies the failed transmission retain time for use by the FSS. */

#define DORETANS 0x0036
/* RETAINS FieldCount: 1 FieldLength 1-10
   see z/OS MVS JCL Reference.
   Specifies the successful transmission retain time for use by the FSS. */

#define DORETRYL 0x0035
/* RETRYL FieldCount: 1 FieldLength 1-3
   see z/OS MVS JCL Reference.
   Specifies the maximum number of transmission retries used . by the FSS. */

#define DORETRYT 0x0034
/* RETRYT FieldCount: 1 FieldLength 1-8
   see z/OS MVS JCL Reference.
   Specifies the length of time that the FSS will wait between retries. */

#define DOROOM 0x0026
/* ROOM FieldCount: 1 FieldLength 1-60
   EBCDIC text characters
   Specifies the room identification to be associated with the SYSOUT data set.  */

#define DOSYSARE 0x0024
/* SYSAREA FieldCount: 1 FieldLength 1
   X40 for YES X80 for NO
   Indicates whether you want to use the system printable area of each output page. YES means you want to use the area. NO means you do not want to use the area. */

#define DOTHRESH 0x0022
/* THRESHLD FieldCount: 1 FieldLength 4
   binary number from 1 to 99999999 decimal
   Specifies the maximum size for a sysout data set. Use it to obtain simultaneous printing of large data sets or many data sets from one job. (JES3 only) */

#define DOTITLE 0x002A
/* TITLE FieldCount: 1 FieldLength 1-60
   EBCDIC text characters
   Specifies a title for the SYSOUT data set to be placed on the separator pages. */

#define DOTRC 0x001A
/* TRC FieldCount: 1 FieldLength 1
   X80 for NO X40 for YES
   Specifies whether or not the SYSOUT data sets records contain table reference codes (TRC) as the second character. */

#define DOUCS 0x001B
/* UCS FieldCount: 1 FieldLength 1-4
   alphanumeric or national (@, $, #) characters
   Specifies universal character set, print train, or character arrangement table for a 3800 Printing Subsystem. */

#define DOUSERDA 0x0031
/* USERDATA FieldCount: 16 FieldLength 1-60
   EBCDIC text '40'X - 'FE'X
   User-oriented information as defined by the installation. */

#define DOUSERLI 0x002E
/* USERLIB FieldCount: 8 FieldLength 44
   cataloged data set name
   Specifies the names of libraries containing AFP resources. */

#define DOUSERPAT 0x004F
/* USERPATH FieldCount: 8 FieldLength 1255
   SPECIAL text. See z/OS MVS JCL Reference.
   Specifies up to eight HFS or ZFS system paths containing resources to be used by PSF when processing SYSOUT data sets. */

#define DOWRITER 0x001C
/* WRITER FieldCount: 1 FieldLength 1-8
   alphanumeric or national (@, $, #) characters
   Names an external writer to process the SYSOUT data set rather than JES. */


/* This include contains functions for handling dynamic allocation / deallocation of datasets
 * and creation of dynamic DD names. */

#define DATASET_NAME_LEN 44
#define DATASET_MEMBER_NAME_LEN 8
#define DD_NAME_LEN 8

// Values for disposition field
#define DISP_OLD 0x01
#define DISP_MOD 0x02
#define DISP_SHARE 0x08
#define DISP_DELETE 0x04

/* Use this structure to pass parameters to DYNALLOC functions.
 * Dsname should be padded by spaces. */
typedef struct DynallocInputParms_tag {

  char dsName[DATASET_NAME_LEN];
  char ddName[DD_NAME_LEN];
  int  memberNameLength;
  char memberName[8];
  char disposition;
  char reserved[3];
} DynallocInputParms;

#pragma map(dynallocDataset, "DYNAUALC")
#pragma map(dynallocDatasetMember, "DYNAUALM")
#pragma map(unallocDataset, "DYNADALC")

int dynallocDataset(DynallocInputParms *inputParms, int *reasonCode);
int dynallocDatasetMember(DynallocInputParms *inputParms, int *reasonCode,
                          char *member);
int unallocDataset(DynallocInputParms *inputParms, int *reasonCode);

typedef struct DynallocDatasetName_tag {
  char name[44]; /* space-padded */
} DynallocDatasetName;

typedef struct DynallocMemberName_tag {
  char name[8]; /* space-padded */
} DynallocMemberName;

typedef struct DynallocDDName_tag {
  char name[8]; /* space-padded */
} DynallocDDName;

typedef enum DynallocAllocFags_tag {
  DYNALLOC_ALLOC_FLAG_NONE            = 0x00000000,
  DYNALLOC_ALLOC_FLAG_NO_CONVERSION   = 0x00000001,
  DYNALLOC_ALLOC_FLAG_NO_MOUNT        = 0x00000002,
} DynallocAllocFlags;

typedef enum DynallocUnallocFags_tag {
  DYNALLOC_UNALLOC_FLAG_NONE          = 0x00000000,
} DynallocUnallocFlags;

typedef enum DynallocDisposition_tag {
  DYNALLOC_DISP_OLD   = 0x00000001,
  DYNALLOC_DISP_MOD   = 0x00000002,
  DYNALLOC_DISP_SHR   = 0x00000008,
} DynallocDisposition;

#ifndef __LONGNAME__
#pragma map(dynallocAllocDataset, "DYNALAD2")
#pragma map(dynallocUnallocDatasetByDDName, "DYNALUD2")
#endif

/**
 * @brief The function dynamically allocates a dataset using SVC99.
 *
 * @param dsn Space-padded dataset name. The value must be non-NULL.
 * @param member Space-padded dataset member name. Pass NULL if no specific
 * member needs to be allocated.
 * @param ddName Space-padded DD name used for the allocation. Pass "????????"
 * to let the system generate a name. The value must be non-NULL.
 * @param disp Dataset disposition. See the DYNALLOC_DISP_xxxx flags (mutually
 * exclusive values).
 * @param flags Control flags. See the DYNALLOC_ALLOC_FLAG_xxxx flags (values
 * can be ORed).
 * @param sysRC SVC99 return codes. The value must be non-NULL.
 * @param sysRSN SVC99 reason codes. The value must be non-NULL.
 *
 * @return One of the RC_DYNALLOC_xx return codes.
 */
int dynallocAllocDataset(const DynallocDatasetName *dsn,
                         const DynallocMemberName *member,
                         DynallocDDName *ddName,
                         DynallocDisposition disp,
                         DynallocAllocFlags flags,
                         int *sysRC, int *sysRSN);

/**
 * @brief The function unallocates a previously allocated dataset.
 *
 * @param ddName The space-padded DD name of the dataset being unallocated.
 * The value must be non-NULL.
 * @param flags Control flags. See the DYNALLOC_UNALLOC_FLAG_xxxx flags (values
 * can be ORed).
 * @param sysRC SVC99 return codes. The value must be non-NULL.
 * @param sysRSN SVC99 reason codes. The value must be non-NULL.
 *
 * @return One of the RC_DYNALLOC_xx return codes.
 */
int dynallocUnallocDatasetByDDName(const DynallocDDName *ddName,
                                   DynallocUnallocFlags flags,
                                   int *sysRC, int *sysRSN);

#define RC_DYNALLOC_OK                0
#define RC_DYNALLOC_TU_ALLOC_FAILED   8
#define RC_DYNALLOC_SVC99_FAILED      9

#endif



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

