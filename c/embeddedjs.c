

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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>  
#include <sys/stat.h>
#include <sys/types.h> /* JOE 1/20/22 */
#include <stdint.h> /* JOE 1/20/22 */
#include <stdbool.h> /* JOE 1/20/22 */
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

#ifdef __ZOWE_OS_WINDOWS
typedef int64_t ssize_t;
#endif

JSValue ejsEvalBuffer(EmbeddedJS *ejs,
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
      js_std_dump_error(ctx);
      ret = -1;
  } else {
    ret = 0;
  }
  *statusPtr = ret;
  return val;
}

void ejsFreeJSValue(EmbeddedJS *ejs, JSValue value){
  JS_FreeValue(ejs->ctx, value);
}

int ejsSetGlobalProperty(EmbeddedJS *ejs, const char *propertyName, JSValue value){
  JSContext *ctx = ejs->ctx;
  JSValue theGlobal = JS_GetGlobalObject(ctx);
  return JS_SetPropertyStr(ctx,theGlobal,propertyName,value);
}

int ejsEvalFile(EmbeddedJS *ejs, const char *filename, int loadMode){
    uint8_t *buf;
    int ret, eval_flags;
    size_t bufferLength;
    
    buf = js_load_file(ejs->ctx, &bufferLength, filename);
    if (!buf) {
        perror(filename);
        exit(1);
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
    JSValue evalResult = ejsEvalBuffer(ejs, buf, bufferLength, filename, eval_flags, &ret);
    js_free(ejs->ctx, buf);
    JS_FreeValue(ejs->ctx, evalResult);
    return ret;
}

static JSClassID js_joe_thingy_class_id;

static void js_joe_thingy_finalizer(JSRuntime *rt, JSValue val){
  /* JSSTDFile *s = JS_GetOpaque(val, js_std_file_class_id); */
  printf("Joe Thingy Finalizer running\n");
}


static JSClassDef js_joe_thingy_class = {
    "THINGY",
    .finalizer = js_joe_thingy_finalizer,
};

/*
  #define JS_CFUNC_DEF(name, length, func1) 

  name is JS name

  #define JS_CFUNC_MAGIC_DEF(name, length, func1, magic)

*/

JSValue js_joe_thingy_three(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv){
  return JS_NewInt32(ctx, 3);
}

static const JSCFunctionListEntry js_joe_thingy_proto_funcs[] = {
    JS_CFUNC_DEF("three", 0, js_joe_thingy_three ),
};

static JSValue js_joe_boop(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv)
{
  printf("boop boop boop!!\n");
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_joe_funcs[] = {
    JS_CFUNC_DEF("boop", 0, js_joe_boop ),
};



static int js_joe_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue proto;
    
    /* FILE class */
    /* the class ID is created once */
    JS_NewClassID(&js_joe_thingy_class_id);
    /* the class is created once per runtime */
    JS_NewClass(JS_GetRuntime(ctx), js_joe_thingy_class_id, &js_joe_thingy_class);
    proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx,
                               proto,
                               js_joe_thingy_proto_funcs,
                               countof(js_joe_thingy_proto_funcs));
    JS_SetClassProto(ctx,
                     js_joe_thingy_class_id,
                     proto);

    JS_SetModuleExportList(ctx, m, js_joe_funcs,
                           countof(js_joe_funcs));
    return 0;
}


JSModuleDef *js_init_module_joe(JSContext *ctx, const char *module_name)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, js_joe_init);
    if (!m){
        return NULL;
    }
    JS_AddModuleExportList(ctx, m, js_joe_funcs, countof(js_joe_funcs));
    return m;
}

