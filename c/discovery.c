

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
#include "metalio.h"
#else
#include <stdio.h>
#include <ctest.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "copyright.h"
#include "alloc.h"
#include "utils.h"
#include "zos.h"
#include "collections.h"
#include "le.h"
#include "recovery.h"
#include "scheduling.h"
#include "pdsutil.h"
#include "vtam.h"
#include "ezbnmrhc.h"
#include "timeutls.h"
#include "discovery.h"

#include "crossmemory.h"
#include "zis/client.h"

static int string8Hash(void *key){
  char *s = (char*)key;
  int hash = 0;
  int i;
  int len = 8;

  for (i=0; i<len; i++){
    hash = (hash << 4) + s[i];
  }
  return hash&0x7fffffff;
}


static int string8Compare(void *key1, void *key2){
  char *s1 = (char*)key1;
  char *s2 = (char*)key2;

  return !memcmp(s1,s2,8);
}

static void showVersionInfo(DiscoveryContext *context, char *ssName){
  if (context->ssctTraceLevel >= 1){
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "versionInfo can be accessed in authorized mode only\n");
  }
}

static GDA *getGDA(){
  CVT *cvt = getCVT();
  ECVT *ecvt = getECVT();
  GDA *gda = (GDA*)cvt->cvtgda;
  return gda;
}

static int isPointerCommon(GDA *gda, void *pointer){
  if ( ((pointer >= gda->gdacsa) && (pointer < gda->gdacsa+gda->gdacsasz)) ||
       ((pointer >= gda->gdasqa) && (pointer < gda->gdasqa+gda->gdasqasz)) ||
       ((pointer >= gda->gdaesqa) && (pointer < gda->gdaesqa+gda->gdaesqas)) || 
       ((pointer >= gda->gdaecsa) && (pointer < gda->gdaecsa+gda->gdaecsas)) ){
    return TRUE;
  } else{
    return FALSE;
  }
}


DiscoveryContext *makeDiscoveryContext(ShortLivedHeap *outerSLH, ZOSModel *model){
  ShortLivedHeap *slh = (outerSLH ? outerSLH :
                                    makeShortLivedHeap(262144,400));  /* 256K chunk size */
  DiscoveryContext *context = (DiscoveryContext*)SLHAlloc(slh,sizeof(DiscoveryContext));
  memset(context,0,sizeof(DiscoveryContext));
  memcpy(context->eyecatcher,"DSCVCNTXT",8);
  context->slh = slh;
  
  context->model = model;
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG,
          "mDCntxt() model=%p pSN=\'%.16s\'\n", model,
          model ? model->privilegedServerName.nameSpacePadded : "");
  if (model != NULL) {
    context->privilegedServerName = model->privilegedServerName;
  } else {
    context->privilegedServerName = zisGetDefaultServerName();
  }
  return context;
}

void freeDiscoveryContext(DiscoveryContext *context){
  SLHFree(context->slh); /* i have everything */
}

static SoftwareInstance *chainSubsystems(SoftwareInstance *info, SoftwareInstance *tail){
  info->next = tail;
  return info;
}

static SoftwareInstance *addSoftware(DiscoveryContext *context, 
                                     uint64 subsystemType, 
                                     uint64 softwareFlags,
                                     SSCT *ssct, 
                                     char *specificBestName){   /* this is not input, this is a filter */
  ShortLivedHeap *slh = context->slh;
  SoftwareInstance *info = (SoftwareInstance*)SLHAlloc(slh,sizeof(SoftwareInstance));
  memset(info,0,sizeof(SoftwareInstance));
  info->type = subsystemType;
  char *bestName = "<unknown>";
  if (ssct){
    info->ssctCopy = (SSCT*)SLHAlloc(slh,sizeof(SSCT));
    memcpy(info->ssctCopy,ssct,sizeof(SSCT));
    bestName = SLHAlloc(slh,12);
    memset(bestName,0,12);
    memcpy(bestName,ssct->sname,4);  /* the SSCT name is pretty good most of the time */

  } else{
    info->ssctCopy = NULL;
  }
  info->flags = softwareFlags;
  if (subsystemType != SOFTWARE_TYPE_CICS){   /* CICS names are not determined this early, must be filtered later */
    if (specificBestName &&
        memcmp(specificBestName,bestName,4)){
      zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "ignoring bestName=%s because not specific request='%s' at 0x%x\n",
             bestName,
             (specificBestName ? specificBestName : NULL),
             specificBestName);
      return NULL;
    }
  }
  info->bestName = bestName;
  if ((context->ssctTraceLevel >= 1) ||
      (context->addressSpaceTraceLevel >= 1)){
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "addSoftware info=0x%x type=0x%llx bestName=%s\n",info,info->type,info->bestName);
  }
  if (softwareFlags | SOFTWARE_FLAGS_SUBSYSTEM){
    switch (subsystemType){
    case SOFTWARE_TYPE_IMS:
      context->imsSubsystems = chainSubsystems(info,context->imsSubsystems);
      break;
    case SOFTWARE_TYPE_DB2:
      context->db2Subsystems = chainSubsystems(info,context->db2Subsystems);
      break;
    case SOFTWARE_TYPE_MQ:
      context->mqSubsystems = chainSubsystems(info,context->mqSubsystems);
      break;
    case SOFTWARE_TYPE_CICS:
      context->cicsSubsystems = chainSubsystems(info,context->cicsSubsystems);
      break;
    default:
      context->softwareInstances = chainSubsystems(info,context->softwareInstances);
      break;
    }
  }
  return info;
} 

