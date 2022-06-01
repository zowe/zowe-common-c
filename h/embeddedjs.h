/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_EMBEDDEDJS__
#define __ZOWE_EMBEDDEDJS__ 1

#include "zowetypes.h"

struct EmbeddedJS_tag;

typedef struct EmbeddedJS_tag EmbeddedJS;

struct JSValueBox_tag;

typedef struct JSValueBox_tag JSValueBox;

JSValueBox ejsEvalBuffer(EmbeddedJS *ejs,
                         const void *buffer, int bufferLength,
                         const char *filename, int eval_flags,
                         int *statusPtr);

int ejsEvalFile(EmbeddedJS *ejs, const char *filename, int loadMode);

void ejsFreeJSValue(EmbeddedJS *ejs, JSValueBox valueBox);


/* usually pass NULL as arg unless trying to build complex embedded JS 
   application that can run multiple threads or other multitasking.
*/
EmbeddedJS *makeEmbeddedJS(EmbeddedJS *sharedEJS);

Json *ejsJSToJson(EmbeddedJS *ejs, JSValueBox value, ShortLivedHeap *slh);
JSValueBox ejsJsonToJS(EmbeddedJS *ejs, Json *json);
int ejsSetGlobalProperty(EmbeddedJS *ejs, const char *propetyName, JSValueBox valueBox);
Json *evaluateJsonTemplates(EmbeddedJS *ejs, ShortLivedHeap *slh, Json *json);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
