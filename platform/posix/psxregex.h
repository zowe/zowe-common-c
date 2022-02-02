#ifndef __ZOWE_PSxREGEX__
#define __ZOWE_PS

#include <regex.h>

regex_t *regexAlloc();
void regexFree(regex_t *m);

#define regexComp regcomp
#define regexExec regexec

#endif