static void copyFromKeyUsingZIS(CrossMemoryServerName *zisServerName, void *target, void *source, int length){
  ZISCopyServiceStatus status = {0};
  int copyRC = 0, copyRSN = 0;

  copyRC = zisCopyDataFromAddressSpace(zisServerName, target, source,
      length, 0, getASCB(), &status);
  if (copyRC != RC_ZIS_SRVC_OK) {
#define FORMAT_ERROR($f, ...)  printf("info: data NOT copied from 0x%p "\
  "(home ASCB) to 0x%p: " $f "\n", source, target, ##__VA_ARGS__)

    ZIS_FORMAT_COPY_CALL_STATUS(copyRC, &status, FORMAT_ERROR);
#undef FORMAT_ERROR
  }
}

static char *getStringFromBlock(ShortLivedHeap *slh, char *s, int length){
  char *copy = SLHAlloc(slh,length+4);
  memset(copy,0,length+4);
  memcpy(copy,s,length);
  return copy;
}

static void trimCString(char *s, int length){
  for (int i=length-1; i>=0; i--){
    if (s[i] > 0x41){
      break;
    } else{
      s[i] = 0x0;
    }
  }
}

static char *discoveryAllocInternal(DiscoveryContext *context, int size){
  char *object = SLHAlloc(context->slh,size);
  memset(object,0,size);
  return object;
}

static char *discoveryCopyInternal(DiscoveryContext *context, void *pointer, int size){
  char *copy = SLHAlloc(context->slh,size);
  copyFromKeyUsingZIS(&context->privilegedServerName, copy, pointer, size);
  return copy;
}

#define discoveryAlloc(context,type) ((type*)discoveryAllocInternal(context,sizeof(type)))
#define discoveryAllocCopyFromKey(context,pointer,type) ((type*)discoveryCopyInternal(context,pointer,sizeof(type)))

static void *getStructCopy(DiscoveryContext *context, 
                           ASCB         *ascb,
                           int           sourceKey,
                           Addr31        address,
                           int           size){
  char *key8Buffer = SLHAlloc(context->slh,size);

  int copyRC = 0;
  ZISCopyServiceStatus serviceStatus = {0};

  copyRC = zisCopyDataFromAddressSpace(&context->privilegedServerName,
      key8Buffer, address, size, sourceKey, ascb, &serviceStatus);
  if (copyRC != RC_ZIS_SRVC_OK) {
#define FORMAT_ERROR($f, ...)  printf("info: data NOT copied from 0x%p "\
  "(ASCB=0x%p) to 0x%p: " $f "\n", address, ascb, key8Buffer, ##__VA_ARGS__)

    ZIS_FORMAT_COPY_CALL_STATUS(copyRC, &serviceStatus, FORMAT_ERROR);
#undef FORMAT_ERROR
    return NULL;
  } else {
    return (void*)key8Buffer;
  }
}

static char *copyASCBJobname(DiscoveryContext *context, ASCB *ascb){
  char *name = getASCBJobname(ascb);
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "SLH MGMT: copy from SLH 0x%x\n",context->slh);
  char *copy = SLHAlloc(context->slh,12);
  copy[8] = 0;
  memcpy(copy,name,8);
  return copy;
}

static ASCB *getASCBByASID(int asid){
  CVT *cvt = getCVT();
  ASVT *asvt = (ASVT*)cvt->cvtasvt;
  
  ASCB *ascb = (ASCB*)INT2PTR(asvt->asvtenty);
  while (ascb){
    if (ascb->ascbasid == asid){
      return ascb;
    }
    ascb = (ASCB*)ascb->ascbfwdp;
  }
  return NULL;
}

#define ANY_TCB 0xFFFFFFFF

