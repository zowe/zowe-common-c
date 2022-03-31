
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include "zowetypes.h"
#ifdef METTLE
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "utils.h"
#include "xlate.h"
#include "bpxnet.h"
#include "httpclient.h"

#define HTTP_CLIENT_SLH_BLOCKSIZE    65536
#define HTTP_CLIENT_MAX_RESPONSE     65536
#define HTTP_CLIENT_SETTINGS_SLHSIZE 8192
#define HTTP_CLIENT_DEFAULT_TIMEOUT  3
#define HTTP_CLIENT_JSONBLOB_BUFSIZE 512

#define HTTP_STATE_RESP_STATUS_VERSION   1
#define HTTP_STATE_RESP_STATUS_GAP1      2
#define HTTP_STATE_RESP_STATUS_STATUS    3
#define HTTP_STATE_RESP_STATUS_GAP2      4
#define HTTP_STATE_RESP_STATUS_REASON    5
#define HTTP_STATE_RESP_STATUS_CR_SEEN   6
#define HTTP_STATE_HEADER_FIELD_NAME     7
#define HTTP_STATE_HEADER_GAP1           8
#define HTTP_STATE_HEADER_FIELD_CONTENT  9
#define HTTP_STATE_HEADER_GAP2           10
#define HTTP_STATE_HEADER_CR_SEEN        11
#define HTTP_STATE_END_CR_SEEN           12
#define HTTP_STATE_READING_FIXED_BODY    13
#define HTTP_STATE_READING_CHUNK_HEADER  14
#define HTTP_STATE_CHUNK_HEADER_CR_SEEN  15
#define HTTP_STATE_READING_CHUNK_DATA    16
#define HTTP_STATE_CHUNK_DATA_CR_SEEN    17
#define HTTP_STATE_READING_CHUNK_TRAILER 18
#define HTTP_STATE_CHUNK_TRAILER_CR_SEEN 19

#define ASCII_SPACE     0x20
#define ASCII_HASH      0x23
#define ASCII_AMPERSAND 0x26
#define ASCII_PLUS      0x2B
#define ASCII_SLASH     0x2F
#define ASCII_COLON     0x3A
#define ASCII_EQUALS    0x3D
#define ASCII_QUESTION  0x3F

#define HTTP_CLIENT_TRACE_ALWAYS(...)  zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_ALWAYS, __VA_ARGS__)
#define HTTP_CLIENT_TRACE_MAJOR(...)   zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_WARNING, __VA_ARGS__)
#define HTTP_CLIENT_TRACE_MINOR(...)   zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_INFO, __VA_ARGS__)
#define HTTP_CLIENT_TRACE_VERBOSE(...) zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, __VA_ARGS__)

static char *respStateNames[20] = {
  "initial",
  "respVersion",
  "respGap1", /* 2 */
  "respStatus",
  "requGap2",
  "respReason", /* 5 */
  "respCRSeen",
  "headerFieldName",
  "headerGap1",       /* 8 */
  "headerFieldValue",
  "headerGap2",
  "headerCRSeen",
  "endCRSeen",        /* 12 */
  "readingFixedBody",
  "chunkHeader",
  "chunkHeaderCRSeen",
  "chunkData",        /* 16 */
  "chunkDataCRSeen",
  "chunkTrailer",
  "chunkTrailerCRSeen" /* 19 */
};

static char *duplicateString(const char *s) {
  char *outstr = NULL;
  if (s) {
    int len = strlen(s);
    if (len) {
      outstr = (char*)safeMalloc(len + 1, "duplicate string");
      if (outstr) {
        memcpy(outstr, s, len);
        outstr[len] = 0;
      }
    }
  }
  return outstr;
}

static char *slhDuplicateString(ShortLivedHeap *slh, char *s) {
  return copyString(slh, s, strlen(s));
}

static void resetHttpResponseParser(HttpResponseParser *parser) {
  ShortLivedHeap *slh = parser->slh;
  memset(parser, 0, sizeof(HttpResponseParser));
  parser->slh = slh;
  parser->state = HTTP_STATE_RESP_STATUS_VERSION;
  parser->specifiedContentLength = -1; /* unspecified */
}

static HttpResponseParser *makeHttpResponseParser(size_t maxResponseSize) {
  ShortLivedHeap *slh = makeShortLivedHeap(sizeof(HttpResponseParser) + maxResponseSize, 8);
  if (NULL == slh) {
    return NULL;
  }
  HttpResponseParser *parser = (HttpResponseParser*)SLHAlloc(slh, sizeof(HttpResponseParser));
  if (NULL == parser) {
    SLHFree(slh);
    return NULL;
  }
  memset(parser, 0, sizeof(HttpResponseParser));
  parser->slh = slh;
  parser->state = HTTP_STATE_RESP_STATUS_VERSION;
  parser->specifiedContentLength = -1; /* unspecified */
  return parser;
}

static void freeHttpResponseParser(HttpResponseParser *hrp) {
  if (hrp) {
    ShortLivedHeap *slh = hrp->slh;
    memset(hrp, 0x00, sizeof(HttpResponseParser));
    if (slh) {
      SLHFree(slh);
    }
  }
}

