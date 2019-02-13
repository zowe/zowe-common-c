

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
#include <metal/string.h>
#include <metal/stdarg.h>
#include <metal/ctype.h>  
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
#include <ctype.h>  
#include <sys/stat.h>
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

/*
 * 
 c89 -DTEST_JSON_PARSER "-Wc,langlvl(extc99),gonum,goff,hgpr,roconst,ASM,asmlib('SYS1.MACLIB')" -o parser -I ../h json.c utils.c alloc.c bpxskt.c charsets.c
 c89 -DTEST_JSON_PRINTER "-Wc,langlvl(extc99),gonum,goff,hgpr,roconst,ASM,asmlib('SYS1.MACLIB')" -o printer -I ../h json.c utils.c alloc.c bpxskt.c charsets.c
 */

#define CONVERSION_BUFFER_DEFAULT_SIZE 8192

#if defined(__ZOWE_OS_ZOS)
#  define SOURCE_CODE_CHARSET CCSID_IBM1047
#else
#  define SOURCE_CODE_CHARSET CCSID_UTF_8
#endif

#define DEBUG(...) /* fprintf(stderr, __VA_ARGS__) */
#define DUMPBUF($b, $l) /* dumpBufferToStream($b, $l, stderr) */

#ifdef METTLE
#  define ERROR(...) /* TODO */
#else
/* TODO Implement good error handling: the caller must have a way to know if
 * an error happened. Most methods currently are void and there's no error
 * flag on the printer structure */
#  define ERROR(...) fprintf(stderr, __VA_ARGS__)
#endif

static
void jsonWriteEscapedString(jsonPrinter *p, char *s, int len);

static
int jsonShouldStopWriting(jsonPrinter *p);

static
int jsonIsError(Json *json) {
  return json->type == JSON_TYPE_ERROR;
}

jsonPrinter *makeJsonPrinter(int fd) {
  jsonPrinter *p = (jsonPrinter*) safeMalloc(sizeof (jsonPrinter), "JSON Printer");

  p->fd = fd;
  p->depth = 0;
  p->indentString = "  ";
  p->isStart = TRUE;
  p->isFirstLine = TRUE;
  p->_conversionBufferSize = 0;
  p->_conversionBuffer = NULL;
  p->mode = JSON_MODE_NATIVE_CHARSET;
  return p;
}

jsonPrinter *makeUtf8JsonPrinter(int fd, int inputCCSID) {
  jsonPrinter *p = makeJsonPrinter(fd);

  p->mode = JSON_MODE_CONVERT_TO_UTF8;
  p->inputCCSID = inputCCSID;
  return p;
}

jsonPrinter *makeCustomJsonPrinter(void (*writeMethod)(jsonPrinter *, char *, int), void *object) {
  jsonPrinter *p = (jsonPrinter*) safeMalloc(sizeof (jsonPrinter), "JSON Custom Printer");
  p->isCustom = TRUE;
  p->customWrite = writeMethod;
  p->customObject = object;
  p->depth = 0;
  p->indentString = "  ";
  p->isStart = TRUE;
  p->isFirstLine = TRUE;
  p->_conversionBufferSize = 0;
  p->_conversionBuffer = NULL;
  p->mode = JSON_MODE_NATIVE_CHARSET;
  return p;
}

jsonPrinter *makeCustomUtf8JsonPrinter(void (*writeMethod)(jsonPrinter *, char *, int),
    void *object, int inputCCSID) {
  jsonPrinter *p = makeCustomJsonPrinter(writeMethod, object);

  p->mode = JSON_MODE_CONVERT_TO_UTF8;
  p->inputCCSID = inputCCSID;
  return p;
}

void jsonEnablePrettyPrint(jsonPrinter *p) {
  p->prettyPrint = TRUE;
}

void freeJsonPrinter(jsonPrinter *p) {
  if (p->_conversionBufferSize > 0) {
    safeFree(p->_conversionBuffer, p->_conversionBufferSize);
  }
  safeFree((char*)p, sizeof (jsonPrinter));
}

static
void jsonWriteBufferInternal(jsonPrinter *p, char *text, int len) {
  int bytesWritten = 0;
  int loopCount = 0;
  int returnCode = 0;
  int reasonCode = 0;
  if (jsonShouldStopWriting(p)) {
    return;
  }
  DEBUG("write buffer internal: text at %p, len %d\n", text, len);
  DUMPBUF(text, len);
  if (p->isCustom) {
    p->customWrite(p, text, len);
  } else {
    while (bytesWritten < len) {
#ifndef METTLE
      int newWriteReturn = write(p->fd,text+bytesWritten,len-bytesWritten);
#else
      int newWriteReturn = socketWrite((Socket *)p->fd,text+bytesWritten,len-bytesWritten,
               &returnCode,&reasonCode);
#endif
      loopCount++;
      if (newWriteReturn < 0) {
        /* TODO: Replace by zowelog(...) */
        ERROR("JSON: write error, rc %d, return code %d, reason code %08X\n",
                newWriteReturn, returnCode, reasonCode);
        jsonSetIOErrorFlag(p);
        break;
      }
      if (loopCount > 10) {
        /* TODO: Replace by zowelog(...) */
        ERROR("JSON: write error, too many attempts\n");
        jsonSetIOErrorFlag(p);
        break;
      }
      bytesWritten += newWriteReturn;
    }
  }
}

void *safeRealloc(void *ptr, size_t size, size_t oldSize, char *site) {
  void *new;

  new = safeMalloc(size, site);
  if (new == NULL) {
    return NULL;
  }
  if ((ptr != NULL) && (oldSize > 0)) {
    memmove(new, ptr, oldSize);
    safeFree(ptr, oldSize);
  }
  return new;
}

#define MAX($a, $b) ((($a) > ($b))? ($a) : ($b))

#define ESCAPE_LEN 6 /* \u0123 */
#define ESCAPE_NATIVE_BUF_SIZE 12 /* some bytes have been added for safety */

#if defined(__ZOWE_OS_ZOS)
#  pragma convert("UTF8")
#endif
static char UTF8_QUOTE = '"';
static char UTF8_BACKSLASH = '\\';
static char UTF8_ESCAPED_QUOTE[2] = "\\\"";
static char UTF8_ESCAPED_BACKSLASH[2]= "\\\\";
#if defined(__ZOWE_OS_ZOS)
#  pragma convert(pop)
#endif

#define UTF8_GET_NEXT_CHAR($size, $buf, $idx, $outChar, $err) do { \
    if ((($buf)[*($idx)] & 0x80) == 0) { \
      /* U+0000 .. U+007F: here the control characters actually reside */ \
      *($outChar) = ($buf)[*($idx)]; \
      *($idx) += 1; \
    } else if ((($buf)[*($idx)] & 0xE0) == 0xC0) { \
      /* TODO properly reconstruct the character. we don't care for now */ \
      *($outChar) = 0x80; \
      *($idx) += 2; \
    } else if ((($buf)[*($idx)] & 0xF0) == 0xE0) { \
      /* TODO properly reconstruct the character. we don't care for now */ \
      *($outChar) = 0x0800; \
      *($idx) += 3; \
    } else if ((($buf)[*($idx)] & 0xF8) == 0xF0) { \
      /* TODO properly reconstruct the character. we don't care for now */ \
      *($outChar) = 0x10000; \
      *($idx) += 4; \
    } \
} while (0)

static ssize_t
convertToUtf8(jsonPrinter *p, size_t len, char text[len], int inputCCSID) {
  int convesionOutputLength = 0;
  int reasonCode = 0;
  int reallocLimit = 10;
  int convRc = 0;

  do {
    if ((p->_conversionBufferSize == 0)
        || (convRc == CHARSET_SHORT_BUFFER)) {
      size_t newSize = MAX(3 * len + 1,
          p->_conversionBufferSize + CONVERSION_BUFFER_DEFAULT_SIZE);
      void *newBuf = NULL;
      newBuf = safeRealloc(p->_conversionBuffer, newSize,
          p->_conversionBufferSize, "JSON conversion buffer");
      if (newBuf == NULL) {
        //the old buffer will be free'd by freeJsonPrinter() if allocated
        ERROR("JSON: error, not enough memory to convert\n");
        return -1;
      }
      p->_conversionBufferSize = newSize;
      p->_conversionBuffer = newBuf;
    }
    convRc = convertCharset(text, len, inputCCSID, CHARSET_OUTPUT_USE_BUFFER,
        &p->_conversionBuffer, p->_conversionBufferSize, CCSID_UTF_8,
        NULL, &convesionOutputLength, &reasonCode);
    if (convRc == CHARSET_CONVERSION_SUCCESS) {
      return  convesionOutputLength;
    } else if (convRc != CHARSET_SHORT_BUFFER) {
      ERROR("JSON: conversion error, rc %d, reason %d\n", convRc,
              reasonCode);
      return -1;
    }
  } while (reallocLimit-- > 0);
  if (reallocLimit == 0) {
    //the old buffer will be free'd by freeJsonPrinter() if allocated
    ERROR("JSON: error, not enough memory to convert\n");
    return -1;
  }
  return len;
}