static int walkTCBs1(DiscoveryContext *context, 
                     ASCB *ascb,
                     TCB *tcb,
                     TCB *particularTCB,
                     int depth,
                     void (*visitor)(DiscoveryContext *discoveryContext,
                                     void   *visitorContext,
                                     int     depth,
                                     ASCB   *ascb,
                                     TCB    *tcb,
                                     char   *desiredSubsystemName),
                     void *visitorContext,
                     char *desiredSubsystemName){
  if (FALSE){
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "TCB at depth=%d\n",depth);
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "  tcbntc(sibling): 0x%x\n",tcb->tcbntc);
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "  tcbotc(parent ): 0x%x\n",tcb->tcbotc);
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "  tcbltc(child  ): 0x%x\n",tcb->tcbltc);
    /* dumpbuffer((char*)tcb,sizeof(TCB)); */
    fflush(stdout);
  }
  
  if ((particularTCB == (Addr31)ANY_TCB) ||   /* either ALL_TCBs or be the one requested */
      (tcb == particularTCB)){
    if (tcb && visitor){
      visitor(context,visitorContext,depth,ascb,tcb,desiredSubsystemName);
    }
  }
  if (tcb){
    if (tcb->tcbltc){   /* walk children first */
      /* printf("TCBLTC = 0x%x\n",tcb->tcbltc); */
      TCB *childTCB = (TCB*)getStructCopy(context,ascb,0,tcb->tcbltc,sizeof(TCB));
      if (childTCB){
        walkTCBs1(context,ascb,childTCB,particularTCB,depth+1,visitor,visitorContext,desiredSubsystemName);
      }
    }
    if (tcb->tcbntc){
      TCB *siblingTCB = (TCB*)getStructCopy(context,ascb,0,tcb->tcbntc,sizeof(TCB));
      /* printf("TCBNTC = 0x%x\n",tcb->tcbntc); */
      if (siblingTCB){
        walkTCBs1(context,ascb,siblingTCB,particularTCB,depth,visitor,visitorContext,desiredSubsystemName);
      }
    }
  }
  return 0;
}


int walkTCBs(DiscoveryContext *context,
	     ASCB *ascb,
	     TCB *tcb,
	     void (*visitor)(DiscoveryContext *discoveryContext,
			     void   *visitorContext,
			     int     depth,
			     ASCB   *ascb,
			     TCB    *tcb,
			     char   *mustBeNull),
	     void *visitorContext){
  if (ascb){
    ASXB *asxb = (ASXB*)ascb->ascbasxb;
    TCB *firstTCB = (TCB*)getStructCopy(context,ascb,0,asxb->asxbftcb,sizeof(TCB));
    walkTCBs1(context,ascb,firstTCB,(TCB*)ANY_TCB,0,visitor,visitorContext,NULL);
  }
  return 0;
}


static void visitSSCTEntry(DiscoveryContext *context, 
                           SSCT *ssctChain, GDA *gda, int subsystemTypeMask,
                           char *specificBestName){
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "SSCT %.4s at 0x%x specificBestName ptr is 0x%x\n",&(ssctChain->sname),ssctChain,specificBestName);fflush(stdout);
  if (context->ssctTraceLevel >= 1){
    dumpbuffer((char*)ssctChain,sizeof(SSCT));
  }
  void *usr1 = (void*)INT2PTR(ssctChain->ssctsuse);
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "user pointer at 0x%x COMMON?=%s\n",usr1,isPointerCommon(gda,usr1) ? "YES" : "NO");
  if (isPointerCommon(gda,usr1)){
    if (context->ssctTraceLevel >= 1){
      dumpbuffer((char*)usr1,48);
    }
    char *sname = &(ssctChain->sname[0]);
    char *usrData = (char*)usr1;
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "sname=%4.4s\n",sname);
    if (!memcmp(sname,"CICS",4) && (context->ssctTraceLevel >= 1)){
      dumpbuffer(usrData+0x08,6);
    }
    fflush(stdout);
    if (!memcmp(usrData+4,"ERLY",4)){  
      ERLYFragment *erly = (ERLYFragment*)usr1;
      zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "module name %8.8s\n", erly->erlymodn);
      if (!memcmp(erly->erlymodn,"CSQ3EPX ",8)){
        if (subsystemTypeMask & SOFTWARE_TYPE_MQ){
          zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "Possible MQ\n");
          addSoftware(context,SOFTWARE_TYPE_MQ,0,ssctChain,specificBestName);
        }
      } else if (!memcmp(erly->erlymodn,"DSN3EPX ",8)){
        if (subsystemTypeMask & SOFTWARE_TYPE_DB2){
          zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "Possible DB2\n");
          addSoftware(context,
                      SOFTWARE_TYPE_DB2,
                      SOFTWARE_FLAGS_SUBSYSTEM | SOFTWARE_FLAGS_OLTP,
                      ssctChain,
                      specificBestName);
        }
      }
    }
  }

  showVersionInfo(context, (char*)&(ssctChain->sname));
}