static void prepareForNewResponse(HttpClientSession *session) {
  if (NULL != session) {
    session->response = NULL;
    if (NULL != session->responseParser) {
      freeHttpResponseParser(session->responseParser);
    }
    session->responseParser = makeHttpResponseParser(HTTP_CLIENT_MAX_RESPONSE);
  }
}

static void addResponseHeader(HttpResponseParser *parser) {
  HttpHeader *newHeader = (HttpHeader*)SLHAlloc(parser->slh, sizeof(HttpHeader));
  newHeader->name = copyString(parser->slh, parser->headerName, parser->headerNameLength);
  newHeader->nativeName = copyStringToNative(parser->slh, parser->headerName, parser->headerNameLength);
  newHeader->value = copyString(parser->slh, parser->headerValue, parser->headerValueLength);
  newHeader->nativeValue = copyStringToNative(parser->slh, parser->headerValue, parser->headerValueLength);

#ifdef DEBUG
  printf("adding response header %s=%s\n", newHeader->nativeName, newHeader->nativeValue);
#endif

  /* pull out enough data for parsing the entity body */
  if (!compareIgnoringCase(newHeader->nativeName, "Transfer-Encoding", parser->headerNameLength)) {
    if (!compareIgnoringCase(newHeader->nativeValue, "chunked", parser->headerValueLength)) {
      parser->isChunked = TRUE;
    }
  } else if (!compareIgnoringCase(newHeader->nativeName, "Content-Length", parser->headerNameLength)) {
    /* printf("worry about atoi\n"); */
    parser->specifiedContentLength = atoi(newHeader->nativeValue);
  } else if (!compareIgnoringCase(newHeader->nativeName, "Content-Type", parser->headerNameLength)) {
    parser->contentType = newHeader->nativeValue;
  }

  HttpHeader *headerChain = parser->headerChain;
  if (headerChain) {
    HttpHeader *prev;
    while (headerChain) {
      prev = headerChain;
      headerChain = headerChain->next;
    }
    prev->next = newHeader;
  } else {
    parser->headerChain = newHeader;
  }
}

#define IS_ASCII_HEX_DIGIT(c) (((c > 0x2F) && (c < 0x3A)) || ((c > 0x40) && (c < 0x47)) || ((c > 0x60) && (c < 0x67)))

static int asciiHexDigitDecimalValue(char c) {
  if ((c > 0x2F) && (c < 0x3A)) {
    return (c - 0x30);
  }
  if ((c > 0x40) && (c < 0x47)) {
    return (10 + (c - 0x41));
  }
  if ((c > 0x60) && (c < 0x67)) {
    return (10 + (c - 0x61));
  }
  return -1;
}

