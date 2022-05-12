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


/*
  clang -I../h -I../platform/windows -D_CRT_SECURE_NO_WARNINGS -Dstrdup=_strdup -DYAML_DECLARE_STATIC=1 -Wdeprecated-declarations --rtlib=compiler-rt -o convtest.exe convtest.c ../c/charsets.c ../platform/windows/winfile.c ../c/timeutls.c ../c/utils.c ../c/alloc.c

   Windows doc 

   int MultiByteToWideChar(
   _In_      UINT   CodePage,          // expected
   _In_      DWORD  dwFlags,           // composite|precomposed|failOnInputError
   _In_      LPCSTR lpMultiByteStr,    // inputString
   _In_      int    cbMultiByte,       // length of inputString or -1 if nullTerminated and feeling adventurous
   _Out_opt_ LPWSTR lpWideCharStr,     // receivingBuffer (or NULL for brave souls who want Windows to allocate)
   _In_      int    cchWideChar        // length in WideChars of output
  );

  IBM resources
    http://www-01.ibm.com/software/globalization/ccsid/ccsid_registered.html


  Testing on Z:

  xlclang -q64 -I ../h -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DSUBPOOL=132 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" -o convtest convtest.c ../c/charsets.c ../c/zosfile.c  ../c/timeutls.c ../c/utils.c ../c/alloc.c ../c/logging.c ../c/collections.c ../c/le.c ../c/recovery.c ../c/zos.c ../c/scheduling.c

*/


int main(int argc, char **argv){
  char *oldFile = argv[1];
  int   oldCCSID = atoi(argv[2]);
  char *newFile = argv[3];
  int   newCCSID = atoi(argv[4]);

  int returnCode;
  int reasonCode;
  int status = fileCopyConverted(oldFile,newFile,oldCCSID,newCCSID,&returnCode,&reasonCode);
  printf("copy status=%d, ret=%d, reason=0x%x\n",status,returnCode,reasonCode);
  
  return 0;
}