void discoverSubsystems(DiscoveryContext *context, int subsystemTypeMask, char *specificBestName){
  CVT *theCVT = getCVT();
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "discoverSubsystems cvt at 0x%x specificBestName\n",theCVT,specificBestName);
  fflush(stdout);
  JESCT *theJESCT = (JESCT*)(theCVT->cvtjesct);
  SSCT *ssctChain = theJESCT->jesssct;
  GDA *gda = getGDA();
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "CSA  start 0x%08x end=0x%08x size=0x%08x\n",
          gda->gdacsa,gda->gdacsa+gda->gdacsasz,gda->gdacsasz);
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "SQA  start 0x%08x end=0x%08x size=0x%08x\n",
          gda->gdasqa,gda->gdasqa+gda->gdasqasz,gda->gdasqasz);
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "ESQA start 0x%08x end=0x%08x size=0x%08x\n",
          gda->gdaesqa,gda->gdaesqa+gda->gdaesqas,gda->gdaesqas);
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "ECSA start 0x%08x end=0x%08x size=0x%08x\n",
          gda->gdaecsa,gda->gdaecsa+gda->gdaecsas,gda->gdaecsas);
  
  ZOSModel *model = context->model;
  SubsystemVisitor *userVisitor = model ? model->subsystemVisitor : NULL;
  void* userVisitorData = model ? model->visitorsData : NULL;
  
  int subsystemCount = 0;
  
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "SSCT Chain start = 0x%x\n",ssctChain);
  do{
    visitSSCTEntry(context,ssctChain,gda,subsystemTypeMask,specificBestName);
    if (userVisitor != NULL) {
      userVisitor(
          context,
          ssctChain,
          gda,
          subsystemTypeMask,
          specificBestName,
          userVisitorData
      );
    }
    ssctChain = ssctChain->scta;
    subsystemCount++;
  } while (ssctChain != NULL);
  
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "Inspected %d subsystems\n",subsystemCount);
}

/* each ISPTASK in TCB Tree has one TLD for each ISPF split
   up to the limit of 8/16.
 */

static void ispfAnchorVisitor(DiscoveryContext *context,
                              void *visitorContext,
                              int depth,
                              ASCB *ascb,
                              TCB *tcbCopy,
                              char *desiredBestName){

  /* printf("ISPF visitor\n"); */
  ISPFTLD **tldResult = (ISPFTLD**)visitorContext;
  if (tcbCopy){
    /* printf("TCB (copy) at 0x%x\n",tcbCopy); */
    int *tcbfsaPtr = (int*)(tcbCopy->tcbfsa);
    /* printf("TCBFSA PTR = 0x%x\n",tcbCopy->tcbfsa); */
    if (tcbfsaPtr){
      int *tcbfsa = (int*)getStructCopy(context,ascb,0,tcbfsaPtr,64);
      if (tcbfsa){
        /*
           printf("TCBFSA\n");
           dumpbuffer((char*)tcbfsa,64);
           */
        if (tcbfsa[10] == *((int*)"ISPF")){
          /* printf("WOO HOO\n"); */
          int *tldHandle = (int*)INT2PTR(tcbfsa[6]);
          int *tldPtr = (int*)((int*)getStructCopy(context,ascb,0,tldHandle,4))[0];
          /* printf("tldPtr=0x%x\n",tldPtr); */
          if (tldPtr){
            if ((((int)tldPtr)&0xFF000000) == 0){
              ISPFTLD *tld = (ISPFTLD*)getStructCopy(context,ascb,0,tldPtr,sizeof(ISPFTLD));
              /*
                 printf("TLD = 0x%x\n",tld);
                 dumpbuffer((char*)tld,sizeof(ISPFTLD));
               */
              *tldResult = tld;
            }
          }
        }
      }
    }
  }
}

static ISPFTLD *findISPFAnchor(DiscoveryContext *context, ASCB *ascb){
  /* TCB Tree walk */
  ISPFTLD *tld = NULL;
  if (ascb){
    ASXB *asxb = (ASXB*)ascb->ascbasxb;
    TCB *firstTCB = (TCB*)getStructCopy(context,ascb,0,asxb->asxbftcb,sizeof(TCB));
    walkTCBs1(context,ascb,firstTCB,(TCB*)ANY_TCB,0,ispfAnchorVisitor,&tld,NULL);
  }
  return tld;
}

#define NMIBUFSIZE 0x20000
#define NOT_ENOUGH_SPACE 1122

typedef struct NMIBufferType_tag{
  NWMHeader    header;
  NWMFilter    filters[2];  /* the filters exist in an OR of an and of the properties in the Filterr Object */
} NMIBufferType;

static int isTSOAppl(char *applName){
  for (int i=0; i<6; i++){
    if (!memcmp(applName+i,"TSO",3)){
      return TRUE;
    }
  }
  return FALSE;
}

#define EMPIRICAL_SOCKET_ADDR_V4   2
#define EMPIRICAL_SOCKET_ADDR_V6  19

#define EMPIRICAL_SOCKET_PSEUDO_V4_MARK 0xFFFF

