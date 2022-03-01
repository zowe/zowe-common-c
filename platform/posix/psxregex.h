#ifndef __ZOWE_PSXREGEX__
#define __ZOWE_PSXREGEX__

#include <regex.h>

regex_t *regexAlloc();
void regexFree(regex_t *m);

#define regexComp regcomp
#define regexExec regexec

#endif
