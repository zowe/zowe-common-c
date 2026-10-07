#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>

#include "utils.h"
#include "httpclient.h"

#define HTTP_STATE_RESP_STATUS_VERSION 1
#define HTTP_CLIENT_MAX_RESPONSE 65536
#define STATUS_REASON_LENGTH (sizeof(responseParser->statusReason)*2)
#define HEADER_NAME_LENGTH (sizeof(responseParser->headerName)*2)

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

bool processHttpResponseFragmentShouldNotBufferOverrunForLongStatusReason() {
    HttpResponseParser *responseParser = makeHttpResponseParser(HTTP_CLIENT_MAX_RESPONSE);
    HttpClientResponse *resp;

    char reason[STATUS_REASON_LENGTH + 1];
    memset(reason, 'A', STATUS_REASON_LENGTH);
    reason[STATUS_REASON_LENGTH] = '\0';
    char response[STATUS_REASON_LENGTH + 128];
    snprintf(response, sizeof(response),
             "HTTP/1.1 500 %s\r\n"
             "Content-Length: 0\r\n"
             "\r\n",
             reason);

    int ret = processHttpResponseFragment(responseParser, response, sizeof(response), &resp);

    printf("[%s] processHttpResponseFragmentShouldNotBufferOverrunForLongStatusReason\n", ret == ANSI_FAILED ? "GOOD": " BAD");
    return ret == ANSI_FAILED;
}

bool processHttpResponseFragmentShouldNotBufferOverrunForLongHeaderName() {
    HttpResponseParser *responseParser = makeHttpResponseParser(HTTP_CLIENT_MAX_RESPONSE);
    HttpClientResponse *resp;

    char headerName[HEADER_NAME_LENGTH + 1];

    memset(headerName, 'H', HEADER_NAME_LENGTH);
    headerName[HEADER_NAME_LENGTH] = '\0';

    char response[HEADER_NAME_LENGTH + 128];

    snprintf(response, sizeof(response),
             "HTTP/1.1 200\r\n"
             "%s: test-value\r\n"
             "\r\n",
             headerName);

    int ret = processHttpResponseFragment(responseParser, response, sizeof(response), &resp);

    printf("[%s] processHttpResponseFragmentShouldNotBufferOverrunForLongHeaderName\n", ret == ANSI_FAILED ? "GOOD": " BAD");
    return ret == ANSI_FAILED;
}

int main(int argc, char *argv[]) {
    bool good = true;

    LoggingContext *logContext = makeLoggingContext();
    logConfigureStandardDestinations(logContext);
    logConfigureComponent(NULL, LOG_COMP_HTTPSERVER, "httpserver", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
    logSetLevel(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_INFO);

    good &= processHttpResponseFragmentShouldNotBufferOverrunForLongStatusReason();
    good &= processHttpResponseFragmentShouldNotBufferOverrunForLongHeaderName();

    return good ? 0 : 1;
}