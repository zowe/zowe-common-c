#ifdef METTLE 
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>  
#include <metal/stdbool.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>  
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#define _LARGE_TIME_API 
#include <sys/ps.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "zos.h"
#include "ssi.h"

/*

  This is a simple test for calling ssi interfaces.
  
  Here's a compilation line for compiling with an LE 64 target:

  xlc -q64 -DHAVE_PRINTF=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DSUBPOOL=132 "-Wc,float(hex),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -I ../h -I -o ssitest1 ssitest1.c ../c/ssi.c ../c/zos.c ../c/timeutls.c ../c/utils.c ../c/alloc.c

  */

static int ssviTest(char *subsystemName){
  int ssviLength = 1024;
  IEFSSOBH *__ptr32 ssob = (IEFSSOBH *__ptr32)safeMalloc31(sizeof(IEFSSOBH),"SSOB");
  IEFJSSIB *__ptr32 ssib = (IEFJSSIB *__ptr32)safeMalloc31(sizeof(IEFJSSIB),"SSIB");
  SSVI *__ptr32 ssvi = (SSVI *__ptr32)safeMalloc31(ssviLength,"SSVI");
  memset(ssob,0,sizeof(IEFSSOBH));
  memset(ssib,0,sizeof(IEFJSSIB));
  memset(ssvi,0,ssviLength);
  ssvi->ssvilen = ssviLength;
  memcpy(ssvi->ssviid,"SSVI",4);
  initSSIB(ssib);
  memcpy(ssib->ssibssnm,subsystemName,4);
  initSSOB(ssob,54,ssib,ssvi);
  memcpy(ssob->ssobid,"SSOB",4);
  printf("SSOB\n");
  dumpbuffer((char*)ssob,sizeof(IEFSSOBH));
  printf("SSIB\n");
  dumpbuffer((char*)ssib,sizeof(IEFJSSIB));
  fflush(stdout);

  int status = callSSI(ssob);
  printf("SSI test=%d, ssobRET=0x%x\n",status,ssob->ssobretn);
  if (status == 0){
    printf("SSVI output??\n");
    dumpbuffer((char*)ssvi,0x100);
  } else if (status == 4){
    printf("SSVI function not supported for this subsystem (ret==4)\n");
  } else if (status == 8){
    printf("Subsystem not active (ret==8)\n");
  } else{
    printf("Failed\n");
    printf("SSOB\n");
    dumpbuffer((char*)ssob,sizeof(IEFSSOBH));
    printf("SSIB\n");
    dumpbuffer((char*)ssib,sizeof(IEFJSSIB));
    fflush(stdout);
  }
  return status;
}

static int ssvsTest(char *subsystemName){
  IEFSSOBH *__ptr32 ssob = (IEFSSOBH *__ptr32)safeMalloc31(sizeof(IEFSSOBH),"SSOB");
  IEFJSSIB *__ptr32 ssib = (IEFJSSIB *__ptr32)safeMalloc31(sizeof(IEFJSSIB),"SSIB");
  SSVS *__ptr32 ssvs = (SSVS *__ptr32)safeMalloc31(sizeof(SSVS),"SSVS");
  memset(ssvs,0,sizeof(SSVS));
  ssvs->ssvslen = sizeof(SSVS);

  initSSIB(ssib);
  memcpy(ssib->ssibssnm,"MSTR",4);
  memset(ssib->ssibjbid,' ',8);
  memcpy(ssib->ssibjbid,subsystemName,strlen(subsystemName));

  initSSOB(ssob,15,ssib,ssvs);
  printf("SSOB\n");
  dumpbuffer((char*)ssob,sizeof(IEFSSOBH));
  printf("SSIB\n");
  dumpbuffer((char*)ssib,sizeof(IEFJSSIB));
  fflush(stdout);

  int status = callSSI(ssob);
  printf("SSI test=%d, ssobRET=0x%x\n",status,ssob->ssobretn);
  if (status == 0){
    printf("SSVS output??\n");
    dumpbuffer((char*)ssvs,0x100);
  } else if (status == 4){
    printf("SSVI function not supported for this subsystem (ret==4)\n");
  } else if (status == 8){
    printf("Subsystem not active (ret==8)\n");
  } else{
    printf("Failed\n");
    printf("SSOB\n");
    dumpbuffer((char*)ssob,sizeof(IEFSSOBH));
    printf("SSIB\n");
    dumpbuffer((char*)ssib,sizeof(IEFJSSIB));
    fflush(stdout);
  }
  return status;
}