#define ENSURE31BIT(a) ((Addr31)(((int)a)&0x7FFFFFFF))

typedef struct EmpiricalSocketAddr_tag{
  char            length;
  char            family;
  unsigned short  port;
  unsigned int    noIdea;
  unsigned short  v6Parts[8];
  char            v4Address[4];
} EmpiricalSocketAddr;

char *getSoftwareTypeName(TN3270Info *info){
  switch (info->softwareType){
  case SOFTWARE_TYPE_IMS:
    return "IMS";
  case SOFTWARE_TYPE_DB2:
    return "DB2";
  case SOFTWARE_TYPE_MQ:
    return "MQ";
  default:
    return "unknown";
  }
}


char *getSoftwareSubtypeName(TN3270Info *info){
  switch (info->softwareType){
  default:
    return "unknown";
  }
}

static TN3270Info *addTN3270Info(DiscoveryContext *context, 
                                 NWMConnEntry *cnxn, 
                                 int flags,
                                 char *addressString){
  TN3270Info *info = (TN3270Info*)SLHAlloc(context->slh,sizeof(TN3270Info));
  memset(info,0,sizeof(TN3270Info));
  int addressStringLength = strlen(addressString);
  char *addressStringCopy = SLHAlloc(context->slh,addressStringLength+1);
  addressStringCopy[addressStringLength] = 0;
  memcpy(addressStringCopy,addressString,addressStringLength);
  
  info->addressString = addressStringCopy;
  memcpy(info->eyecatcher,"3270INFO",8);
  info->flags = flags;
  memcpy(info->clientLUName,cnxn->NWMConnLuName,8);
  info->next = context->tn3270Infos;
  context->tn3270Infos = info;
  return info;
}
                                    