/* returns ansi status */
static int processHttpResponseFragment(HttpResponseParser *parser,
                                       char *data,
                                       int len,
                                       HttpClientResponse **outClientResponse) {
  for (int i = 0; i < len; i++) {
    char c = data[i];
    int isWhitespace = FALSE;
    int isCR = FALSE;
    int isLF = FALSE;
    int isAsciiPrintable = FALSE;
    switch (c) {
      case 10:
        isLF = TRUE;
        break;
      case 13:
        isCR = TRUE;
        break;
      default:
        if (c > 1 && c <= 32) {
          zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "isWhitespace\n");
          isWhitespace = TRUE;
        } else if (c > 32 && c < 127) {
          isAsciiPrintable = TRUE;
        }
    }
    if (parser->state >= HTTP_STATE_RESP_STATUS_VERSION) {
      zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "loop top i=%d c=0x%x wsp=%d cr/lf=%d state=%s\n", 
             i, c, isWhitespace, (isCR || isLF),
             respStateNames[parser->state]);
    }
    switch (parser->state) {
      case HTTP_STATE_RESP_STATUS_VERSION:
        if (isWhitespace) {
          if (parser->versionLength < 1) {
            return ANSI_FAILED;
          }
          zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "RESP_STATUS_VERSION state to GAP1\n");
          parser->state = HTTP_STATE_RESP_STATUS_GAP1;
        } else if (isAsciiPrintable) {
          if (parser->versionLength >= MAX_HTTP_VERSION) {
            return ANSI_FAILED;
          } else {
            parser->version[parser->versionLength++] = c;
          }
        } else {
          return ANSI_FAILED;
        }
        break;
      case HTTP_STATE_RESP_STATUS_GAP1:
        if (!isWhitespace) {
          parser->statusReasonLength = 0;
          parser->statusReason[parser->statusReasonLength++] = c;
          parser->state = HTTP_STATE_RESP_STATUS_STATUS;
        } else if (isCR || isLF) {
          zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "Failing because saw CR or LF in GAP1 state\n");
          return ANSI_FAILED;
        }
        break;
      case HTTP_STATE_RESP_STATUS_STATUS:
        if (isCR || isLF) {
          zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "RespStatus CR/LF\n");
          return ANSI_FAILED;
        } else if (isWhitespace) {
          zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "RespStatus white\n");
          char *nativeStatusReason = copyStringToNative(parser->slh, parser->statusReason, parser->statusReasonLength);
          parser->httpStatusCode = atoi(nativeStatusReason);
          parser->state = HTTP_STATE_RESP_STATUS_GAP2;
        } else {
          parser->statusReason[parser->statusReasonLength++] = c;
        }
        break;
      case HTTP_STATE_RESP_STATUS_GAP2:
        if (isCR) {
          parser->state = HTTP_STATE_RESP_STATUS_CR_SEEN;
        } else if (isLF) {
          return ANSI_FAILED;
        } else if (isWhitespace) {
          /* include whitespace in statusReason string */
          parser->statusReason[parser->statusReasonLength++] = c;
        } else {
          parser->state = HTTP_STATE_RESP_STATUS_REASON;
          parser->statusReason[parser->statusReasonLength++] = c;
        }
        break;
      case HTTP_STATE_RESP_STATUS_REASON:
        if (isCR) {
          parser->state = HTTP_STATE_RESP_STATUS_CR_SEEN;
        } else if (isAsciiPrintable || isWhitespace) {
          parser->statusReason[parser->statusReasonLength++] = c;
        } else {
          return ANSI_FAILED;
        }
        break;
      case HTTP_STATE_RESP_STATUS_CR_SEEN:
        if (isLF) {
          parser->headerNameLength = 0;
          parser->state = HTTP_STATE_HEADER_FIELD_NAME;
        } else {
          return ANSI_FAILED;
        }
        break;
      case HTTP_STATE_HEADER_FIELD_NAME:
        if (isCR) {
          zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "field name CR seen: NameLen=%d\n", parser->headerNameLength);
          if (parser->headerNameLength == 0) {
            parser->state = HTTP_STATE_END_CR_SEEN;
          } else {
            return ANSI_FAILED;
          }
        } else if (c == ASCII_COLON) {
          if (parser->headerNameLength == 0) {
            return ANSI_FAILED;
          } else {
            parser->state = HTTP_STATE_HEADER_GAP1;
          }
        } else if (isAsciiPrintable) {
          parser->headerName[parser->headerNameLength++] = c;
        } else {
          return ANSI_FAILED;
        }
        break;
      case HTTP_STATE_HEADER_GAP1:
        if (isWhitespace) {
          /* do nothing */
        } else if (isAsciiPrintable) {
          parser->headerValueLength = 0;
          parser->headerValue[parser->headerValueLength++] = c;
          parser->state = HTTP_STATE_HEADER_FIELD_CONTENT;
        } else {
          return ANSI_FAILED;
        }
        break;
      case HTTP_STATE_HEADER_FIELD_CONTENT:
        if (isCR) {
          parser->state = HTTP_STATE_HEADER_CR_SEEN;
          addResponseHeader(parser);
        } else if (isLF) {
          return ANSI_FAILED;
        } else {
          if (parser->headerValueLength < MAX_HTTP_FIELD_VALUE) {
            parser->headerValue[parser->headerValueLength++] = c;
          } else {
            /* The server is attempting to pass a header value that
             * is larger than we support. */
            return ANSI_FAILED;
          }
        }
        break;
      case HTTP_STATE_HEADER_GAP2:
        break;
      case HTTP_STATE_HEADER_CR_SEEN:
        if (isLF) {
          parser->state = HTTP_STATE_HEADER_FIELD_NAME;
          parser->headerNameLength = 0;
          parser->headerValueLength = 0;
        } else {
          return ANSI_FAILED;
        }
        break;
      case HTTP_STATE_END_CR_SEEN:
        if (isLF) {
          /* read entity body - if present */
          if (parser->isChunked) {
            zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ end CR -> READING_CHUNK_HEADER ____\n");
            parser->specifiedChunkLength = -1;
            parser->state = HTTP_STATE_READING_CHUNK_HEADER;
            if (parser->specifiedContentLength > 0) {
              zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ Chunked body specified len %d ____\n", 
                      parser->specifiedContentLength);
              parser->content = SLHAlloc(parser->slh, parser->specifiedContentLength);
              parser->contentSize = parser->specifiedContentLength;

            } else {
              /* assume all chunks will fit within an SLH block */
              zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ Chunked body assumed within len %d ____\n", HTTP_CLIENT_MAX_RESPONSE);
              parser->content = SLHAlloc(parser->slh, HTTP_CLIENT_MAX_RESPONSE);
              parser->contentSize = HTTP_CLIENT_MAX_RESPONSE;
            }
            /* allocate response and use resp->contentLength as accumulator */
            parser->resp = (HttpClientResponse*)SLHAlloc(parser->slh, sizeof(HttpClientResponse));
            parser->resp->slh = parser->slh;
            parser->resp->statusCode = parser->httpStatusCode;
            parser->resp->headers = parser->headerChain;
            parser->resp->contentLength = 0;
            parser->resp->body = parser->content;
            /* do NOT output resp address until chunk parsing succeeds */
          } else {
            /* fixed or no body */
            if (parser->specifiedContentLength > 0) { /* fixed body known length */
              zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ Fixed body length %d ____\n", parser->specifiedContentLength);
              parser->state = HTTP_STATE_READING_FIXED_BODY;
              parser->content = SLHAlloc(parser->slh, parser->specifiedContentLength);
              parser->contentSize = parser->specifiedContentLength;
              parser->remainingContentLength = parser->specifiedContentLength;
            } else {
              if (i < (len - 1)) { /* fixed body intuited length */
                                   /* there was no Content-length header, but we got more data */
                zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ Simulating Content-length of %d ____\n", (len - i) - 1);
                parser->specifiedContentLength = (len - i) - 1;
                parser->state = HTTP_STATE_READING_FIXED_BODY;
                parser->content = SLHAlloc(parser->slh, parser->specifiedContentLength);
                parser->contentSize = parser->specifiedContentLength;
                parser->remainingContentLength = parser->specifiedContentLength;
              } else { /* no body */
                zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ No body, output response  ______\n");
                parser->resp = (HttpClientResponse*)SLHAlloc(parser->slh, sizeof(HttpClientResponse));
                parser->resp->slh = parser->slh;
                parser->resp->statusCode = parser->httpStatusCode;
                parser->resp->headers = parser->headerChain;
                parser->resp->contentLength = 0;
                parser->resp->body = NULL;
                *outClientResponse = parser->resp;
              }
            }
          }
        } else {
          return ANSI_FAILED;
        }
        break;
      case HTTP_STATE_READING_FIXED_BODY:
        parser->content[parser->specifiedContentLength - parser->remainingContentLength] = (char)c;
        --(parser->remainingContentLength);
        if (parser->remainingContentLength <= 0) {
          zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "_____ END OF FIXED BODY _________\n");
          parser->resp = (HttpClientResponse*)SLHAlloc(parser->slh, sizeof(HttpClientResponse));
          parser->resp->slh = parser->slh;
          parser->resp->statusCode = parser->httpStatusCode;
          parser->resp->headers = parser->headerChain;
          parser->resp->contentLength = parser->specifiedContentLength;
          parser->resp->body = parser->content;
          *outClientResponse = parser->resp;
        }
        break;
      case HTTP_STATE_READING_CHUNK_HEADER:
        if (isCR) {
          if (-1 == parser->specifiedChunkLength) {
            zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ CHUNKHEADER_CR with unspecified chunklen->LF->READING_CHUNK_TRAILER ____\n");
            parser->state = HTTP_STATE_READING_CHUNK_TRAILER;
          } else {
            zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ CHUNKHEADER_CR with chunklen (%d) ->LF->HEADER_CR_SEEN ____\n", parser->specifiedChunkLength);
            parser->state = HTTP_STATE_CHUNK_HEADER_CR_SEEN;
          }
        } else {
          if (IS_ASCII_HEX_DIGIT(c)) {
            if (-1 == parser->specifiedChunkLength) {
              parser->specifiedChunkLength = asciiHexDigitDecimalValue(c);
            } else {
              parser->specifiedChunkLength = (16 * parser->specifiedChunkLength) + asciiHexDigitDecimalValue(c);
            }
          } else {
            return ANSI_FAILED;
          }
        }
        break;
      case HTTP_STATE_CHUNK_HEADER_CR_SEEN:
        if (isLF) {
          if (0 < parser->specifiedChunkLength) {
            parser->remainingChunkLength = parser->specifiedChunkLength;
            parser->state = HTTP_STATE_READING_CHUNK_DATA;
            zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ CHUNKHEADER_CR->LF->CHUNK_DATA length %d ____\n", parser->specifiedChunkLength);
          } else if (0 == parser->specifiedChunkLength) {
            parser->state = HTTP_STATE_READING_CHUNK_TRAILER;
          }
        } else {
          return ANSI_FAILED;
        }
        break;
      case HTTP_STATE_READING_CHUNK_DATA:
        if (0 < parser->remainingChunkLength) {
          if (parser->contentSize > parser->resp->contentLength) {
            parser->content[parser->resp->contentLength++] = c;
            --(parser->remainingChunkLength);
          } else {
            zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ CHUNKDATA would overflow parser content buffer! ____\n");
            return ANSI_FAILED;
          }
        } else {
          zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ CHUNKDATA zero remaining, expect CR ____\n");
          if (isCR) {
            parser->state = HTTP_STATE_CHUNK_DATA_CR_SEEN;
          } else {
            return ANSI_FAILED;
          }
        }
        break;
      case HTTP_STATE_CHUNK_DATA_CR_SEEN:
        zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ CHUNKDATA CR expect LF ____\n");
        if (isLF) {
          zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ done reading a valid chunk, looking for next header ____\n");
          parser->specifiedChunkLength = -1;
          parser->state = HTTP_STATE_READING_CHUNK_HEADER;
        } else {
          return ANSI_FAILED;
        }
        break;
      case HTTP_STATE_READING_CHUNK_TRAILER:
        if (isCR) {
          parser->state = HTTP_STATE_CHUNK_TRAILER_CR_SEEN;
        } else {
          zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "____ CHUNK TRAILER unsupported ____\n");
          return ANSI_FAILED;
        }
        break;
      case HTTP_STATE_CHUNK_TRAILER_CR_SEEN:
        if (isLF) {
          /* output response and reset parser */
          *outClientResponse = parser->resp;
          resetHttpResponseParser(parser);
        } else {
          return ANSI_FAILED;
        }
        break;
    }
  }
  return ANSI_OK;
}

