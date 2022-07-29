#ifndef __ZOWE_MICROJQ__
#define __ZOWE_MICROJQ__ 1

#define JQ_FLAG_RAW_STRINGS 0x0001
#define JQ_FLAG_PRINT_PRETTY 0x0002

#include "json.h"
#include "parsetools.h"

Json *parseJQ(JQTokenizer *jqt, ShortLivedHeap *slh, int traceLevel);
int evalJQ(Json *value, Json *jqTree, FILE *out, int flags, int traceLevel);

#endif