int findSessions(DiscoveryContext *context,
                 int               sessionKeyType,
                 int               numericKeyValue,
                 char             *stringKeyValue){
  int bufferLength = NMIBUFSIZE;
  int attempts     = 0;

  SoftwareInstance *instanceList = zModelGetSoftware(context->model);
  hashtable *tsbTable = zModelGetTSBs(context->model);

  while (attempts < 2){
    NMIBufferType *nmiBuffer = (NMIBufferType*)SLHAlloc(context->slh,bufferLength);
    memset(nmiBuffer,0,bufferLength);
    /* Fill Header */
    nmiBuffer->header.NWMHeaderIdent=NWMHEADERIDENTIFIER;
    nmiBuffer->header.NWMHeaderLength=sizeof(NWMHeader);
    nmiBuffer->header.NWMVersion=NWMVERSION1;
    
    nmiBuffer->header.NWMType=NWMTCPCONNTYPE;
    nmiBuffer->header.NWMBytesNeeded=0;
    nmiBuffer->header.NWMInputDataDescriptors.NWMFiltersDesc.NWMTOffset=sizeof(NWMHeader);
    nmiBuffer->header.NWMInputDataDescriptors.NWMFiltersDesc.NWMTLength=sizeof(NWMFilter);
    nmiBuffer->header.NWMInputDataDescriptors.NWMFiltersDesc.NWMTNumber=1;
    
    nmiBuffer->filters[0].NWMFilterIdent=NWMFILTERIDENTIFIER;
    nmiBuffer->filters[0].NWMFilterFlags=NWMFILTERRESNAMEMASK;
    memcpy(nmiBuffer->filters[0].NWMFilterResourceName,"TN3270  ",8);   /* is this wrong if TN3270 is not name of TN3270 sever */

    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "request\n");
    if (context->vtamTraceLevel >= 1){
      dumpbuffer((char*)nmiBuffer,0x100);
    }
    attempts++;

    ZISNWMJobName jobName = {.value = "TCPIP   "};
    ZISNWMServiceStatus zisStatus = {0};
    int currTraceLevel = logGetLevel(NULL, LOG_COMP_DISCOVERY);

    int zisRC = zisCallNWMService(&context->privilegedServerName,
                                  jobName, (char *)nmiBuffer, bufferLength,
                                  &zisStatus, currTraceLevel);

    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2,
            "ZIS NWM RC = %d, NWM RV = 0x%X,  RC = %d,  RSN = 0x%X\n", zisRC,
            zisStatus.nmiReturnValue,
            zisStatus.nmiReturnCode,
            zisStatus.nmiReasonCode);

    if (zisRC != RC_ZIS_SRVC_OK) {

      bool isNWMError =
          (zisRC == RC_ZIS_SRVC_SERVICE_FAILED) &&
          (zisStatus.baseStatus.serviceRC == RC_ZIS_NWMSRV_NWM_FAILED);

      bool isNotEnoughSpace = (zisStatus.nmiReturnValue == -1) &&
                              (zisStatus.nmiReturnCode == NOT_ENOUGH_SPACE);

      if (isNWMError && isNotEnoughSpace){
        bufferLength = nmiBuffer->header.NWMBytesNeeded + 0x1000;
        zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2,
                "NWM retry with more space 0x%x\n", bufferLength);
        zowedump(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2,
                 (char*)nmiBuffer, 0x100);
        continue;
      } else {
        break; /* either ZIS failed or NWM returned an unrecoverable error */
      }

    }

    int i;
    if (zisStatus.nmiReturnValue > 0){
      int startingOffset = nmiBuffer->header.NWMOutputDesc.NWMQOffset;
      char *dataElement = ((char*)nmiBuffer)+startingOffset;
      int elementCount = nmiBuffer->header.NWMOutputDesc.NWMQNumber;
      int elementLength = nmiBuffer->header.NWMOutputDesc.NWMQLength;
      for (i=0; i<elementCount; i++){
        NWMConnEntry *cnxn = (NWMConnEntry*)dataElement;
        zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "cnxn %d  resourceName=%8.8s luName=%8.8s asid=0x%x\n",i,cnxn->NWMConnResourceName,cnxn->NWMConnLuName,cnxn->NWMConnAsid);
        zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "         targetAppl=%8.8s \n",cnxn->NWMConnTargetAppl);
        zowedump(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, dataElement,sizeof(NWMConnEntry));

        char addressAsString[256];
        addressAsString[0] = 0;
        EmpiricalSocketAddr *remoteAddr = (EmpiricalSocketAddr*)(dataElement+0x20);
        zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "empirical socket address at 0x%x, fam=%d\n",remoteAddr,remoteAddr->family); 
        zowedump(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, (char*)remoteAddr,sizeof(EmpiricalSocketAddr));
        int v6FlagValue = 0;
        switch (remoteAddr->family){
        case EMPIRICAL_SOCKET_ADDR_V4:
          sprintf(addressAsString,"%d.%d.%d.%d",(int)dataElement[0x20],(int)dataElement[0x21],(int)dataElement[0x22],(int)dataElement[0x23]);
          break;
        case EMPIRICAL_SOCKET_ADDR_V6:
          v6FlagValue = TN3270_HAS_V6_ADDRESS;
          if (remoteAddr->v6Parts[5] == EMPIRICAL_SOCKET_PSEUDO_V4_MARK){
            sprintf(addressAsString,"%d.%d.%d.%d",
                    (((int)remoteAddr->v6Parts[6])>>8)&0xff,
                    ((int)remoteAddr->v6Parts[6])&0xff,
                    (((int)remoteAddr->v6Parts[7])>>8)&0xff,
                    ((int)remoteAddr->v6Parts[7])&0xff);
          } else{
            sprintf(addressAsString,"%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
                    (int)remoteAddr->v6Parts[0],(int)remoteAddr->v6Parts[1],
                    (int)remoteAddr->v6Parts[2],(int)remoteAddr->v6Parts[3],
                    (int)remoteAddr->v6Parts[4],(int)remoteAddr->v6Parts[5],
                    (int)remoteAddr->v6Parts[6],(int)remoteAddr->v6Parts[7]);
          }
          break;
        }

        int filtering = FALSE;
        switch (sessionKeyType){
        case SESSION_KEY_TYPE_ALL:
          /* No Filtering */
          break;
        case SESSION_KEY_TYPE_LUNAME:
          {
            int matchLen = strlen(stringKeyValue);         
            zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "LU Match case %s, len=%d\n",stringKeyValue,matchLen);
            if (matchLen > 8){
              filtering = TRUE; /* cannot match */
            } else{
              char matchString[12];
              memset(matchString,' ',8);
              matchString[8] = 0;
              memcpy(matchString,stringKeyValue,matchLen);
              zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "matchString '%s'\n",matchString);
              if (memcmp(cnxn->NWMConnLuName,matchString,8)){
                zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "diffs!\n");
                filtering = TRUE;
              }
            }
          }
          break;
        case SESSION_KEY_TYPE_IP4_NUMERIC:
          {
            int v4Value = *((int*)(dataElement+4));
            if (v4Value != numericKeyValue){
              filtering = TRUE;
            }
          }
        break;
        case SESSION_KEY_TYPE_IP4_STRING:
        case SESSION_KEY_TYPE_IP6_STRING:
          {
            zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "Sesssion remote address is %s\n",addressAsString);
            
            if (strcmp(stringKeyValue,addressAsString)){
              filtering = TRUE;
            }
          }
          break;
        default:
          zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "unsupported SESSION_KEY_TYPE for tn3270 session filtering, %d\n",sessionKeyType);
          break;
        }
        zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "fitlering=%d connProto=%d\n",filtering,cnxn->NWMConnProto);
        if (!filtering && ((cnxn->NWMConnProto == NWMConnPROTO_TN3270E) ||
                           (cnxn->NWMConnProto == NWMConnPROTO_TN3270))){
          if (isTSOAppl(&cnxn->NWMConnTargetAppl[0])){
            zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "SESSION IS: TSO/ISPF\n");
            TN3270Info *info = addTN3270Info(context,cnxn,TN3270_IS_TSO|v6FlagValue,addressAsString);
            TSBInfo *tsbInfo = htGet(tsbTable,cnxn->NWMConnLuName);
            if (tsbInfo){
              ASCB *ascb = tsbInfo->ascb;
              zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "ASCB for TSO is 0x%x, asid=0x%04X\n",ascb,ascb->ascbasid);
              ISPFTLD *tld = findISPFAnchor(context,ascb);
              zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "tld found 0x%x\n",tld);
              if (tld){
                info->flags |= TN3270_IS_ISPF;
                memcpy(info->ispfPanelid,tld->panelid,8);
              }
            }
          } else{
            zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "SESSION IS: OMEGAMON or some other VTAM APPL\n");
            TN3270Info *info = addTN3270Info(context,cnxn,v6FlagValue,addressAsString);
          }
        }
        dataElement += elementLength;
        
      }  
      break;   /* if we ever get a good result, leave the while loop */
    }
  }
  return 0;
}