static ssize_t
writeBufferWithEscaping(jsonPrinter *p, size_t len, char text[len]) {
  //a quick hack
#if defined(__ZOWE_OS_ZOS)
  static const char nativeControlCharBoundary = 0x3F;
#else
  static const char nativeControlCharBoundary = 0x1F;
#endif
  char effectiveControlCharBoundary = (p->mode == JSON_MODE_CONVERT_TO_UTF8)?
      0x1F : nativeControlCharBoundary;
  size_t i;
  size_t currentChunkOffset = 0;
  ssize_t bytesWritten = 0;
  size_t chunkLen;

  for (i = 0; i < len;) {
    uint32_t utf8Char = 0;
    int getCharErr = 0;

    UTF8_GET_NEXT_CHAR(len, text, &i, &utf8Char, &getCharErr);
    if (getCharErr != 0) {
      ERROR("JSON: invalid UTF-8, rc %d\n", getCharErr);
      return -1;
    }
    DEBUG("character at %p + %d, len %d, char %x\n", text, i, len, utf8Char);
    if (((utf8Char >= 0) && (utf8Char <= effectiveControlCharBoundary))
        || (utf8Char == UTF8_BACKSLASH) || (utf8Char == UTF8_QUOTE)) {

      chunkLen = i - 1 - currentChunkOffset;
      if (chunkLen > 0) {
        jsonWriteBufferInternal(p, text + currentChunkOffset, chunkLen);
        bytesWritten += chunkLen;
      }
      if (utf8Char == UTF8_BACKSLASH) {
        jsonWriteBufferInternal(p, UTF8_ESCAPED_BACKSLASH,
            sizeof (UTF8_ESCAPED_BACKSLASH));
        bytesWritten += sizeof (UTF8_ESCAPED_BACKSLASH);
      } else if (utf8Char == UTF8_QUOTE) {
        jsonWriteBufferInternal(p, UTF8_ESCAPED_QUOTE,
            sizeof (UTF8_ESCAPED_QUOTE));
        bytesWritten += sizeof (UTF8_ESCAPED_QUOTE);
      } else {
        char escape[ESCAPE_NATIVE_BUF_SIZE];

        snprintf(escape, sizeof (escape), "\\u%04x", utf8Char);
        if (SOURCE_CODE_CHARSET != CCSID_UTF_8) {
          char escapeUtf[ESCAPE_NATIVE_BUF_SIZE * 3];
          char *ptrToUtf8Buf = escapeUtf;
          int convesionOutputLength = 0;
          int convRc = 0;
          int reasonCode = 0;

          convRc = convertCharset(escape, ESCAPE_LEN, SOURCE_CODE_CHARSET,
              CHARSET_OUTPUT_USE_BUFFER, &ptrToUtf8Buf, sizeof (escapeUtf),
              CCSID_UTF_8, NULL, &convesionOutputLength, &reasonCode);
          if (convRc != CHARSET_CONVERSION_SUCCESS) {
            ERROR("JSON: conversion error, rc %d, reason %d\n",
                    convRc, reasonCode);
            return -1;
          }
          jsonWriteBufferInternal(p, escapeUtf, ESCAPE_LEN);
          bytesWritten += ESCAPE_LEN;
        } else {
          jsonWriteBufferInternal(p, escape, ESCAPE_LEN);
          bytesWritten += ESCAPE_LEN;
        }
      }
      currentChunkOffset = i;
    }
  }
  chunkLen = len - currentChunkOffset;
  if (chunkLen > 0) {
    jsonWriteBufferInternal(p, text + currentChunkOffset, chunkLen);
    bytesWritten += chunkLen;
  }
  return bytesWritten;
}

static
void jsonConvertAndWriteBuffer(jsonPrinter *p, char *text, size_t len,
    bool escape, int inputCCSID) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  if (p->mode == JSON_MODE_CONVERT_TO_UTF8) {
    if (inputCCSID != CCSID_UTF_8) {
      ssize_t newLen;

      DEBUG("before conversion, len %d:\n", len);
      DUMPBUF(text, len);
      newLen = convertToUtf8(p, len, text, inputCCSID);
      if (newLen < 0) {
        ERROR("jsonConvertAndWriteBuffer() error: newLen = %d\n",
                newLen);
        return;
      }
      DEBUG("utf8, len %d:\n", newLen);
      text = p->_conversionBuffer;
      len = newLen;
      DUMPBUF(text, len);
    }
    if (escape) {
      size_t bytesWritten;

      bytesWritten = writeBufferWithEscaping(p, len, text);
      if (bytesWritten < 0) {
        ERROR("jsonConvertAndWriteBuffer() error: bytesWritten = %d\n",
                bytesWritten);
        return;
      }
    } else {
      jsonWriteBufferInternal(p, text, len);
    }
  } else if (p->mode == JSON_MODE_NATIVE_CHARSET) {
    /* TODO currently it just does "the old thing". we potentially
       could enhance this "EBCDIC JSON" format a lot */
    if (escape) {
      jsonWriteEscapedString(p, text, len);
    } else {
      jsonWriteBufferInternal(p, text, len);
    }
  }
}

static
void jsonWrite(jsonPrinter *p, char *text, bool escape, int inputCCSID) {
  jsonConvertAndWriteBuffer(p, text, strlen(text), escape, inputCCSID);
}

static
void jsonIndent(jsonPrinter *p) {
  int depth = p->depth;
  while (depth > 0) {
    jsonWrite(p, p->indentString, false, SOURCE_CODE_CHARSET);
    depth--;
  }
}

static
void jsonNewLine(jsonPrinter *p) {
  if (p->isEnd) {
    p->isEnd = FALSE;
  } else if (p->isStart) {
    p->isStart = FALSE;
  } else {
    jsonWrite(p, ",", false, SOURCE_CODE_CHARSET);
  }
  if (p->prettyPrint) {
    jsonWrite(p, "\n", false, SOURCE_CODE_CHARSET);
    jsonIndent(p);
  }
}

void jsonStart(jsonPrinter *p) {
  jsonStartObject(p, NULL);
}

void jsonEnd(jsonPrinter *p) {
  jsonEndObject(p);
}

static
void jsonWriteInt(jsonPrinter *p, int value) {
  char buffer[64];
  snprintf(buffer, sizeof (buffer), "%d", value);
  jsonWrite(p, buffer, false, SOURCE_CODE_CHARSET);
}

static
void jsonWriteUInt(jsonPrinter *p, unsigned int value) {
  char buffer[64];
  snprintf(buffer, sizeof (buffer), "%u", value);
  jsonWrite(p, buffer, false, SOURCE_CODE_CHARSET);
}

static
void jsonWriteInt64(jsonPrinter *p, int64 value) {
  char buffer[64];
  snprintf(buffer, sizeof (buffer), "%lld", value);
  jsonWrite(p, buffer, false, SOURCE_CODE_CHARSET);
}

static
void jsonWriteBoolean(jsonPrinter *p, int value) {
  jsonWrite(p, value ? "true" : "false", false, SOURCE_CODE_CHARSET);
}

static
void jsonWriteNull(jsonPrinter *p) {
  jsonWrite(p, "null", false, SOURCE_CODE_CHARSET);
}

//FIXME: this escaping method is very non-standard and is kept only for backward
//compatibility
static
void jsonWriteEscapedString(jsonPrinter *p, char *s, int len) {
  int i, specialCharCount = 0;
  for (i = 0; i < len; i++) {
    if (s[i] == '"' || s[i] == '\\' || s[i] == '\n' || s[i] == '\r') {
      specialCharCount++;
    }
  }
  if (specialCharCount > 0) {
    int pos = 0;
    int escapedSize = len + specialCharCount;
    char *escaped = (char*)safeMalloc(escapedSize, "JSON escaped string");
    for (i = 0; i < len; i++) {
      if (s[i] == '\n') {
        escaped[pos++] = '\\';
        escaped[pos++] = 'n';
      } else if (s[i] == '\r') {
        escaped[pos++] = '\\';
        escaped[pos++] = 'r';
      } else {
        if (s[i] == '"' || s[i] == '\\') {
          escaped[pos++] = '\\';
        }
        escaped[pos++] = s[i];
      }
    }
    jsonWriteBufferInternal(p, escaped, escapedSize);
    safeFree(escaped, escapedSize);
  } else {
    jsonWriteBufferInternal(p, s, len);
  }
}

