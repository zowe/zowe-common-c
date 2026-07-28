#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>

#include "httpserver.h"
#include "charsets.h"

/*

  _C89_L6SYSLIB="CEE.SCEEBND2:SYS1.CSSLIB:CSF.SCSFMOD0" make

 */

#define READ_BUFFER_SIZE 65536

static int EBCDIC2UTF8(char *bufIn, char *bufOut, int bufOutLen, int *convertOutLength) {
  int convertRC;
  int convertReason;
  if (CHARSET_CONVERSION_SUCCESS != (convertRC = convertCharset(bufIn, strlen(bufIn), CCSID_IBM1047, CHARSET_OUTPUT_USE_BUFFER, &bufOut, bufOutLen, CCSID_UTF_8, NULL, convertOutLength, &convertReason))) {
    printf("convertCharset failed with rc=%d, rsn=%d\n", convertRC, convertReason);
    return 1;
  }
  return 0;
}

static int itShouldNotAbendBySingleBigRequest() {
  puts("Big-request/single-body test");

  const int HTTPREQUESTSIZE_EBCDIC = 
    78 +    /* header                           */
    65537 + /* 65537 bytes body                 */
    1;      /* \0 to make string functions work */

  char httpRequest[HTTPREQUESTSIZE_EBCDIC];
  char httpRequest_utf8[HTTPREQUESTSIZE_EBCDIC*2];
  memset(httpRequest, '\0', HTTPREQUESTSIZE_EBCDIC);
  strcpy(httpRequest, 
    "POST / HTTP/1.1\r\n"            /* 17 */
    "Host: localhost\r\n"            /* 17 */
    "Connection: close\r\n"          /* 19 */
    "Content-Length: 65537\r\n"      /* 23 */
    "\r\n"                           /*  2 */
    /*                                  78 */
  );
  int httpRequestLength;
  if(0 != EBCDIC2UTF8(httpRequest, httpRequest_utf8, HTTPREQUESTSIZE_EBCDIC * 2, &httpRequestLength)) {
    return 1;
  }

  ShortLivedHeap *slh = makeShortLivedHeap(READ_BUFFER_SIZE, 2); /* two 64k blocks in total     */
  HttpRequestParser *parser = makeHttpRequestParser(slh); /* parser uses part of BLOCK-1         */

  /* processHttpFragment takes up the rest of BLOCK-1 as its workarea.                           */
  /* when it comes to the body, processHttpFragment will use BLOCK-2, but the heap will run      */
  /* out of space because the block size is READ_BUFFER_SIZE and the body is READ_BUFFER_SIZE+1. */
  /* we expect processHttpFragment to return 0 and set httpReasonCode to 500, instead of ABEND.  */
  if (processHttpFragment(parser, httpRequest_utf8, httpRequestLength) != 0) {
    puts("[FAILED] processHttpFragment returned non-zero value");
    return 1;
  }

  if (parser->httpReasonCode != 500) {
    printf("[FAILED] httpReasonCode has unexpected value: %d\n", parser->httpReasonCode);
    return 1;
  }
  puts("[SUCCEEDED] processHttpFragment returned 0 and httpReasonCode is 500, no ABEND occurred");
  SLHFree(slh);
  return 0;
}

static int itShouldNotAbendByChunkedBigRequest() {
  puts("Big-request/chunked-body test");

  const int HTTPREQUESTSIZE_EBCDIC = 
    83 +        /* header                           */
    7 +         /* 10000\r\n                        */
    65536 + 2 + /* 65536 bytes chunk + \r\n         */
    3 +         /* 1\r\n                            */
    1 + 2 +     /* 1 bytes chunk + \r\n             */
    5 +         /* 0\r\n\r\n                        */
    1;          /* \0 to make string functions work */

  char httpRequest[HTTPREQUESTSIZE_EBCDIC];
  char httpRequest_utf8[HTTPREQUESTSIZE_EBCDIC*2];
  memset(httpRequest, 'C', HTTPREQUESTSIZE_EBCDIC);
  strcpy(httpRequest, 
    "POST / HTTP/1.1\r\n"            /* 17 */
    "Host: localhost\r\n"            /* 17 */
    "Connection: close\r\n"          /* 19 */
    "Transfer-Encoding: chunked\r\n" /* 28 */
    "\r\n"                           /*  2 */
    /*                                  83 */
  );
  memcpy(httpRequest + 83, "10000\r\n", 7);                               /* 1st chunk header */
  memcpy(httpRequest + 83 + (7 + 65536), "\r\n1\r\n", 5);                 /* 2nd chunk header */
  strcpy(httpRequest + 83 + (7 + 65536 + 2) + (3 + 1) , "\r\n0\r\n\r\n"); /* end of body      */

  int httpRequestLength;
  if(0 != EBCDIC2UTF8(httpRequest, httpRequest_utf8, HTTPREQUESTSIZE_EBCDIC * 2, &httpRequestLength)) {
    return 1;
  }
  printf("httpRequestLength=%d\n", httpRequestLength);

  ShortLivedHeap *slh = makeShortLivedHeap(READ_BUFFER_SIZE, 3); /* 3 64k blocks in total      */
  HttpRequestParser *parser = makeHttpRequestParser(slh); /* parser takes part of BLOCK-1       */

  /* processHttpFragment takes up the rest of BLOCK-1 as its workarea.                          */
  /* when it comes to the body, processHttpFragment will take BLOCK-2 to receive CHUNK-1, and   */
  /* then it will take BLOCK-3 to receive CHUNK-2, but the heap will run out of space because   */
  /* now it needs READ_BUFFER_SIZE+1 to receive CHUNK-1 + CHUNK-2.                              */
  /* we expect processHttpFragment to return 0 and set httpReasonCode to 500, instead of ABEND. */
  if (processHttpFragment(parser, httpRequest_utf8, httpRequestLength) != 0) {
    puts("[FAILED] processHttpFragment returned non-zero value");
    return 1;
  }

  if (parser->httpReasonCode != 500) {
    printf("[FAILED] httpReasonCode has unexpected value: %d\n", parser->httpReasonCode);
    return 1;
  }
  puts("[SUCCEEDED] processHttpFragment returned 0 and httpReasonCode is 500, no ABEND occurred");
  SLHFree(slh);
  return 0;
}


int main(int argc, char **argv){
  puts("Test processHttpFragment in case the SLH has no space to accommodate the request body");

  LoggingContext *logContext = makeLoggingContext();
  logConfigureStandardDestinations(logContext);
  logConfigureComponent(NULL, LOG_COMP_HTTPSERVER, "httpserver", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logSetLevel(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_INFO);

  if (itShouldNotAbendBySingleBigRequest() != 0) {
    return 1;
  }

  if (itShouldNotAbendByChunkedBigRequest() != 0) {
    return 1;
  }

  puts("All done");
  return 0;
}


