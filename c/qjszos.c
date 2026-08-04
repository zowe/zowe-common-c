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
#include <sys/statvfs.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>

#endif

#include "zowetypes.h"
#include "zos.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "bpxnet.h"
#include "unixfile.h"
#include "json.h"
#include "charsets.h"
#include "embeddedjs.h"
#include "ejsinternal.h"
#include "jcsi.h"

#ifdef __ZOWE_OS_ZOS

#include "porting/polyfill.h"

#endif

#include "nativeconversion.h"

/* zos module - moved to its own file because it may grow a lot */

static JSValue zosResolveSymbol(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv){
  size_t len;
  const char *symbol = JS_ToCStringLen(ctx, &len, argv[0]);
  if (!symbol){
    return JS_EXCEPTION;
  }

#ifdef __ZOWE_OS_ZOS
  char symbolNative[len+1];
  memcpy(symbolNative,symbol,len+1);
  convertToNative(symbolNative,len);


  int rc = 0;
  int rsn = 0;
  char *result = resolveSymbol(symbolNative, &rc, &rsn);
#endif

  JS_FreeCString(ctx,symbol);

#ifdef __ZOWE_OS_ZOS
  if (result == NULL) {
    if (rc != 0) {
      return JS_ThrowReferenceError(ctx,
                                    "resolveSymbol failed for '%s' (rc=%d, rsn=%d)",
                                    symbolNative, rc, rsn);
    }
    return JS_NULL;
  }
  return newJSStringFromNative(ctx, result, strlen(result));
#else
  return JS_NewString(ctx, NULL);
#endif
}


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

static JSValue getZosVersion(JSContext *ctx, JSValueConst this_val,
				    int argc, JSValueConst *argv){

#ifdef __ZOWE_OS_ZOS
  ECVT *ecvt = getECVT();
  int version = ecvt->ecvtpseq;
#else
  int version = -1;
#endif
  return JS_NewInt64(ctx,(int64_t)version);
}


static JSValue getEsm(JSContext *ctx, JSValueConst this_val,
				    int argc, JSValueConst *argv){

#ifdef __ZOWE_OS_ZOS
  switch(getExternalSecurityManager()) {
  case ZOS_ESM_RTSS:
    return newJSStringFromNative(ctx, "TSS", 3);
  case ZOS_ESM_RACF:
    return newJSStringFromNative(ctx, "RACF", 4);
  case ZOS_ESM_ACF2:
    return newJSStringFromNative(ctx, "ACF2", 4);
  case ZOS_ESM_NONE:
    return newJSStringFromNative(ctx, "NONE", 4);
  default:
    return JS_NewString(ctx, NULL);
  }
#else
  return JS_NewString(ctx, NULL);
#endif

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
  return ejsMakeObjectAndErrorArray(ctx, obj, err);
}

static char *defaultCSIFields[] ={ "NAME    ", "TYPE    ", "VOLSER  "};
static int defaultCSIFieldCount = 3;