static
void jsonWriteQuotedString(jsonPrinter *p, char *s) {
  jsonWrite(p, "\"", false, SOURCE_CODE_CHARSET);
  jsonWrite(p, s, true, p->inputCCSID);
  jsonWrite(p, "\"", false, SOURCE_CODE_CHARSET);
}

static
void jsonWriteQuotedUnterminatedString(jsonPrinter *p, char *s, int len) {
  jsonWrite(p, "\"", false, SOURCE_CODE_CHARSET);
  jsonConvertAndWriteBuffer(p, s, len, true, p->inputCCSID);
  jsonWrite(p, "\"", false, SOURCE_CODE_CHARSET);
}

static
void jsonWriteKeyAndSemicolon(jsonPrinter *p, char *key) {
  if (key) {
    jsonWriteQuotedString(p, key);
    if (p->prettyPrint) {
      jsonWrite(p, ": ", false, SOURCE_CODE_CHARSET);
    } else {
      jsonWrite(p, ":", false, SOURCE_CODE_CHARSET);
    }
  }
}

void jsonStartObject(jsonPrinter *p, char *keyOrNull) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  if (p->isFirstLine) {
    p->isFirstLine = FALSE;
  } else {
    jsonNewLine(p);
  }
  jsonWriteKeyAndSemicolon(p, keyOrNull);
  jsonWrite(p, "{", false, SOURCE_CODE_CHARSET);
  p->depth++;
  p->isStart = TRUE;
  p->isEnd = FALSE;
}

void jsonEndObject(jsonPrinter *p) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  p->depth--;
  p->isStart = FALSE;
  p->isEnd = TRUE;
  jsonNewLine(p);
  jsonWrite(p, "}", false, SOURCE_CODE_CHARSET);
}

void jsonStartArray(jsonPrinter *p, char *keyOrNull) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  if (p->isFirstLine) {
    p->isFirstLine = FALSE;
  } else {
    jsonNewLine(p);
  }
  jsonWriteKeyAndSemicolon(p, keyOrNull);
  jsonWrite(p, "[", false, SOURCE_CODE_CHARSET);
  p->depth++;
  p->isStart = TRUE;
  p->isEnd = FALSE;
}

void jsonEndArray(jsonPrinter *p) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  p->depth--;
  p->isStart = FALSE;
  p->isEnd = TRUE;
  jsonNewLine(p);
  jsonWrite(p, "]", false, SOURCE_CODE_CHARSET);
}

void jsonAddString(jsonPrinter *p, char *keyOrNull, char *value) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  if (p->isFirstLine) {
    p->isFirstLine = FALSE;
  } else {
    jsonNewLine(p);
  }
  jsonWriteKeyAndSemicolon(p, keyOrNull);
  jsonWriteQuotedString(p, value);
}

void jsonAddJSONString(jsonPrinter *p, char *keyOrNull, char *validJsonValue) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  if (p->isFirstLine) {
    p->isFirstLine = FALSE;
  } else {
    jsonNewLine(p);
  }
  jsonWriteKeyAndSemicolon(p, keyOrNull);
  jsonWrite(p, validJsonValue, false, p->inputCCSID);
}

void jsonAddUnterminatedString(jsonPrinter *p, char *keyOrNull, char *value, int valueLength) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  if (p->isFirstLine) {
    p->isFirstLine = FALSE;
  } else {
    jsonNewLine(p);
  }
  jsonWriteKeyAndSemicolon(p, keyOrNull);
  jsonWriteQuotedUnterminatedString(p, value, valueLength);
}

/* hides from length any control characters back from tail */
void jsonAddLimitedString(jsonPrinter *p, char *keyOrNull, char *value, int lengthLimit) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  int valueLength = lengthLimit;
  while ((valueLength > 0) && (value[valueLength-1] < ' ')){
    valueLength--;
  }

  if (p->isFirstLine) {
    p->isFirstLine = FALSE;
  } else {
    jsonNewLine(p);
  }
  jsonWriteKeyAndSemicolon(p, keyOrNull);
  jsonWriteQuotedUnterminatedString(p, value, valueLength);
}

void jsonAddBoolean(jsonPrinter *p, char *keyOrNull, int value) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  if (p->isFirstLine) {
    p->isFirstLine = FALSE;
  } else {
    jsonNewLine(p);
  }
  jsonWriteKeyAndSemicolon(p, keyOrNull);
  jsonWriteBoolean(p, value);
}

void jsonAddNull(jsonPrinter *p, char *keyOrNull) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  if (p->isFirstLine) {
    p->isFirstLine = FALSE;
  } else {
    jsonNewLine(p);
  }
  jsonWriteKeyAndSemicolon(p, keyOrNull);
  jsonWriteNull(p);
}

void jsonAddUInt(jsonPrinter *p, char *keyOrNull, unsigned int value) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  if (p->isFirstLine) {
    p->isFirstLine = FALSE;
  } else {
    jsonNewLine(p);
  }
  jsonWriteKeyAndSemicolon(p, keyOrNull);
  jsonWriteUInt(p, value);
}

void jsonAddInt(jsonPrinter *p, char *keyOrNull, int value) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  if (p->isFirstLine) {
    p->isFirstLine = FALSE;
  } else {
    jsonNewLine(p);
  }  
  jsonWriteKeyAndSemicolon(p, keyOrNull);
  jsonWriteInt(p, value);
}

void jsonAddInt64(jsonPrinter *p, char *keyOrNull, int64 value) {
  if (jsonShouldStopWriting(p)) {
    return;
  }
  if (p->isFirstLine) {
    p->isFirstLine = FALSE;
  } else {
    jsonNewLine(p);
  }
  jsonWriteKeyAndSemicolon(p, keyOrNull);
  jsonWriteInt64(p, value);
}

void jsonSetIOErrorFlag(jsonPrinter *p) {
  p->ioErrorFlag = TRUE;
}

int jsonShouldStopWriting(jsonPrinter *p) {
  return p->ioErrorFlag;
}

int jsonCheckIOErrorFlag(jsonPrinter *p) {
  return p->ioErrorFlag;
}

#ifndef JSON_TOKEN_BUFFER_SIZE
#define JSON_TOKEN_BUFFER_SIZE  16384
#endif

typedef struct JsonParser_tag JsonParser;
typedef struct JsonTokenizer_tag JsonTokenizer;
typedef struct JsonToken_tag JsonToken;

struct JsonParser_tag {
  JsonTokenizer *tokenizer;
  JsonToken *unreadToken;
  ShortLivedHeap *slh;
  CharStream *in;
  Json *jsonError;
};

struct JsonTokenizer_tag {
  CharStream *in;
  ShortLivedHeap *slh;
  int unreadChar;
  int lineNumber;
  int columnNumber;
  int lastTokenLineNumber;
  int lastTokenColumnNumber;
  int trace;
  char *buffer;
  int bufferSize;
};

struct JsonToken_tag {
#define JSON_TOKEN_NUMBER 0
#define JSON_TOKEN_STRING 1
#define JSON_TOKEN_BOOLEAN 2
#define JSON_TOKEN_COLON 3
#define JSON_TOKEN_COMMA 4
#define JSON_TOKEN_NULL 5
#define JSON_TOKEN_OBJECT_START 6
#define JSON_TOKEN_OBJECT_END 7
#define JSON_TOKEN_ARRAY_START 8
#define JSON_TOKEN_ARRAY_END 9
#define JSON_TOKEN_EOF 10
#define JSON_TOKEN_BAD_RESERVED_WORD 11
#define JSON_TOKEN_UNTERMINATED_STRING 12
#define JSON_TOKEN_BAD_NUMBER 13
#define JSON_TOKEN_LINE_COMMENT 14
#define JSON_TOKEN_BLOCK_COMMENT 15
#define JSON_TOKEN_UNTERMINATED_BLOCK_COMMENT 16
#define JSON_TOKEN_BAD_COMMENT 17
#define JSON_TOKEN_UNEXPECTED_CHAR 18
#define JSON_TOKEN_UNMATCHED 666
  int type;
  char *text;
  int row;
  int col;
};

