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
#else 
#include <sys/ipc.h>
#include <sys/msg.h>
#endif

#ifdef __ZOWE_OS_ZOS

#include "porting/polyfill.h"

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

static void ejsDumpObject(JSContext *ctx, FILE *f, JSValueConst val)
{
    const char *str;
    
    str = JS_ToCString(ctx, val);
    if (str) {
      size_t copyLen = strlen(str);
      char copy[copyLen+1];
      snprintf(copy, copyLen + 1, "%.*s", (int)copyLen, str);
      convertToNative(copy,(int)copyLen);
      fprintf(f, "%s\n", copy);
      JS_FreeCString(ctx, str);
    } else {
      fprintf(f, "[exception]\n");
    }
}

static char asciiStack[6] ={ 0x73, 0x74, 0x61, 0x63, 0x6b, 0x00};

static void ejsDumpError(JSContext *ctx, JSValueConst exception_val)
{
  JSValue val;
  BOOL is_error;
  
  is_error = JS_IsError(ctx, exception_val);
  
  ejsDumpObject(ctx, stderr, exception_val);
  if (is_error) {
    val = JS_GetPropertyStr(ctx, exception_val, asciiStack);
    if (!JS_IsUndefined(val)) {
      ejsDumpObject(ctx, stderr, val);
    } 
    JS_FreeValue(ctx, val);
  }
}