/* return [obj, errcode] */
static JSValue zosDatasetInfo(JSContext *ctx, JSValueConst this_val,
			      int argc, JSValueConst *argv){
  const char *dsn;
  JSValue result;
  size_t len;
  bool trace = false;

  dsn = JS_ToCStringLen(ctx, &len, argv[0]);
  if (!dsn){
    return JS_EXCEPTION;
  }

  if (trace){
    printf("dslist start\n");
  }
  char dsnNative[len+1];
  memcpy(dsnNative,dsn,len+1);
  convertToNative(dsnNative,len);

  if (trace){
    printf("dsn %s\n",dsnNative);
  }

  char *resumeNameArg = NULL; /* (resumeNameParam ? resumeNameParam->stringValue : NULL); */
  char *resumeCatalogNameArg = NULL; /* (resumeCatalogNameParam ? resumeCatalogNameParam->stringValue : NULL); */

  csi_parmblock * __ptr32 returnParms = (csi_parmblock* __ptr32)safeMalloc31(sizeof(csi_parmblock),"CSI ParmBlock");

  char *typesArg = "ADX";
  int datasetTypeCount = (typesArg == NULL) ? 3 : strlen(typesArg);
  int workAreaSizeArg = 0; /* (workAreaSizeParam ? workAreaSizeParam->intValue : 0); */
  int fieldCount = defaultCSIFieldCount;
  char **csiFields = defaultCSIFields;

  EntryDataSet *entrySet = returnEntries(dsnNative,
					 typesArg, datasetTypeCount,
					 workAreaSizeArg,
					 csiFields, fieldCount,
					 resumeNameArg, resumeCatalogNameArg, returnParms);
  if (trace){
    printf("entrySet=0x%p\n",entrySet);
  }
  if (!entrySet || entrySet->length == 0){
    return JS_NULL;
  }
  char *resumeName = returnParms->resume_name;
  char *catalogName = returnParms->catalog_name;
  int isResume = (returnParms->is_resume == 'Y');
  result = JS_NewObject(ctx);
  if (JS_IsException(result)){
    return JS_EXCEPTION;
  }
  if (isResume){
    ejsDefinePropertyValueStr(ctx, result, "resumeName",
			      newJSStringFromNative(ctx, resumeName, strlen(resumeName)),
			      JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, result, "resumeCatalogName",
			      newJSStringFromNative(ctx, catalogName, strlen(catalogName)),
			      JS_PROP_C_W_E);
  }

  char volser[7];
  memset(volser,0,7);

  if (trace){
    printf("%d datasets\n",entrySet->length);
  }
  JSValue datasetArray = JS_NewArray(ctx);
  if (JS_IsException(datasetArray)){
    return JS_EXCEPTION;
  }
  ejsDefinePropertyValueStr(ctx, result, "datasets", datasetArray, JS_PROP_C_W_E);

  int dsCount = 0;
  for (int i = 0; i < entrySet->length; i++){
    EntryData *entry = entrySet->entries[i];

    if (entry) {
      JSValue info = JS_NewObject(ctx);
      JS_DefinePropertyValueUint32(ctx, datasetArray, dsCount++, info, JS_PROP_C_W_E);
      int fieldDataLength = entry->data.fieldInfoHeader.totalLength;
      int entrySize = sizeof(EntryData)+fieldDataLength-4; /* -4 for the fact that the length is 4 from end of EntryData */
      int isMigrated = FALSE;
      /* jsonStartObject(jPrinter, NULL); */
      int dsnLength = sizeof(entry->name);
      char *datasetName = entry->name;
      if (trace){
	printf("%*.*s\n",dsnLength,dsnLength,datasetName);
      }
      ejsDefinePropertyValueStr(ctx, info, "dsn",
				newJSStringFromNative(ctx, datasetName, 44),
				JS_PROP_C_W_E);
      ejsDefinePropertyValueStr(ctx, info, "entryType",
				newJSStringFromNative(ctx, &entry->type, 1),
				JS_PROP_C_W_E);
    }
  }

  JS_FreeCString(ctx, dsn);

  return result;
}

/* return [obj, errcode] */
static JSValue getStatvfs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  const char *path;
  int res, err;
  struct statvfs statvfsBuffer;
  JSValue obj;
  size_t len;

  path = JS_ToCStringLen(ctx, &len, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  char pathNative[len + 1];
  memcpy(pathNative, path, len + 1);
  convertToNative(pathNative, len);

  res = statvfs(pathNative, &statvfsBuffer);
  JS_FreeCString(ctx, path);

  if (res < 0) {
    err = errno;
    obj = JS_NULL;
  } else {
    err = 0;
    obj = JS_NewObject(ctx);
    if (JS_IsException(obj)) {
      return JS_EXCEPTION;
    }
    ejsDefinePropertyValueStr(ctx, obj, "bsize", JS_NewInt64(ctx, statvfsBuffer.f_bsize), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "blocks", JS_NewInt64(ctx, statvfsBuffer.f_blocks), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "bavail", JS_NewInt64(ctx, statvfsBuffer.f_bavail), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "fsid", JS_NewInt64(ctx, statvfsBuffer.f_fsid), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "flag", JS_NewInt64(ctx, statvfsBuffer.f_flag), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "frsize", JS_NewInt64(ctx, statvfsBuffer.f_frsize), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "bfree", JS_NewInt64(ctx, statvfsBuffer.f_bfree), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "files", JS_NewInt64(ctx, statvfsBuffer.f_files), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "ffree", JS_NewInt64(ctx, statvfsBuffer.f_ffree), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "favail", JS_NewInt64(ctx, statvfsBuffer.f_favail), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "namemax", JS_NewInt64(ctx, statvfsBuffer.f_namemax), JS_PROP_C_W_E);
#ifdef __ZOWE_OS_ZOS
    ejsDefinePropertyValueStr(ctx, obj, "OEmaxfilesizehw", JS_NewInt32(ctx, statvfsBuffer.f_OEmaxfilesizehw), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "OEmaxfilesizelw", JS_NewInt64(ctx, statvfsBuffer.f_OEmaxfilesizelw), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "OEusedspace", JS_NewInt64(ctx, statvfsBuffer.f_OEusedspace), JS_PROP_C_W_E);
    ejsDefinePropertyValueStr(ctx, obj, "OEinvarsec", JS_NewInt64(ctx, statvfsBuffer.f_OEinvarsec), JS_PROP_C_W_E);
#endif
  }
  return ejsMakeObjectAndErrorArray(ctx, obj, err);
}


