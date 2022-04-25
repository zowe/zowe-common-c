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
#include <metal/stdbool.h>
#include <metal/string.h>
#include <metal/stdarg.h>
#include <metal/ctype.h>  
#include <metal/stdbool.h>  
#include "metalio.h"
#include "qsam.h"

#else

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>  
#include <sys/stat.h>
#include <sys/types.h> 
#include <stdint.h> 
#include <stdbool.h> 
#include <errno.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "bpxnet.h"
#include "unixfile.h"
#include "json.h"
#include "charsets.h"
#include "embeddedjs.h"

#include "cutils.h"
#include "quickjs-libc.h"

#ifdef __ZOWE_OS_WINDOWS
typedef int64_t ssize_t;
#endif

#ifdef __ZOWE_OS_ZOS

int __atoe_l(char *bufferptr, int leng);
int __etoa_l(char *bufferptr, int leng);

#endif

static int convertToNative(char *buf, size_t size) {
#ifdef __ZOWE_OS_ZOS
  return __atoe_l(buf, size);
#endif
  return 0;
}

static int convertFromNative(char *buf, size_t size) {
#ifdef __ZOWE_OS_ZOS
  return __etoa_l(buf, size);
#endif
  return 0;
}

struct trace_malloc_data {
    uint8_t *base;
};

#define FILE_LOAD_AUTODETECT -1
#define FILE_LOAD_GLOBAL      0 /* not module */
#define FILE_LOAD_MODULE      1 

struct EmbeddedJS_tag {
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
  LongHashtable *nativeModuleTable;
  LongHashtable *nativeClassTable;
#ifdef CONFIG_BIGNUM
  int load_jscalc;
#endif
  size_t stack_size; /* = 0; */
  void *userPointer;
  ShortLivedHeap *fileEvalHeap;
  int traceLevel;
};

struct JSValueBox_tag {
  JSValue value;
};

static JSValue ejsEvalBuffer1(EmbeddedJS *ejs,
                              const void *buffer, int bufferLength,
                              const char *filename, int eval_flags,
                              int *statusPtr){
  JSContext *ctx = ejs->ctx;
  JSValue val;
  int ret;
  
  if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
    /* for the modules, we compile then run to be able to set
       import.meta */
    val = JS_Eval(ctx, buffer, bufferLength, filename,
                  eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
    if (!JS_IsException(val)) {
      js_module_set_import_meta(ctx, val, TRUE, TRUE);
      val = JS_EvalFunction(ctx, val);
    }
  } else {
    val = JS_Eval(ctx, buffer, bufferLength, filename, eval_flags);
  }
  if (JS_IsException(val)) {
    JSValue exception = JS_GetException(ctx);
    const char *message = JS_ToCString(ctx, exception);
    printf("EJS exceptionMessage\n");
    dumpbuffer(message,strlen(message));
    JS_FreeCString(ctx,message);
    JS_FreeValue(ctx, exception);
    ret = -1;
  } else {
    ret = 0;
  }
  *statusPtr = ret;
  return val;
}

JSValueBox ejsEvalBuffer(EmbeddedJS *ejs,
                         const void *buffer, int bufferLength,
                         const char *filename, int eval_flags,
                         int *statusPtr){
  JSValue value = ejsEvalBuffer1(ejs,buffer,bufferLength,filename,eval_flags,statusPtr);
  return *(JSValueBox*)(&value);
}

void ejsFreeJSValue(EmbeddedJS *ejs, JSValueBox valueBox){
  JS_FreeValue(ejs->ctx, *((JSValue*)&valueBox));
}

static int ejsSetGlobalProperty_internal(EmbeddedJS *ejs, const char *propertyName, JSValue value){
  JSContext *ctx = ejs->ctx;
  JSValue theGlobal = JS_GetGlobalObject(ctx);

  return JS_SetPropertyStr(ctx,theGlobal,propertyName,value);
}

int ejsSetGlobalProperty(EmbeddedJS *ejs, const char *propertyName, JSValueBox valueBox){
  return ejsSetGlobalProperty_internal(ejs,propertyName,valueBox.value);
}

int ejsEvalFile(EmbeddedJS *ejs, const char *filename, int loadMode){
    uint8_t *buf;
    int ret, eval_flags;
    size_t bufferLength;
    if (ejs->fileEvalHeap){
      SLHFree(ejs->fileEvalHeap);
    }
    ejs->fileEvalHeap = makeShortLivedHeap(0x10000,0x100); /* hacky constants, I know */
    
    buf = js_load_file(ejs->ctx, &bufferLength, filename);
    if (!buf) {
        perror(filename);
        return errno;
    }

    /* Beware of overly-expedient 3-valued logic here! */
    if (loadMode < 0) {
        loadMode = (has_suffix(filename, ".mjs") ||
                  JS_DetectModule((const char *)buf, bufferLength));
    }
    if (loadMode)
        eval_flags = JS_EVAL_TYPE_MODULE;
    else
        eval_flags = JS_EVAL_TYPE_GLOBAL;
    JSValue evalResult = ejsEvalBuffer1(ejs, buf, bufferLength, filename, eval_flags, &ret);
    js_free(ejs->ctx, buf);
    JS_FreeValue(ejs->ctx, evalResult);
    return ret;
}

static JSClassID js_experiment_thingy_class_id;

typedef struct CThingy_tag {
  int a;
  int b;
} CThingy;

static void js_experiment_thingy_finalizer(JSRuntime *rt, JSValue val){
  /* JSSTDFile *s = JS_GetOpaque(val, js_std_file_class_id); */
  printf("Experiment Thingy Finalizer running\n");
}


static JSClassDef js_experiment_thingy_class = {
    "Thingy",
    .finalizer = js_experiment_thingy_finalizer,
};

/*
  #define JS_CFUNC_DEF(name, length, func1) 

  name is JS name

  #define JS_CFUNC_MAGIC_DEF(name, length, func1, magic)

*/

JSValue js_experiment_thingy_three(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv){
  return JS_NewInt32(ctx, 3);
}

static const JSCFunctionListEntry js_experiment_thingy_proto_funcs[] = {
  JS_CFUNC_DEF("three", 0, js_experiment_thingy_three ),
};

static void *unsafeGetOpaque(JSValueConst obj){
  JSObject *p;
  if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
    return NULL;
  p = JS_VALUE_GET_OBJ(obj);
  return NULL; /* p->u.opaque; */
}

static Json *ejsJSToJson_internal(EmbeddedJS *ejs, JSValue value, ShortLivedHeap *slh);
static JSValue ejsJsonToJS_internal(EmbeddedJS *ejs, Json *json);

typedef struct NativeClassInternals_tag {
  char *asciiName;
  EJSNativeClass *nativeClass;
  JSClassID classID;
  JSClassDef classDef;
} NativeClassInternals;

/* every one of these function pointer types takes the "this" pointer (a void *) first
   I - int
   S - read/write string
   C - const (read only) char string
   J - Json (always a fresh copy)
   V - void (return type only)
 */

