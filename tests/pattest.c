#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"

#ifdef __ZOWE_OS_WINDOWS
#include "winregex.h"
#else
#include "psxregex.h"
#endif

static char *httpPattern = "^(https?://[^/]+)/([^#]*)(#.*)?$";
/* static char *httpPattern = "^https?://([^/]+)/([^#]*)(#.*)?$"; */
static char *filePattern = "^/([^#]*)(#.*)?$";
static int regexError = 0;
static regex_t *regex = NULL;


static regex_t *compileRegex(char *pattern, int *regexStatus){
  regex_t *rx = regexAlloc();
  int compStatus = regexComp(rx,pattern,REG_EXTENDED);
  if (compStatus){
    regexFree(rx);
    *regexStatus = compStatus;
    printf("*** WARNING *** pattern '%s' failed to compile with status %d\n",
           pattern,compStatus);
    return NULL;
  } else {
    *regexStatus = 0;
    return rx;
  }
}

/*
    clang++ -c ../platform/windows/cppregex.cpp ../platform/windows/winregex.cpp

    clang -I%YAML%/include -I./src -I../h -I ../platform/windows -Dstrdup=_strdup -D_CRT_SECURE_NO_WARNINGS -DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 -DYAML_VERSION_STRING=\"0.2.5\" -DYAML_DECLARE_STATIC=1 -o pattest.exe pattest.c ../c/timeutls.c ../c/utils.c ../c/alloc.c cppregex.o winregex.o

 */

#define MAX_MATCHES 10

int main(int argc, char **argv){
  char *patName = argv[1];
  char *s = argv[2];
  char *pattern = NULL;
  
  if (!strcmp(patName,"http")){
    pattern = httpPattern;
  } else if (!strcmp(patName,"file")){
    pattern = filePattern;
  } else {
    printf("no pattern for '%s'\n",patName);
    return 0;
  }

  regex = compileRegex(pattern,&regexError);
  if (regexError){
    printf("bad pattern error=%d\n",regexError);
    return 0;
  } else {
    regmatch_t matches[MAX_MATCHES];
    memset(matches,0,MAX_MATCHES*sizeof(regmatch_t));
    int regexStatus = regexExec(regex,s,MAX_MATCHES,matches,0);
    printf("regexStatus = %d\n",regexStatus);
    /* write out bindings here */
    if (regexStatus == 0){
      for (int i=0; i<MAX_MATCHES; i++){
        regmatch_t m = matches[i];
        printf("  %d: %2d to %d\n",i,(int)m.rm_so,(int)m.rm_eo);
      }
    }
  }
}
