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

typedef struct EJSNativeInvocation_tag EJSNativeInvocation;

struct JSValueBox_tag;

typedef struct JSValueBox_tag JSValueBox;

JSValueBox ejsEvalBuffer(EmbeddedJS *ejs,
                         const void *buffer, int bufferLength,
                         const char *filename, int eval_flags,
                         int *statusPtr);

#define EJS_LOAD_DETECT_MODULE -1
#define EJS_LOAD_NOT_MODULE 0
#define EJS_LOAD_IS_MODULE 1

int ejsEvalFile(EmbeddedJS *ejs, const char *filename, int loadMode);

void ejsFreeJSValue(EmbeddedJS *ejs, JSValueBox valueBox);

#define EJS_NATIVE_TYPE_INT32 1
#define EJS_NATIVE_TYPE_CONST_STRING 2 /* no copying this argument allowed */
#define EJS_NATIVE_TYPE_STRING 3
#define EJS_NATIVE_TYPE_JSON 4
#define EJS_NATIVE_TYPE_VOID 5

typedef struct EJSNativeArgument_tag {
  char *name;
  int type;
} EJSNativeArgument;

typedef struct EJSNativeMethod_tag {
  char *name;
  char *asciiName;
  struct EJSNativeClass_tag *clazz; /* C++ reserved word justifiable paranoia */
  void *functionPointer;
  ArrayList arguments;
  int returnType;
} EJSNativeMethod;

typedef struct EJSNativeClass_tag {
  char *name;
  char *asciiName;
  struct EJS_NativeModule_tag *module;
  ArrayList instanceMethodFunctions;
  ArrayList staticMethodFunctions;
  void *(*nativeConstructor)(void *userData, EJSNativeInvocation *invocation);
  void (*nativeFinalizer)(EmbeddedJS *ejs, void *userData, void *nativePointer);
  void *userData;
} EJSNativeClass;

typedef struct EJSNativeModule_tag {
  char *name;
  ArrayList classes;  
} EJSNativeModule;

EJSNativeModule *ejsMakeNativeModule(EmbeddedJS *ejs, char *moduleName);

EJSNativeClass *ejsMakeNativeClass(EmbeddedJS *ejs, EJSNativeModule *module,
                                   char *className,
                                   void *(*nativeConstructor)(void *userData, EJSNativeInvocation *invocation),
                                   void (*nativeFinalizer)(EmbeddedJS *ejs, void *userData, void *nativePointer),
                                   void *userData);

EJSNativeMethod *ejsMakeNativeMethod(EmbeddedJS *ejs, EJSNativeClass *clazz, char *methodName,
                                     int returnType,
                                     void *functionPointer);

void ejsAddMethodArg(EmbeddedJS *ejs, EJSNativeMethod *method, char *name, int type);

#define EJS_OK 0
#define EJS_INDEX_OUT_OF_RANGE 12
#define EJS_TYPE_MISMATCH 16

int ejsIntArg(EJSNativeInvocation *invocation,
              int argIndex,
              int *valuePtr);

int ejsStringArg(EJSNativeInvocation *invocation,
                 int argIndex,
                 const char **valuePtr);

int ejsJsonArg(EJSNativeInvocation *invocation,
               int argIndex,
               Json **valuePtr);
int ejsReturnInt(EJSNativeInvocation *invocation,
                 int value);
int ejsReturnString(EJSNativeInvocation *invocation,
                    const char *value,
                    bool needsFree);
int ejsReturnJson(EJSNativeInvocation *invocation,
                  Json *value);


/* usually pass NULL as arg unless trying to build complex embedded JS 
   application that can run multiple threads or other multitasking.

   
*/
EmbeddedJS *allocateEmbeddedJS(EmbeddedJS *sharedRuntimeEJS); /* can be NULL */
bool configureEmbeddedJS(EmbeddedJS *ejs,
                         EJSNativeModule **nativeModules, int nativeModuleCount);


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
