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
#include "xlate.h"
#include "charsets.h"
#include "embeddedjs.h"
#include "ejsinternal.h"
#include "http.h"
#include "httpclient.h"
#include "tls.h"
#include "qjsnet.h"

#ifdef __ZOWE_OS_ZOS

#include "porting/polyfill.h"

#endif

#include "nativeconversion.h"

/* zos module - moved to its own file because it may grow a lot */


static JSValue netTCPPing(JSContext *ctx, JSValueConst this_val,
			  int argc, JSValueConst *argv){
  if (argc != 3){
    return JS_EXCEPTION;
  }
  NATIVE_STR(hostname,nativeHostname,0);
  if (!hostname){
    return JS_EXCEPTION;
  }
  int port;
  JS_ToInt32(ctx, &port, argv[1]);

  int waitMS;
  JS_ToInt32(ctx, &waitMS, argv[2]);
  
  JSValue result = JS_NewObject(ctx);
  if (JS_IsException(result)){
    return JS_EXCEPTION;
  }

  bool ok = true;
  char *message = NULL;
  InetAddr *serverIPAddr = getAddressByName(nativeHostname);
  if (NULL == serverIPAddr || serverIPAddr->data.data4.addrBytes == 0){
    message = "unknown or malformed host address";
    ok = false;
  }
  SocketAddress *serverAddress = NULL;
  if (ok){
    serverAddress = makeSocketAddr(serverIPAddr, port);
    if (NULL == serverAddress) {
      message = "failed to make socket address";
      ok = false;
    }
  }
  Socket *socket = NULL;
  int returnCode = 0;
  int reasonCode = 0;

  if (ok){
    socket = tcpClient2(serverAddress, waitMS, &returnCode, &reasonCode);

    if (socket == NULL || returnCode != 0){
      message = "failed to connect socket to host";
      ejsDefinePropertyValueStr(ctx, result, "returnCode",JS_NewInt32(ctx, returnCode), JS_PROP_C_W_E);
      ejsDefinePropertyValueStr(ctx, result, "reasonCode",JS_NewInt32(ctx, reasonCode), JS_PROP_C_W_E);
      ok = false;
    }
  }
  if (message){
    ejsDefinePropertyValueStr(ctx, result, "error",
			      newJSStringFromNative(ctx, message, strlen(message)),
			      JS_PROP_C_W_E);
  }
  if (serverAddress){
    freeSocketAddr(serverAddress);
  }
  if (socket){
    socketClose(socket,&returnCode,&reasonCode);
    socketFree(socket);
  }
      
  return result;
}

static TlsSettings *defaultTLSSettings = NULL;

static char truststoreASCII[11] ={0x74, 0x72, 0x75, 0x73, 0x74, 0x73, 0x74, 0x6f, 0x72, 0x65,  0x00};
static char passwordASCII[9] ={0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64,  0x00};
static char stashASCII[6] ={0x73, 0x74, 0x61, 0x73, 0x68,  0x00};
static char labelASCII[6] ={0x6c, 0x61, 0x62, 0x65, 0x6c,  0x00};


static JSValue netTLSClientConfig(JSContext *ctx, JSValueConst this_val,
			       int argc, JSValueConst *argv){
  JSValue settings = argv[0];
  JSValueConst truststore = JS_GetPropertyStr(ctx, settings, truststoreASCII);
  JSValueConst password = JS_GetPropertyStr(ctx, settings, passwordASCII);
  JSValueConst stash = JS_GetPropertyStr(ctx, settings, stashASCII);
  JSValueConst label = JS_GetPropertyStr(ctx, settings, labelASCII);
  if (JS_IsUndefined(truststore)){
    return JS_EXCEPTION;
  }
  if (JS_IsUndefined(password) && JS_IsUndefined(stash)){
    return JS_EXCEPTION;
  }
  if (!JS_IsString(truststore) ||
      (!JS_IsUndefined(password) && !JS_IsString(password)) ||
      (!JS_IsUndefined(stash) && !JS_IsString(stash)) ||
      (!JS_IsUndefined(label) && !JS_IsString(label)) ){
    return JS_EXCEPTION;
  }
  if (defaultTLSSettings == NULL){
    defaultTLSSettings = (TlsSettings*)safeMalloc(sizeof(TlsSettings),"QJS Tls Settings");
  }
  defaultTLSSettings->keyring = nativeString(ctx,truststore,NULL);
  if (!JS_IsUndefined(password)){
    defaultTLSSettings->password = nativeString(ctx,password,NULL);
  }
  if (!JS_IsUndefined(stash)){
    defaultTLSSettings->stash = nativeString(ctx,stash,NULL);
  }
  if (!JS_IsUndefined(label)){
    defaultTLSSettings->label = nativeString(ctx,label,NULL);
  }
  return JS_NewInt64(ctx,(int64_t)0);
}

static bool httpTrace = false;