void httpClientSettingsDestroy(HttpClientSettings *settings) {
  if (settings) {
    if (settings->host) {
      safeFree(settings->host, 1 + strlen(settings->host));
    }
    safeFree(settings, sizeof(HttpClientSettings));
  }
}

HttpClientSettings *httpClientSettingsCopy(HttpClientSettings *settings) {
  HttpClientSettings *copy = NULL;
  if (settings) {
    copy = (HttpClientSettings*)safeMalloc(sizeof(HttpClientSettings), "http client settings");
    if (settings->host) {
      copy->host = duplicateString(settings->host);
    }
    copy->port = settings->port;
    copy->recvTimeoutSeconds = settings->recvTimeoutSeconds;
  }
  return copy;
}

void httpClientContextDestroy(HttpClientContext *ctx) {
  if (ctx) {
    if (ctx->settings) {
      httpClientSettingsDestroy(ctx->settings);
    }
    if (ctx->serverAddress) {
      freeSocketAddr(ctx->serverAddress);
    }
    memset(ctx, 0x00, sizeof(HttpClientContext));
    safeFree(ctx, sizeof(HttpClientContext));
  }
}

int httpClientContextInit(HttpClientSettings *settings, LoggingContext *logContext, HttpClientContext **outContext) {
  int sts = 0;
  int bpxrc = 0, bpxrsn = 0;
  HttpClientContext *ctx = NULL;
  InetAddr *serverAddr = NULL;

  do {
    if ((NULL == settings) || (NULL == logContext) || (NULL == outContext)) {
      sts = HTTP_CLIENT_INVALID_ARGUMENT;
      break;
    }
    ctx = (HttpClientContext*)safeMalloc(sizeof(HttpClientContext), "http client ctx");
    ctx->logContext = logContext;
    ctx->settings = httpClientSettingsCopy(settings);
    if (NULL == settings->host) {
      HTTP_CLIENT_TRACE_VERBOSE("%s (host)\n", HTTP_CLIENT_MSG_SETTING_MISSING);
      sts = HTTP_CLIENT_REQDSETTING_MISSING;
      break;
    }
    serverAddr = getAddressByName(settings->host);
    if (NULL == serverAddr) {
      HTTP_CLIENT_TRACE_VERBOSE("%s (host lookup error)\n", HTTP_CLIENT_MSG_SESSION_ERR);
      sts = HTTP_CLIENT_LOOKUP_FAILED;
      break;
    }
    ctx->serverAddress = makeSocketAddr(serverAddr, settings->port);
    if (NULL == ctx->serverAddress) {
      HTTP_CLIENT_TRACE_VERBOSE("%s (makeSocketAddr)\n", HTTP_CLIENT_MSG_SESSION_ERR);
      sts = HTTP_CLIENT_SESSION_ERR;
      break;
    }
    if (0 != settings->recvTimeoutSeconds) {
      ctx->recvTimeoutSeconds = settings->recvTimeoutSeconds;
    } else {
      ctx->recvTimeoutSeconds = HTTP_CLIENT_DEFAULT_TIMEOUT;
    }

    *outContext = ctx;

  } while (0);

  if ((NULL != ctx) && (0 != sts)) {
    httpClientContextDestroy(ctx);
  }

  return sts;
}