typedef int (FP_I)(void*);
typedef int (FP_S)(void*);
typedef int (FP_J)(void*);
typedef int (FP_V)(void*);

/* one-arg functions */
typedef int (FP_I_I)(void *, int);
typedef int (FP_I_S)(void *, char *);
typedef int (FP_I_C)(void *, const char *);
typedef int (FP_I_J)(void *, Json *);

typedef int (FP_S_I)(void *, int);
typedef int (FP_S_S)(void *, char *);
typedef int (FP_S_C)(void *, const char *);
typedef int (FP_S_J)(void *, Json *);

typedef int (FP_J_I)(void *, int);
typedef int (FP_J_S)(void *, char *);
typedef int (FP_J_C)(void *, const char *);
typedef int (FP_J_J)(void *, Json *);

typedef int (FP_J_I)(void *, int);
typedef int (FP_J_S)(void *, char *);
typedef int (FP_J_C)(void *, const char *);
typedef int (FP_J_J)(void *, Json *);

/* 2-arg int-returning functions */
typedef int (FP_I_I_I)(void *, int, int);
typedef int (FP_I_I_S)(void *, int, char *);
typedef int (FP_I_I_C)(void *, int, const char *);
typedef int (FP_I_I_J)(void *, int, Json *);

typedef int (FP_I_S_I)(void *, int, int);
typedef int (FP_I_S_S)(void *, int, char *);
typedef int (FP_I_S_C)(void *, int, const char *);
typedef int (FP_I_S_J)(void *, int, Json *);

typedef int (FP_I_C_I)(void *, int, int);
typedef int (FP_I_C_S)(void *, int, char *);
typedef int (FP_I_C_C)(void *, int, const char *);
typedef int (FP_I_C_J)(void *, int, Json *);

typedef int (FP_I_J_I)(void *, int, int);
typedef int (FP_I_J_S)(void *, int, char *);
typedef int (FP_I_J_C)(void *, int, const char *);
typedef int (FP_I_J_J)(void *, int, Json *);

/* 2-arg string-returning functions */
typedef int (FP_S_I_I)(void *, int, int);
typedef int (FP_S_I_S)(void *, int, char *);
typedef int (FP_S_I_C)(void *, int, const char *);
typedef int (FP_S_I_J)(void *, int, Json *);

typedef int (FP_S_S_I)(void *, int, int);
typedef int (FP_S_S_S)(void *, int, char *);
typedef int (FP_S_S_C)(void *, int, const char *);
typedef int (FP_S_S_J)(void *, int, Json *);

typedef int (FP_S_C_I)(void *, int, int);
typedef int (FP_S_C_S)(void *, int, char *);
typedef int (FP_S_C_C)(void *, int, const char *);
typedef int (FP_S_C_J)(void *, int, Json *);

typedef int (FP_S_J_I)(void *, int, int);
typedef int (FP_S_J_S)(void *, int, char *);
typedef int (FP_S_J_C)(void *, int, const char *);
typedef int (FP_S_J_J)(void *, int, Json *);

/* 2-arg Json-returning functions */
typedef int (FP_J_I_I)(void *, int, int);
typedef int (FP_J_I_S)(void *, int, char *);
typedef int (FP_J_I_C)(void *, int, const char *);
typedef int (FP_J_I_J)(void *, int, Json *);

typedef int (FP_J_S_I)(void *, int, int);
typedef int (FP_J_S_S)(void *, int, char *);
typedef int (FP_J_S_C)(void *, int, const char *);
typedef int (FP_J_S_J)(void *, int, Json *);

typedef int (FP_J_C_I)(void *, int, int);
typedef int (FP_J_C_S)(void *, int, char *);
typedef int (FP_J_C_C)(void *, int, const char *);
typedef int (FP_J_C_J)(void *, int, Json *);

typedef int (FP_J_J_I)(void *, int, int);
typedef int (FP_J_J_S)(void *, int, char *);
typedef int (FP_J_J_C)(void *, int, const char *);
typedef int (FP_J_J_J)(void *, int, Json *);


/* here Weds
   change interface to be functions of: f(ejs, nativeStruct, argc boxedArgArray) -> box|Void
   check arg compatibility with types, Json must be NULL, array or obj

   only export the typedef, not struct
*/

struct EJSNativeInvocation_tag {
  EmbeddedJS *ejs;
  EJSNativeMethod *method;
  int argc;
  JSValueConst *argv;
  JSValue returnValue;
};

/* all return non-0 for failure 
*/

int getArgSpecType(EJSNativeMethod *method, int index){
  ArrayList *arguments = &method->arguments;
  if (index < arguments->size){
    EJSNativeArgument *argument = (EJSNativeArgument*)arrayListElement(arguments,index);
    return argument->type;
  } else {
    return -1;
  }
}

int ejsIntArg(EJSNativeInvocation *invocation,
              int argIndex,
              int *valuePtr){
  if (argIndex < 0 || argIndex >= invocation->argc){
    return EJS_INDEX_OUT_OF_RANGE;
  }
  JSValueConst arg = invocation->argv[argIndex];
  EJSNativeMethod *method = invocation->method;
  if (getArgSpecType(method,argIndex) != EJS_NATIVE_TYPE_INT32){
    return EJS_TYPE_MISMATCH;
  }
  JS_ToInt32(invocation->ejs->ctx, valuePtr, arg);
  return EJS_OK;
}

int ejsStringArg(EJSNativeInvocation *invocation,
                 int argIndex,
                 const char **valuePtr){
  if (argIndex < 0 || argIndex >= invocation->argc){
    return EJS_INDEX_OUT_OF_RANGE;
  }
  JSValueConst arg = invocation->argv[argIndex];
  EJSNativeMethod *method = invocation->method;
  if (getArgSpecType(method,argIndex) != EJS_NATIVE_TYPE_CONST_STRING){
    return EJS_TYPE_MISMATCH;
  }

  size_t len;
  *valuePtr = JS_ToCStringLen(invocation->ejs->ctx, &len, arg);
  return EJS_OK;
}

int ejsJsonArg(EJSNativeInvocation *invocation,
               int argIndex,
               Json **valuePtr){
  if (argIndex < 0 || argIndex >= invocation->argc){
    return EJS_INDEX_OUT_OF_RANGE;
  }
  JSValueConst arg = invocation->argv[argIndex];
  EJSNativeMethod *method = invocation->method;
  if (getArgSpecType(method,argIndex) != EJS_NATIVE_TYPE_JSON){
    return EJS_TYPE_MISMATCH;
  }
  *valuePtr = ejsJSToJson_internal(invocation->ejs,arg,invocation->ejs->fileEvalHeap);
  return EJS_OK;
}

int ejsReturnInt(EJSNativeInvocation *invocation,
                 int value){
  EJSNativeMethod *method = invocation->method;
  if (method->returnType != EJS_NATIVE_TYPE_INT32){
    return EJS_TYPE_MISMATCH;
  }
  invocation->returnValue = JS_NewInt64(invocation->ejs->ctx,(int64_t)value);
  return EJS_OK;
}