static int httpGet(bool isTLS,
		   char *host,
		   int port,
		   char *path,
		   char **content){
  TlsEnvironment *tlsEnv = NULL;
  HttpClientSettings clientSettings;
  HttpClientContext *httpClientContext = NULL;
  HttpClientSession *session = NULL;

  int status = 0;
  char buffer[2048];
  LoggingContext *loggingContext = getLoggingContext();
  int rc = 0;

  do{
    clientSettings.host = host;
    clientSettings.port = port;
    clientSettings.recvTimeoutSeconds = 10;

    if (isTLS){
      status = tlsInit(&tlsEnv, defaultTLSSettings);
      if (status){
	if (httpTrace){
	  printf ("failed to init tls environment, rc=%d (%s)\n", status, tlsStrError(status));
	}
      }
      status = httpClientContextInitSecure(&clientSettings, loggingContext, tlsEnv, &httpClientContext);
    } else{
      status = httpClientContextInit(&clientSettings, loggingContext, &httpClientContext);
    }

    if (status){
      if (httpTrace){
	printf("error in httpcb ctx init: %d\n", status);
      }
      break;
    }
    if (httpTrace){
      printf("successfully initialized http client\n");
    }
    status = httpClientSessionInit2(httpClientContext, &session, &rc);
    if (status){
      if (httpTrace){
	printf("error initing session: %d\n", status);
      }
      break;
    }

    if (httpTrace){
      printf("successfully inited session\n");
    }

    status = httpClientSessionStageRequest(httpClientContext, session, "GET", path, NULL, NULL, NULL, 0);
    if (status){
      if (httpTrace){
	printf("error staging request: %d\n", status);
      }
      break;
    }
    if (httpTrace){
      printf("successfully staged request\n");
    }
      
    status = httpClientSessionSend(httpClientContext, session);
    if (status){
      if (httpTrace){
	printf("error sending request: %d\n", status);
      }
      break;
    }
    if (httpTrace){
      printf("successfully sent request\n");
    }
    /* need to call receive native in a loop*/
    int quit = 0;
    while (!quit){
      status = httpClientSessionReceiveNative(httpClientContext, session, 1024);
      if (status != 0){
	if (httpTrace){
	  printf("error receiving request: %d\n", status);
	}
        break;
      }
      if (session->response != NULL){
	if (httpTrace){
	  printf("received valid response!\n");
	}
        quit = 1;
        break;
      }
      if (httpTrace){
	printf("looping\n");
      }
    }
    if (quit){
      char *response = safeMalloc(session->response->contentLength + 1, "response");
      memcpy(response,session->response->body, session->response->contentLength);
      response[session->response->contentLength] = '\0';
      *content = response;
    } else{
      *content = NULL;
    }
  } while (0);

  if (httpTrace){
    printf("httpGet ending with status: %d\n", status);
  }
  return status;
}


static JSValue netHttpContentAsText(JSContext *ctx, JSValueConst this_val,
				    int argc, JSValueConst *argv){
  if (argc != 4){
    return JS_EXCEPTION;
  }
  if (!JS_IsString(argv[0])){
    return JS_EXCEPTION;
  }
  NATIVE_STR(schema,nativeSchema,0);
  if (!JS_IsString(argv[1])){
    return JS_EXCEPTION;
  }
  NATIVE_STR(host,nativeHost,1);
  int port;
  if (!JS_IsNumber(argv[2])){
    return JS_EXCEPTION;
  }
  JS_ToInt32(ctx, &port, argv[2]);
  if (!JS_IsString(argv[3])){
    return JS_EXCEPTION;
  }
  NATIVE_STR(path,nativePath,3);

  int status = 0;
  bool isTLS = !strcmp(nativeSchema,"https");
  if (isTLS && (defaultTLSSettings == NULL)){
    status = HTTP_CLIENT_TLS_NOT_CONFIGURED;
  }

  char *content = NULL;
  if (!status){
    status = httpGet(isTLS,nativeHost,port,nativePath,&content);
  }

  JS_FreeCString(ctx,schema);
  JS_FreeCString(ctx,host);
  JS_FreeCString(ctx,path);

  JSValue result = (status ? 
		    JS_NULL :
		    JS_NewString(ctx, content));
  
  return ejsMakeObjectAndErrorArray(ctx, result, status);
}

static char tcpPingASCII[8] ={0x74, 0x63, 0x70, 0x50, 0x69, 0x6e, 0x67,  0x00};
static char tlsClientConfigASCII[16] ={0x74, 0x6c, 0x73, 0x43, 0x6c, 0x69, 0x65, 0x6e, 
				       0x74, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x00};
static char httpContentAsTextASCII[18] ={ 0x68, 0x74, 0x74, 0x70, 0x43, 0x6f, 0x6e, 0x74, 
                                          0x65, 0x6e, 0x74, 0x41, 0x73, 0x54, 0x65, 0x78, 
                                          0x74,  0x00};

static const JSCFunctionListEntry netFunctions[] = {
  JS_CFUNC_DEF(tcpPingASCII, 3, netTCPPing),
  JS_CFUNC_DEF(tlsClientConfigASCII, 1, netTLSClientConfig),
  JS_CFUNC_DEF(httpContentAsTextASCII, 4, netHttpContentAsText),
  
};

int ejsInitNetCallback(JSContext *ctx, JSModuleDef *m){

  /* JSValue = proto = JS_NewObject(ctx); */
  JS_SetModuleExportList(ctx, m, netFunctions, countof(netFunctions));
  return 0;
}

JSModuleDef *ejsInitModuleNet(JSContext *ctx, const char *moduleName){
  JSModuleDef *m;
  m = JS_NewCModule(ctx, moduleName, ejsInitNetCallback);
  if (!m){
    return NULL;
  }
  JS_AddModuleExportList(ctx, m, netFunctions, countof(netFunctions));
  return m;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