static int ssjpTest(){
  IEFSSOBH *__ptr32 ssob = (IEFSSOBH *__ptr32)safeMalloc31(sizeof(IEFSSOBH),"SSOB");
  IEFJSSIB *__ptr32 ssib = getJobSSIB();
  /* IEFJSSIB *__ptr32 ssib = (IEFJSSIB *__ptr32)safeMalloc31(sizeof(IEFJSSIB),"SSIB"); */
  IAZSSJP  *__ptr32 ssjp = (IAZSSJP  *__ptr32)safeMalloc31(sizeof(IAZSSJP),"SSJP");
  IAZJPROC *__ptr32 jproc= (IAZJPROC *__ptr32)safeMalloc31(sizeof(IAZJPROC),"JPROC");

  /* This is the initialization of the specific data area for PROCLIB Queries */

  memset(jproc,0,sizeof(IAZJPROC));
  memcpy(jproc->jprcid,"JESPROCI",8);
  jproc->jprclen = sizeof(IAZJPROC);
  jproc->jprcverl = 1;
  jproc->jprcverm = 0;
  /* here friday */
  
  initSSJP(ssjp,SSJPPROD,jproc); /* SSJPPRRS to release storage later */

  initSSOB(ssob,82,ssib,ssjp);
  memcpy(ssob->ssobid,"SSOB",4);
  printf("SSOB\n");
  dumpbuffer((char*)ssob,sizeof(IEFSSOBH));
  printf("SSIB\n");
  dumpbuffer((char*)ssib,sizeof(IEFJSSIB));
  fflush(stdout);

  int status = callSSI(ssob);
  printf("SSI test=%d, ssobRET=0x%x\n",status,ssob->ssobretn);
  if (status == 0){
    printf("SSJP output\n");
    dumpbuffer((char*)ssjp,0x100);
    printf("JPROC output\n");
    dumpbuffer((char*)jproc,sizeof(IAZJPROC));
    JPRHDR *__ptr32 jprhdr = (JPRHDR *__ptr32)jproc->jprclptr;
    while (jprhdr){
      printf("JPRHDR at = 0x%p\n",jprhdr);
      dumpbuffer((char*)jprhdr,sizeof(JPRHDR));
      JPRPREF *prefix = (JPRPREF*)(((char*)jprhdr)+jprhdr->jproprf);
      printf("Prefix at 0x%p\n",prefix);
      dumpbuffer((char*)prefix,prefix->jprprlen);
      jprhdr = jprhdr->jprnxtp;
    }
    /* Re-use same data structures, but tell JES to reclaim temp storage */
    initSSJP(ssjp,SSJPPRRS,jproc);
    status = callSSI(ssob);
    printf("SSJP Storage Release s test=%d, ssobRET=0x%x\n",status,ssob->ssobretn);
  } else if (status == 4){
    printf("JES Properties function not supported for this subsystem (ret==4)\n");
  } else if (status == 8){
    printf("Subsystem not active (ret==8)\n");
  } else{
    printf("Failed\n");
    printf("SSOB\n");
    dumpbuffer((char*)ssob,sizeof(IEFSSOBH));
    printf("SSIB\n");
    dumpbuffer((char*)ssib,sizeof(IEFJSSIB));
    fflush(stdout);
  }
  return status;
}


static int ssviTestAll(){
  CVT *theCVT = getCVT();
  JESCT *theJESCT = (JESCT*)(theCVT->cvtjesct);
  SSCT *ssctChain = theJESCT->jesssct;
  int subsystemCount = 0;
  
  do{    
    ssctChain = ssctChain->scta;
    printf("________ SUBSYSTEM _________________________________________________\n");
    dumpbuffer((char*)ssctChain,sizeof(SSCT));
    if (TRUE){ /* !memcmp(ssctChain->sname,"MSTR",4)){ */
      ssviTest(ssctChain->sname);
    }
    subsystemCount++;
  } while (ssctChain != NULL);

  printf("Subsystem Count %d\n",subsystemCount);
  return 0;
}

int main(int argc, char **argv){
  char *command = argv[1];
  if (!strcmp(command,"ssvi")){
    ssviTestAll();
  } else if (!strcmp(command,"ssvs")){
    ssvsTest("HASP");
  } else if (!strcmp(command,"ssjp")){
    ssjpTest();
  }
  return 0;
}