int ejsReturnJson(EJSNativeInvocation *invocation,
                  Json *value){
  EJSNativeMethod *method = invocation->method;
  if (method->returnType != EJS_NATIVE_TYPE_JSON){
    return EJS_TYPE_MISMATCH;
  }
  invocation->returnValue = ejsJsonToJS_internal(invocation->ejs,value);
  return EJS_OK;
}

int ejsReturnString(EJSNativeInvocation *invocation,
                    const char *value,
                    bool needsFree){
  EJSNativeMethod *method = invocation->method;
  if (method->returnType != EJS_NATIVE_TYPE_CONST_STRING){
    return EJS_TYPE_MISMATCH;
  }
  invocation->returnValue = JS_NewString(invocation->ejs->ctx, value); /* does this copy */
  return EJS_OK;
}

JSValue ejsFunctionTrampoline(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic){
  EmbeddedJS *ejs = (EmbeddedJS*)JS_GetContextOpaque(ctx);
  
  /* void *nativeStruct = unsafeGetOpaque(this_val); */
  /* printf("ejsTrampoline, magic=%d, nativeStruct=0x%p, this_val:\n",magic,NULL); // nativeStruct);
     dumpbuffer((char*)&this_val,sizeof(JSValue));
  */
  JSValue proto = JS_GetPrototype(ctx,this_val);
  if (JS_IsException(proto)){
    goto fail;
  }
  /*
    printf("trampoline this_val proto is 0x%p\n",JS_VALUE_GET_PTR(proto));
    dumpbuffer((char*)&proto,sizeof(JSValue));
  */
  void *protoPointer = JS_VALUE_GET_PTR(proto);
  NativeClassInternals *classInternals = (NativeClassInternals*)lhtGet(ejs->nativeClassTable,(int64_t)protoPointer);
  void *nativeStruct = JS_GetOpaque(this_val,classInternals->classID);
  EJSNativeClass *nativeClass = classInternals->nativeClass;
  EJSNativeMethod *method = arrayListElement(&nativeClass->instanceMethodFunctions,magic);
  if (ejs->traceLevel >= 1){
    printf("ftramp got class name '%s' method # %d (%s) on native struct at 0x%p\n",
           nativeClass->name,magic,method->name,nativeStruct);
    fflush(stdout);
  }
  JS_FreeValue(ctx,proto);
  ArrayList *nativeArgs = &method->arguments;
  if (argc != nativeArgs->size){
    printf("bad %s received %d args, required %d\n",method->name,argc,nativeArgs->size);
  }
  EJSNativeInvocation invocation;
  invocation.ejs = ejs;
  invocation.method = method;
  invocation.argc = argc;
  invocation.argv = argv;
  int ffStatus = method->functionPointer(nativeStruct,&invocation);
  if (ffStatus){
    printf("failed to evaluation native function %s()\n",method->name);
    goto fail;
  } else {
    switch (method->returnType){
    case EJS_NATIVE_TYPE_INT32:
    case EJS_NATIVE_TYPE_CONST_STRING:
    case EJS_NATIVE_TYPE_STRING:
    case EJS_NATIVE_TYPE_JSON:
      return invocation.returnValue;
    case EJS_NATIVE_TYPE_VOID:
    return JS_UNDEFINED;
    default:
      printf("unknown native function return type %d\n",method->returnType);
      goto fail;
    }
  }
 fail:
  return JS_EXCEPTION;  
}

static const JSCFunctionListEntry ejsNativeProtoFuncs[] = {
  JS_CFUNC_MAGIC_DEF("three", 0, ejsFunctionTrampoline, 0 ),
};