/************************** ZOS Model Maintenance ********************************/

static SoftwareInstance *addSTC(DiscoveryContext *context, ZOSModel *model, uint64 softwareType, uint64 softwareFlags, ASCB *ascb){
  SoftwareInstance *info = addSoftware(context,softwareType,(uint64)SOFTWARE_FLAGS_MONITORING|softwareFlags,NULL,NULL);
  char *jobname = copyASCBJobname(context,ascb);
  info->bestName = jobname;
  info->jobname = jobname;
  ASCB *copyOfASCB = (ASCB*)discoveryCopyInternal(context, ascb, sizeof(ASCB));
  info->ascb = ascb;
  return info;
}

static void gatherStartedTasks(DiscoveryContext *context, ZOSModel *model){
  CVT *cvt = getCVT();
  ASVT *asvt = (ASVT*)cvt->cvtasvt;
  
  StartedTaskVisitor *userVisitor = model ? model->startedTaskVisitor : NULL;
  void* userVisitorData = model ? model->visitorsData : NULL;

  ASCB *ascb = (ASCB*)INT2PTR(asvt->asvtenty);
  while (ascb){
    char *jobname = getASCBJobname(ascb);
    char *jobnameCopy = SLHAlloc(context->slh,12);
    memset(jobnameCopy,0,12);
    memcpy(jobnameCopy,jobname,8);
    if ((ascb->ascbjbni == 0) &&
        (ascb->ascbtsb  == 0)){
      if (ascb->ascbjbns == 0){
        zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "skipping bogus ASCB found no JBNS, JBNI or TSB for ASID=0x%04x\n",ascb->ascbasid);
        ascb = (ASCB*)ascb->ascbfwdp;
        continue;
      }
      zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "______________________________________________________________________________________________________\n");
      zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG, "STC ASID=0x%04x JOBNAME=%8.8s  jbns=0x%x jbni=0x%x tsb=0x%x\n",
               (int)(ascb->ascbasid),(jobname ? jobname : "NOJOBNAM"),
               ascb->ascbjbns,
               ascb->ascbjbni,
               ascb->ascbtsb);
      
      addSTC(context,model,0,0,ascb);

      if (userVisitor != NULL) {
        userVisitor(context, ascb, userVisitorData);
      }

    }
    ascb = (ASCB*)ascb->ascbfwdp;
  }

}

static void scanTSBs(ZOSModel *model){
  ShortLivedHeap *oldSLH = model->tsbScanSLH;
  hashtable *oldSIBTable = model->tsbTable;
  model->tsbScanSLH      = makeShortLivedHeap(262144,400);    /* 256K chunk size */
  DiscoveryContext *context = makeDiscoveryContext(model->tsbScanSLH,model);
  context->vtamTraceLevel = 1;

  hashtable *tsbTable = htCreate(857,string8Hash,string8Compare,NULL,NULL);

  CVT *cvt = getCVT();
  IKTTCAST *tcas = (IKTTCAST*)(cvt->cvttcasp);
  IKTTCAST *tcasCopy = discoveryAllocCopyFromKey(context,tcas,IKTTCAST);
  
  IKJTSB *tsb = tcasCopy->tcastsb;
  while (tsb){
    IKJTSB *tsbCopy = discoveryAllocCopyFromKey(context,tsb,IKJTSB);
    if (context->vtamTraceLevel >= 1){
      zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "TSB at 0x%x\n");
      zowedump(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, (char*)tsbCopy,sizeof(IKJTSB));
    }
    if (tsbCopy->tsbextnt){
      IKTTSBX *tsbxCopy = discoveryAllocCopyFromKey(context,tsbCopy->tsbextnt,IKTTSBX);
      if (context->vtamTraceLevel >= 1){
        zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "TSBX at 0x%x\n",tsbCopy->tsbextnt);
        zowedump(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2,(char*)tsbxCopy,sizeof(IKTTSBX));
      }
      TSBInfo *tsbInfo = (TSBInfo*)SLHAlloc(model->tsbScanSLH,sizeof(TSBInfo));
      tsbInfo->ascb = (ASCB*)INT2PTR(tsbCopy->tcbstatAndASCB & 0x00FFFFFF);
      tsbInfo->tsb = tsbCopy;
      tsbInfo->tsbx = tsbxCopy;
      char     *luname = &(tsbCopy->tsbtrmid[0]);
      htPut(tsbTable,luname,tsbInfo);

      tsb = (IKJTSB*)INT2PTR(tsbxCopy->flagAndFwdPointer&0xFFFFFF);
    } else{
      tsb = NULL;
    }
  }

  model->tsbTable = tsbTable;

  if (oldSIBTable){
    htDestroy(oldSIBTable);
  }
  if (oldSLH){
    SLHFree(oldSLH);
  }
  
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "Free sib discoveryContext\n");
}

