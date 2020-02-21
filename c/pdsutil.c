

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
#include <metal/ctype.h>  
#include "qsam.h"
#include "metalio.h"

#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <ctype.h>  
#include <errno.h>
#endif

#include "copyright.h"
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "logging.h"
#include "pdsutil.h"

static char pdsEndName[9] ={ 0xff, 0xff, 0xff, 0xff, 
			     0xff, 0xff, 0xff, 0xff, 0x0};

int startPDSIterator(PDSIterator *iterator, char *pdsName){
#ifdef METTLE 
  return 0;
#else
  char filenameBuffer[50];
  memset(iterator,0,sizeof(PDSIterator));

  sprintf(filenameBuffer,"//'%s'",pdsName);
  iterator->in = fopen(filenameBuffer,"rb");
  if (iterator->in){
    iterator->posInBlock = 0;
    iterator->bytesInBlock = -1;
    return 1;
  } else{
    return 0;
  }
#endif
}

static int isClosed(PDSIterator *iterator){
#ifdef METTLE 
  return 1;
#else
  return feof(iterator->in);
#endif
}

static int close(PDSIterator *iterator){
#ifdef METTLE 
  
#else
  return fclose(iterator->in);
#endif
}

int endPDSIterator(PDSIterator *iterator){
  close(iterator);
}

/* is sort order guaranteed?? */
int nextPDSEntry1(PDSIterator *iterator){
  char *block = iterator->block;
  int flags;
  int entryLength;
  int i;

  /* printf("done %d int 0x%x pos %d bytes %d\n",iterator->done,iterator->in,iterator->posInBlock,iterator->bytesInBlock); */
  if (iterator->done){
    return 0;
  }
  if (iterator->posInBlock >= iterator->bytesInBlock){
    /* get next block */
    if (isClosed(iterator)){
      iterator->done = 1;
      close(iterator);
      return 0;
    } else{
#ifdef METTLE
      int res = 0;
#else 
      int res = fread(iterator->block,1,256,iterator->in);
#endif
      iterator->bytesInBlock = ((block[0]&0xff) << 8) | (block[1]&0xff);
      iterator->posInBlock = 2;
    }
  }
  iterator->currentPosInBlock = iterator->posInBlock;
  memcpy(iterator->memberName,block+iterator->posInBlock,8);
  iterator->memberName[8] = 0;
  /* dumpbuffer(iterator->memberName,9); */
  for (i=0; i<8; i++){
    if (iterator->memberName[i] == ' '){
      iterator->memberName[i] = 0;
      break;
    }
  }
  /* printf("iterator:name %s\n",iterator->memberName); */
  flags = block[iterator->posInBlock+11];
  entryLength = 12 + (2 * (flags & 0x1f));
  /* printf("iterator:name %s flags=0x%x entryLen=%d\n",iterator->memberName,flags,entryLength); */
  if (!memcmp(iterator->memberName,pdsEndName,8)){
    iterator->done = 1;
    close(iterator);
    return 0;
  } else {
    iterator->posInBlock += entryLength;
    return 1;
  }
}

int nextPDSEntry(PDSIterator *iterator){
  int res = nextPDSEntry1(iterator);
  /* printf("HFS next = %d\n",res); */
  return res;
}

#ifdef METTLE
void listDirectory(char *pdsName){
}
#else
void listDirectory(char *pdsName){
  /* 12 byte, last byte * 0x1F gives number of half words */
  /* ISP.SISPMACS(ISPDSTAT)  ISPF stats   */
  FILE *in = NULL;
  char filenameBuffer[50];
  char block[256];
  int blockCount = 0;
  int lastBlock = 0;

  sprintf(filenameBuffer,"//'%s'",pdsName);
  in = fopen(filenameBuffer,"rb");
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "fopen in=0x%x errno=%d\n",in,errno);
  while (!feof(in) && !lastBlock){
    /* You should test for the last directory entry (8 bytes of
       X'FF'). Records and blocks after that point are unpredictable. After
       reading the last allocated directory block, control passes to your
     */
    int res = fread(block,1,256,in);
    int posInBlock = 2;
    blockCount++;
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "PDS BLOCK %d, res=%d\n",blockCount,res);
    /* dumpbuffer(block,256); */
    while (posInBlock < 256){
      int flags = block[posInBlock+11];
      int blockLength = 12 + 2 * (flags & 0x1f);
      char name[9];
      memcpy(name,block+posInBlock,8);
      name[8] = 0;
      if (!strcmp(name,pdsEndName)){
      	lastBlock = 1;
      	break;
      }
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "name='%8s' otherFlags = 0x%x\n",name,flags&0xe0);
      dumpbuffer(block+posInBlock,blockLength);
      posInBlock += blockLength;
    }
  }
  fclose(in);
}
#endif

static char validDatasetQualifierChars[41] = {' ','A','B','C','D','E','F','G','H','I','J',
                                              'K','L','M','N','O','P','Q','R','S','T',
                                              'U','V','W','X','Y','Z','1','2','3','4',
                                              '5','6','7','8','9','0','$','#','@','-',};