static JSValue js_experiment_boop(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
  printf("boop boop boop!!\n");
  return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_experiment_funcs[] = {
    JS_CFUNC_DEF("boop", 0, js_experiment_boop ),
};

static JSValue thingy_ctor(JSContext *ctx, JSValueConst new_target,
                           int argc, JSValueConst *argv){
  JSValue obj = JS_UNDEFINED, proto;
  CThingy *t1;
  
  /* create the object */
  if (JS_IsUndefined(new_target)) {
    printf("thingy ctor undefined\n");
    proto = JS_GetClassProto(ctx, js_experiment_thingy_class_id);
  } else {
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
      goto fail;
    printf("thingy ctor proto is 0x%p\n",JS_VALUE_GET_PTR(proto));
    dumpbuffer((char*)&proto,sizeof(JSValue));
    
  }
  obj = JS_NewObjectProtoClass(ctx, proto, js_experiment_thingy_class_id);
  JS_FreeValue(ctx, proto);
  if (JS_IsException(obj))
    goto fail;
  t1 = js_mallocz(ctx, sizeof(CThingy));
  if (!t1)
    goto fail;
  JS_SetOpaque(obj, t1);
  return obj;
 fail:
  JS_FreeValue(ctx, obj);
  return JS_EXCEPTION;
}


static int js_experiment_init(JSContext *ctx, JSModuleDef *m)
{
  printf("js_experiment_init\n");
  JSValue proto,obj;
  
  /* FILE class */
  /* the class ID is created once */
  JS_NewClassID(&js_experiment_thingy_class_id);
  /* the class is created once per runtime */
  JS_NewClass(JS_GetRuntime(ctx), js_experiment_thingy_class_id, &js_experiment_thingy_class);
  proto = JS_NewObject(ctx);
  /* this places "instance methods" */
  printf("thingy proto is 0x%p\n",JS_VALUE_GET_PTR(proto));
  dumpbuffer((char*)&proto,sizeof(JSValue));
  JS_SetPropertyFunctionList(ctx,
                             proto,
                             js_experiment_thingy_proto_funcs,
                             countof(js_experiment_thingy_proto_funcs));
  
  obj = JS_NewCFunction2(ctx,
                         thingy_ctor,
                         "Thingy",
                         0, /* at least this many args */
                         JS_CFUNC_constructor,
                         0); /* not magic */
  JS_SetConstructor(ctx, obj, proto);
  
  JS_SetClassProto(ctx,
                   js_experiment_thingy_class_id,
                   proto);
  // How is js_new_std_file(JSContext *ctx, FILE *f, called in quickjs-libc?
  JS_SetModuleExportList(ctx, m, js_experiment_funcs,
                         countof(js_experiment_funcs));
  JS_SetModuleExport(ctx, m, "Thingy", obj);
  
  return 0;
}

static char asciiSTD[4] ={ 0x73, 0x74, 0x64, 0};
static char asciiOS[3] ={ 0x6F, 0x73, 0};
static char asciiExperiment[11] ={ 0x65, 0x78, 0x70, 0x65, 
				   0x72, 0x69, 0x6d, 0x65, 
				   0x6e, 0x74, 0};

JSModuleDef *js_init_module_experiment(JSContext *ctx, const char *module_name)
{
  JSModuleDef *m;
  m = JS_NewCModule(ctx, module_name, js_experiment_init);
  if (!m){
    return NULL;
  }
  JS_AddModuleExportList(ctx, m, js_experiment_funcs, countof(js_experiment_funcs));
  JS_AddModuleExport(ctx, m, "Thingy");
  return m;
}


static void ejsNativeFinalizer(JSRuntime *rt, JSValue val){
  /*  printf("JOE Should write native finalizer\n"); */
}

static JSValue ejsNativeConstructorDispatcher(JSContext *ctx, JSValueConst new_target,
                                              int argc, JSValueConst *argv){
  JSValue obj = JS_UNDEFINED, proto;
  void *nativeStruct;
  
  EmbeddedJS *ejs = (EmbeddedJS*)JS_GetContextOpaque(ctx);  
  /* create the object */
  if (JS_IsUndefined(new_target)) {
    printf("*** EJS PANIC *** 'this' is expected in native constructor\n");
    goto fail;
  } else {
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (ejs->traceLevel >= 2){
      printf("native ctor proto is 0x%p\n",JS_VALUE_GET_PTR(proto));
      dumpbuffer((char*)&proto,sizeof(JSValue));
    }

    if (JS_IsException(proto))
      goto fail;
  }
  void *protoPointer = JS_VALUE_GET_PTR(proto);
  NativeClassInternals *classInternals = (NativeClassInternals*)lhtGet(ejs->nativeClassTable,(int64_t)protoPointer);
  if (!classInternals){
    printf("*** EJS PANIC *** could not get class internals in native ctor dispatch\n");
    goto fail;
  }
  EJSNativeClass *nativeClass = classInternals->nativeClass;
  obj = JS_NewObjectProtoClass(ctx, proto, classInternals->classID);
  JS_FreeValue(ctx, proto);
  if (JS_IsException(obj)){
    goto fail;
  }
  EJSNativeInvocation invocation;
  nativeStruct = nativeClass->nativeConstructor(nativeClass->userData, &invocation);
  if (!nativeStruct){
    goto fail;
  }
  if (ejs->traceLevel >= 1){
    printf("Made native opaque for class='%s' instanceAt 0x%p, nativeStruct at 0x%p\n",
           nativeClass->name,JS_VALUE_GET_PTR(obj),nativeStruct);
  }
  JS_SetOpaque(obj, nativeStruct);
  return obj;
 fail:
  JS_FreeValue(ctx, obj);
  return JS_EXCEPTION;
}

static int buildNativePrototypeFunctions(JSContext *ctx,
                                         EmbeddedJS *ejs,
                                         JSValue prototype,
                                         EJSNativeClass *nativeClass,
                                         NativeClassInternals *nativClassInternals){
  ArrayList *methods = &(nativeClass->instanceMethodFunctions);
  int methodCount = methods->size;
  JSCFunctionListEntry *functionList = (JSCFunctionListEntry*)safeMalloc(methodCount*sizeof(JSCFunctionListEntry),
                                                                         "MethodFunctionsJSCFunctionListEntry");
  if (!functionList){
    return 8;
  }
  memset(functionList,0,methodCount*sizeof(JSCFunctionListEntry));
  if (ejs->traceLevel >= 1){
    printf("building %d native functions of class='%s'\n",methodCount,nativeClass->name);
  }
  for (int i=0; i<methodCount; i++){
    EJSNativeMethod *nativeMethod = (EJSNativeMethod*)arrayListElement(methods,i);
    ArrayList *nativeArgs = &nativeMethod->arguments;
    size_t mnameLen = strlen(nativeMethod->name);
    nativeMethod->asciiName = safeMalloc(mnameLen + 1, "MethodAsciiName");
    snprintf(nativeMethod->asciiName, mnameLen + 1, "%.*s", (int)mnameLen, nativeMethod->name);
    convertToNative(nativeMethod->asciiName, mnameLen);

    if (ejs->traceLevel >= 1){
      printf("  adding func %s() with %d args\n",nativeMethod->name,nativeArgs->size);
    }
    functionList[i].name = nativeMethod->asciiName;
    functionList[i].prop_flags = JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE;
    functionList[i].def_type = JS_DEF_CFUNC;
    functionList[i].magic = (int16_t)i;
    functionList[i].u.func.length = nativeArgs->size;
    functionList[i].u.func.cproto = JS_CFUNC_generic_magic;
    functionList[i].u.func.cfunc.generic_magic = ejsFunctionTrampoline;
  }
  JS_SetPropertyFunctionList(ctx,prototype,functionList,methodCount);
  return 0;
}

static int initNativeModuleCallback(JSContext *ctx, JSModuleDef *m){
  EmbeddedJS *ejs = (EmbeddedJS*)JS_GetContextOpaque(ctx);
  EJSNativeModule *nativeModule = (EJSNativeModule*)lhtGet(ejs->nativeModuleTable,(int64_t)m);
  ArrayList *classes = &(nativeModule->classes);
  for (int i=0; i<classes->size; i++){
    
    EJSNativeClass *nativeClass = (EJSNativeClass*)arrayListElement(classes,i);
    NativeClassInternals *nativeClassInternals = (NativeClassInternals*)safeMalloc(sizeof(NativeClassInternals),
                                                                                   "NativeClassInternals");
    memset(nativeClassInternals,0,sizeof(NativeClassInternals));

    nativeClassInternals->asciiName = nativeClass->asciiName;
    nativeClassInternals->classDef.class_name = nativeClassInternals->asciiName;
    nativeClassInternals->classDef.finalizer = ejsNativeFinalizer;
    nativeClassInternals->nativeClass = nativeClass;

    /* the class ID is created once */
    JS_NewClassID(&nativeClassInternals->classID);
    
    /* the class is created once per runtime */
    JS_NewClass(JS_GetRuntime(ctx), nativeClassInternals->classID, &nativeClassInternals->classDef);
    JSValue proto = JS_NewObject(ctx);
    void *protoPointer = JS_VALUE_GET_PTR(proto);
    lhtPut(ejs->nativeClassTable,(int64_t)protoPointer,nativeClassInternals);
    buildNativePrototypeFunctions(ctx,ejs,proto,nativeClass,nativeClassInternals);

    
    JSValue obj = JS_NewCFunction2(ctx,
                                   ejsNativeConstructorDispatcher,
                                   nativeClassInternals->asciiName,
                                   0, /* at least this many args */
                                   JS_CFUNC_constructor,
                                   0); /* not magic */
    JS_SetConstructor(ctx, obj, proto);
    
    JS_SetClassProto(ctx,
                     nativeClassInternals->classID,
                     proto);

    if (ejs->traceLevel >= 1){
      printf("native module export class name %s\n",nativeClassInternals->asciiName);
      fflush(stdout);
    }
    JS_SetModuleExport(ctx, m, nativeClassInternals->asciiName, obj);
    /*
    JS_SetModuleExportList(ctx, m, js_experiment_funcs,
                           
                           
                           countof(js_experiment_funcs));
    */
  }
  return 0;
}


JSModuleDef *ejsInitNativeModule(JSContext *ctx, EJSNativeModule *nativeModule){
  EmbeddedJS *ejs = (EmbeddedJS*)JS_GetContextOpaque(ctx);
  if (ejs->traceLevel >= 1){
    printf("ejsInitNativeModule %s\n",nativeModule->name);
    fflush(stdout);
  }
  JSModuleDef *m;
  /*
    Here, need a closure or global to get back the particulars of this module, because per_module init functions
    would have to see the qjs/qascii side of the code
    
    init is called with (JSContext *ctx, JSModuleDef *m) -> how to derive pointer to EJS or NativeModule

    void *JS_GetContextOpaque(JSContext *ctx);
    void JS_SetContextOpaque(JSContext *ctx, void *opaque);
    
    put array of modules in EJS including index by JSModuleDef to match

    and then experiment with person class slot:age, method birthday() getAge internal CStruct with data
  */
  size_t nameLen = strlen(nativeModule->name);
  char *asciiName = safeMalloc(nameLen + 1,"ModuleAsciiName");
  snprintf(asciiName, nameLen + 1, "%.*s", (int)nameLen, nativeModule->name);
  convertToNative(asciiName, nameLen);

  ArrayList *classes = &(nativeModule->classes);
  for (int i=0; i<classes->size; i++){
    EJSNativeClass *nativeClass = (EJSNativeClass*)arrayListElement(classes,i);
    size_t cnameLen = strlen(nativeClass->name);
    nativeClass->asciiName = safeMalloc(cnameLen+1, "NativeAsciiClassName");
    snprintf(nativeClass->asciiName, cnameLen + 1, "%.*s", (int)cnameLen, nativeClass->name);
    convertToNative(nativeClass->asciiName, cnameLen);
  }

  if (ejs->traceLevel >= 1){
    printf("before NewCModule\n");
  }
  m = JS_NewCModule(ctx, asciiName, initNativeModuleCallback);
  /* NOTE: we are trusting that the callback is NOT called anywhere in the call graph of NewCModule */
  if (m){
    lhtPut(ejs->nativeModuleTable,(int64_t)m,nativeModule);
  }
  if (ejs->traceLevel >= 1){
    printf("after NewCModule, m=0x%p\n",m);
  }
  if (!m){
    return NULL;
  } 
  /* JS_AddModuleExportList(ctx, m, js_experiment_funcs, countof(js_experiment_funcs)); */

  for (int i=0; i<classes->size; i++){
    EJSNativeClass *nativeClass = (EJSNativeClass*)arrayListElement(classes,i);
    JS_AddModuleExport(ctx, m, nativeClass->asciiName);    
  }

  return m;
}

EJSNativeModule *ejsMakeNativeModule(EmbeddedJS *ejs, char *moduleName){
  EJSNativeModule *m = (EJSNativeModule*)safeMalloc(sizeof(EJSNativeModule),"EJSNativeModule");
  memset(m,0,sizeof(EJSNativeModule));
  m->name = moduleName;
  initEmbeddedArrayList(&m->classes,NULL);
  return m;
}

EJSNativeClass *ejsMakeNativeClass(EmbeddedJS *ejs, EJSNativeModule *module,
                                   char *className,
                                   void *(*nativeConstructor)(void *userData, EJSNativeInvocation *invocation),
                                   void (*nativeFinalizer)(EmbeddedJS *ejs, void *userData, void *nativePointer),
                                   void *userData){
  EJSNativeClass *c = (EJSNativeClass*)safeMalloc(sizeof(EJSNativeClass),"EJSNativeClass");
  memset(c,0,sizeof(EJSNativeClass));
  c->name = className;
  c->nativeConstructor = nativeConstructor;
  c->nativeFinalizer = nativeFinalizer;
  c->userData = userData;
  initEmbeddedArrayList(&c->instanceMethodFunctions,NULL);
  initEmbeddedArrayList(&c->staticMethodFunctions,NULL);
  arrayListAdd(&module->classes,c);
  return c;
}

EJSNativeMethod *ejsMakeNativeMethod(EmbeddedJS *ejs, EJSNativeClass *clazz, char *methodName,
                                     int returnType,
                                     EJSForeignFunction *functionPointer){
  /* int (*functionPointer)(void *nativeStruct, EJSNativeInvocation *invocation)){ */
  EJSNativeMethod *m = (EJSNativeMethod*)safeMalloc(sizeof(EJSNativeMethod),"EJSNativeMethod");
  memset(m,0,sizeof(EJSNativeMethod));
  m->name = methodName;
  m->returnType = returnType;
  m->functionPointer = functionPointer;
  if (ejs->traceLevel >= 1){
    printf("native method %s fp=0x%p\n",methodName,functionPointer);
  }
  initEmbeddedArrayList(&m->arguments,NULL);
  arrayListAdd(&clazz->instanceMethodFunctions,m);
  return m;
}

void ejsAddMethodArg(EmbeddedJS *ejs, EJSNativeMethod *method, char *name, int type){
  EJSNativeArgument *arg = (EJSNativeArgument*)safeMalloc(sizeof(EJSNativeArgument),"EJSNativeArgument");
  memset(arg,0,sizeof(EJSNativeArgument));
  arg->name = name;
  arg->type = type;
  arrayListAdd(&method->arguments,arg);
}

void ejsSetMethodVarargType(EmbeddedJS *ejs, EJSNativeMethod *method, char *name, int type){
  printf("write me!\n");
}



/* also used to initialize the worker context */
static JSContext *makeEmbeddedJSContext(JSRuntime *rt){
  JSContext *ctx;
  ctx = JS_NewContext(rt);
  if (!ctx)
    return NULL;
#ifdef CONFIG_BIGNUM
  if (bignum_ext) {
    JS_AddIntrinsicBigFloat(ctx);
    JS_AddIntrinsicBigDecimal(ctx);
    JS_AddIntrinsicOperators(ctx);
    JS_EnableBignumExt(ctx, TRUE);
  }
#endif
  return ctx;
}

static void initContextModules(JSContext *ctx, EJSNativeModule **nativeModules, int nativeModuleCount){    
  /* system modules */
  /* printf("before init std\n");*/
  js_init_module_std(ctx, asciiSTD);
  js_init_module_os(ctx, asciiOS);
  js_init_module_experiment(ctx, asciiExperiment);
  /* printf("after init experiment\n");*/
  for (int i=0; i<nativeModuleCount; i++){
    ejsInitNativeModule(ctx, nativeModules[i]);
  }
}

/*

  How to build JSValues 
    
    many are static inlines in quickjs.h
  
  int's
    static js_force_inline JSValue JS_NewInt64(JSContext *ctx, int64_t val)

  floats 
    static js_force_inline JSValue JS_NewFloat64(JSContext *ctx, double d)

  bool
    static js_force_inline JSValue JS_NewBool(JSContext *ctx, JS_BOOL val)

  string 
    JSValue JS_NewStringLen(JSContext *ctx, const char *str1, size_t len1);
    JSValue JS_NewString(JSContext *ctx, const char *str);

  Array 
    JSValue JS_NewArray(JSContext *ctx);
    
    setting values
      int JS_SetPropertyUint32(JSContext *ctx, 
                               JSValueConst this_obj,      <--- JSValue of array ok here
                               uint32_t idx, JSValue val);


    JS_SetPropertyUint32(

  Object
    JSValue JS_NewObject(JSContext *ctx);
 
    setting props
       int JS_SetPropertyStr(JSContext *ctx, JSValueConst this_obj,
                              const char *prop, JSValue val);

   testing
      static inline JS_BOOL JS_IsNumber(JSValueConst v)
      static inline JS_BOOL JS_IsBool(JSValueConst v)
      static inline JS_BOOL JS_IsNull(JSValueConst v)
      static inline JS_BOOL JS_IsUndefined(JSValueConst v)
      static inline JS_BOOL JS_IsString(JSValueConst v)      
      static inline JS_BOOL JS_IsSymbol(JSValueConst v)
      static inline JS_BOOL JS_IsObject(JSValueConst v)
      int JS_IsArray(JSContext *ctx, JSValueConst val);

    Accessors:
      Object:
        JSValue JS_GetPropertyStr(JSContext *ctx, JSValueConst this_obj,
                                  const char *prop);
      Array:  
        JSValue JS_GetPropertyUint32(JSContext *ctx, JSValueConst this_obj,
                                     uint32_t idx);

    Here
       Convert with deletions/filtering
       ejsEvalBuffer needs to return value and not free the JSValue 
         JSTest can show values
       simple print
         optional pretty print
       expression can be as-is
         function( <rootName> <provisioningVars> -the 2nd dimensions) {  <-- if object
           return <sourceCode> 
        
 */

static Json *jsToJson1(EmbeddedJS *ejs,
                       JsonBuilder *b, Json *parent, char *parentKey,
                       JSValue value,
                       int depth){
  int buildStatus = 0;
  int32_t tag = JS_VALUE_GET_TAG(value);
  JSContext *ctx = ejs->ctx;
  /* printf("jsToJSON1 tag=%d\n",tag);*/
  if (JS_IsUndefined(value)){
    fprintf(stderr,"*** Panic *** Undefined not handled\n");
    fflush(stderr);
    return NULL;
  } else if (JS_IsSymbol(value)){
    fprintf(stderr,"*** Panic *** Symbol not handled\n");
    fflush(stderr);
    return NULL;
  } else if (JS_IsNumber(value)){
    /* printf("number case\n"); */
    // Int64 vs, 32 vs float
    if (JS_TAG_IS_FLOAT64(tag)){
      return jsonBuildDouble(b,parent,parentKey,JS_VALUE_GET_FLOAT64(value),&buildStatus);
    } else {
      int intValue = 0;
      int intStatus = JS_ToInt32(ctx,&intValue,value);
      /* printf("non float %d, status=%d\n",intValue,intStatus); */
      return jsonBuildInt64(b,parent,parentKey,(int64_t)intValue,&buildStatus);
    }
  } else if (JS_IsBool(value)){
    return jsonBuildBool(b,parent,parentKey,(bool)JS_VALUE_GET_BOOL(value),&buildStatus);
  } else if (JS_IsString(value)){
    const char *str = JS_ToCString(ctx, value);
    if (str == NULL){
      fprintf(stderr,"*** Panic *** Could not extract CString from Value\n");
      fflush(stderr);
      return NULL;
    } else {
      size_t strLen = strlen(str);
      char nativeStr[strLen+1];
      snprintf (nativeStr, strLen + 1, "%.*s", (int)strLen, str);
      convertToNative(nativeStr, strLen);
      Json *jsonString = jsonBuildString(b,parent,parentKey,(char*)nativeStr,strlen(nativeStr),&buildStatus);
      JS_FreeCString(ctx, str);
      return jsonString;
    }
  } else if (JS_IsNull(value)){
    return jsonBuildNull(b,parent,parentKey,&buildStatus);
  } else if (JS_IsObject(value)){
    Json *jsonObject = jsonBuildObject(b,parent,parentKey,&buildStatus);
    /* iterate properties */
    JSPropertyEnum *properties = NULL; /* I assume this will get fresh storage */
    uint32_t propertyCount = 0;
    if (JS_GetOwnPropertyNames(ctx,&properties,&propertyCount,value,JS_GPN_STRING_MASK /* flags */)){
      printf("getOwnProperties failed\n");
      return NULL;
    } else {
      for (uint32_t i=0; i<propertyCount; i++){
        JSPropertyEnum *property = &properties[i];
        JSValue propertyName = JS_AtomToString(ctx,property->atom);
        JSPropertyDescriptor descriptor;
        /* BEWARE!!!
           JS_GetOwnProperty has *NASTY* 3-valued logic.  < 0 error, 0 not found, 1 found */
        int valueStatus = JS_GetOwnProperty(ctx,&descriptor,value,property->atom);
        const char *cPropertyName = JS_ToCString(ctx,propertyName);
        /* printf("jsToJson property i=%d %s valStatus=%d\n",i,cPropertyName,valueStatus); */
        if (valueStatus > 0){
          jsToJson1(ejs,b,jsonObject,(char*)cPropertyName,descriptor.value,depth+1);
        } else {
          printf("*** WARNING *** could not get value for property '%s', status=%d\n",cPropertyName,valueStatus);
        }
        
      }
    }

    return jsonObject;
  } else if (JS_IsArray(ctx,value)){    
    Json *jsonArray = jsonBuildArray(b,parent,parentKey,&buildStatus);
    JSValue aLenJS = JS_GetPropertyStr(ctx,value,"length");
    int aLen = 0;
    buildStatus = JS_ToInt32(ctx,&aLen,aLenJS);
    printf("JS array len = %d\n",aLen);
    for (int i=0; i<aLen; i++){
      JSValue element = JS_GetPropertyUint32(ctx, value, i);
      jsToJson1(ejs,b,jsonArray,NULL,element,depth+1);
    }
    return jsonArray;
  } else {
    fprintf(stderr,"*** Panic *** JSValue is not number, object, array, string, bool, symbol, null, or undefined\n");
    fflush(stderr);
    return NULL;
  }
}

static bool isZoweUnevaluated(Json *json){
  if (jsonIsObject(json)){
    JsonObject *object = jsonAsObject(json);
    Json *propertyValue = jsonObjectGetPropertyValue(object,ZOWE_INTERNAL_TYPE);
    if (propertyValue && jsonIsString(propertyValue)){
      return !strcmp(jsonAsString(propertyValue),ZOWE_UNEVALUATED);
    }
  }
  return false;
}

static Json *ejsJSToJson_internal(EmbeddedJS *ejs, JSValue value, ShortLivedHeap *slh){
  JsonBuilder *builder = makeJsonBuilder(slh);
  Json *json = jsToJson1(ejs,builder,NULL,NULL,value,0);
  freeJsonBuilder(builder,false);
  return json;
}

Json *ejsJSToJson(EmbeddedJS *ejs, JSValueBox valueBox, ShortLivedHeap *slh){
  return ejsJSToJson_internal(ejs,valueBox.value,slh);
}

static JSValue jsonToJS1(EmbeddedJS *ejs, Json *json, bool hideUnevaluated){
  JSContext *ctx = ejs->ctx;
  switch (json->type) {
  case JSON_TYPE_NUMBER:
  case JSON_TYPE_INT64:
    return JS_NewInt64(ctx,(int64_t)jsonAsInt64(json));
  case JSON_TYPE_DOUBLE:
    return JS_NewFloat64(ctx,jsonAsDouble(json));
  case JSON_TYPE_STRING:
    {
      char *str = jsonAsString(json);
      size_t strLen = strlen(str);
      char convertedStr[strLen+1];
      snprintf (convertedStr, strLen + 1, "%.*s", (int)strLen, str);
      /* printf ("about to convert string '%s'\n", convertedStr); */
      convertFromNative(convertedStr, strLen);
      return JS_NewString(ctx, convertedStr);
    }
  case JSON_TYPE_BOOLEAN:
    return JS_NewBool(ctx,jsonAsBoolean(json));
  case JSON_TYPE_NULL:
    return JS_NULL;
  case JSON_TYPE_OBJECT:
    {
      if (hideUnevaluated && isZoweUnevaluated(json)){
        return JS_UNDEFINED;
      } else {
        JsonObject *jsonObject = jsonAsObject(json);
        JSValue object = JS_NewObject(ctx);
        JsonProperty *property;
        
        for (property = jsonObjectGetFirstProperty(jsonObject);
             property != NULL;
             property = jsonObjectGetNextProperty(property)) {
              {
                char *key = jsonPropertyGetKey(property);
                size_t keyLen = strlen(key);
                char convertedKey[keyLen+1];
                snprintf (convertedKey, keyLen + 1, "%.*s", (int)keyLen, key);
                /* printf ("about to convert key '%s'\n", convertedKey); */
                convertFromNative(convertedKey, keyLen);
                JS_SetPropertyStr(ctx,
                                  object,
                                  convertedKey,
                                  jsonToJS1(ejs,jsonPropertyGetValue(property),hideUnevaluated));
              }
        }
        return object;
      }
    }
  case JSON_TYPE_ARRAY:
    {
      JsonArray *jsonArray = jsonAsArray(json);
      JSValue array = JS_NewArray(ctx);
      int count = jsonArrayGetCount(jsonArray);
      
      for (uint32_t i = 0; i < count; i++) {
        JS_SetPropertyUint32(ctx,
                             array,
                             i,
                             jsonToJS1(ejs,jsonArrayGetItem(jsonArray,i),hideUnevaluated));
      }
      return array;
    }
  default:
    printf("*** PANIC *** unknown JSON type %d\n",json->type);
    /* The next line is cheesy, but ... */
    return JS_NULL;
  }
}




static JSValue ejsJsonToJS_internal(EmbeddedJS *ejs, Json *json){
  return jsonToJS1(ejs,json,false);
}

JSValueBox ejsJsonToJS(EmbeddedJS *ejs, Json *json){
  JSValue value = ejsJsonToJS_internal(ejs,json);
  return *((JSValueBox*)&value);
}

char *json2JS(Json *json){
  JsonBuffer *buffer = makeJsonBuffer();
  /* jsonPrinter *p = makeBufferJsonPrinter(sourceCCSID,buffer); */
#ifdef __ZOWE_OS_WINDOWS
  int stdoutFD = _fileno(stdout);
#elif defined(STDOUT_FILENO)
  int stdoutFD = STDOUT_FILENO;
#else
  int stdoutFD = 1; /* this looks hacky, but it's been true for about 50 years */
#endif
  jsonPrinter *p = makeJsonPrinter(stdoutFD);
  jsonEnablePrettyPrint(p);
  
  return NULL;
}

static void visitJSON(Json *json, Json *parent, char *key, int index,
                      bool (*visitor)(void *context, Json *value, Json *parent, char *key, int index),
                      void *context){
  switch (json->type) {
  case JSON_TYPE_NUMBER:
  case JSON_TYPE_INT64:
  case JSON_TYPE_DOUBLE:
  case JSON_TYPE_STRING:
  case JSON_TYPE_BOOLEAN:
  case JSON_TYPE_NULL:
    visitor(context,json,parent,key,index);
    break;
  case JSON_TYPE_OBJECT:
    {
      JsonObject *jsonObject = jsonAsObject(json);
      JsonProperty *property;
      for (property = jsonObjectGetFirstProperty(jsonObject);
           property != NULL;
           property = jsonObjectGetNextProperty(property)) {
        char *key = jsonPropertyGetKey(property);
        Json *value = jsonPropertyGetValue(property);
        bool shouldVisitChildren = visitor(context,value,json,key,-1);
        if (shouldVisitChildren){
          visitJSON(value,json,key,-1,visitor,context);
        }
      }
    }
    break;
  case JSON_TYPE_ARRAY:
    {
      JsonArray *jsonArray = jsonAsArray(json);
      int count = jsonArrayGetCount(jsonArray);
      for (uint32_t i = 0; i < count; i++) {
        Json *element = jsonArrayGetItem(jsonArray,i);
        bool shouldVisitChildren = visitor(context,element,json,NULL,i);
        if (shouldVisitChildren){
          visitJSON(element,json,NULL,i,visitor,context);
        }
      }
    }
    break;
  default:
    printf("*** PANIC *** unknown JSON type %d\n",json->type);
    /* The next line is cheesy, but ... */
    break;
  }
}

static void setJsonProperty(JsonObject *object, char *key, Json *value){
  JsonProperty *property;
  for (property = jsonObjectGetFirstProperty(object);
       property != NULL;
       property = jsonObjectGetNextProperty(property)) {
    char *propertyKey = jsonPropertyGetKey(property);
    if (!strcmp(propertyKey,key)){
      property->value = value;
      break;
    }
  }
}

static void setJsonArrayElement(JsonArray *array, int index, Json *value){
  array->elements[index] = value;
}

typedef struct TemplateEvaluationContext_tag {
  EmbeddedJS *ejs;
  Json *topJson;
  ShortLivedHeap *slh;
} TemplateEvaluationContext;

static bool evaluationVisitor(void *context, Json *json, Json *parent, char *keyInParent, int indexInParent){
  if (isZoweUnevaluated(json)){
    JsonObject *object = jsonAsObject(json);
    JsonProperty *property;
    TemplateEvaluationContext *evalContext = (TemplateEvaluationContext*)context;
    EmbeddedJS *ejs = evalContext->ejs;
    JsonObject *topObject = jsonAsObject(evalContext->topJson);
    for (property = jsonObjectGetFirstProperty(topObject);
         property != NULL;
         property = jsonObjectGetNextProperty(property)) {
      char *key = jsonPropertyGetKey(property);
      Json *value = jsonPropertyGetValue(property);
      /* printf ("global object key '%s'\n", key); */
      size_t keyLen = strlen(key);
      char convertedKey[keyLen+1];
      snprintf (convertedKey, keyLen + 1, "%.*s", (int)keyLen, key);
      convertFromNative(convertedKey, keyLen);
      ejsSetGlobalProperty_internal(ejs,convertedKey,ejsJsonToJS_internal(ejs,value));
    }
    Json *sourceValue = jsonObjectGetPropertyValue(object,"source");
    if (sourceValue){
      char *source = jsonAsString(sourceValue);
      size_t sourceLen = strlen(source);
      char asciiSource[sourceLen + 1];
      snprintf (asciiSource, sourceLen + 1, "%.*s", (int)sourceLen, source);
      convertFromNative(asciiSource, sourceLen);
      printf("should evaluate: %s\n",source);
      
      int evalStatus = 0;
      char embedded[] = "<embedded>";
      convertFromNative(embedded, sizeof(embedded));
      printf("in ascii\n");
      dumpbuffer(asciiSource,strlen(asciiSource));
      JSValue output = ejsEvalBuffer1(ejs,asciiSource,strlen(asciiSource),embedded,0,&evalStatus);
      if (evalStatus){
        printf("failed to evaluate '%s', status=%d\n",source,evalStatus);
      } else {
        /* 
           printf("evaluation succeeded\n");
           dumpbuffer((char*)&output,sizeof(JSValue));
        */
        Json *evaluationResult = ejsJSToJson_internal(ejs,output,evalContext->slh);
        if (evaluationResult){
          if (keyInParent){
            setJsonProperty(jsonAsObject(parent),keyInParent,evaluationResult);
          } else {
            setJsonArrayElement(jsonAsArray(parent),indexInParent,evaluationResult);
          }
        } else {
          printf("Warning EJS failed to translate eval result JSValue to Json\n");
        }
      }
    }
    return false;
  } else {
    return true;
  }
}

Json *evaluateJsonTemplates(EmbeddedJS *ejs, ShortLivedHeap *slh, Json *json){
  /* jsonToJS1(ejs,json,false); */
  TemplateEvaluationContext evalContext;
  if (jsonIsObject(json)){
    JsonObject *topObject = jsonAsObject(json);
      
    evalContext.ejs = ejs;
    evalContext.slh = slh;
    evalContext.topJson = json;
    visitJSON(json,NULL,NULL,-1,evaluationVisitor,&evalContext);
    return json;
  } else {
    printf("top json is not an object\n");
    return NULL;
  }
}

EmbeddedJS *allocateEmbeddedJS(EmbeddedJS *sharedRuntimeEJS /* can be NULL */){
  EmbeddedJS *embeddedJS = (EmbeddedJS*)safeMalloc(sizeof(EmbeddedJS),"EmbeddedJS");
  memset(embeddedJS,0,sizeof(EmbeddedJS));
  if (sharedRuntimeEJS){
    embeddedJS->rt = sharedRuntimeEJS->rt;
  } else {
    JSRuntime *rt = JS_NewRuntime();
    embeddedJS->rt = rt;
  }
  embeddedJS->nativeClassTable = lhtCreate(257,NULL);
  embeddedJS->nativeModuleTable = lhtCreate(101,NULL); 
  return embeddedJS;
}

static char *copyFromNative(char *source, ShortLivedHeap *slh){
  size_t sourceLen = strlen(source);
  char *dest = (slh ? SLHAlloc(slh,sourceLen+1) : safeMalloc(sourceLen+1,"copyFromNative"));
  snprintf (dest, sourceLen + 1, "%.*s", (int)sourceLen, source);
  convertFromNative(dest, sourceLen);
  return dest;
}

static char **copyArrayFromNative(int sourceCount, char **sourceArray, ShortLivedHeap *slh){
  char **copy = (slh ?
                 (char**)SLHAlloc(slh,sourceCount*sizeof(char*)) :
                 (char**)safeMalloc(sourceCount*sizeof(char*),"copyArrayFromNative"));
  for (int i=0; i<sourceCount; i++){
    copy[i] = copyFromNative(sourceArray[i],slh);
  }
  return copy;
}


bool configureEmbeddedJS(EmbeddedJS *embeddedJS, 
                         EJSNativeModule **nativeModules, int nativeModuleCount,
                         int argc, char **argv){
  /* 
     JS_SetMemoryLimit(rt, memory_limit);
     JS_SetMaxStackSize(rt, stack_size);
  */
  js_std_set_worker_new_context_func(makeEmbeddedJSContext);
  js_std_init_handlers(embeddedJS->rt);
  JSContext *ctx = makeEmbeddedJSContext(embeddedJS->rt);
  if (!ctx) {
    fprintf(stderr, "qjs: cannot allocate JS context\n");
    safeFree((char*)embeddedJS,sizeof(EmbeddedJS));
    return false;
  }
  /* Establish bijection between EJS and QJS top-level structures */
  embeddedJS->ctx = ctx;
  JS_SetContextOpaque(ctx,embeddedJS);
  if (embeddedJS->traceLevel >= 1){
    printf("configEJS embeddedJS=0x%p ctxOpaque=0x%p\n",
           embeddedJS,JS_GetContextOpaque(ctx));
  }
  /* how to do this workers, is still not well explored */
  initContextModules(ctx,nativeModules,nativeModuleCount);

  /* loader for ES6 modules */
  JS_SetModuleLoaderFunc(embeddedJS->rt, NULL, js_module_loader, NULL);
  
  if (false){ /* dump_unhandled_promise_rejection) { - do this later */
    JS_SetHostPromiseRejectionTracker(embeddedJS->rt, js_std_promise_rejection_tracker,
                                      NULL);
  }
  js_std_add_helpers(ctx, argc, copyArrayFromNative(argc, argv, NULL));

  int evalStatus = 0;
  /* make 'std' and 'os' visible to non module code */
  if (true){ /* load_std) {*/
    const char *source = "import * as std from 'std';\n"
      "import * as os from 'os';\n"
      /*  "import * as experiment from 'experiment';\n" */
      /*       "import * as FFI1 from 'FFI1';\n" */
      "globalThis.std = std;\n"
      "globalThis.os = os;\n"
      /* "globalThis.experiment = experiment;\n"; */
      ;
    size_t sourceLen = strlen(source);
    char asciiSource[sourceLen + 1];
    snprintf (asciiSource, sourceLen + 1, "%.*s", (int)sourceLen, source);
    convertFromNative(asciiSource, sourceLen);
    char input[] = "<input>";
    convertFromNative(input, sizeof(input));
    if (embeddedJS->traceLevel >= 1){
      printf("before eval buffer in configure\n");
    }
    JSValue throwaway = ejsEvalBuffer1(embeddedJS, asciiSource, strlen(asciiSource), input, JS_EVAL_TYPE_MODULE, &evalStatus);
  }
  return true;
}

EmbeddedJS *ejsGetEnvironment(EJSNativeInvocation *invocation){
  return invocation->ejs;
}

JsonBuilder *ejsMakeJsonBuilder(EmbeddedJS *ejs){
  return makeJsonBuilder(ejs->fileEvalHeap);
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