static char *getTokenTypeString(int type);
static char *jsonParserAlloc(JsonParser *parser, int size);
static char *jsonTokenizerAlloc(JsonTokenizer *tokenizer, int size);
static int jsonTokenizerLookahead(JsonTokenizer *t);
static int jsonTokenizerRead(JsonTokenizer *t);
static Json *jsonGetArray(JsonParser* parser);
static Json *jsonGetBoolean(JsonParser* parser);
static Json *jsonGetNull(JsonParser* parser);
static Json *jsonGetNumber(JsonParser* parser);
static Json *jsonGetString(JsonParser* parser);
static Json *jsonParse(JsonParser* parser);
static JsonParser *makeJsonParser(ShortLivedHeap *slh, char *jsonString, int len);
static JsonToken *getReservedWordToken(JsonTokenizer *tokenizer);
static JsonToken *getNumberToken(JsonTokenizer *tokenizer);
static JsonToken *getStringToken(JsonTokenizer *tokenizer);
static JsonToken *getLineCommentToken(JsonTokenizer *tokenizer);
static JsonToken *getBlockCommentToken(JsonTokenizer *tokenizer);
static JsonToken *getCommentToken(JsonTokenizer *tokenizer);
static void jsonValidateToken(JsonParser *parser, JsonToken *token);
static JsonToken *jsonLookaheadToken(JsonParser* parser);
static JsonToken *jsonMatchToken(JsonParser* parser, int tokenType);
static JsonToken *jsonNextToken(JsonParser* parser);
static JsonToken *makeJsonToken(JsonTokenizer* tokenizer, int type, char *text);
static JsonTokenizer *makeJsonTokenizer(CharStream *in, ShortLivedHeap *slh, int trace);
static void freeJsonParser(JsonParser *parser);
static void freeJsonTokenizer(JsonTokenizer *tokenizer);
static void jsonArrayAddElement(JsonParser *parser, JsonArray *arr, Json *element);
static void jsonPrintInternal(jsonPrinter* printer, char* keyOrNull, Json *json);
static void jsonOjectAddProperty(JsonParser *parser, JsonObject *obj, char *key, Json *value);
static void jsonParseFail(JsonParser *parser, char *formatString, ...);
static void jsonTokenizerSkipWhitespace(JsonTokenizer *t);

static 
char *getTokenTypeString(int type) {
  switch (type) {
    case JSON_TOKEN_NUMBER:
      return "number";
    case JSON_TOKEN_STRING:
      return "string";
    case JSON_TOKEN_BOOLEAN:
      return "boolean";
    case JSON_TOKEN_COLON:
      return ":";
    case JSON_TOKEN_COMMA:
      return ",";
    case JSON_TOKEN_NULL:
      return "null";
    case JSON_TOKEN_OBJECT_START:
      return "{";
    case JSON_TOKEN_OBJECT_END:
      return "}";
    case JSON_TOKEN_ARRAY_START:
      return "[";
    case JSON_TOKEN_ARRAY_END:
      return "]";
    case JSON_TOKEN_EOF:
      return "end of file";
    case JSON_TOKEN_BAD_RESERVED_WORD:
      return "bad reserved word";
    case JSON_TOKEN_UNTERMINATED_STRING:
      return "unterminated string";
    case JSON_TOKEN_BAD_NUMBER:
      return "bad number";
    case JSON_TOKEN_UNEXPECTED_CHAR:
      return "unexpected character";
    case JSON_TOKEN_UNMATCHED:
      return "unmatched token";
    default:
      return "unknown token";
  }
}

static 
char *jsonParserAlloc(JsonParser *parser, int size) {
  char *mem = SLHAlloc(parser->slh, size);
  memset(mem, 0, size);
  return mem;
}

static 
char *jsonTokenizerAlloc(JsonTokenizer *tokenizer, int size) {
  char *mem = SLHAlloc(tokenizer->slh, size);
  memset(mem, 0, size);
  return mem;
}

static 
JsonTokenizer *makeJsonTokenizer(CharStream *in, ShortLivedHeap *slh, int trace) {
  JsonTokenizer *tokenizer = (JsonTokenizer*) safeMalloc(sizeof (JsonTokenizer), "JSON Tokenizer");
  tokenizer->in = in;
  tokenizer->slh = slh;
  tokenizer->unreadChar = 0;
  tokenizer->trace = trace;
  tokenizer->lineNumber = 1;
  tokenizer->buffer = safeMalloc(JSON_TOKEN_BUFFER_SIZE, "JSON token buffer");
  tokenizer->bufferSize = JSON_TOKEN_BUFFER_SIZE;
  return tokenizer;
}

static int readFileCharacterMethod(CharStream *s, int trace){
  int returnCode = 0;
  int reasonCode = 0;
  UnixFile *file = (UnixFile*)s->internalStream;
  int c = fileGetChar(file, &returnCode, &reasonCode);
  if (trace) {
    printf("readFileCharacter '%c'(0x%x)\n", c, c);
  }
  return c;
}

static int closeFileMethod(CharStream *s){
  int returnCode = 0;
  int reasonCode = 0;
  UnixFile *file = (UnixFile*)s->internalStream;
  fileClose(file, &returnCode, &reasonCode);
  return 0;
}

static CharStream *makeFileCharStream(UnixFile *file, int fileLen, int trace) {
  CharStream *s = (CharStream*)safeMalloc(sizeof(CharStream),"FileCharStream");
  s->byteCount = 0;
  s->size = fileLen;
  s->internalStream = file;
  s->readMethod = readFileCharacterMethod;
  s->positionMethod = NULL;
  s->closeMethod = closeFileMethod;
  return s;
}

static 
JsonParser *makeJsonParser(ShortLivedHeap *slh, char *jsonString, int len){
  JsonParser *parser = (JsonParser*) safeMalloc(sizeof (JsonParser), "JSON Parser");
  parser->slh = slh;
  parser->in = makeBufferCharStream(jsonString, len, 0);
  parser->tokenizer = makeJsonTokenizer(parser->in, slh, 0);
  return parser;
}

static 
JsonParser *makeJsonFileParser(ShortLivedHeap *slh, UnixFile *file, int fileLen){
  JsonParser *parser = (JsonParser*) safeMalloc(sizeof (JsonParser), "JSON Parser");
  parser->slh = slh;
  parser->in = makeFileCharStream(file, fileLen, 0);
  parser->tokenizer = makeJsonTokenizer(parser->in, slh, 0);
  return parser;
}

static 
void freeJsonTokenizer(JsonTokenizer *tokenizer) {
  safeFree((char*) tokenizer->buffer, tokenizer->bufferSize);
  safeFree((char*) tokenizer, sizeof (JsonTokenizer));
}

static 
void freeJsonParser(JsonParser *parser) {
  charStreamClose(parser->in);
  charStreamFree(parser->in);
  freeJsonTokenizer(parser->tokenizer);
  safeFree((char*) parser, sizeof (JsonParser));
}

static 
void jsonParseFail(JsonParser *parser, char *formatString, ...) {
  if (parser->jsonError == NULL) {
    int size = 1024;
    char *buffer = jsonParserAlloc(parser, size);
    int pos = snprintf(buffer, size, "%d:%d: ",
                      parser->tokenizer->lastTokenLineNumber,
                      parser->tokenizer->lastTokenColumnNumber);
    va_list argPointer;
    va_start(argPointer, formatString);
    vsnprintf(buffer + pos, size - pos, formatString, argPointer);
    va_end(argPointer);
    parser->jsonError = (Json*) jsonParserAlloc(parser, sizeof (Json));
    parser->jsonError->type = JSON_TYPE_ERROR;
    JsonError *error = (JsonError*) jsonParserAlloc(parser, sizeof (JsonError));
    error->message = buffer;
    parser->jsonError->data.error = error;
  }
}

static 
int jsonTokenizerRead(JsonTokenizer *t) {
  if (t->unreadChar != 0) {
    int x = t->unreadChar;
    t->unreadChar = 0;
    return x;
  } else {
    int c = charStreamGet(t->in, t->trace);
    if (c == '\n') {
      t->lineNumber++;
      t->columnNumber = 0;
    } else {
      t->columnNumber++;
    }
    return c;
  }
}

static 
int jsonTokenizerLookahead(JsonTokenizer *t) {
  int c;
  if (t->unreadChar) {
    c = t->unreadChar;
  } else {
    c = t->unreadChar = jsonTokenizerRead(t);
  }
  return c;
}

static 
void jsonTokenizerSkipWhitespace(JsonTokenizer *t) {
  while (TRUE) {
    char c = jsonTokenizerLookahead(t);
    if (isspace(c)) {
      jsonTokenizerRead(t);
    } else {
      break;
    }
  }
}

static 
JsonToken *makeJsonToken(JsonTokenizer* tokenizer, int type, char *text) {
  JsonToken *token = (JsonToken*) jsonTokenizerAlloc(tokenizer, sizeof (JsonToken));
  token->type = type;
  token->text = text;
  return token;
}