/* also used to initialize the worker context */
static JSContext *makeEmbeddedJSContext(JSRuntime *rt)
{
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
    /* system modules */
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");
    js_init_module_joe(ctx, "joe");
    return ctx;
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
      Json *jsonString = jsonBuildString(b,parent,parentKey,(char*)str,strlen(str),&buildStatus);
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

Json *ejsJSToJson(EmbeddedJS *ejs, JSValue value, ShortLivedHeap *slh){
  JsonBuilder *builder = makeJsonBuilder(slh);
  Json *json = jsToJson1(ejs,builder,NULL,NULL,value,0);
  freeJsonBuilder(builder,false);
  return json;
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
    return JS_NewString(ctx,jsonAsString(json));
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
          JS_SetPropertyStr(ctx,
                            object,
                            jsonPropertyGetKey(property),
                            jsonToJS1(ejs,jsonPropertyGetValue(property),hideUnevaluated));
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




JSValue ejsJsonToJS(EmbeddedJS *ejs, Json *json){
  return jsonToJS1(ejs,json,false);
}

char *json2JS(Json *json){
  JsonBuffer *buffer = makeJsonBuffer();
  /* jsonPrinter *p = makeBufferJsonPrinter(sourceCCSID,buffer); */
#ifdef __ZOWE_OS_WINDOWS
  int stdoutFD = _fileno(stdout);
#else
  int stdoutFD = STDOUT_FILENO;
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
      ejsSetGlobalProperty(ejs,key,ejsJsonToJS(ejs,value));
    }
    Json *sourceValue = jsonObjectGetPropertyValue(object,"source");
    if (sourceValue){
      char *source = jsonAsString(sourceValue);
      printf("should evaluate: %s\n",source);
      int evalStatus = 0;
      JSValue output = ejsEvalBuffer(ejs,source,strlen(source),"<embedded>",0,&evalStatus);
      if (evalStatus){
        printf("failed to evaluate '%s', status=%d\n",source,evalStatus);
      } else {
        printf("evaluation succeeded\n");
        dumpbuffer((char*)&output,sizeof(JSValue));
        Json *evaluationResult = ejsJSToJson(ejs,output,evalContext->slh);
        printf("evaluationResult back in Json 0x%p\n",evaluationResult);
        fflush(stdout);
        if (keyInParent){
          setJsonProperty(jsonAsObject(parent),keyInParent,evaluationResult);
        } else {
          setJsonArrayElement(jsonAsArray(parent),indexInParent,evaluationResult);
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




EmbeddedJS *makeEmbeddedJS(EmbeddedJS *sharedRuntimeEJS){ /* can be NULL */
  EmbeddedJS *embeddedJS = (EmbeddedJS*)safeMalloc(sizeof(EmbeddedJS),"EmbeddedJS");
  memset(embeddedJS,0,sizeof(EmbeddedJS));
  if (sharedRuntimeEJS){
    embeddedJS->rt = sharedRuntimeEJS->rt;
  } else {
    JSRuntime *rt = JS_NewRuntime();
    embeddedJS->rt = rt;
  }
  /* 
     JS_SetMemoryLimit(rt, memory_limit);
     JS_SetMaxStackSize(rt, stack_size);
  */
  js_std_set_worker_new_context_func(makeEmbeddedJSContext);
  js_std_init_handlers(embeddedJS->rt);
  JSContext *ctx = makeEmbeddedJSContext(embeddedJS->rt);
  embeddedJS->ctx = ctx;
  if (!ctx) {
    fprintf(stderr, "qjs: cannot allocate JS context\n");
    exit(2);
  }

  printf("JSTest Made Runtime and context \n");
  fflush(stdout);

  
  /* loader for ES6 modules */
  JS_SetModuleLoaderFunc(embeddedJS->rt, NULL, js_module_loader, NULL);
  
  if (false){ /* dump_unhandled_promise_rejection) { - JOE doe this later */
    JS_SetHostPromiseRejectionTracker(embeddedJS->rt, js_std_promise_rejection_tracker,
                                      NULL);
  }
  js_std_add_helpers(ctx, 0, NULL); /* argc - optind, argv + optind); */

  int evalStatus = 0;
  /* make 'std' and 'os' visible to non module code */
  if (true){ /* load_std) {*/
    const char *str = "import * as std from 'std';\n"
      "import * as os from 'os';\n"
      "import * as joe from 'joe';\n"
      "globalThis.std = std;\n"
      "globalThis.os = os;\n"
      "globalThis.joe = joe;\n";
    JSValue throwaway = ejsEvalBuffer(embeddedJS, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE, &evalStatus);
  }


  return embeddedJS;
}

