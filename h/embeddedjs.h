/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_EMBEDDEDJS__
#define __ZOWE_EMBEDDEDJS__ 1

#include "cutils.h"
#include "quickjs-libc.h"

#include "zowetypes.h"

struct trace_malloc_data {
    uint8_t *base;
};

#define FILE_LOAD_AUTODETECT -1
#define FILE_LOAD_GLOBAL      0 /* not module */
#define FILE_LOAD_MODULE      1 

typedef struct EmbeddedJS_tag {
  JSRuntime *rt;
  JSContext *ctx;
  struct trace_malloc_data trace_data; /*  = { NULL }; */
  int optind;
  char *expr; /*  = NULL; */
  int interactivel; /*  = 0; */
  int dump_memory; /* = 0; */
  int trace_memory; /* = 0;*/
  int empty_run; /* = 0; */
  int loadMode; /* FILE_LOAD_AUTODETECT */
  int load_std; /* = 0; */
  int dump_unhandled_promise_rejection; /*  = 0; */
  size_t memory_limit; /*  = 0; */
  char *include_list[32];
#ifdef CONFIG_BIGNUM
  int load_jscalc;
#endif
  size_t stack_size; /* = 0; */
} EmbeddedJS;

JSValue ejsEvalBuffer(EmbeddedJS *ejs,
                      const void *buffer, int bufferLength,
                      const char *filename, int eval_flags,
                      int *statusPtr);

int ejsEvalFile(EmbeddedJS *ejs, const char *filename, int loadMode);

void ejsFreeJSValue(EmbeddedJS *ejs, JSValue value);


/* usually pass NULL as arg unless trying to build complex embedded JS 
   application that can run multiple threads or other multitasking.
*/
EmbeddedJS *makeEmbeddedJS(EmbeddedJS *sharedRuntimeEJS);

Json *ejsJSToJson(EmbeddedJS *ejs, JSValue value, ShortLivedHeap *slh);
JSValue ejsJsonToJS(EmbeddedJS *ejs, Json *json);
int ejsSetGlobalProperty(EmbeddedJS *ejs, const char *propetyName, JSValue value);
Json *evaluateJsonTemplates(EmbeddedJS *ejs, ShortLivedHeap *slh, Json *json);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