/*
 * zos.spawnProcInNewGroup(path, argv, envp)
 *
 * Spawns a process in a new process group so the entire tree can be
 * killed later with killProcessGroup().
 *
 * argv and envp are JS arrays of strings.
 *
 * Returns the PID (number) on success, or throws on failure.
 */
/*
 * Convert a JS array of strings into a NULL-terminated native (EBCDIC on z/OS)
 * C string array. Returns the array (caller must free with freeNativeCStringArray)
 * and writes the element count to *outLen. Returns NULL on error.
 */
static const char **jsArrayToNativeCStrings(JSContext *ctx, JSValueConst jsArr,
                                            int32_t *outLen, const char *tag) {
  int32_t len = 0;
  JSValue jsLen = JS_GetPropertyStr(ctx, jsArr, "length");
  JS_ToInt32(ctx, &len, jsLen);
  JS_FreeValue(ctx, jsLen);
  if (len < 0) len = 0;

  const char **arr = (const char **)safeMalloc((len + 1) * sizeof(char *), tag);
  for (int i = 0; i < len; i++) {
    JSValue elem = JS_GetPropertyUint32(ctx, jsArr, i);
    size_t elemLen = 0;
    const char *str = JS_ToCStringLen(ctx, &elemLen, elem);
    JS_FreeValue(ctx, elem);
    if (!str) {
      arr[i] = NULL;
      continue;
    }
    char *nativeStr = safeMalloc(elemLen + 1, tag);
    memcpy(nativeStr, str, elemLen + 1);
    convertToNative(nativeStr, elemLen);
    JS_FreeCString(ctx, str);
    arr[i] = nativeStr;
  }
  arr[len] = NULL;
  *outLen = len;
  return arr;
}

static void freeNativeCStringArray(const char **arr, int32_t len) {
  if (!arr) return;
  for (int i = 0; i < len; i++) {
    if (arr[i]) {
      safeFree((char *)arr[i], strlen(arr[i]) + 1);
    }
  }
  safeFree((char *)arr, (len + 1) * sizeof(char *));
}

static JSValue jsSpawnProcessInNewGroup(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv) {
  size_t pathLen = 0;
  const char *path = JS_ToCStringLen(ctx, &pathLen, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }
  char pathNative[pathLen + 1];
  memcpy(pathNative, path, pathLen + 1);
  convertToNative(pathNative, pathLen);
  JS_FreeCString(ctx, path);

  int32_t argsLen = 0;
  const char **c_args = jsArrayToNativeCStrings(ctx, argv[1], &argsLen, "spawnArgs");
  int32_t envLen = 0;
  const char **c_envp = jsArrayToNativeCStrings(ctx, argv[2], &envLen, "spawnEnvp");

#ifdef __ZOWE_OS_ZOS
  int fd_map[3] = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };
  pid_t pid = spawnProcessInNewGroup(pathNative, 3, fd_map,
                                     argsLen, c_args, c_envp);
#else
  pid_t pid = -1;
  errno = ENOSYS;
#endif

  freeNativeCStringArray(c_args, argsLen);
  freeNativeCStringArray(c_envp, envLen);

  if (pid == -1) {
    return JS_ThrowInternalError(ctx, "spawnProcessInNewGroup failed: %s", strerror(errno));
  }

  return JS_NewInt64(ctx, (int64_t)pid);
}

/*
 * zos.killProcessGroup(pid, signal)
 *
 * Sends a signal to the process group led by pid.
 * Returns 0 on success, or throws on failure.
 */
static JSValue jsKillProcessGroup(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
  int32_t pid = 0;
  int32_t sig = 0;
  JS_ToInt32(ctx, &pid, argv[0]);
  JS_ToInt32(ctx, &sig, argv[1]);

#ifdef __ZOWE_OS_ZOS
  int rc = killProcessGroup((pid_t)pid, sig);
#else
  int rc = -1;
  errno = ENOSYS;
#endif

  if (rc != 0) {
    return JS_ThrowInternalError(ctx, "killProcessGroup failed for pid %d: %s",
                                 pid, strerror(errno));
  }

  return JS_NewInt32(ctx, 0);
}

static const char changeTagASCII[10] ={ 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65,
					0x54, 0x61, 0x67, 0x00};