#ifdef METTLE
StringList *getPDSMembers(char *pdsName){

}
#else
StringList *getPDSMembers(char *pdsName){
  FILE *in = NULL;
  char filenameBuffer[50];
  char block[256];
  int blockCount = 0;
  int lastBlock = 0;

  ShortLivedHeap *slh = makeShortLivedHeap(65536,100);
  StringList *list = makeStringList(slh);

  sprintf(filenameBuffer,"//'%s'",pdsName);
  in = fopen(filenameBuffer,"rb");
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "fopen in=0x%x errno=%d\n",in);
  if (in == 0){
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING, "Error encountered on reading PDS member, returning empty list\n");
    fflush(stdout);
    return list;
  }
  while (!feof(in) && !lastBlock){
    /* You should test for the last directory entry (8 bytes of
       X'FF'). Records and blocks after that point are unpredictable. After
       reading the last allocated directory block, control passes to your
     */
    int bytesRead = fread(block,1,256,in);
    int posInBlock = 2;
    blockCount++;
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "PDS BLOCK %d, bytesRead=%d\n",blockCount,bytesRead);
    if (bytesRead == 0){
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING, "Error encountered reading PDS member, returning current list\n");
      fflush(stdout);
      return list;
    }
    while (posInBlock < 256){
      int flags = block[posInBlock+11];
      int blockLength = 12 + 2 * (flags & 0x1f);
      char *name = safeMalloc(sizeof(char)*9, "Name buffer");
      memcpy(name,block+posInBlock,8);
      name[8] = 0;
      if (!strcmp(name,pdsEndName)){
      	lastBlock = 1;
      	break;
      }
      addToStringListUnique(list, name);
      posInBlock += blockLength;
    }
  }
  fclose(in);  

  return list;
}
#endif

#define GETDSAB_PLIST_LENGTH 16



typedef struct DSAB_tag{
  char eyecatcher[4];
  int   dsabfchn;
  int   dsabbchn;
  short dsablnth; /* length */
  short dsabopct; /* OPEN DCB COUNT FOR TIOT DD ENTRY */
  int   dsabtiot; 
  int   dsabssva; /* only low 24bits used SWA VIRTUAL ADDRESS OF SIOT    */
  int   dsabxsva; /* only low 24 - SVA of XSIOT                */
  int   dsabanmp; /* &NAME OR GDG-ALL DSNAME PTR, 0 IF NONE */
  char dsaborg1;
  char dsaborg2;
  char dsabflg1;
  char dsabflg2;
  char dsabflg3;
  char dsabflg4;
  short dsabdext; /*     INDEX TO DEXT TABLE   */
  int  dsabtcbp; /* tcb under which set in-use */
  int  dsabpttr; /* relative TTR of data set password */
  char dsabssnm[4]; /* subsystem name */
  int  dsabsscm;    /* subsystem communication area */
  char dsabdcbm[6]; /* bit map of DCB fields */
  short dsabtctl;   /* offset of lookup entry from beginning of TCTIOT 
                       If DSABATCT is on, use DSABTCT2 instead               */
  int  dsabsiot;    /* SIOT incore address */
  
  int dsabtokn;    /* DD Token */
  int dsabtct2;    /* offset of lookup entry from beg of TCTIOT, always valid */
  int dsabsiox;    /* virtual address of SIOTX */
  int dsabfcha;
  int dsabbcha;
  char dsabflg5;
  char reserved[7];
} DSAB;

typedef struct TIOT_tag{
  char tiocnjob[8];
  char tiocstpn[8]; 
  
} TIOT;

int memberExistsInDDName(char *ddname){
  /* 
     DDNAME

     PDS YES, MEMBER=YES
     PDS YES, MEMBER=NO
     
     DS YES, MEMBER=NOT_SPECIFIED
     DS_NO,  MEMBER=NOT_SPECIFIED

     dsab->tiot
   */

#ifdef METTLE 
  char plist[GETDSAB_PLIST_LENGTH];
  DSAB *dsab = NULL; // (DSAB*)safeMalloc(sizeof(DSAB),"dsab");
  DSAB **dsabHandle = NULL;
  int rc;

  memset(plist,0,GETDSAB_PLIST_LENGTH);

  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "ddname at 0x%x\n",ddname);
  __asm(" GETDSAB DDNAME=(%2),DSABPTR=%1,LOC=ANY,RETCODE=%0,MF=(E,%3) " :
        "=m"(rc), "=m"(dsab) :
        "r"(ddname),  "m"(plist) :
        "r15");

  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "after GETDSAB rc=%d, dsab at 0x%x\n",
         rc,dsab);
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_INFO, "plist:\n");
  dumpbuffer(plist,16);
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_INFO, "dd:\n");
  dsabHandle = (DSAB**)((int*)plist)[3];
  /* int foo = ((int*)dsabHandle)[0]; */
  int foo = 0;
  dsab = (DSAB*)((int*)dsabHandle)[0];
  char *ddname2 = (char*)((int*)plist)[2];
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "dsabHandle=0x%x ddname2=0x%x foo=0x%x\n",dsabHandle,ddname2,foo);
  
  if (rc == 0){
    dumpbuffer((char*)dsabHandle,16);
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "dsab at 0x%x\n",dsab);
    dumpbuffer((char*)dsab,64);
    

    TIOT *tiot = (TIOT*)dsab->dsabtiot;
    
    /*
      Turn an SVA (Scheduler Virtual Address)

     SWAREQ FCODE=RL,             issue SWAREQ read mode             - 
         EPA=WA_EPA_ADDR,      using this WPA                    - 
         UNAUTH=YES,           not auth                          - 
         MF=(E,WA_SWAREQ_PLIST)                                    
         */
    
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "TIOT:\n");
    dumpbuffer((char*)dsab->dsabtiot,64);
    int tioejfcb = ((int*)dsab->dsabtiot)[3];
    int jfcbSVA = (tioejfcb&0xFFFFFF00)>>8;
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "JFCB SVA = 0x%x\n",jfcbSVA);
  }

  return FALSE;



#else
  return FALSE; /* hack */
#endif
}

#ifdef METTLE
#pragma insert_asm(" CVT DSECT=YES,LIST=NO ")
#pragma insert_asm(" IEFJESCT ")
#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