static 
JsonToken *getStringToken(JsonTokenizer *tokenizer) {
  char *buffer = tokenizer->buffer;
  int pos = 0;
  int badString = FALSE;
  
  int quote = jsonTokenizerRead(tokenizer);
  while (TRUE) {
    int lookahead = jsonTokenizerLookahead(tokenizer);
    if (lookahead == EOF || lookahead == '\n') {
      badString = TRUE;
      break;
    } else if (lookahead == '\"') {
      jsonTokenizerRead(tokenizer);
      break;
    } else if (lookahead == '\\') {
      int backSlash = jsonTokenizerRead(tokenizer);
      int c = jsonTokenizerRead(tokenizer);
      if (pos < tokenizer->bufferSize) {
        switch (c) {
          case 'b':
            buffer[pos++] = '\b';
            break;
          case 'n':
            buffer[pos++] = '\n';
            break;
          case 'f':
            buffer[pos++] = '\f';
            break;
          case 'r':
            buffer[pos++] = '\r';
            break;
          case 't':
            buffer[pos++] = '\t';
            break;
          default:
            buffer[pos++] = c;
        }
      }
    } else {
      int c = jsonTokenizerRead(tokenizer);
      if (isprint(c) && pos < tokenizer->bufferSize) {
        buffer[pos++] = c;
      }
    }
  }
  char *text = jsonTokenizerAlloc(tokenizer, pos + 1);
  memcpy(text, buffer, pos);
  if (badString) {
    return makeJsonToken(tokenizer, JSON_TOKEN_UNTERMINATED_STRING, text);
  } else {
    return makeJsonToken(tokenizer, JSON_TOKEN_STRING, text);
  }
}

static 
JsonToken *getNumberToken(JsonTokenizer *tokenizer) {
  char *buffer = tokenizer->buffer;
  int pos = 0;
  int badNumber = FALSE;
  
  if (jsonTokenizerLookahead(tokenizer) == '-') {
    buffer[pos++] = jsonTokenizerRead(tokenizer);
  }
  if (isdigit(jsonTokenizerLookahead(tokenizer))) {
    buffer[pos++] = jsonTokenizerRead(tokenizer);
    while (isdigit(jsonTokenizerLookahead(tokenizer))) {
      int c = jsonTokenizerRead(tokenizer);
      if (pos < tokenizer->bufferSize) {
        buffer[pos++] = c;
      }
    }
  } else {
    badNumber = TRUE;
    int c = jsonTokenizerRead(tokenizer);
    if (pos < tokenizer->bufferSize) {
      buffer[pos++] = c;
    }
  }
  char *text = jsonTokenizerAlloc(tokenizer, pos + 1);
  memcpy(text, buffer, pos);
  return makeJsonToken(tokenizer, badNumber ? JSON_TOKEN_BAD_NUMBER : JSON_TOKEN_NUMBER, text);
}

static
JsonToken *getReservedWordToken(JsonTokenizer *tokenizer) {
  char *buffer = tokenizer->buffer;
  int pos = 0;
  int lookahead;
  JsonToken *token = NULL;
  
  while (isalpha(lookahead = jsonTokenizerLookahead(tokenizer))) {
    int c = jsonTokenizerRead(tokenizer);
    if (pos < tokenizer->bufferSize) { 
      buffer[pos++] = c;
    }
  }
  char *text = jsonTokenizerAlloc(tokenizer, pos + 1);
  memcpy(text, buffer, pos);
  if (!strcmp(text, "false") || !strcmp(text, "true")) {
    token = makeJsonToken(tokenizer, JSON_TOKEN_BOOLEAN, text);
  } else if (!strcmp(text, "null")) {
    token = makeJsonToken(tokenizer, JSON_TOKEN_NULL, text);
  } else {
    token = makeJsonToken(tokenizer, JSON_TOKEN_BAD_RESERVED_WORD, text);
  }
  return token;
}

static
JsonToken *getLineCommentToken(JsonTokenizer *tokenizer) {
  char *buffer = tokenizer->buffer;
  int pos = 0;
  int lookahead;
  int slash = jsonTokenizerRead(tokenizer);
  JsonToken *token = NULL;


  lookahead = jsonTokenizerLookahead(tokenizer);
  while (lookahead != '\n' && lookahead != EOF) {
    int c = jsonTokenizerRead(tokenizer);
    if (pos < tokenizer->bufferSize) {
      buffer[pos++] = c;
    }
    lookahead = jsonTokenizerLookahead(tokenizer);
  }
  char *text = jsonTokenizerAlloc(tokenizer, pos + 1);
  memcpy(text, buffer, pos);
  token = makeJsonToken(tokenizer, JSON_TOKEN_LINE_COMMENT, text);
  return token;
}

static
JsonToken *getBlockCommentToken(JsonTokenizer *tokenizer) {
  char *buffer = tokenizer->buffer;
  int pos = 0;
  int lookahead;
  int star = jsonTokenizerRead(tokenizer);
  int badComment = TRUE;
  JsonToken *token = NULL;

  int c = jsonTokenizerRead(tokenizer);
  while (c != EOF) {
    if (c == '*') {
      lookahead = jsonTokenizerLookahead(tokenizer);
      if (lookahead == '/') {
        int slash = jsonTokenizerRead(tokenizer);
        badComment = FALSE;
        break;
      }
    }
    if (pos < tokenizer->bufferSize) {
      buffer[pos++] = c;
    }
    c = jsonTokenizerRead(tokenizer);
  }

  char *text = jsonTokenizerAlloc(tokenizer, pos + 1);
  memcpy(text, buffer, pos);
  token = makeJsonToken(tokenizer, badComment ? JSON_TOKEN_UNTERMINATED_BLOCK_COMMENT : JSON_TOKEN_BLOCK_COMMENT, text);
  return token;
}

static
JsonToken *getCommentToken(JsonTokenizer *tokenizer) {
  char *buffer = tokenizer->buffer;
  int pos = 0;
  int slash = jsonTokenizerRead(tokenizer);
  int lookahead = jsonTokenizerLookahead(tokenizer);
  JsonToken *token = NULL;

  if (lookahead == '/') {
    token = getLineCommentToken(tokenizer);
  } else if (lookahead == '*') {
    token = getBlockCommentToken(tokenizer);
  } else {
    buffer[pos++] = slash;
    int c = jsonTokenizerRead(tokenizer);
    if (isprint(c) && pos < tokenizer->bufferSize) {
      buffer[pos++] = c;
    }
    char *text = jsonTokenizerAlloc(tokenizer, pos + 1);
    memcpy(text, buffer, pos);
    token = makeJsonToken(tokenizer, JSON_TOKEN_BAD_COMMENT, text);
  }
  return token;
}

static 
void jsonValidateToken(JsonParser *parser, JsonToken *token) {
  switch (token->type) {
    case JSON_TOKEN_UNTERMINATED_STRING:
      jsonParseFail(parser, "unterminated string \"%s", token->text);
      break;
    case JSON_TOKEN_BAD_COMMENT:
      jsonParseFail(parser, "invalid comment start %s, must be // or /*", token->text);
      break;
    case JSON_TOKEN_UNTERMINATED_BLOCK_COMMENT:
      jsonParseFail(parser, "unterminated comment /*%s", token->text);
  }
}

static 
JsonToken *jsonNextToken(JsonParser* parser) {
  JsonTokenizer *tokenizer = parser->tokenizer;
  JsonToken *token = NULL;
  int lookahead;

  if (parser->unreadToken) {
    token = parser->unreadToken;
    parser->unreadToken = NULL;
    return token;
  }
  jsonTokenizerSkipWhitespace(tokenizer);
  lookahead = jsonTokenizerLookahead(tokenizer);
  tokenizer->lastTokenLineNumber = tokenizer->lineNumber;
  tokenizer->lastTokenColumnNumber = tokenizer->columnNumber;
  if (lookahead == '\"') {
    token = getStringToken(tokenizer);
  } else if (isdigit(lookahead) || lookahead == '-') {
    token = getNumberToken(tokenizer);
  } else if (isalpha(lookahead)) {
    token = getReservedWordToken(tokenizer);
  } else if (lookahead == '{') {
    jsonTokenizerRead(tokenizer);
    token = makeJsonToken(tokenizer, JSON_TOKEN_OBJECT_START, "{");
  } else if (lookahead == '}') {
    jsonTokenizerRead(tokenizer);
    token = makeJsonToken(tokenizer, JSON_TOKEN_OBJECT_END, "}");
  } else if (lookahead == '[' || lookahead == '\xBA') {
    jsonTokenizerRead(tokenizer);
    token = makeJsonToken(tokenizer, JSON_TOKEN_ARRAY_START, "[");
  } else if (lookahead == ']' || lookahead == '\xBB') {
    jsonTokenizerRead(tokenizer);
    token = makeJsonToken(tokenizer, JSON_TOKEN_ARRAY_END, "]");
  } else if (lookahead == ':') {
    jsonTokenizerRead(tokenizer);
    token = makeJsonToken(tokenizer, JSON_TOKEN_COLON, ":");
  } else if (lookahead == ',') {
    jsonTokenizerRead(tokenizer);
    token = makeJsonToken(tokenizer, JSON_TOKEN_COMMA, ",");
  } else if (lookahead == '/') {
    token = getCommentToken(tokenizer);
  } else if (lookahead == EOF) {
    jsonTokenizerRead(tokenizer);
    token = makeJsonToken(tokenizer, JSON_TOKEN_EOF, "EOF");
  } else {
    jsonParseFail(parser, "unexpected character %c(0x%x)\n", lookahead, lookahead);
    token = makeJsonToken(tokenizer, JSON_TOKEN_UNEXPECTED_CHAR, "");
  }
  
  if (token->type == JSON_TOKEN_LINE_COMMENT || token->type == JSON_TOKEN_BLOCK_COMMENT) {
    token = jsonNextToken(parser);
  }
  jsonValidateToken(parser, token);

  return token;
}