static void scanSlowSoftware(ZOSModel *model){
  ShortLivedHeap *oldSLH = model->slowScanSLH;
  SoftwareInstance *oldInstanceList = model->softwareInstances;
  model->slowScanSLH        = makeShortLivedHeap(262144,400);    /* 256K chunk size */
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "SLH MGMT: make for scan 0x%x\n",model->slowScanSLH);
  DiscoveryContext *context = makeDiscoveryContext(model->slowScanSLH,model);

  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "first gather ITM WLM info, because it depends on nothing else\n");
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "about to scan started tasks\n");
  gatherStartedTasks(context,model);

  context->imsTraceLevel = 0;
  context->cicsTraceLevel = 0;
  context->db2TraceLevel = 0;
  context->model = model;
  discoverSubsystems(context,SOFTWARE_TYPE_ALL,NULL);
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "after discoverySubsystems() in slowScan\n");
  fflush(stdout);

  /* move (not copy) the discovered systems to model */
  model->imsSubsystems = context->imsSubsystems;
  model->mqSubsystems = context->mqSubsystems;
  model->cicsSubsystems = context->cicsSubsystems;
  model->db2Subsystems = context->db2Subsystems;
  model->wasSubsystems = context->wasSubsystems;
  model->softwareInstances = context->softwareInstances;

  if (oldInstanceList){
    /* kill off the detailSLH of each */
  }
  if (oldSLH){
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "SLH MGMT: free for scan 0x%x\n",oldSLH);
    SLHFree(oldSLH);
  }
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "Free scan discoveryContext\n");
}

SoftwareInstance *zModelGetSoftware(ZOSModel *model){
  uint64 now = 0;
  getSTCKU(&now);
  if ((model->softwareInstances == NULL) ||
      ((now - model->lastSlowScan) > model->slowScanExpiry)){
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "Rescan slow software info\n");
    scanSlowSoftware(model);
    model->lastSlowScan = now;
  }
  return model->softwareInstances;
}

hashtable *zModelGetTSBs(ZOSModel *model){
  uint64 now = 0;
  getSTCKU(&now);
  if ((model->tsbTable == NULL) ||
      ((now - model->lastTSBScan) > model->tsbScanExpiry)){
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "Rescan tsb info\n");
    scanTSBs(model);
    model->lastTSBScan = now;
  }
  return model->tsbTable;
}

/*
  This is a singleton for the AddressSpace 
 */

ZOSModel *makeZOSModel(CrossMemoryServerName *privilegedServerName) {

  return makeZOSModel2(privilegedServerName, NULL, NULL, NULL);

}

ZOSModel *makeZOSModel2(CrossMemoryServerName *privilegedServerName,
                        SubsystemVisitor *subsystemVisitor,
                        StartedTaskVisitor *stcVisitor,
                        void *visitorsData) {
  ZOSModel *model = (ZOSModel*)safeMalloc(sizeof(ZOSModel),"ZOSModel");
  memset(model,0,sizeof(ZOSModel));

  model->slowScanExpiry = DEFAULT_SSCT_INTERVAL;
  if (privilegedServerName != NULL) {
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG,
            "makeZOSModel case 1 %p\n", privilegedServerName);
    model->privilegedServerName = *privilegedServerName;
    dumpbuffer((char*)&(model->privilegedServerName),16);
  } else  {
    model->privilegedServerName = zisGetDefaultServerName();
  }

  model->subsystemVisitor = subsystemVisitor;
  model->startedTaskVisitor = stcVisitor;
  model->visitorsData = visitorsData;

  return model;
}



/***************** END ZOS Model Maintenance *************************************/


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

