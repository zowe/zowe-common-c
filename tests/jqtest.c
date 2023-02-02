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
#include <metal/ctype.h>  
#include <metal/stdbool.h>  
#include "metalio.h"
#include "qsam.h"

#else

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>  
#include <sys/stat.h>
#include <sys/types.h> 
#include <stdint.h> 
#include <stdbool.h> 
#include <errno.h>
#include <setjmp.h>
#include <math.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "bpxnet.h"
#include "unixfile.h"
#include "json.h"
#include "parsetools.h"
#include "microjq.h"

/*
  _Compiling on Windows_

  clang -I../platform/windows -I ..\h -Wdeprecated-declarations -D_CRT_SECURE_NO_WARNINGS -o jqtest.exe jqtest.c ../c/microjq.c ../c/parsetools.c ../c/json.c ../c/xlate.c ../c/charsets.c ../c/winskt.c ../c/logging.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc.c

 */

static void pprintJSON(Json *tree){
#ifdef __ZOWE_OS_WINDOWS
  int stdoutFD = _fileno(stdout);
#else
  int stdoutFD = STDOUT_FILENO;
#endif
  if (tree){
    jsonPrinter *p = makeJsonPrinter(stdoutFD);
    jsonEnablePrettyPrint(p);
    printf("parse result as json\n");
    jsonPrint(p,tree);
  }
}

int main(int argc, char **argv){
  char *command = argv[1];
  char *filter = argv[2];
  char *filename = argv[3];
  JQTokenizer jqt;
  memset(&jqt,0,sizeof(JQTokenizer));
  jqt.data = argv[2];
  jqt.length = strlen(jqt.data);
  jqt.ccsid = 1208;
  ShortLivedHeap *slh = makeShortLivedHeap(0x10000, 100);
  if (!strcmp(command,"parse")){
    printf("parse...\n");
    Json *tree = parseJQ(&jqt,slh,1);
    if (tree){
      pprintJSON(tree);
    } else {
      printf("Parse failed\n");
    }
  } else if (!strcmp(command,"eval") || 
             !strcmp(command,"evalTrace") ||
             !strcmp(command,"evalCompact")){
    Json *jqTree = parseJQ(&jqt,slh,0);
    if (jqTree){
      int errorBufferSize = 1024;
      char *errorBuffer = safeMalloc(errorBufferSize,"ErrorBuffer");
      memset(errorBuffer,0,errorBufferSize);
      Json *json = jsonParseFile2(slh,filename,errorBuffer,errorBufferSize);
      if (json == NULL){
        printf("json parsed json=0x%p, with error '%s'\n",json,errorBuffer);
        return 8;
      }
      int traceLevel = 0;
      int flags = JQ_FLAG_PRINT_PRETTY;
      if (!strcmp(command,"evalTrace")){
        traceLevel = 1;
      }
      if (!strcmp(command,"evalRaw")){
        flags |= JQ_FLAG_RAW_STRINGS;
      }
      if (!strcmp(command,"evalCompact")){
        flags ^= JQ_FLAG_PRINT_PRETTY;
      }
      int evalStatus = evalJQ(json,jqTree,stdout,flags,traceLevel);
      if (evalStatus != 0){
        fprintf(stderr,"micro jq eval problem %d\n",evalStatus);
      }
    } else {
      printf("Parse failed\n");
    }
  } else {
    printf("bad command: %s\n",command);
  }
  return 0;
}
