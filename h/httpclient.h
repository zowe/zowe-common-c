
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef HTTPCLIENT_H_
#define HTTPCLIENT_H_

#include "json.h"
#include "socketmgmt.h"
#include "http.h"
#include "logging.h"

#ifdef USE_ZOWE_TLS
#include "tls.h"
#endif // USE_ZOWE_TLS

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_CLIENT_INVALID_ARGUMENT      1
#define HTTP_CLIENT_OUTPUT_WOULD_OVERFLOW 2
#define HTTP_CLIENT_INVALID_PORT          3
#define HTTP_CLIENT_REQDSETTING_MISSING   4
#define HTTP_CLIENT_MSG_SETTING_MISSING   "One or more required HTTP client settings is missing"
#define HTTP_CLIENT_LOOKUP_FAILED         5
#define HTTP_CLIENT_CONNECT_FAILED        6
#define HTTP_CLIENT_MSG_CONNECT_FAILED    "Failed to connect to HTTP server"
#define HTTP_CLIENT_SESSION_ERR           7
#define HTTP_CLIENT_MSG_SESSION_ERR       "Failed to make HTTP client session"
#define HTTP_CLIENT_ADDRBYNAME_ERR        8
#define HTTP_CLIENT_SEND_ERROR            9
#define HTTP_CLIENT_MSG_SEND_ERROR        "Failed to send complete HTTP request"
#define HTTP_CLIENT_SOCK_UNREGISTERED     10
#define HTTP_CLIENT_SXREAD_ERROR          11
#define HTTP_CLIENT_NO_REQUEST            12
#define HTTP_CLIENT_NO_SOCKET             13
#define HTTP_CLIENT_RESP_PARSE_FAILED     14
#define HTTP_CLIENT_READ_ERROR            15
#define HTTP_CLIENT_RESPONSE_ZEROLEN      16
#define HTTP_CLIENT_TLS_ERROR             17
#define HTTP_CLIENT_TLS_NOT_CONFIGURED    18

typedef struct HttpClientSettings_tag {
  char *host;
  int port;
  int recvTimeoutSeconds; /* try next server on timeout */
} HttpClientSettings;

typedef struct HttpClientContext_tag {
  HttpClientSettings *settings;
  LoggingContext *logContext;
  SocketAddress *serverAddress;
  int recvTimeoutSeconds; /* try next server on timeout */
#ifdef USE_ZOWE_TLS
  TlsEnvironment *tlsEnvironment;
#endif
} HttpClientContext;

typedef struct HttpClientResponse_tag {
  int statusCode;
  int contentLength;
  ShortLivedHeap *slh;
  HttpHeader *headers;
  char *body;
} HttpClientResponse;

typedef struct HttpResponseParser_tag {
  ShortLivedHeap *slh;
  int state;
  int versionLength;
  char version[MAX_HTTP_VERSION + 8];
  int statusReasonLength;
  char statusReason[MAX_HTTP_VERSION + 8];
  int headerNameLength;
  char headerName[MAX_HTTP_FIELD_NAME + 8];
  int headerValueLength;
  char headerValue[MAX_HTTP_FIELD_VALUE + 8];
  HttpHeader *headerChain;
  int isChunked;
  char *contentType; /* native */
  int specifiedContentLength;
  int remainingContentLength; /* fixed-length body */
  int specifiedChunkLength;
  int remainingChunkLength;
  int contentSize; /* set when content buffer is allocated */
  char *content;
  int httpStatusCode;
  HttpClientResponse *resp;
} HttpResponseParser;

typedef struct HttpClientSession_tag {
  Socket *socket;
  ShortLivedHeap *slh;
  HttpRequest *request;
  HttpResponseParser *responseParser;
  HttpClientResponse *response;
} HttpClientSession;

void httpClientSettingsDestroy(HttpClientSettings *settings);
HttpClientSettings *httpClientSettingsCopy(HttpClientSettings *settings);

void httpClientContextDestroy(HttpClientContext *ctx);

int httpClientContextInit(HttpClientSettings *settings, LoggingContext *logContext, HttpClientContext **outCtx);

#ifdef USE_ZOWE_TLS
int httpClientContextInitSecure(HttpClientSettings *settings,
                                LoggingContext *logContext,
                                TlsEnvironment *tlsEnv,
                                HttpClientContext **outCtx
                                );
#endif // USE_ZOWE_TLS

void httpClientSessionDestroy(HttpClientSession *session);

int httpClientSessionInit2(HttpClientContext *ctx, HttpClientSession **outSession, int *rc);

int httpClientSessionStageRequest(HttpClientContext *ctx,
                                  HttpClientSession *session,
                                  char *method,   /* required, e.g. GET, PUT, POST */
                                  char *urlQuery, /* required, include query string if applicable */
                                  char *userid,
                                  char *password, /* optional, for basic auth if desired */
                                  char *rawBody,  /* optional */
                                  int bodylen);   /* optional */

int httpClientSessionSetBasicAuthHeader(HttpClientContext *ctx,
                                        HttpClientSession *session,
                                        char *userid,
                                        char *password);

int httpClientSessionSetBearerTokenAuthHeader(HttpClientContext *ctx, HttpClientSession *session, char *nativeToken);

int httpClientSessionSetRequestBody(HttpClientContext *ctx,
                                    HttpClientSession *session,
                                    char *body,
                                    int bodylen,
                                    int bodyIsNativeText);

int httpClientSessionSend(HttpClientContext *ctx, HttpClientSession *session);

/* parse response based on a prior sxEnqueueLastRead */
int httpClientSessionReceiveSX(HttpClientContext *ctx, HttpClientSession *session);

/* parse response based on reading up to maxlen bytes from the socket */
int httpClientSessionReceiveNative(HttpClientContext *ctx, HttpClientSession *session, int maxlen);

/* returns 0 when read/parse loop makes session->response non-NULL */
int httpClientSessionReceiveNativeLoop(HttpClientContext *ctx, HttpClientSession *session);

#ifdef __cplusplus
}
#endif

#endif /* HTTPCLIENT_H_ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
