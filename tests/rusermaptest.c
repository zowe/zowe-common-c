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
#include <metal/stdarg.h>  
#include "metalio.h"

#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#endif /* METTLE */

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "unixfile.h"
#include "rusermap.h"


/*

  Testing on ZOS:  (This program can *ONLY* run on ZOS).

  64-bit build

  xlclang -q64 -I ../h -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DSUBPOOL=132 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -o rusermaptest rusermaptest.c ../c/rusermap.c ../c/charsets.c ../c/zosfile.c  ../c/timeutls.c ../c/utils.c ../c/alloc.c ../c/logging.c ../c/collections.c ../c/le.c ../c/recovery.c ../c/zos.c ../c/scheduling.c

  31-bit build

  xlc -I ../h -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DSUBPOOL=132 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -o rusermaptest31 rusermaptest.c ../c/rusermap.c ../c/charsets.c ../c/zosfile.c  ../c/timeutls.c ../c/utils.c ../c/alloc.c ../c/logging.c ../c/collections.c ../c/le.c ../c/recovery.c ../c/zos.c ../c/scheduling.c


  Configuring the user to have a certificate:

  Note for the following call the cert should be B64:

  tsocmd "racdcert add('{{ zowe_dataset_prefix }}.CERT.{{ dataset }}') id({{ zowe_runtime_user }}) withlabel('{{ label }}') trust" 
  tsocmd "SETROPTS RACLIST(DIGTCERT, DIGTRING) REFRESH"

*/

int main(int argc, char **argv){
  if (argc < 2){
    printf("must supply a command of either 'cert' or 'dn'\n");
  }
  char *command = argv[1];
  char userid[9];
  int retCode = 0;
  int reason = 0;

  

  if (!strcmp(command,"cert")){
    FILE *ptr;
    ptr = fopen("apimtst.der","rb");
    printf("pointer=%p\n",ptr);
    fflush(stdout);
    char buffer[4096];
    memset(buffer,0,4096);
    int bytesRead = fread(buffer,1,4096,ptr);
    printf("bytesRead=%d\n",bytesRead);
    fflush(stdout);
    fclose( ptr );
    dumpbuffer(buffer,bytesRead);
    int rc = getUseridByCertificate(buffer, bytesRead, userid, &retCode, &reason);
    if (rc){
      printf("Could not get userid, racfFC=0x%x, reason=0x%x\n",retCode,reason);
    } else {
      printf("Userid: %s\n",userid);
    }
  } else if (!strcmp(command,"dn")){
    char *fakeDN = "FooBarBaz";
    char *fakeRegistry = "zowe.org";
    int rc = getUseridByDN(fakeDN, strlen(fakeDN), fakeRegistry, strlen(fakeRegistry), userid, &retCode, &reason);
    if (rc){
      printf("Could not get userid, racfFC=0x%x, reason=0x%x\n",retCode,reason);
    } else {
      printf("Userid: %s\n",userid);
    }
  } else{
    printf("unknown command '%s'\n",command);
  }

  return 0;
}