static 
JsonToken *jsonLookaheadToken(JsonParser* parser) {
  JsonToken *token = NULL;
  if (parser->unreadToken) {
    token = parser->unreadToken;
  } else {
    token = parser->unreadToken = jsonNextToken(parser);
  }
  return token;
}

static
JsonToken *jsonMatchToken(JsonParser* parser, int tokenType) {
  JsonToken *token = jsonNextToken(parser);
  if (token->type != tokenType) {
    switch (token->type) {
      case JSON_TOKEN_STRING:
        jsonParseFail(parser, "expected %s, got %s \"%s\"\n",
                      getTokenTypeString(tokenType),
                      getTokenTypeString(token->type),
                      token->text);
        break;
      case JSON_TOKEN_NUMBER:
      case JSON_TOKEN_BOOLEAN:
        jsonParseFail(parser, "expected %s, got %s %s\n",
                      getTokenTypeString(tokenType),
                      getTokenTypeString(token->type),
                      token->text);
        break;
      case JSON_TOKEN_NULL:
      case JSON_TOKEN_EOF:
        jsonParseFail(parser, "expected %s, got %s\n",
                      getTokenTypeString(tokenType),
                      getTokenTypeString(token->type));
        break;
      default:
        jsonParseFail(parser, "expected %s, got %s",
                      getTokenTypeString(tokenType),
                      token->text);
        break;
    }
    token->type = JSON_TOKEN_UNMATCHED;
  }
  return token;
}

static
int jsonIsTokenUnmatched(JsonToken *token) {
  return token->type == JSON_TOKEN_UNMATCHED;
}

static 
Json *jsonGetNumber(JsonParser* parser) {
  Json *json = NULL;
  JsonToken *token = jsonMatchToken(parser, JSON_TOKEN_NUMBER);
  if (!jsonIsTokenUnmatched(token)) {
    json = (Json*) jsonParserAlloc(parser, sizeof (Json));
    json->type = JSON_TYPE_NUMBER;
    json->data.number = atoi(token->text);
  } else {
    json = parser->jsonError;
  }
  return json;
}

static 
Json *jsonGetString(JsonParser* parser) {
  Json *json = NULL;
  JsonToken *token = jsonMatchToken(parser, JSON_TOKEN_STRING);
  if (!jsonIsTokenUnmatched(token)) {
    json = (Json*) jsonParserAlloc(parser, sizeof (Json));
    json->type = JSON_TYPE_STRING;
    json->data.string = token->text;
  } else {
    json = parser->jsonError;
  }
  return json;
}

static 
Json *jsonGetBoolean(JsonParser* parser) {
  Json *json = NULL;
  JsonToken *token = jsonMatchToken(parser, JSON_TOKEN_BOOLEAN);
  if (!jsonIsTokenUnmatched(token)) {
    json = (Json*) jsonParserAlloc(parser, sizeof (Json));
    json->type = JSON_TYPE_BOOLEAN;
    json->data.boolean = !strcmp(token->text, "true");
  } else {
    json = parser->jsonError;
  }
  return json;
}

static 
Json *jsonGetNull(JsonParser* parser) {
  Json *json = NULL;
  JsonToken *token = jsonMatchToken(parser, JSON_TOKEN_NULL);
  if (!jsonIsTokenUnmatched(token)) {
    json = (Json*) jsonParserAlloc(parser, sizeof (Json));
    json->type = JSON_TYPE_NULL;
  } else {
    json = parser->jsonError;
  }
  return json;
}

static 
void jsonOjectAddProperty(JsonParser *parser, JsonObject *obj, char *key, Json *value) {
  JsonProperty *property = (JsonProperty*) jsonParserAlloc(parser, sizeof (JsonProperty));
  property->key = key;
  property->value = value;
  if (obj->lastProperty) {
    obj->lastProperty->next = property;
    obj->lastProperty = property;
  } else {
    obj->firstProperty = property;
    obj->lastProperty = property;
  }
}

JsonProperty *jsonObjectGetFirstProperty(JsonObject *object) {
  return object->firstProperty;
}

JsonProperty *jsonObjectGetNextProperty(JsonProperty *property) {
  return property->next;
}

char *jsonPropertyGetKey(JsonProperty *property) {
  return property->key;
}

Json *jsonPropertyGetValue(JsonProperty *property) {
  return property->value;
}

Json *jsonObjectGetPropertyValue(JsonObject *object, const char *key) {
  JsonProperty *property = NULL;
  for (property = jsonObjectGetFirstProperty(object); property != NULL; property = jsonObjectGetNextProperty(property)) {
    if (!strcmp(key, property->key)) {
      break;
    }
  }
  return property ? property->value : NULL;
}

static void jsonArrayAddElement(JsonParser *parser, JsonArray *arr, Json *element) {
  if (arr->count == arr->capacity) {
    int newCapacity = arr->capacity * 2;
    Json **newElements = (Json**) jsonParserAlloc(parser, sizeof (Json*) * newCapacity);
    memcpy(newElements, arr->elements, sizeof (Json*) * arr->capacity);
    arr->elements = newElements;
    arr->capacity = newCapacity;
  }
  arr->elements[arr->count++] = element;
}

int jsonArrayGetCount(JsonArray *array) {
  return array->count;
}

Json *jsonArrayGetItem(JsonArray *array, int n) {
  if (n < array->count) {
    return array->elements[n];
  }
  return NULL;
}

int jsonVerifyHomogeneity(JsonArray *array, int type){
  for (int i=0; i<array->count; i++){
    Json *item = array->elements[i];
    if (!item || item->type != type){
      return FALSE;
    }
  }
  return TRUE;
}

int jsonArrayContainsString(JsonArray *array, const char *s){
  for (int i=0; i<array->count; i++){
    Json *item = array->elements[i];
    if (item && (item->type == JSON_TYPE_STRING) && !strcmp(item->data.string,s)){
      return TRUE;
    }
  }
  return FALSE;
}

static Json *jsonGetObject(JsonParser* parser) {
  JsonToken *token = jsonMatchToken(parser, JSON_TOKEN_OBJECT_START);
  if (jsonIsTokenUnmatched(token)) {
    return parser->jsonError;
  }
  Json *json = (Json*) jsonParserAlloc(parser, sizeof (Json));
  JsonObject *obj = (JsonObject*) jsonParserAlloc(parser, sizeof (JsonObject));
  json->type = JSON_TYPE_OBJECT;
  json->data.object = obj;
  while (TRUE) {
    JsonToken *lookahead = jsonLookaheadToken(parser);
    if (lookahead->type == JSON_TOKEN_STRING) {
      JsonToken *keyToken = jsonMatchToken(parser, JSON_TOKEN_STRING);
      JsonToken *colonToken = jsonMatchToken(parser, JSON_TOKEN_COLON);
      if (jsonIsTokenUnmatched(colonToken)) {
        return parser->jsonError;
      }
      Json *valueJSON = jsonParse(parser);
      if (jsonIsError(valueJSON)) {
        return parser->jsonError;
      }
      jsonOjectAddProperty(parser, obj, keyToken->text, valueJSON);
      lookahead = jsonLookaheadToken(parser);
      if (lookahead->type == JSON_TOKEN_COMMA) {
        jsonMatchToken(parser, JSON_TOKEN_COMMA);
      } else {
        break;
      }
    } else {
      break;
    }
  }
  token = jsonMatchToken(parser, JSON_TOKEN_OBJECT_END);
  if (jsonIsTokenUnmatched(token)) {
    return parser->jsonError;
  }
  return json;
}