#ifdef USE_ZOWE_TLS
int httpClientContextInitSecure(HttpClientSettings *settings,
                                LoggingContext *logContext,
                                TlsEnvironment *tlsEnv,
                                HttpClientContext **outCtx
                                ) {
  int sts = httpClientContextInit(settings, logContext, outCtx);
  if (sts == 0 && *outCtx != NULL) {
    (*outCtx)->tlsEnvironment = tlsEnv;
  }
  return sts;                             
}
#endif // USE_ZOWE_TLS

void httpClientSessionDestroy(HttpClientSession *session) {
  int bpxrc = 0, bpxrsn = 0;
  if (session) {
    if (session->socket) {
      if (session->socket->userData) {
        /* socket has a SocketExtension; assume socket will be cleaned up
         * by whomever cleans up the SocketExtension */
        session->socket = NULL;
      } else {
        socketClose(session->socket, &bpxrc, &bpxrsn);
        socketFree(session->socket);
      }
    }
    if (session->responseParser) {
      freeHttpResponseParser(session->responseParser);
    }
    if (session->slh) {
      SLHFree(session->slh);
    }
    memset(session, 0x00, sizeof(HttpClientSession));
    safeFree(session, sizeof(HttpClientSession));
  }
}

/**
 * After this call, an stcbase'd caller should 'register' the socket
 */
