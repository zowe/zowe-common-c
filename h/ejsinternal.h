/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_EJSINTERNAL__
#define __ZOWE_EJSINTERNAL__ 1

#include "zowetypes.h"
#include "cutils.h"
#include "quickjs-libc.h"


JSValue ejsMakeObjectAndErrorArray(JSContext *ctx,
				   JSValue obj,
				   int err);

void ejsDefinePropertyValueStr(JSContext *ctx, JSValueConst obj, 
			       const char *propertyName, JSValue value, int flags);

JSValue newJSStringFromNative(JSContext *ctx, char *str, int len);
char *nativeString(JSContext *ctx, JSValue str, ShortLivedHeap *heapOrNull);

#define NATIVE_STR(name,nativeName,i) const char *name; \
  size_t name##Len; \
  name = JS_ToCStringLen(ctx, &name##Len, argv[i]); \
  if (!name) return JS_EXCEPTION; \
  char nativeName[ name##Len + 1 ]; \
  memcpy(nativeName, name, name##Len+1); \
  convertToNative(nativeName, (int) name##Len); 


#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