static Json *jsonGetArray(JsonParser* parser) {
  JsonToken *token = jsonMatchToken(parser, JSON_TOKEN_ARRAY_START);
  if (jsonIsTokenUnmatched(token)) {
    return parser->jsonError;
  }
  Json *json = (Json*) jsonParserAlloc(parser, sizeof (Json));
  JsonArray *arr = (JsonArray*) jsonParserAlloc(parser, sizeof (JsonArray));
  arr->count = 0;
  arr->capacity = 8;
  arr->elements = (Json**) jsonParserAlloc(parser, sizeof (Json*) * arr->capacity);
  json->type = JSON_TYPE_ARRAY;
  json->data.array = arr;
  while (TRUE) {
    JsonToken *lookahead = jsonLookaheadToken(parser);
    if (lookahead->type != JSON_TOKEN_ARRAY_END) {
      Json *element = jsonParse(parser);
      if (jsonIsError(element)) {
        return parser->jsonError;
      }
      jsonArrayAddElement(parser, arr, element);
      lookahead = jsonLookaheadToken(parser);
      if (lookahead->type == JSON_TOKEN_COMMA) {
        jsonMatchToken(parser, JSON_TOKEN_COMMA);
      } else {
        break;
      }
    } else {
      break;
    }
  }
  token = jsonMatchToken(parser, JSON_TOKEN_ARRAY_END);
  if (jsonIsTokenUnmatched(token)) {
    return parser->jsonError;
  }
  return json;
}

static Json *jsonParse(JsonParser* parser) {
  JsonToken *lookahead = jsonLookaheadToken(parser);
  Json *json = NULL;
  if (lookahead) {
    switch (lookahead->type) {
      case JSON_TOKEN_NUMBER:
        json = jsonGetNumber(parser);
        break;
      case JSON_TOKEN_STRING:
        json = jsonGetString(parser);
        break;
      case JSON_TOKEN_BOOLEAN:
        json = jsonGetBoolean(parser);
        break;
      case JSON_TOKEN_NULL:
        json = jsonGetNull(parser);
        break;
      case JSON_TOKEN_OBJECT_START:
        json = jsonGetObject(parser);
        break;
      case JSON_TOKEN_ARRAY_START:
        json = jsonGetArray(parser);
        break;
      default:
        jsonParseFail(parser, "unexpected %s", lookahead->text);
        json = parser->jsonError;
    }
  }
  return json;
}

int jsonIsObject(Json *json) {
  return json->type == JSON_TYPE_OBJECT;
}

int jsonIsArray(Json *json) {
  return json->type == JSON_TYPE_ARRAY;
}

int jsonIsString(Json *json) {
  return json->type == JSON_TYPE_STRING;
}

int jsonIsBoolean(Json *json) {
  return json->type == JSON_TYPE_BOOLEAN;
}

int jsonIsNumber(Json *json) {
  return json->type == JSON_TYPE_NUMBER;
}

int jsonIsNull(Json *json) {
  return json->type == JSON_TYPE_NULL;
}

JsonObject *jsonAsObject(Json *json) {
  if (json->type == JSON_TYPE_OBJECT) {
    return json->data.object;
  }
  return NULL;
}

JsonArray *jsonAsArray(Json *json) {
  if (json->type == JSON_TYPE_ARRAY) {
    return json->data.array;
  }
  return NULL;
}

char *jsonAsString(Json *json) {
  if (json->type == JSON_TYPE_STRING) {
    return json->data.string;
  }
  return NULL;
}

int jsonAsBoolean(Json *json) {
  if (json->type == JSON_TYPE_BOOLEAN) {
    return json->data.boolean;
  }
  return 0;
}

int jsonAsNumber(Json *json) {
  if (json->type == JSON_TYPE_NUMBER) {
    return json->data.number;
  }
  return 0;
}

JsonArray *jsonArrayGetArray(JsonArray *array, int i) {
  Json *json = jsonArrayGetItem(array, i);
  if (json && jsonIsArray(json)) {
    return jsonAsArray(json);
  } else {
    return NULL;
  }
}

int jsonArrayGetBoolean(JsonArray *array, int i) {
  Json *json = jsonArrayGetItem(array, i);
  if (json && jsonIsBoolean(json)) {
    return jsonAsBoolean(json);
  } else {
    return FALSE;
  }
}

int jsonArrayGetNumber(JsonArray *array, int i) {
  Json *json = jsonArrayGetItem(array, i);
  if (json && jsonIsNumber(json)) {
    return jsonAsNumber(json);
  } else {
    return 0;
  }
}

JsonObject *jsonArrayGetObject(JsonArray *array, int i) {
  Json *json = jsonArrayGetItem(array, i);
  if (json && jsonIsObject(json)) {
    return jsonAsObject(json);
  } else {
    return NULL;
  }
}

char *jsonArrayGetString(JsonArray *array, int i) {
  Json *json = jsonArrayGetItem(array, i);
  if (json && jsonIsString(json)) {
    return jsonAsString(json);
  } else {
    return NULL;
  }
}

JsonArray *jsonObjectGetArray(JsonObject *object, const char *key) {
  Json *json = jsonObjectGetPropertyValue(object, key);
  if (json && jsonIsArray(json)) {
    return jsonAsArray(json);
  } else {
    return NULL;
  }
}

int jsonObjectGetBoolean(JsonObject *object, const char *key) {
  Json *json = jsonObjectGetPropertyValue(object, key);
  if (json && jsonIsBoolean(json)) {
    return jsonAsBoolean(json);
  } else {
    return FALSE;
  }
}

int jsonObjectGetNumber(JsonObject *object, const char *key) {
  Json *json = jsonObjectGetPropertyValue(object, key);
  if (json && jsonIsNumber(json)) {
    return jsonAsNumber(json);
  } else {
    return 0;
  }
}

JsonObject *jsonObjectGetObject(JsonObject *object, const char *key) {
  Json *json = jsonObjectGetPropertyValue(object, key);
  if (json && jsonIsObject(json)) {
    return jsonAsObject(json);
  } else {
    return NULL;
  }
}

char *jsonObjectGetString(JsonObject *object, const char *key) {
  Json *json = jsonObjectGetPropertyValue(object, key);
  if (json && jsonIsString(json)) {
    return jsonAsString(json);
  } else {
    return NULL;
  }
}

int jsonObjectHasKey(JsonObject *object, const char *key) {
  int found = FALSE;
  JsonProperty *property = NULL;
  for (property = jsonObjectGetFirstProperty(object); property != NULL; property = jsonObjectGetNextProperty(property)) {
    if (!strcmp(key, property->key)) {
      found = TRUE;
      break;
    }
  }
  return found;
}

void jsonPrintProperty(jsonPrinter* printer, JsonProperty *property) {
  jsonPrintInternal(printer, jsonPropertyGetKey(property), jsonPropertyGetValue(property));
}

void jsonPrintObject(jsonPrinter* printer, JsonObject *object) {
  JsonProperty *property;

  for (property = jsonObjectGetFirstProperty(object); property != NULL; property = jsonObjectGetNextProperty(property)) {
    jsonPrintInternal(printer, jsonPropertyGetKey(property), jsonPropertyGetValue(property));
  }
}

void jsonPrintArray(jsonPrinter* printer, JsonArray *array) {
  int i, count = jsonArrayGetCount(array);
  for (i = 0; i < count; i++) {
    jsonPrintInternal(printer, NULL, jsonArrayGetItem(array, i));
  }
}

static 
void jsonPrintInternal(jsonPrinter* printer, char* keyOrNull, Json *json) {
  switch (json->type) {
    case JSON_TYPE_NUMBER:
      jsonAddInt(printer, keyOrNull, jsonAsNumber(json));
      break;
    case JSON_TYPE_STRING:
      jsonAddString(printer, keyOrNull, jsonAsString(json));
      break;
    case JSON_TYPE_BOOLEAN:
      jsonAddBoolean(printer, keyOrNull, jsonAsBoolean(json));
      break;
    case JSON_TYPE_NULL:
      jsonAddNull(printer, keyOrNull);
      break;
    case JSON_TYPE_OBJECT:
      jsonStartObject(printer, keyOrNull);
      jsonPrintObject(printer, jsonAsObject(json));
      jsonEndObject(printer);
      break;
    case JSON_TYPE_ARRAY:
      jsonStartArray(printer, keyOrNull);
      jsonPrintArray(printer, jsonAsArray(json));
      jsonEndArray(printer);
      break;
  }
}

