

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/stdio.h>
#include <metal/string.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "zos.h"
#include "ssi.h"

#ifdef _LP64 
#define SAM31_IF_NEEDED " SAM31 \n"
#else
#define SAM31_IF_NEEDED ""
#endif

#ifdef _LP64 
#define SAM64_IF_NEEDED " SAM64 \n"
#else
#define SAM64_IF_NEEDED ""
#endif

void initSSIB(IEFJSSIB *__ptr32 ssib){
  memset(ssib,0,sizeof(IEFJSSIB));
  memcpy(ssib->ssibid,"SSIB",4);
  ssib->ssiblen = sizeof(IEFJSSIB);
}

IEFJSSIB *__ptr32 getJobSSIB(){
  IEFJSSIB *__ptr32 ssib = NULL;
  TCB *tcb = getTCB();
  JSCB *jscb = tcb->tcbjscb;
  JSCB *activeJSCB = jscb->jscbact;
  return activeJSCB->jscbssib;
}

void initSSOB(IEFSSOBH *__ptr32 ssob, 
	      int functionCode,
	      IEFJSSIB *__ptr32 ssib,
	      Addr31 functionDataBlock){
  memset(ssob,0,sizeof(IEFSSOBH));
  memcpy(ssob->ssobid,"SSOB",4);
  ssob->ssobfunc = functionCode;
  ssob->ssoblen  = sizeof(IEFSSOBH);
  ssob->ssobssib = ssib;
  ssob->ssobindv = functionDataBlock;
}

void initSSJP(IAZSSJP *__ptr32 ssjp, char requestType, Addr31 specificBlock){
  memset(ssjp,0,sizeof(IAZSSJP));
  memcpy(ssjp->ssjpid,"SSJP",4);
  ssjp->ssjplen = sizeof(IAZSSJP);
  ssjp->ssjpuser = specificBlock;
  ssjp->ssjpfreq = requestType;
  ssjp->ssjpver = 1;
}


int callSSI(IEFSSOBH *__ptr32 ssob){
  /* Register 1 is address of SSOB 31 bit pointer with high bit set 
     Register 13 needs an 18-word save area in 31 bit storage
     */

  int returnCode = 0;
  int  r13;
  printf("callSSI\n");
  fflush(stdout);
#ifdef _LP64
  char *data31 = (char *__ptr32)safeMalloc31(72+4,"CallSSI");
#else
  char data31[72+4];
#endif
  char *__ptr32 parmlist = (char *__ptr32)&data31[0];
  printf("data31 0x%p parmlist 0x%p\n",data31,parmlist);
  fflush(stdout);
  char *__ptr32 saveArea = (char *__ptr32)&data31[4];
  *((int*)parmlist) = ((int)ssob)|0x80000000;
  memset(saveArea,0,72);
  __asm(" ST 13,%0 " : :"m"(r13):);
  ((int*)saveArea)[1] = r13; /* back chain */
  __asm(ASM_PREFIX
	" LLGF 1,%0 \n"
	" L    13,%1 \n"
	" LLGT 15,16(0,0) \n"
	" L    15,296(0,15) \n" /* This is the JESCT */
	" L    15,20(0,15)  \n" /* This is IEFSSREQ */
	SAM31_IF_NEEDED
	/* " DC   XL2'0000' \n" */
	" BASR 14,15 \n"
	SAM64_IF_NEEDED 
	" ST 15,%2"
	:
	:"m"(parmlist),"m"(saveArea),"m"(returnCode)
	:"r15");
  printf("back from IEFSSEQ\n");
  fflush(stdout);
  return returnCode;
}



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