int httpClientSessionInit(HttpClientContext *ctx, HttpClientSession **outSession) {
  int sts = 0;
  int bpxrc = 0, bpxrsn = 0;

  HttpClientSession *session = NULL;
  ShortLivedHeap *slh = NULL;

  do {
    if ((NULL == ctx) || (NULL == outSession)) {
      sts = HTTP_CLIENT_INVALID_ARGUMENT;
      break;
    }

    Socket *socket = tcpClient2(ctx->serverAddress, 1000 * ctx->recvTimeoutSeconds, &bpxrc, &bpxrsn);
    if ((bpxrc != 0) || (NULL == socket)) {
#ifdef __ZOWE_OS_ZOS
      HTTP_CLIENT_TRACE_VERBOSE("%s (rc=%d, rsn=0x%x, addr=0x%08x, port=%d)\n", HTTP_CLIENT_MSG_CONNECT_FAILED, bpxrc,
                                bpxrsn, ctx->serverAddress->v4Address, ctx->serverAddress->port);
#else
      HTTP_CLIENT_TRACE_VERBOSE("%s (rc=%d, rsn=0x%x, addr=0x%08x, port=%d)\n", HTTP_CLIENT_MSG_CONNECT_FAILED, bpxrc,
                                bpxrsn, ctx->serverAddress->internalAddress.v4Address, ctx->serverAddress->port);
#endif
      sts = HTTP_CLIENT_CONNECT_FAILED;
      break;
    } else {
#ifdef __ZOWE_OS_ZOS
      HTTP_CLIENT_TRACE_VERBOSE("Connected to peer addr=0x%08x, port=%d)\n", ctx->serverAddress->v4Address,
                                ctx->serverAddress->port);
#else
      HTTP_CLIENT_TRACE_VERBOSE("Connected to peer port=%d)\n", ctx->serverAddress->port);
#endif
    }
#ifdef USE_ZOWE_TLS
  if (ctx->tlsEnvironment) {
    int rc = tlsSocketInit(ctx->tlsEnvironment, &socket->tlsSocket, socket->sd, false);
    if (rc != 0) {
      HTTP_CLIENT_TRACE_VERBOSE("failed to init tls socket, rc=%d, (%s)", rc, tlsStrError(rc));
      socketClose(socket, &bpxrc, &bpxrsn);
      sts = HTTP_CLIENT_TLS_ERROR;
      break;
    }
  }
#endif // USE_ZOWE_TLS
    slh = makeShortLivedHeap(HTTP_CLIENT_SLH_BLOCKSIZE, 100);

    session = (HttpClientSession*)safeMalloc(sizeof(HttpClientSession), "http client session");
    session->slh = slh;
    session->socket = socket;

    *outSession = session;

  } while (0);

  return sts;
}

int httpClientSessionStageRequest(HttpClientContext *ctx,
                                  HttpClientSession *session,
                                  char *method,   /* required, e.g. GET, PUT, POST */
                                  char *urlQuery, /* required, include query string if applicable */
                                  char *userid,
                                  char *password, /* optional, for basic auth if desired */
                                  char *rawBody,  /* optional */
                                  int bodylen)    /* optional */
{
  int sts = 0;
  int bpxrc = 0, bpxrsn = 0;

  HttpRequest *req = NULL;

  do {
    if ((NULL == ctx) || (NULL == session) || (NULL == method) || (NULL == urlQuery)) {
      sts = HTTP_CLIENT_INVALID_ARGUMENT;
      break;
    }

    ShortLivedHeap *slh = session->slh;
    req = (HttpRequest*)SLHAlloc(slh, sizeof(HttpRequest));
    memset(req, 0, sizeof(HttpRequest));
    req->slh = slh;
    req->method = slhDuplicateString(slh, method);
    req->uri = slhDuplicateString(slh, urlQuery);
    requestStringHeader(req, TRUE, "Host", slhDuplicateString(slh, ctx->settings->host));

    /* compose basic auth header if userid and password are specified */
    if ((NULL != userid) && (NULL != password)) {
      /* compose basic auth header */
      int space = 1 + strlen(userid) + 1 + strlen(password);
      char *auth = (char*)SLHAlloc(slh, space);
      snprintf(auth, space, "%s:%s", userid, password);
      int auth64Len = 0;
#ifdef __ZOWE_OS_ZOS
      char *ascauth = e2a(auth, space);
      char *auth64 = encodeBase64(slh, ascauth, space, &auth64Len, TRUE);
#else
      char *ascauth = auth;
      char *auth64 = encodeBase64(slh, ascauth, space, &auth64Len, FALSE);
#endif
      int headerLen = strlen("Basic ") + auth64Len;
      char *headerVal = (char*)SLHAlloc(slh, headerLen);
      snprintf(headerVal, headerLen, "%s%s", "Basic ", auth64);
      requestStringHeader(req, TRUE, "Authorization", headerVal);
    }

    /* add body if specified, without translation */
    if ((NULL != rawBody) && (0 < bodylen)) {
      req->contentLength = bodylen;
      req->contentBody = SLHAlloc(slh, 1 + bodylen);
      if (NULL != req->contentBody) {
        memcpy(req->contentBody, rawBody, bodylen);
      }
    }

    session->request = req;

  } while (0);

  return sts;
}

int httpClientSessionSetRequestBody(HttpClientContext *ctx,
                                    HttpClientSession *session,
                                    char *body,
                                    int bodylen,
                                    int bodyIsNativeText) {
  int sts = 0;

  do {
    if ((NULL == ctx) || (NULL == session) || (NULL == body) || (0 >= bodylen)) {
      sts = HTTP_CLIENT_INVALID_ARGUMENT;
      break;
    }
    if (NULL == session->request) {
      sts = HTTP_CLIENT_NO_REQUEST;
      break;
    }
    HttpRequest *req = session->request;
    ShortLivedHeap *slh = req->slh;
    req->contentLength = bodylen;
    req->contentBody = SLHAlloc(slh, 1 + bodylen);
    memcpy(req->contentBody, body, bodylen);

#ifdef __ZOWE_OS_ZOS
    if (bodyIsNativeText) {
      e2a(req->contentBody, req->contentLength);
    }
#endif

  } while (0);

  return sts;
}