Json *jsonParseUnterminatedString(ShortLivedHeap *slh, char *jsonString, int len, char* errorBufferOrNull, int errorBufferSize) {
  Json *json = NULL;
  JsonParser *parser = makeJsonParser(slh, jsonString, len);
  json = jsonParse(parser);
  JsonToken *token = jsonMatchToken(parser, JSON_TOKEN_EOF);
  if (jsonIsError(json) || jsonIsTokenUnmatched(token)) {
    json = NULL;
    if (errorBufferOrNull) {
      snprintf(errorBufferOrNull, errorBufferSize, "%s", parser->jsonError->data.error->message);
    }
  }
  freeJsonParser(parser);
  return json;
}

Json *jsonParseFile(ShortLivedHeap *slh, const char *filename, char* errorBufferOrNull, int errorBufferSize) {
  Json *json = NULL;
  int returnCode = 0;
  int reasonCode = 0;
  FileInfo info;
  int status = 0;
  UnixFile *file = NULL;
  int fileLen = 0;

  file = fileOpen(filename, FILE_OPTION_READ_ONLY, 0, 1024, &returnCode, &reasonCode);
  status = fileInfo(filename, &info, &returnCode, &reasonCode);
  if (file != NULL && status == 0) {
    fileLen = (int)fileInfoSize(&info);
    JsonParser *parser = makeJsonFileParser(slh, file, fileLen);
    json = jsonParse(parser);
    JsonToken *token = jsonMatchToken(parser, JSON_TOKEN_EOF);
    if (jsonIsError(json) || jsonIsTokenUnmatched(token)) {
      json = NULL;
      if (errorBufferOrNull) {
        snprintf(errorBufferOrNull, errorBufferSize, "%s", parser->jsonError->data.error->message);
      }
    }
    freeJsonParser(parser);
  } else {
    if (errorBufferOrNull) {
      snprintf(errorBufferOrNull, errorBufferSize, "Couldn't open '%s'", filename);
    }
  }
  return json;
}

Json *jsonParseString(ShortLivedHeap *slh, char *jsonString, char* errorBufferOrNull, int errorBufferSize) {
  return jsonParseUnterminatedString(slh,jsonString,strlen(jsonString),errorBufferOrNull,errorBufferSize);
}



void jsonPrint(jsonPrinter *printer, Json *json) {
  jsonPrintInternal(printer, NULL, json);
}

JsonArray *jsonArrayProperty(JsonObject *object, char *propertyName, int *status){
  Json *propertyValue = jsonObjectGetPropertyValue(object,propertyName);
  if (propertyValue == NULL){
    *status = JSON_PROPERTY_NOT_FOUND;
    return NULL;
  }
  if (jsonIsArray(propertyValue)){
    *status = JSON_PROPERTY_OK;
    return jsonAsArray(propertyValue);
  } else{
    *status = JSON_PROPERTY_UNEXPECTED_TYPE;
    return NULL;
  }
}

int jsonIntProperty(JsonObject *object, char *propertyName, int *status, int defaultValue){
  Json *propertyValue = jsonObjectGetPropertyValue(object,propertyName);
  if (propertyValue == NULL){
    *status = JSON_PROPERTY_NOT_FOUND;
    return defaultValue;
  }
  if (jsonIsNumber(propertyValue)){
    *status = JSON_PROPERTY_OK;
    return jsonAsNumber(propertyValue);
  } else{
    *status = JSON_PROPERTY_UNEXPECTED_TYPE;
    return defaultValue;
  }
}

char *jsonStringProperty(JsonObject *object, char *propertyName, int *status){
  Json *propertyValue = jsonObjectGetPropertyValue(object,propertyName);
  if (propertyValue == NULL){
    *status = JSON_PROPERTY_NOT_FOUND;
    return NULL;
  }
  if (jsonIsString(propertyValue)){
    *status = JSON_PROPERTY_OK;
    return jsonAsString(propertyValue);
  } else{
    *status = JSON_PROPERTY_UNEXPECTED_TYPE;
    return NULL;
  }
}

JsonObject *jsonObjectProperty(JsonObject *object, char *propertyName, int *status){
  Json *propertyValue = jsonObjectGetPropertyValue(object,propertyName);
  if (propertyValue == NULL){
    *status = JSON_PROPERTY_NOT_FOUND;
    return NULL;
  }
  if (jsonIsObject(propertyValue)){
    *status = JSON_PROPERTY_OK;
    return jsonAsObject(propertyValue);
  } else{
    *status = JSON_PROPERTY_UNEXPECTED_TYPE;
    return NULL;
  }
}

void reportJSONDataProblem(void *jsonObject, int status, char *propertyName){
  switch (status){
  case JSON_PROPERTY_NOT_FOUND:
    printf("JSON property '%s' not found in object at 0x%x\n",propertyName,jsonObject);
    break;
  case JSON_PROPERTY_UNEXPECTED_TYPE:
    printf("JSON property '%s' has wrong type in object at 0x%x\n",propertyName,jsonObject);
    break;
  }
}

#ifdef TEST_JSON_PARSER
#define STDOUT_FILENO 1
static 
void testParser() {
  char *jsonString = "{\n /* comment */ \"a\": true, \n\"b\": [123], \"c\"//line comment\n: {\"x\"/*block \ncomment */: [123, -456789, true, null, \"end of array\"]}} ";
  char errorBuffer[2048];
  ShortLivedHeap *slh = makeShortLivedHeap(0x40000, 0x40);
  Json *json = jsonParseString(slh, jsonString, errorBuffer, sizeof(errorBuffer));
  if (json) {
    jsonPrinter *printer = makeJsonPrinter(STDOUT_FILENO);
    jsonPrint(printer, json);
    freeJsonPrinter(printer);
  } else {
    printf("%s\n", errorBuffer);
  }
  SLHFree(slh);
  printf("\n");
}

void testFileParser(char *filename) {
  char errorBuffer[2048];
  ShortLivedHeap *slh = makeShortLivedHeap(0x40000, 0x40);
  Json *json = jsonParseFile(slh, filename, errorBuffer, sizeof(errorBuffer));
  if (json) {
    jsonPrinter *printer = makeJsonPrinter(STDOUT_FILENO);
    jsonPrint(printer, json);
    freeJsonPrinter(printer);
  } else {
    printf("%s\n", errorBuffer);
  }
  SLHFree(slh);
  printf("\n");
}

int main(int argc, char *argv[]) {
  testParser();
  testFileParser(argc > 1 ? argv[1] : "test.json");
}
#endif

#ifdef TEST_JSON_PRINTER
#define STDOUT_FILENO 1

int main(int argc, char *argv[]) {
  jsonPrinter *p;
  if (argc > 1) {
    /* e.g.
     * ./printer -U |iconv -f UTF8 -t 1047
     */
    p = makeUtf8JsonPrinter(STDOUT_FILENO, SOURCE_CODE_CHARSET);
  } else {
    p = makeJsonPrinter(STDOUT_FILENO);
  }
  jsonStart(p);
  {
    jsonAddString(p, "stringKey", "Paste me into the browser console! Don't just assume that I'm correct");
    jsonAddString(p, "escapeStringKey1", "This is a string with control charaters: \n\r\t\b");
    jsonAddString(p, "escapeStringKey2", "\x01\x02\x03\x04\x05");
    jsonAddString(p, "escapeStringKey3", "a\001bb\002ccc\003dddd\004eeeee\005ffffff");
    jsonAddString(p, "escapeStringKey4", "quote: \", backslash: \\");
    jsonAddInt(p, "intKey1", 123);
    jsonAddBoolean(p, "booleanKey1", FALSE);
    jsonStartObject(p, "fruts");
    {
      jsonAddString(p, "description", "These are fruits");
      jsonAddInt(p, "id", 1);
      jsonAddBoolean(p, "booleanKey", TRUE);
      jsonAddNull(p, "nullTest");
      jsonStartArray(p, "examples");
      {
        jsonAddString(p, NULL, "apple");
        jsonAddString(p, NULL, "banana");
        jsonAddString(p, NULL, "orange");
        jsonAddString(p, NULL, "lemon");
      }
      jsonEndArray(p);
    }
    jsonEndObject(p);
    jsonStartObject(p, "vegetables");
    {
      jsonAddString(p, "description", "These are vegetables");
      jsonAddInt(p, "id", 2);
      jsonAddBoolean(p, "booleanKey", TRUE);
      jsonStartArray(p, "examples");
      {
        jsonAddString(p, NULL, "onion");
        jsonAddString(p, NULL, "potato");
        jsonAddString(p, NULL, "tomato");
        jsonAddString(p, NULL, "carrot");
        jsonAddNull(p, NULL);
      }
      jsonEndArray(p);
    }
    jsonEndObject(p);
  }
  jsonAddString(p, "stringKey2 ", "This is a string");
  jsonAddInt(p, "intKey2", 123);
  jsonAddBoolean(p, "booleanKey2", FALSE);
  jsonEnd(p);
  freeJsonPrinter(p);
}

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