static JSValue ejsEvalBuffer1(EmbeddedJS *ejs,
                              const void *buffer, int bufferLength,
                              const char *filename,   /* expects ascii */
                              int eval_flags,
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
    
    size_t messageLen = strlen(message);
    char nativeMessage[messageLen+1];
    snprintf(nativeMessage, messageLen + 1, "%.*s", (int)messageLen, message);
    convertToNative(nativeMessage,(int)messageLen);
    if (ejs->traceLevel >= 1){
      printf("EJS exceptionMessage:\n%s\n",nativeMessage);
      dumpbuffer(message,strlen(message));
    }
    ejsDumpError(ctx,exception);
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
    char *buf;
    int ret, eval_flags;
    size_t bufferLength;
    if (ejs->fileEvalHeap){
      SLHFree(ejs->fileEvalHeap);
    }
    ejs->fileEvalHeap = makeShortLivedHeap(0x10000,0x100); /* hacky constants, I know */
    size_t sourceLen = strlen(filename);
    char asciiFilename[sourceLen + 1];
    snprintf (asciiFilename, sourceLen + 1, "%.*s", (int)sourceLen, filename);
    convertFromNative(asciiFilename, sourceLen);
 
    buf = (char*)js_load_file(ejs->ctx, &bufferLength, asciiFilename);
    if (!buf) {
        perror(filename);
        return errno;
    } else{
      convertFromNative(buf,(int)bufferLength);
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
    JSValue evalResult = ejsEvalBuffer1(ejs, buf, bufferLength, asciiFilename, eval_flags, &ret);
    js_free(ejs->ctx, buf);
    JS_FreeValue(ejs->ctx, evalResult);
    return ret;
}

/* Some conveniences for building 'native' modules */

static JSValue makeObjectAndErrorArray(JSContext *ctx,
				       JSValue obj,
				       int err){
  JSValue arr;
  if (JS_IsException(obj))
    return obj;
  arr = JS_NewArray(ctx);
  if (JS_IsException(arr))
    return JS_EXCEPTION;
  JS_DefinePropertyValueUint32(ctx, arr, 0, obj,
			       JS_PROP_C_W_E);
  JS_DefinePropertyValueUint32(ctx, arr, 1, JS_NewInt32(ctx, err),
			       JS_PROP_C_W_E);
  return arr;
}

static JSValue makeStatusAndErrnoArray(JSContext *ctx,
				       int status,
				       int errorNumber){
  JSValue arr = JS_NewArray(ctx);
  if (JS_IsException(arr))
    return JS_EXCEPTION;
  JS_DefinePropertyValueUint32(ctx, arr, 0, JS_NewInt32(ctx, status),
			       JS_PROP_C_W_E);
  JS_DefinePropertyValueUint32(ctx, arr, 1, JS_NewInt32(ctx, errorNumber),
			       JS_PROP_C_W_E);
  return arr;
}

static JSValue makeStatusErrnoAndDetailArray(JSContext *ctx,
                                             int status,
                                             int errorNumber,
                                             int errorDetail){
  JSValue arr = JS_NewArray(ctx);
  if (JS_IsException(arr))
    return JS_EXCEPTION;
  JS_DefinePropertyValueUint32(ctx, arr, 0, JS_NewInt32(ctx, status),
			       JS_PROP_C_W_E);
  JS_DefinePropertyValueUint32(ctx, arr, 1, JS_NewInt32(ctx, errorNumber),
			       JS_PROP_C_W_E);
  JS_DefinePropertyValueUint32(ctx, arr, 2, JS_NewInt32(ctx, errorDetail),
			       JS_PROP_C_W_E);
  return arr;
}

#define NATIVE_STR(name,nativeName,i) const char *name; \
  size_t name##Len; \
  name = JS_ToCStringLen(ctx, &name##Len, argv[i]); \
  if (!name) return JS_EXCEPTION; \
  char nativeName[ name##Len + 1 ]; \
  memcpy(nativeName, name, name##Len+1); \
  convertToNative(nativeName, (int) name##Len); 

/* zos module */

static JSValue zosChangeTag(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv){
  size_t len;
  const char *pathname = JS_ToCStringLen(ctx, &len, argv[0]);
  if (!pathname){
    return JS_EXCEPTION;
  }
  /* 
     Since target function is QASCII, do NOT convert the path 
   */

  int ccsidInt = 0;
  JS_ToInt32(ctx, &ccsidInt, argv[1]);
  unsigned short ccsid = (unsigned short)ccsidInt;
#ifdef __ZOWE_OS_ZOS
  int status = tagFile(pathname, ccsid);
#else
  int status = -1;
#endif
  JS_FreeCString(ctx,pathname);
  return JS_NewInt64(ctx,(int64_t)status);
}

static JSValue zosChangeExtAttr(JSContext *ctx, JSValueConst this_val,
				int argc, JSValueConst *argv){
  size_t len;
  const char *pathname = JS_ToCStringLen(ctx, &len, argv[0]);
  if (!pathname){
    return JS_EXCEPTION;
  }
  /* 
     Since target function is QASCII, do NOT convert the path 
   */

  int extattrInt = 0;
  JS_ToInt32(ctx, &extattrInt, argv[1]);
  unsigned char extattr = (unsigned char)extattrInt;
  int onOffInt = JS_ToBool(ctx, argv[2]);
#ifdef __ZOWE_OS_ZOS
  int status = changeExtendedAttributes(pathname, extattr, onOffInt ? true : false);
#else
  int status = -1;
#endif
  JS_FreeCString(ctx,pathname);
  return JS_NewInt64(ctx,(int64_t)status);
}

static void ejsDefinePropertyValueStr(JSContext *ctx, JSValueConst obj, 
				      const char *propertyName, JSValue value, int flags){
  int len = strlen(propertyName);
  char asciiName[len+1];
  memcpy(asciiName,propertyName,len+1);
  convertFromNative(asciiName,len);
  JS_DefinePropertyValueStr(ctx, obj, asciiName, value, flags);
}
  

static JSValue zosChangeStreamCCSID(JSContext *ctx, JSValueConst this_val,
				    int argc, JSValueConst *argv){
  int fd;
  JS_ToInt32(ctx, &fd, argv[0]);

  int ccsidInt = 0;
  JS_ToInt32(ctx, &ccsidInt, argv[1]);
  unsigned short ccsid = (unsigned short)ccsidInt;
#ifdef __ZOWE_OS_ZOS
  int status = convertOpenStream(fd, ccsid);
#else
  int status = -1;
#endif
  return JS_NewInt64(ctx,(int64_t)status);
}

/* return [obj, errcode] */
static JSValue zosStat(JSContext *ctx, JSValueConst this_val,
		       int argc, JSValueConst *argv){
  const char *path;
  int err, res;
  struct stat st;
  JSValue obj;
  size_t len;

  path = JS_ToCStringLen(ctx, &len, argv[0]);
  if (!path){
    return JS_EXCEPTION;
  }

  char pathNative[len+1];
  memcpy(pathNative,path,len+1);
  convertToNative(pathNative,len);

  res = stat(pathNative, &st);
  
  JS_FreeCString(ctx, path);
  if (res < 0){
    err = errno;
    obj = JS_NULL;
  } else{
    err = 0;
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj)){
      return JS_EXCEPTION;
    }
    ejsDefinePropertyValueStr(ctx, obj, "dev",
			      JS_NewInt64(ctx, st.st_dev),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "ino",
			      JS_NewInt64(ctx, st.st_ino),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "mode",
			      JS_NewInt32(ctx, st.st_mode),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "nlink",
			      JS_NewInt64(ctx, st.st_nlink),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "uid",
			      JS_NewInt64(ctx, st.st_uid),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "gid",
			      JS_NewInt64(ctx, st.st_gid),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "rdev",
			      JS_NewInt64(ctx, st.st_rdev),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "size",
			      JS_NewInt64(ctx, st.st_size),
			      JS_PROP_C_W_E);
#if !defined(_WIN32)
    ejsDefinePropertyValueStr(ctx, obj, "blocks",
			      JS_NewInt64(ctx, st.st_blocks),
			      JS_PROP_C_W_E);
#endif
#if defined(_WIN32) || defined(__MVS__) /* JOENemo */
    ejsDefinePropertyValueStr(ctx, obj, "atime",
			      JS_NewInt64(ctx, (int64_t)st.st_atime * 1000),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "mtime",
			      JS_NewInt64(ctx, (int64_t)st.st_mtime * 1000),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "ctime",
			      JS_NewInt64(ctx, (int64_t)st.st_ctime * 1000),
			      JS_PROP_C_W_E);
#elif defined(__APPLE__)
    ejsDefinePropertyValueStr(ctx, obj, "atime",
			      JS_NewInt64(ctx, timespec_to_ms(&st.st_atimespec)),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "mtime",
			      JS_NewInt64(ctx, timespec_to_ms(&st.st_mtimespec)),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "ctime",
			      JS_NewInt64(ctx, timespec_to_ms(&st.st_ctimespec)),
			      JS_PROP_C_W_E);
#else
    ejsDefinePropertyValueStr(ctx, obj, "atime",
			      JS_NewInt64(ctx, timespec_to_ms(&st.st_atim)),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "mtime",
			      JS_NewInt64(ctx, timespec_to_ms(&st.st_mtim)),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "ctime",
			      JS_NewInt64(ctx, timespec_to_ms(&st.st_ctim)),
			      JS_PROP_C_W_E);
#endif
    /* Non-standard, not-even-close-to-standard attributes. */
#ifdef __ZOWE_OS_ZOS
    ejsDefinePropertyValueStr(ctx, obj, "extattrs",
			      JS_NewInt64(ctx, (int64_t)st.st_genvalue&EXTATTR_MASK),
			      JS_PROP_C_W_E);
    struct file_tag *tagInfo = &st.st_tag;
    ejsDefinePropertyValueStr(ctx, obj, "isText",
			      JS_NewBool(ctx, tagInfo->ft_txtflag),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "ccsid",
			      JS_NewInt64(ctx, (int64_t)(tagInfo->ft_ccsid)),
			      JS_PROP_C_W_E);
#endif
  }
  return makeObjectAndErrorArray(ctx, obj, err);
}

static const char changeTagASCII[10] ={ 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65, 
					0x54, 0x61, 0x67, 0x00};
static const char changeExtAttrASCII[14] ={ 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65, 
					    0x45, 0x78, 0x74, 0x41, 0x74, 0x74, 0x72, 0x00};
static const char changeStreamCCSIDASCII[18] ={ 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65, 
						0x53, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x43, 0x43, 0x53, 0x49, 0x44, 0x00};
static const char zstatASCII[6] ={ 0x7A, 0x73, 0x74, 0x61, 0x74, 0};

static const char EXTATTR_SHARELIB_ASCII[17] = { 0x45, 0x58, 0x54, 0x41, 0x54, 0x54, 0x52, 0x5f,
						 0x53, 0x48, 0x41, 0x52, 0x45, 0x4c, 0x49, 0x42, 0x0};

static const char EXTATTR_PROGCTL_ASCII[16] = { 0x45, 0x58, 0x54, 0x41, 0x54, 0x54, 0x52, 0x5f,
						0x50, 0x52, 0x4f, 0x47, 0x43, 0x54, 0x4c, 0x00};

#ifndef __ZOWE_OS_ZOS
/* stub the constants that non-ZOS does not define */
#define EXTATTR_SHARELIB 1
#define EXTATTR_PROGCTL 1
#endif

static const JSCFunctionListEntry zosFunctions[] = {
  JS_CFUNC_DEF(changeTagASCII, 2, zosChangeTag),
  JS_CFUNC_DEF(changeExtAttrASCII, 3, zosChangeExtAttr),
  JS_CFUNC_DEF(changeStreamCCSIDASCII, 2, zosChangeStreamCCSID),
  JS_CFUNC_DEF(zstatASCII, 1, zosStat),
  JS_PROP_INT32_DEF(EXTATTR_SHARELIB_ASCII, EXTATTR_SHARELIB, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF(EXTATTR_PROGCTL_ASCII, EXTATTR_PROGCTL, JS_PROP_CONFIGURABLE ),
  /* ALSO, "cp" with magic ZOS-unix see fopen not fileOpen */
};


static int ejsInitZOSCallback(JSContext *ctx, JSModuleDef *m){

  /* JSValue = proto = JS_NewObject(ctx); */
  JS_SetModuleExportList(ctx, m, zosFunctions, countof(zosFunctions));
  return 0;
}

JSModuleDef *ejsInitModuleZOS(JSContext *ctx, const char *module_name){
  JSModuleDef *m;
  m = JS_NewCModule(ctx, module_name, ejsInitZOSCallback);
  if (!m){
    return NULL;
  }
  JS_AddModuleExportList(ctx, m, zosFunctions, countof(zosFunctions));

  return m;
}

/*** POSIX Module - low level services not in the QuickJS os module.  ***/

static JSValue posixMessageSend(JSContext *ctx, JSValueConst this_val,
				int argc, JSValueConst *argv){
#ifdef __ZOWE_OS_WINDOWS
  return makeStatusAndErrnoArray(ctx, -1, EPERM);
#else
  int qid = 0;
  JS_ToInt32(ctx, &qid, argv[0]);
  size_t bufSize = 0;
  uint8_t *buf = JS_GetArrayBuffer(ctx, &bufSize, argv[1]);
  int msgSizeInt = 0;
  JS_ToInt32(ctx, &msgSizeInt, argv[2]);
  int flags;
  JS_ToInt32(ctx, &flags, argv[3]);
  
  if (!buf){
    return JS_EXCEPTION;
  }
  int status = msgsnd(qid, buf, (size_t)msgSizeInt, flags);
  return makeStatusAndErrnoArray(ctx, status, (status < 0 ? errno : 0));
#endif
}

static JSValue posixMessageReceive(JSContext *ctx, JSValueConst this_val,
				   int argc, JSValueConst *argv){
#ifdef __ZOWE_OS_WINDOWS
  return makeStatusAndErrnoArray(ctx, -1, EPERM);
#else
  int qid = 0;
  JS_ToInt32(ctx, &qid, argv[0]);
  size_t bufSize = 0;
  uint8_t *buf = JS_GetArrayBuffer(ctx, &bufSize, argv[1]);
  int msgSizeInt = 0;
  JS_ToInt32(ctx, &msgSizeInt, argv[2]);
  int msgType;
  JS_ToInt32(ctx, &msgType, argv[3]);
  int flags;
  JS_ToInt32(ctx, &flags, argv[4]);

  if (!buf){
    return JS_EXCEPTION;
  }

  int status = msgrcv(qid, buf, (size_t)msgSizeInt, (long)msgType, flags);

  return makeStatusAndErrnoArray(ctx, status, (status < 0 ? errno : 0));  
#endif
}

static JSValue posixMessageControl(JSContext *ctx, JSValueConst this_val,
				   int argc, JSValueConst *argv){
  return JS_EXCEPTION;
}

static JSValue posixMessageGet(JSContext *ctx, JSValueConst this_val,
			       int argc, JSValueConst *argv){
#ifdef __ZOWE_OS_WINDOWS
  return makeStatusAndErrnoArray(ctx, -1, EPERM);
#else
  int key = 0;
  JS_ToInt32(ctx, &key, argv[0]);
  int flags = 0;
  JS_ToInt32(ctx, &key, argv[1]);

  int status = msgget((key_t)key, flags);
  return makeStatusAndErrnoArray(ctx, status, (status < 0 ? errno : 0));  
#endif 
}

  

/* return [obj, errcode] */
static JSValue posixChmod(JSContext *ctx, JSValueConst this_val,
			  int argc, JSValueConst *argv){
#ifdef __ZOWE_OS_WINDOWS
  return JS_NewInt64(ctx,(int64_t)EPERM);
#else

  NATIVE_STR(path,pathNative,0);
  
  int mode;
  JS_ToInt32(ctx, &mode, argv[1]);

  int result = chmod(pathNative, (mode_t)mode);

  JS_FreeCString(ctx,path);
  
  return JS_NewInt64(ctx,(int64_t)result);
#endif
}

/* iconv_t iconv_open(const char *tocode, const char *fromcode);
   int iconv_close(iconv_t cd);
size_t iconv(iconv_t cd, char **__restrict__ inbuf,
size_t *__restrict__ inbytesleft, char **__restrict__ outbuf,
size_t *__restrict__ outbytesleft);

    https://gist.github.com/hakre/4188459 Iconv charset names
    "UTF-8"
    "00037"
    "01047"
   */

static JSValue posixIconvOpen(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv){
#ifdef __ZOWE_OS_WINDOWS
  return makeStatusAndErrnoArray(ctx, -1, EPERM);
#else 
  
  return JS_EXCEPTION;
#endif
}

static char msgsndASCII[7] ={ 0x6d, 0x73, 0x67, 0x73, 0x6e, 0x64,  0x00};
static char msgrcvASCII[7] ={ 0x6d, 0x73, 0x67, 0x72, 0x63, 0x76,  0x00};
static char msgctlASCII[7] ={ 0x6d, 0x73, 0x67, 0x63, 0x74, 0x6c,  0x00};
static char msggetASCII[7] ={ 0x6d, 0x73, 0x67, 0x67, 0x65, 0x74,  0x00};
static char chmodASCII[6] ={ 0x63, 0x68, 0x6d, 0x6f, 0x6d, 0x00};

static char IPC_PRIVATE_ASCII[12] ={ 0x49, 0x50, 0x43, 0x5f, 0x50, 0x52, 0x49, 0x56, 0x41, 0x54, 0x45,  0x00};
static char IPC_CREAT_ASCII[10] ={ 0x49, 0x50, 0x43, 0x5f, 0x43, 0x52, 0x45, 0x41, 0x54,  0x00};
static char IPC_EXCL_ASCII[9] = { 0x49, 0x50, 0x43, 0x5f, 0x45, 0x58, 0x43, 0x4c,  0x00};
static char IPC_NOWAIT_ASCII[11] ={0x49, 0x50, 0x43, 0x5f, 0x4e, 0x4f, 0x57, 0x41, 0x49, 0x54,  0x00};
static char MSG_EXCEPT_ASCII[11] ={0x4d, 0x53, 0x47, 0x5f, 0x45, 0x58, 0x43, 0x45, 0x50, 0x54,  0x00};
static char MSG_NOERROR_ASCII[12] ={0x4d, 0x53, 0x47, 0x5f, 0x4e, 0x4f, 0x45, 0x52, 0x52, 0x4f, 0x52,  0x00};

/* ZOS Might not define all constants ??  (or not might not implement) */
#ifndef MSG_EXCEPT
#define MSG_EXCEPT      020000  /* recv any msg except of specified type.*/
#endif

/* #define MSG_COPY        040000 */ /* copy (not remove) all queue messages */

static const JSCFunctionListEntry posixFunctions[] = {
  /* Sean wants...
     configmgr->env
     tr
     awk 
     curl 
       sockets
       httpclient
     netstat for zos, posix.netstat() might call zos.netstat(), etc
     uname 
     hostname
     dig 
   */
  JS_CFUNC_DEF(msgsndASCII, 4, posixMessageSend),
  JS_CFUNC_DEF(msgrcvASCII, 5, posixMessageReceive),
  JS_CFUNC_DEF(msgctlASCII, 3, posixMessageControl),
  JS_CFUNC_DEF(msggetASCII, 2, posixMessageGet),
  JS_CFUNC_DEF(chmodASCII, 2, posixChmod),
#ifndef __ZOWE_OS_WINDOWS
  JS_PROP_INT32_DEF(IPC_PRIVATE_ASCII, IPC_PRIVATE, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF(IPC_CREAT_ASCII, IPC_CREAT, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF(IPC_EXCL_ASCII, IPC_EXCL, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF(IPC_NOWAIT_ASCII, IPC_NOWAIT, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF(MSG_EXCEPT_ASCII, MSG_EXCEPT, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF(MSG_NOERROR_ASCII, MSG_NOERROR, JS_PROP_CONFIGURABLE ),
#endif
};

static int ejsInitPOSIXCallback(JSContext *ctx, JSModuleDef *m){
  JS_SetModuleExportList(ctx, m, posixFunctions, countof(posixFunctions));
  return 0;
}

JSModuleDef *ejsInitModulePOSIX(JSContext *ctx, const char *module_name){
  JSModuleDef *m;
  m = JS_NewCModule(ctx, module_name, ejsInitPOSIXCallback);
  if (!m){
    return NULL;
  }
  JS_AddModuleExportList(ctx, m, posixFunctions, countof(posixFunctions));

  return m;
}

/* XPlatform (Cross Platform) Support Module 

   This module exposes various non-POSIX useful features that have
   very similar behavior on zos/linux/windows as already supported by
   the zowe-common-c libraries.
   
 */

static JSValue xplatformFileCopy(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv){
  NATIVE_STR(source,sourceNative,0);
  NATIVE_STR(destination,destinationNative,1);

  int returnCode = 0;
  int reasonCode = 0;
  int status = fileCopy(sourceNative, destinationNative, &returnCode, &reasonCode);

  JS_FreeCString(ctx, source);
  JS_FreeCString(ctx, destination);

  return makeStatusErrnoAndDetailArray(ctx, status, returnCode, reasonCode);
}

static JSValue xplatformFileCopyConverted(JSContext *ctx, JSValueConst this_val,
					  int argc, JSValueConst *argv){
  NATIVE_STR(source,sourceNative,0);
  int sourceCCSID = 0;
  JS_ToInt32(ctx, &sourceCCSID, argv[1]);
  NATIVE_STR(destination,destinationNative,2);
  int destinationCCSID = 0;
  JS_ToInt32(ctx, &destinationCCSID, argv[3]);

  int returnCode = 0;
  int reasonCode = 0;
  int status = fileCopyConverted(sourceNative, destinationNative, 
				 sourceCCSID, destinationCCSID,
				 &returnCode, &reasonCode);

  JS_FreeCString(ctx, source);
  JS_FreeCString(ctx, destination);

  return makeStatusErrnoAndDetailArray(ctx, status, returnCode, reasonCode);
}

static char fileCopyASCII[9] = {0x66, 0x69, 0x6c, 0x65, 0x43, 0x6f, 0x70, 0x79,  0x00 };
static char fileCopyConvertedASCII[18] = {0x66, 0x69, 0x6c, 0x65, 0x43, 0x6f, 0x70, 0x79, 
					  0x43, 0x6f, 0x6e, 0x76, 0x65, 0x72, 0x74, 0x65, 0x64, 0x00 };

static const JSCFunctionListEntry xplatformFunctions[] = {
  JS_CFUNC_DEF(fileCopyASCII, 2, xplatformFileCopy),
  JS_CFUNC_DEF(fileCopyConvertedASCII, 2, xplatformFileCopyConverted),
};

static int ejsInitXPlatformCallback(JSContext *ctx, JSModuleDef *m){
  JS_SetModuleExportList(ctx, m, xplatformFunctions, countof(xplatformFunctions));
  return 0;
}

JSModuleDef *ejsInitModuleXPlatform(JSContext *ctx, const char *module_name){
  JSModuleDef *m;
  m = JS_NewCModule(ctx, module_name, ejsInitXPlatformCallback);
  if (!m){
    return NULL;
  }
  JS_AddModuleExportList(ctx, m, xplatformFunctions, countof(xplatformFunctions));

  return m;
}

/* Temporary experimental extension module */


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

#define MAX_CLEANUPS 10

#define CLEANUP_STRING 1

typedef struct InvocationCleanup_tag{
  void *pointer;
  int   type;
} InvocationCleanup;

struct EJSNativeInvocation_tag {
  EmbeddedJS *ejs;
  EJSNativeMethod *method;
  int argc;
  JSValueConst *argv;
  JSValue returnValue;
  int cleanupCount;
  InvocationCleanup cleanups[MAX_CLEANUPS];
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
  const char *cString = JS_ToCStringLen(invocation->ejs->ctx, &len, arg);
#ifdef __ZOWE_OS_ZOS
  char *nativeString = safeMalloc((int)len+1,"EJSStringArg");
  memcpy(nativeString,cString,len+1);
  convertToNative(nativeString,len);
  *valuePtr = nativeString;
  /* currently leaking until placed in invocation cleanups */
#else
  *valuePtr = cString;
#endif
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
  memset(&invocation,0,sizeof(EJSNativeInvocation));
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

static char asciiPrototype[10] = { 0x70, 0x72, 0x6f, 0x74, 0x6f,
				   0x74, 0x79, 0x70, 0x65, 0};

static JSValue thingy_ctor(JSContext *ctx, JSValueConst new_target,
                           int argc, JSValueConst *argv){
  JSValue obj = JS_UNDEFINED, proto;
  CThingy *t1;
  
  /* create the object */
  if (JS_IsUndefined(new_target)) {
    printf("thingy ctor undefined\n");
    proto = JS_GetClassProto(ctx, js_experiment_thingy_class_id);
  } else {
    proto = JS_GetPropertyStr(ctx, new_target, asciiPrototype);
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
static char asciiZOS[4] = { 0x7a, 0x6F, 0x73, 0};
static char asciiPosix[6] = { 0x70, 0x6F, 0x73, 0x69, 0x78, 0};
static char asciiXPlatform[10] = { 0x78, 0x70, 0x6C, 0x61, 0x74, 0x66, 0x6F, 0x72, 0x6D, 0};

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
    proto = JS_GetPropertyStr(ctx, new_target, asciiPrototype);
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
    printf("*** EJS PANIC *** could not get class internals in native ctor dispatch protoPointer=0x%p\n",protoPointer);
    goto fail;
  }
  EJSNativeClass *nativeClass = classInternals->nativeClass;
  obj = JS_NewObjectProtoClass(ctx, proto, classInternals->classID);
  JS_FreeValue(ctx, proto);
  if (JS_IsException(obj)){
    goto fail;
  }
  EJSNativeInvocation invocation;
  memset(&invocation,0,sizeof(EJSNativeInvocation));
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
    convertFromNative(nativeMethod->asciiName, mnameLen);

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
    printf("JOE putting proto pointer for '%s' as proto=0x%p, nCI=0x%p\n",nativeClass->name,protoPointer,nativeClassInternals);
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
  convertFromNative(asciiName, nameLen);

  ArrayList *classes = &(nativeModule->classes);
  for (int i=0; i<classes->size; i++){
    EJSNativeClass *nativeClass = (EJSNativeClass*)arrayListElement(classes,i);
    size_t cnameLen = strlen(nativeClass->name);
    nativeClass->asciiName = safeMalloc(cnameLen+1, "NativeAsciiClassName");
    snprintf(nativeClass->asciiName, cnameLen + 1, "%.*s", (int)cnameLen, nativeClass->name);
    convertFromNative(nativeClass->asciiName, cnameLen);
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
  ejsInitModuleZOS(ctx, asciiZOS);
  ejsInitModulePOSIX(ctx, asciiPosix);
  ejsInitModuleXPlatform(ctx, asciiXPlatform);
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

#define MAX_PRINT_ELT_SIZE 0x10000

static JSValue js_native_print(JSContext *ctx, JSValueConst this_val,
			       int argc, JSValueConst *argv) {
  int i;
  const char *str;
  size_t len;
  char *native = safeMalloc(MAX_PRINT_ELT_SIZE,"native print");
  
  for(i = 0; i < argc; i++){
    if (i != 0){
      putchar(' ');
    }
    str = JS_ToCStringLen(ctx, &len, argv[i]);
    if (!str){
      safeFree(native,MAX_PRINT_ELT_SIZE);
      return JS_EXCEPTION;
    }
    size_t printLen = min(len,MAX_PRINT_ELT_SIZE);
    memcpy(native,str,printLen);
    convertToNative(native,printLen);
    fwrite(native, 1, printLen, stdout);
    JS_FreeCString(ctx, str);
  }
  putchar('\n');
  safeFree(native,MAX_PRINT_ELT_SIZE);
  return JS_UNDEFINED;
}

static char asciiConsole[8] = { 0x63, 0x6f, 0x6e, 0x73, 0x6f, 0x6c, 0x65, 0x00 };
static char asciiLog[4] = { 0x6c, 0x6f, 0x67, 0x00};

JSModuleDef *ejsModuleLoader(JSContext *ctx,
			     const char *module_name, void *opaque) {
  JSModuleDef *m;

  if (has_suffix(module_name, ".so")){
    m = js_module_loader(ctx, module_name, NULL);
  } else{
    size_t buf_len;
    uint8_t *buf;
    JSValue func_val;
    
    buf = js_load_file(ctx, &buf_len, module_name);
    if (!buf){
      fprintf(stderr,"Could not load module name '%s'\n",module_name);
      fflush(stderr);
      JS_ThrowReferenceError(ctx, "could not load module filename '%s'",
			     module_name);
      return NULL;
    } else {
      convertFromNative((char*)buf, (int)buf_len);
    }

    /* compile the module */
    func_val = JS_Eval(ctx, (char *)buf, buf_len, module_name,
		       JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    js_free(ctx, buf);
    if (JS_IsException(func_val))
      return NULL;
    /* XXX: could propagate the exception */
    js_module_set_import_meta(ctx, func_val, TRUE, FALSE);
    /* the module is already referenced, so we must free it */
    m = JS_VALUE_GET_PTR(func_val);
    JS_FreeValue(ctx, func_val);
  }
  return m;
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
  JS_SetModuleLoaderFunc(embeddedJS->rt, NULL, ejsModuleLoader, NULL);
  
  if (false){ /* dump_unhandled_promise_rejection) { - do this later */
    JS_SetHostPromiseRejectionTracker(embeddedJS->rt, js_std_promise_rejection_tracker,
                                      NULL);
  }
  js_std_add_helpers(ctx, argc, copyArrayFromNative(argc, argv, NULL));
#ifdef __ZOWE_OS_ZOS
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue console = JS_GetPropertyStr(ctx, global_obj, asciiConsole);
  if (embeddedJS->traceLevel >= 1){
    printf("found console to override log method = 0x%p\n",JS_VALUE_GET_PTR(console));
  }
  JS_SetPropertyStr(ctx, console, asciiLog,
		    JS_NewCFunction(ctx, js_native_print, asciiLog, 1));
#endif

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