int httpClientSessionSetBasicAuthHeader(HttpClientContext *ctx,
                                        HttpClientSession *session,
                                        char *userid,   /* native input */
                                        char *password) /* native input */
{
  int sts = 0;

  do {
    if ((NULL == ctx) || (NULL == session) || (NULL == userid) || (NULL == password)) {
      sts = HTTP_CLIENT_INVALID_ARGUMENT;
      break;
    }
    if (NULL == session->request) {
      sts = HTTP_CLIENT_NO_REQUEST;
      break;
    }
    HttpRequest *req = session->request;
    ShortLivedHeap *slh = req->slh;
    if ((NULL != userid) && (NULL != password)) {
      /* compose basic auth header */
      int space = 1 + strlen(userid) + 1 + strlen(password);
      char *authstr = (char*)SLHAlloc(slh, space);
      snprintf(authstr, space, "%s:%s", userid, password);
#ifdef __ZOWE_OS_ZOS
      int authstrlen = strlen(authstr);
      e2a(authstr, authstrlen);
#endif

      int auth64Len = 0;
#ifdef __ZOWE_OS_ZOS
      char *auth64 = encodeBase64(slh, authstr, space, &auth64Len, TRUE);
#else
      char *auth64 = encodeBase64(slh, authstr, space, &auth64Len, FALSE);
#endif
      int headerLen = strlen("Basic ") + auth64Len;
      char *headerVal = (char*)SLHAlloc(slh, headerLen);
      snprintf(headerVal, headerLen, "%s%s", "Basic ", auth64);
      requestStringHeader(req, TRUE, "Authorization", headerVal);
    }
  } while (0);

  return sts;
}

int httpClientSessionSetBearerTokenAuthHeader(HttpClientContext *ctx, HttpClientSession *session, char *nativeToken) {
  int sts = 0;

  do {
    if ((NULL == ctx) || (NULL == session) || (NULL == nativeToken)) {
      sts = HTTP_CLIENT_INVALID_ARGUMENT;
      break;
    }
    if (NULL == session->request) {
      sts = HTTP_CLIENT_NO_REQUEST;
      break;
    }
    HttpRequest *req = session->request;
    ShortLivedHeap *slh = req->slh;

#define BEARER_PREFIX "Bearer "
    int prefixLen = strlen(BEARER_PREFIX);
    int headerLen = prefixLen + strlen(nativeToken) + 1;
    char *headerVal = (char*)SLHAlloc(slh, headerLen);
    snprintf(headerVal, headerLen, "%s%s", BEARER_PREFIX, nativeToken);
    requestStringHeader(req, TRUE, "Authorization", headerVal);
  } while (0);

  return sts;
}

/* This function expects that the request does NOT already have a
 * Content-Length header already set! */
static void writeRequestWithBody(HttpRequest *request, Socket *socket) {
  int returnCode;
  int reasonCode;
  char line[1024];
  int len = 0;
  HttpHeader *headerChain = NULL;
  char crlf[] = {0x0d, 0x0a};

  zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "in writeRequestWithBody\n");
  // printf("socket->sslHandle is 0x%p\n", socket->sslHandle);

  len = snprintf(line, 1024, "%s %s HTTP/1.1", request->method, request->uri);
  zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "header: %s\n", line);
#ifdef __ZOWE_OS_ZOS
  e2a(line, len);
#endif

  writeFully(socket, line, len);
#ifdef DEBUG
  dumpbuffer(line, len);
#endif

  writeFully(socket, crlf, 2);
#ifdef DEBUG
  dumpbuffer(crlf, 2);
#endif

  zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "write header chain 0x%x\n", headerChain);

  if (request->contentBody && (0 < request->contentLength)) {
    requestIntHeader(request, TRUE, "Content-Length", request->contentLength);
  }
  headerChain = request->headerChain;
  while (headerChain) {
#ifdef DEBUG
    printf("headerChain %s %s or %d\n", headerChain->name, (headerChain->nativeValue ? headerChain->nativeValue : "<n/a>"),
           headerChain->intValue);
#endif

    if (headerChain->nativeValue) {
      len = snprintf(line, 1024, "%s: %s", headerChain->nativeName, headerChain->nativeValue);
    } else {
      len = snprintf(line, 1024, "%s: %d", headerChain->nativeName, headerChain->intValue);
    }
#ifdef __ZOWE_OS_ZOS
    e2a(line, len);
#endif

    writeFully(socket, line, len);
#ifdef DEBUG
    dumpbuffer(line, len);
#endif
    writeFully(socket, crlf, 2);
#ifdef DEBUG
    dumpbuffer(crlf, 2);
#endif
    headerChain = headerChain->next;
  }

  writeFully(socket, crlf, 2);
#ifdef DEBUG
  dumpbuffer(crlf, 2);
#endif

  if (request->contentBody && (0 < request->contentLength)) {
    writeFully(socket, request->contentBody, request->contentLength);
#ifdef DEBUG
    dumpbuffer(request->contentBody, request->contentLength);
#endif
  }
}