static const char changeExtAttrASCII[14] ={ 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65,
					    0x45, 0x78, 0x74, 0x41, 0x74, 0x74, 0x72, 0x00};
static const char changeStreamCCSIDASCII[18] ={ 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65,
						0x53, 0x74, 0x72, 0x65, 0x61, 0x6d, 0x43, 0x43, 0x53, 0x49, 0x44, 0x00};
static const char zstatASCII[6] ={ 0x7A, 0x73, 0x74, 0x61, 0x74, 0};

static const char getZosVersionASCII[14] ={ 0x67, 0x65, 0x74, 0x5A, 0x6F, 0x73, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6F, 0x6E, 0x00};

static const char getEsmASCII[7] = { 0x67, 0x65, 0x74, 0x45, 0x73, 0x6D, 0x00 };

static const char EXTATTR_SHARELIB_ASCII[17] = { 0x45, 0x58, 0x54, 0x41, 0x54, 0x54, 0x52, 0x5f,
						 0x53, 0x48, 0x41, 0x52, 0x45, 0x4c, 0x49, 0x42, 0x0};

static const char EXTATTR_PROGCTL_ASCII[16] = { 0x45, 0x58, 0x54, 0x41, 0x54, 0x54, 0x52, 0x5f,
						0x50, 0x52, 0x4f, 0x47, 0x43, 0x54, 0x4c, 0x00};

static const char dslistASCII[7] ={ 0x64, 0x73, 0x6c, 0x69, 0x73, 0x74, 0x00};

static const char resolveSymbolASCII[14] ={ 0x72, 0x65, 0x73, 0x6f, 0x6c, 0x76, 0x65, 0x53, 0x79, 0x6d, 0x62, 0x6f, 0x6c, 0x00};

static const char getStatvfsASCII[11] = {0x67, 0x65, 0x74, 0x53, 0x74, 0x61, 0x74, 0x76, 0x66, 0x73, 0x00};

/* "spawnProcInNewGroup" in ASCII */
static const char spawnProcInNewGroupASCII[20] = {
  0x73, 0x70, 0x61, 0x77, 0x6e, 0x50, 0x72, 0x6f, 0x63, 0x49,
  0x6e, 0x4e, 0x65, 0x77, 0x47, 0x72, 0x6f, 0x75, 0x70, 0x00
};

/* "killProcessGroup" in ASCII */
static const char killProcessGroupASCII[17] = {
  0x6b, 0x69, 0x6c, 0x6c, 0x50, 0x72, 0x6f, 0x63, 0x65, 0x73,
  0x73, 0x47, 0x72, 0x6f, 0x75, 0x70, 0x00
};

/* "SIGTERM" in ASCII */
static const char SIGTERM_ASCII[8] = {0x53, 0x49, 0x47, 0x54, 0x45, 0x52, 0x4d, 0x00};

/* "SIGKILL" in ASCII */
static const char SIGKILL_ASCII[8] = {0x53, 0x49, 0x47, 0x4b, 0x49, 0x4c, 0x4c, 0x00};


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
  JS_CFUNC_DEF(getZosVersionASCII, 0, getZosVersion),
  JS_CFUNC_DEF(getEsmASCII, 0, getEsm),
  JS_CFUNC_DEF(dslistASCII, 1, zosDatasetInfo),
  JS_CFUNC_DEF(resolveSymbolASCII, 1, zosResolveSymbol),
  JS_CFUNC_DEF(getStatvfsASCII, 1, getStatvfs),
  JS_CFUNC_DEF(spawnProcInNewGroupASCII, 3, jsSpawnProcessInNewGroup),
  JS_CFUNC_DEF(killProcessGroupASCII, 2, jsKillProcessGroup),
  JS_PROP_INT32_DEF(SIGTERM_ASCII, SIGTERM, JS_PROP_CONFIGURABLE),
  JS_PROP_INT32_DEF(SIGKILL_ASCII, SIGKILL, JS_PROP_CONFIGURABLE),
  JS_PROP_INT32_DEF(EXTATTR_SHARELIB_ASCII, EXTATTR_SHARELIB, JS_PROP_CONFIGURABLE ),
  JS_PROP_INT32_DEF(EXTATTR_PROGCTL_ASCII, EXTATTR_PROGCTL, JS_PROP_CONFIGURABLE ),
  /* ALSO, "cp" with magic ZOS-unix see fopen not fileOpen */
};


int ejsInitZOSCallback(JSContext *ctx, JSModuleDef *m){

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
  loadCsi();
  return m;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