int httpClientSessionSend(HttpClientContext *ctx, HttpClientSession *session) {
  int sts = 0;

  do {
    if ((NULL == ctx) || (NULL == session)) {
      sts = HTTP_CLIENT_INVALID_ARGUMENT;
      break;
    }
    if (NULL == session->request) {
      sts = HTTP_CLIENT_NO_REQUEST;
      break;
    }
    if (NULL == session->socket) {
      sts = HTTP_CLIENT_NO_SOCKET;
      break;
    }

    writeRequestWithBody(session->request, session->socket);

    prepareForNewResponse(session);

  } while (0);

  return sts;
}

/* parse response based on a prior sxEnqueueLastRead */
int httpClientSessionReceiveSX(HttpClientContext *ctx, HttpClientSession *session) {
  int sts = 0;
  int ansiStatus = ANSI_OK;
  SocketAddress *senderAddress = NULL;
  SocketExtension *sext = NULL;
  char *buf = NULL;
  int buflen = 0;
  do {
    if ((NULL == ctx) || (NULL == session)) {
      sts = HTTP_CLIENT_INVALID_ARGUMENT;
      break;
    }
    if (NULL == session->socket->userData) {
      sts = HTTP_CLIENT_SOCK_UNREGISTERED;
      break;
    }
    if (NULL == session->request) {
      sts = HTTP_CLIENT_NO_REQUEST;
      break;
    }

    sext = (SocketExtension*)session->socket->userData;
    ansiStatus = sxDequeueRead(sext, &buf, &buflen, &senderAddress);
    if (ansiStatus == ANSI_FAILED) {
      sts = HTTP_CLIENT_SXREAD_ERROR;
      break;
    }
    if (0 == buflen) {
      sts = HTTP_CLIENT_RESPONSE_ZEROLEN;
      break;
    }

    HTTP_CLIENT_TRACE_VERBOSE("httpClientSessionReceiveSX will pass %d bytes to response parser\n", buflen);
    ansiStatus = processHttpResponseFragment(session->responseParser, buf, buflen, &(session->response));

    if (ANSI_OK != ansiStatus) {
      sts = HTTP_CLIENT_RESP_PARSE_FAILED;
      break;
    }

  } while (0);

  return sts;
}

/* returns 0 when session->response is non-NULL, non-zero otherwise */
int httpClientSessionReceiveNativeLoop(HttpClientContext *ctx, HttpClientSession *session) {
  int sts = 0;
  int ansiStatus = ANSI_OK;
  int bpxrc = 0, bpxrsn = 0;
  char buf[1024] = {0};
  int bytesRead = 0;

  do {
    if ((NULL == ctx) || (NULL == session)) {
      sts = HTTP_CLIENT_INVALID_ARGUMENT;
      break;
    }
    if (NULL == session->request) {
      sts = HTTP_CLIENT_NO_REQUEST;
      break;
    }

    while (0 == sts) {
      bytesRead = socketRead(session->socket, buf, sizeof(buf), &bpxrc, &bpxrsn);
      if (bytesRead < 1) {
        HTTP_CLIENT_TRACE_VERBOSE("http client nativeLoop socket read error rc=%d, rsn=0x%x\n", bpxrc, bpxrsn);
        sts = HTTP_CLIENT_READ_ERROR;
        break;
      }
      ansiStatus = processHttpResponseFragment(session->responseParser, buf, bytesRead, &(session->response));
      if (ANSI_FALSE == ansiStatus) {
        HTTP_CLIENT_TRACE_VERBOSE("http client nativeLoop response parse error\n");
        sts = HTTP_CLIENT_RESP_PARSE_FAILED;
        break;
      }
      if (NULL != session->response) {
        break;
      }
      memset(buf, 0x00, sizeof(buf));
      bytesRead = 0;
    }

  } while (0);

  return sts;
}

/* parse response based on reading up to maxlen bytes from the socket */
int httpClientSessionReceiveNative(HttpClientContext *ctx, HttpClientSession *session, int maxlen) {
  int sts = 0;
  int bpxrc = 0, bpxrsn = 0;
  int ansiStatus = ANSI_OK;

  SocketAddress *senderAddress = NULL;
  SocketExtension *sext = NULL;
  char *buf = NULL;
  int buflen = 0;
  do {
    if ((NULL == ctx) || (NULL == session)) {
      sts = HTTP_CLIENT_INVALID_ARGUMENT;
      break;
    }
    if (NULL == session->request) {
      sts = HTTP_CLIENT_NO_REQUEST;
      break;
    }

    buf = SLHAlloc(session->slh, maxlen);
    buflen = socketRead(session->socket, buf, maxlen, &bpxrc, &bpxrsn);
    if (buflen < 1) {
      sts = HTTP_CLIENT_READ_ERROR;
      break;
    }

    zowelog(NULL, LOG_COMP_HTTPCLIENT, ZOWE_LOG_DEBUG, "httpClientSessionReceiveNative will pass %d bytes to response parser\n", buflen);
#ifdef DEBUG
    dumpbuffer(buf, buflen);
#endif
    ansiStatus = processHttpResponseFragment(session->responseParser, buf, buflen, &(session->response));

    if (ANSI_OK != ansiStatus) {
      sts = HTTP_CLIENT_RESP_PARSE_FAILED;
      break;
    }

  } while (0);

  return sts;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
