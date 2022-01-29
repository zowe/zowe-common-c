

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
#include <metal/ctype.h>
#include "qsam.h"
#include "metalio.h"

#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <ctype.h>  
#endif

#include "copyright.h"
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "debug.h"
#include "printables_for_dump.h"
#include "timeutls.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif

char * strcopy_safe(char * dest, const char * source, int dest_size) {
  if( dest_size == 0 )
    return dest;
  dest[0] = '\0';
  return strncat(dest, source, dest_size - 1);
}

int parseInt(const char *str, int start, int end){
  int x = 0;
  int i;
                                          
  for (i=start; i<end; i++){
    x *= 10;
    x += (str[i] - '0');
  }
  return x;
}

int parseInitialInt(const char *str, int start, int end){
  int x = 0;
  int i;
                                          
  for (i=start; i<end; i++){
    int c = str[i]&0xff;
    if (c < '0' || c > '9'){
      break;
    } else{
      x *= 10;
      x += (str[i] - '0');
    }
  }
  return x;
}

int nullTerminate(char *str, int len){
  int i;

  for (i=len-1; i>=0; i--){
    if ((str[i] != 0x40) && (str[i] != 0)){
      str[i+1] = 0;
      return i+1;
    }
  }
  str[0] = 0;
  return 0;
}

int isZeros(char *data, int offset, int length){
  int i;
  for (i=0; i<length; i++){
    if (data[offset+i] != 0){
      return FALSE;
    }
  }
  return TRUE;
}

int hasText(char* data, int offset, int length){
  int hasText = FALSE;
  int i;

  for (i=0; i<length; i++){
    int b = data[offset+i]&0xff;
    if (b < 40){
      return TRUE;
    } else if (b > 40){
      hasText = TRUE;
    }
  }
  return hasText;
}

int isBlanks(char * data, int offset, int length){
  int i;

  for (i=0; i<length; i++){
    int b = data[offset+i]&0xff;
    if (b != ' '){
      return FALSE;
    }
  }
  return TRUE;
}

int indexOf(char *str, int len, char c, int startPos){
  int pos = startPos;
  while (pos < len){
    char c1 = str[pos];
    if (c1 == c){
      return pos;
    }
    pos++;
  }
  return -1;
}

int lastIndexOf(const char *str, int len, char c) {
  int pos = len - 1;
  while(pos >= 0) {
    char c1 = str[pos];
    if (c1 == c) {
      return pos;
    }
    pos--;
  }
  return -1;
}

int indexOfString(char *str, int len, char *searchString, int startPos){
  int searchLen = strlen(searchString);
  int lastPossibleStart = len-searchLen;
  int pos = startPos;

  if (startPos > lastPossibleStart){
    return -1;
  }
  while (pos <= lastPossibleStart){
    if (!memcmp(str+pos,searchString,searchLen)){
      return pos;
    }
    pos++;
  }
  return -1;
}

int lastIndexOfString(char *str, int len, char *searchString) {
  int searchLen = strlen(searchString);
  int pos = len - searchLen;
  
  if (searchLen > len) {
    return -1;
  }
  while (pos >= 0) {
    if (!memcmp(str+pos,searchString,searchLen)) {
      return pos;
    }
    pos--;
  } 
  return -1;
}

#if defined(__ZOWE_OS_WINDOWS) || defined(__ZOWE_OS_LINUX) || defined(METTLE) || defined(__ZOWE_OS_AIX)

static int cheesyInsensitiveMatch(char *str, int len, char *searchString, int searchLen){
  int i;
  if (searchLen != len){
    return FALSE;
  }
  for (i=0; i<searchLen; i++){
    int c1 = str[i];
    int c2 = searchString[i];
    if (c1 != c2){
      return FALSE;
    }
  }
  return TRUE;
}

#endif 

int indexOfStringInsensitive(char *str, int len, char *searchString, int startPos){
  int searchLen = strlen(searchString);
  int lastPossibleStart = len-searchLen;
  int pos = startPos;

  if (startPos > lastPossibleStart){
    return -1;
  }
  while (pos <= lastPossibleStart){
#if defined(METTLE) || defined (__ZOWE_OS_WINDOWS) || defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
    if (cheesyInsensitiveMatch(str+pos,len-pos,searchString,searchLen)){
#else
    if (!strncasecmp(str+pos,searchString,searchLen)){
#endif
      return pos;
    }
    pos++;
  }
  return -1;
}

int upchar(char c){
  char low = (char)(c & 0xf);
  char high = (char)(c &0xf0);
                                          
  switch ((unsigned char)high){
  case 0x80:
  case 0x90:
    if (low >= 0x01 && low <= 0x09){
      return c|0x40;
    } else{
      return c&0xff;
    }
  case 0xA0:
    if (low >= 0x02 && low <= 0x09){
      return c|0x40;
    } else{
      return c&0xff;
    }
  default:
    return c&0xff;
  }
}

int compareIgnoringCase(char *s1, char *s2, int len){
  int i;

  for (i=0; i<len; i++){
    int c1 = upchar(s1[i]);
    int c2 = upchar(s2[i]);
    int diff = c1-c2;
    if (diff){
      return diff;
    }
  }
  return 0;
}
                                          
int isCharAN(char c){
  char low = (char)(c & 0xf);
  char high = (char)(c &0xf0);
                                          
  switch ((unsigned char)high){
  case 0x80:
  case 0x90:
  case 0xC0:
  case 0xD0:
    return (low > 0) && (low < 0x0A);
  case 0xA0:
  case 0xE0:
    return (low > 1) && (low < 0x0A);     
  case 0xF0:
    return (low >= 0) && (low < 0x0A);    
  default:
    return 0;                             
  }                                       
}

void freeToken(token *t){
  safeFree((char*)t,sizeof(token));
}

token* tknGetDecimal(char *str, int len, int start){
  int i;
  int inToken = 0;
  int tokenStart = -1;

  for (i=start; i<len; i++){
    char c = str[i];
    /* printf("tknGetDecimal i=%d, c=%c\n",i,c); */
    if (c >= '0' && c <= '9'){
      if (!inToken){                                          
        inToken = 1;
        tokenStart = i;
        /* intf("getAlphaNum start=%d tokenStart=%d\n",
                start,tokenStart);  */
      }
    } else{
      if (inToken){
        token *t = (token*)safeMalloc(sizeof(token),"Token Decimal 1");
        t->start = tokenStart;
        t->end = i;
        return t;
      }
    }                                                          
  }
  if (inToken){
    token *t = (token*)safeMalloc(sizeof(token),"Token Decimal 2");
    t->start = tokenStart;
    t->end = i;
    return t;
  } else{
    return NULL;                                               
  }

}
                                                               
                                                              
token* tknGetAlphanumeric(char *str, int len, int start){    
  int i;
  int inToken = 0;
  int tokenStart = -1;

  for (i=start; i<len; i++){
    char c = str[i];
    if (isCharAN(c)){
      if (!inToken){                                         
        inToken = 1;
        tokenStart = i;
        /* intf("getAlphaNum start=%d tokenStart=%d\n",
                start,tokenStart);  */
      }
    } else{
      if (inToken){
        token *t = (token*)safeMalloc(sizeof(token),"Token Alphanumeric 1");
        t->start = tokenStart;
        t->end = i;
        return t;
      }
    }
  }
  if (inToken){
    token *t = (token*)safeMalloc(sizeof(token),"Token Alphanumeric 2");
    t->start = tokenStart;
    t->end = i;
    return t;
  } else{
    return NULL;
  }
}

token* tknGetQuoted(char *str, int len, int start, char quote, char escape){
  int i;
  int inToken = 0;
  int tokenStart = -1;

  for (i=start; i<len; i++){
    char c = str[i];

    /* printf("tknGetQuot i=%d c=%c\n",i,c); */
    if (c == escape){
      
    } else if (c == quote){
      if (!inToken){                                         
        inToken = 1;
        tokenStart = i;
      } else{
        token *t = (token*)safeMalloc(sizeof(token),"Token Quoted");
        t->start = tokenStart;
        t->end = i+1;
        return t;
      }
    }
  }
  return NULL;
}

/* alphanumeric plus hash dollar and at sign */
token* tknGetStandard(char *str, int len, int start){    
  int i;
  int inToken = 0;
  int tokenStart = -1;

  for (i=start; i<len; i++){
    char c = str[i];
    if (c == '@' ||
	c == '#' ||
	c == '$' ||
        c == '.' ||
	isCharAN(c)){
      if (!inToken){                                         
        inToken = 1;
        tokenStart = i;
        /* intf("getAlphaNum start=%d tokenStart=%d\n",
                start,tokenStart);  */
      }
    } else{
      if (inToken){
        token *t = (token*)safeMalloc(sizeof(token),"Token Standard 1");
        t->start = tokenStart;
        t->end = i;
        return t;
      }
    }
  }
  if (inToken){
    token *t = (token*)safeMalloc(sizeof(token),"Token Standard 2");
    t->start = tokenStart;
    t->end = i;
    return t;
  } else{
    return NULL;
  }
}

token* tknGetNonWhitespace(char *str, int len, int start){    
  int i;
  int inToken = 0;
  int tokenStart = -1;

  for (i=start; i<len; i++){
    char c = str[i];
    if ((c != ' ') && (c != '\t') && (c != '\n') && (c != 0)){
      if (!inToken){
        inToken = 1;
        tokenStart = i;
      }
    } else{
      if (inToken){
        token *t = (token*)safeMalloc(sizeof(token),"Token NonWhite 1");
        t->start = tokenStart;
        t->end = i;
        return t;
      }
    }
  }
  if (inToken){
    token *t = (token*)safeMalloc(sizeof(token),"Token NonWhite 2");
    t->start = tokenStart;
    t->end = i;
    return t;
  } else{
    return NULL;
  }
} 


token* tknGetTerminating(char *str, int len,                  
                          char *match, int start){            
  int i;
  int matchLen = strlen(match);

  for (i=start; i<len; i++){
    if (str[i] == match[0]){
      int j=0;
      int matched = 1;

      for (j=1; j<matchLen; j++){
        if (i+j >= len){
          return NULL; /* no chance of finding token */
        }
        if (str[i+j] != match[j]){
          matched = 0;
          break;
        }
      }
      if (matched){
        token *t = (token*)safeMalloc(sizeof(token),"Token Terminating");
        t->start = i;
        t->end = i+j; /* j should == matchlen */
        return t;
      }
    }
  }
  return NULL;
}

int tknTextEquals(token *t, char *str, char *match){
  int matchLen = strlen(match);
  int start = t->start;
  int i;

  if (t->end - start != matchLen){
    return 0;
  }
  for (i=0; i<matchLen; i++){
    if (str[start+i] != match[i]){
      return 0;
    }
  }
  return 1;
}
                                                               
char *tknText(token *t, char *str){
  int len = t->end - t->start;
  char *text = safeMalloc(len+1,"Token Text");
  memcpy(text,str+t->start,len);
  text[len] = 0;
  return text;
}

int tknInt(token *t, char *str){
  int x = 0;
  int i;
                                          
  for (i=t->start; i<t->end; i++){
    x *= 10;
    x += (str[i] - '0');
  }
  return x;
}

int tknLength(token *t){
  return t->end - t->start;
}

static char hexDigits[16] ={ '0', '1', '2', '3',
			     '4', '5', '6', '7', 
			     '8', '9', 'A', 'B',
			     'C', 'D', 'E', 'F'};

static char hexDigitsLower[16] = { '0', '1', '2', '3',
    '4', '5', '6', '7',
    '8', '9', 'a', 'b',
    'c', 'd', 'e', 'f'};

int hexFill(char *buffer, int offset, int prePad, int formatWidth, int postPad, int value){
  int i;

  for (i=0; i<prePad; i++){
    buffer[offset+i] = ' ';
  }
  offset += prePad;
  
  for (i=0; i<formatWidth; i++){
    int shift = 4 * (formatWidth - i - 1);
    buffer[offset+i] = hexDigits[(value>>shift)&0xf];
  }
  offset += formatWidth;
  for (i=0; i<postPad; i++){
    buffer[offset+i] = ' ';
  }
  return offset+postPad;
}

char *simpleHexFill(char *buffer, int formatWidth, int value){
  int pos = hexFill(buffer,0,0,formatWidth,0,value);
  buffer[pos] = 0;
  return buffer;
}

char *simpleHexPrint(char *buffer, char *data, int len){
  int i;
  for (i=0; i<len; i++){
    buffer[(2*i)] = hexDigits[(data[i]&0xf0)>>4];
    buffer[(2*i)+1] = hexDigits[(data[i]&0x0f)];
  }
  buffer[(2*i)] = 0;
  return buffer;
}

char *simpleHexPrintLower(char *buffer, char *data, int len){
  int i;
  for (i=0; i<len; i++){
    buffer[(2*i)] = hexDigitsLower[(data[i]&0xf0)>>4];
    buffer[(2*i)+1] = hexDigitsLower[(data[i]&0x0f)];
  }
  buffer[(2*i)] = 0;
  return buffer;
}

static char *ascii =
"                "
"                "
" !\"#$%&'()*+,-./"
"0123456789:;<=>?"
"@ABCDEFGHIJKLMNO"
"PQRSTUVWXYZ[\\]^_"
"`abcdefghijklmno"
"pqrstuvwxyz{|}~ ";

void dumpBufferToStream(const char *buffer, int length, /* FILE* */void *traceOut)
{
  int i;
  int index = 0;
  int last_index;
  char lineBuffer[256];

#ifdef __ZOWE_EBCDIC
  const unsigned char *trans = printableEBCDIC;
#else
  const unsigned char *trans = printableASCII;
#endif

  if (length <= 0){
    return;
  }

  for (last_index = length-1; last_index>=0 && 0 == buffer[last_index]; last_index--){}
  if (last_index < 0)
#ifdef METTLE
    printf("the buffer is empty at %p\n",buffer);
#else
    fprintf((FILE*)traceOut,"the buffer is empty at %p\n",buffer);
#endif

  while (index <= last_index){
    int pos;
    int linePos = 0;
    linePos = hexFill(lineBuffer,linePos,0,8,2,index);
    for (pos=0; pos<32; pos++){

      if (((index+pos)%4 == 0) && ((index+pos)%32 != 0)){
	if ((index+pos)%16 == 0){
	  lineBuffer[linePos++] = ' ';
	}
	lineBuffer[linePos++] = ' ';
      }
      if ((index+pos)<length){
        linePos = hexFill(lineBuffer,linePos,0,2,0,(0xFF&buffer[index+pos])); /* sprintf(lineBuffer+linePos,"%0.2x",(int)(0xFF&buffer[index+pos])); */
      } else {
        linePos += sprintf(lineBuffer+linePos,"  ");
      }
    }
    linePos += sprintf(lineBuffer+linePos," |");
    for (pos=0; pos<32 && (index+pos)<length; pos++){
      int ch = trans[0xFF & buffer[index+pos]];
      lineBuffer[linePos++] = ch;
    }
    lineBuffer[linePos++] = '|';
    lineBuffer[linePos++] = 0;
#ifdef METTLE
    printf("%s\n",lineBuffer);
#else
    fprintf((FILE*)traceOut,"%s\n",lineBuffer);
#endif
    index += 32;
  }
#ifndef METTLE
  fflush((FILE*)traceOut);
#endif
}

void dumpbuffer(const char *buffer, int length) {
#ifdef METTLE
  dumpBufferToStream(buffer, length, 0);
#else
  dumpBufferToStream(buffer, length, stdout);
#endif
}

void hexdump(char *buffer, int length, int nominalStartAddress, int formatWidth, char *pad1, char *pad2){
  int i;
  int index = 0;
  int last_index = length-1;
  char lineBuffer[256];
  const unsigned char *trans = printableEBCDIC;
  int pad1Len = strlen(pad1);

  while (index <= last_index){
    int pos;
    int linePos = 8+pad1Len;
    simpleHexFill(lineBuffer,8,index+nominalStartAddress);
    memcpy(lineBuffer+8,pad1,pad1Len);
    for (pos=0; pos<formatWidth; pos++){
      if ((index+pos)<length){
	simpleHexFill(lineBuffer+linePos,2,(int)(0xFF&buffer[index+pos]));
	linePos += 2;
        /* linePos += sprintf(lineBuffer+linePos,"%0.2x",(int)(0xFF&buffer[index+pos])); */
      } else{
        linePos += sprintf(lineBuffer+linePos,"  ");
      }
      if ((pos % 4) == 3){
	linePos += sprintf(lineBuffer+linePos," ");
      }
    }
    linePos += sprintf(lineBuffer+linePos,"%s|",pad2);
    for (pos=0; pos<formatWidth && (index+pos)<length; pos++){
      int ch = trans[0xFF & buffer[index+pos]];
      lineBuffer[linePos++] = ch;
    }
    lineBuffer[linePos++] = '|';
    lineBuffer[linePos++] = 0;
    printf("%s\n",lineBuffer);
    index += formatWidth;
  }
  fflush(stdout);
}

void dumpbuffer2(char *buffer, int length){
  hexdump(buffer,length,0,32," "," ");
}

void dumpbufferA(const char *buffer, int length){
  int i;
  int index = 0;
  int last_index;
  char lineBuffer[256];
  const unsigned char *trans = printableASCII;
  void *traceOut = (void*)stdout;

  if (length <= 0){
    return;
  }

  for (last_index = length-1; last_index>=0 && 0 == buffer[last_index]; last_index--){}
  if (last_index < 0)
#ifdef METTLE
    printf("the buffer is empty at %x\n",buffer);
#else
    fprintf((FILE*)traceOut,"the buffer is empty at %p\n",buffer);
#endif

  while (index <= last_index){
    int pos;
    int linePos = 0;
    linePos = hexFill(lineBuffer,linePos,0,8,2,index);
    for (pos=0; pos<32; pos++){

      if (((index+pos)%4 == 0) && ((index+pos)%32 != 0)){
	if ((index+pos)%16 == 0){
	  lineBuffer[linePos++] = ' ';
	}
	lineBuffer[linePos++] = ' ';
      }
      if ((index+pos)<length){
        linePos = hexFill(lineBuffer,linePos,0,2,0,(0xFF&buffer[index+pos])); /* sprintf(lineBuffer+linePos,"%0.2x",(int)(0xFF&buffer[index+pos])); */
      } else {
        linePos += sprintf(lineBuffer+linePos,"  ");
      }
    }
    linePos += sprintf(lineBuffer+linePos," |");
    for (pos=0; pos<32 && (index+pos)<length; pos++){
      int ch = trans[0xFF & buffer[index+pos]];
      lineBuffer[linePos++] = ch;
    }
    lineBuffer[linePos++] = '|';
    lineBuffer[linePos++] = 0;
#ifdef METTLE
    printf("%s\n",lineBuffer);
#else
    fprintf((FILE*)traceOut,"%s\n",lineBuffer);
#endif
    index += 32;
  }
#ifndef METTLE
  fflush((FILE*)traceOut);
#endif
}


char *strupcase(char *s){
  if (s == NULL){
    return NULL;
  } else{
    int i,len = strlen(s);
    
    for (i=0; i<len; i++){
      s[i] = toupper(s[i]);
    }
    return s;
  }
}


#define URL_PARM_NORMAL 1
#define URL_PARM_PERCENT 2
#define URL_PARM_PERCENT_NUMBER 3

/* problems:  hash mark at 0x23, dollar at 0x24 */

int percentEncode(char *value, char *buffer, int len){
  int i;
  int pos = 0;

  for (i=0; i<len; i++){
    char c = value[i];
    switch(c) {
      case ' ':
        buffer[pos++] = '%';
        buffer[pos++] = '2';
        buffer[pos++] = '0';
        break;
      case '!':
        buffer[pos++] = '%';
        buffer[pos++] = '2';
        buffer[pos++] = '1';
        break;
      case '"':
        buffer[pos++] = '%';
        buffer[pos++] = '2';
        buffer[pos++] = '2';
        break;
      case '#':
        buffer[pos++] = '%';
        buffer[pos++] = '2';
        buffer[pos++] = '3';
        break;
      case '%':
        buffer[pos++] = '%';
        buffer[pos++] = '2';
        buffer[pos++] = '5';
        break;
      case '&':
        buffer[pos++] = '%';
        buffer[pos++] = '2';
        buffer[pos++] = '6';
        break;
      case '\'':
        buffer[pos++] = '%';
        buffer[pos++] = '2';
        buffer[pos++] = '7';
        break;
      case '*':
        buffer[pos++] = '%';
        buffer[pos++] = '2';
        buffer[pos++] = 'A';
        break;
      case '+':
        buffer[pos++] = '%';
        buffer[pos++] = '2';
        buffer[pos++] = 'B';
        break;
      case ',':
        buffer[pos++] = '%';
        buffer[pos++] = '2';
        buffer[pos++] = 'C';
        break;
      case ':':
        buffer[pos++] = '%';
        buffer[pos++] = '3';
        buffer[pos++] = 'A';
        break;
      case ';':
        buffer[pos++] = '%';
        buffer[pos++] = '3';
        buffer[pos++] = 'B';
        break;
      case '<':
        buffer[pos++] = '%';
        buffer[pos++] = '3';
        buffer[pos++] = 'C';
        break;
      case '=':
        buffer[pos++] = '%';
        buffer[pos++] = '3';
        buffer[pos++] = 'D';
        break;
      case '>':
        buffer[pos++] = '%';
        buffer[pos++] = '3';
        buffer[pos++] = 'E';
        break;
      case '?':
        buffer[pos++] = '%';
        buffer[pos++] = '3';
        buffer[pos++] = 'F';
        break;
      case '[':
        buffer[pos++] = '%';
        buffer[pos++] = '5';
        buffer[pos++] = 'B';
        break;
      case ']':
        buffer[pos++] = '%';
        buffer[pos++] = '5';
        buffer[pos++] = 'D';
        break;
      case '^':
        buffer[pos++] = '%';
        buffer[pos++] = '5';
        buffer[pos++] = 'E';
        break;
      case '`':
        buffer[pos++] = '%';
        buffer[pos++] = '6';
        buffer[pos++] = '0';
        break;
      case '{':
        buffer[pos++] = '%';
        buffer[pos++] = '7';
        buffer[pos++] = 'B';
        break;
      case '|':
        buffer[pos++] = '%';
        buffer[pos++] = '7';
        buffer[pos++] = 'C';
        break;
      case '}':
        buffer[pos++] = '%';
        buffer[pos++] = '7';
        buffer[pos++] = 'D';
        break;
      default:
        buffer[pos++] = c;
        break;
    }
  }
  buffer[pos] = 0;
  return pos;
}

char *cleanURLParamValue(ShortLivedHeap *slh, char *value){
  int len = strlen(value);
  int i;
  int pos = 0;
  char *cleanValue = SLHAlloc(slh,len+1);
  int state = URL_PARM_NORMAL;
  char numChar;
  int highDigit;

  for (i=0; i<len; i++){
    char c = value[i];
    /* printf("c = %c state=%d\n",c,state); */
    switch (state){
    case URL_PARM_NORMAL:
      if (c == '%'){
	state = URL_PARM_PERCENT;
      } else if (c == '+'){
	cleanValue[pos++] = ' ';
      } else{
	cleanValue[pos++] = c;
	}
      break;
    case URL_PARM_PERCENT:
      if (c == '%'){
	cleanValue[pos++] = '%';
      } else if ((c >= '2') && (c <= '7')){
	state = URL_PARM_PERCENT_NUMBER;
	numChar = c;
      } else{
	cleanValue[pos++] = '%';
        cleanValue[pos++] = c;
	state = URL_PARM_NORMAL;
      }
      break;
    case URL_PARM_PERCENT_NUMBER:
      highDigit = (numChar-'0')*0x10;
      if ((c >= '0') && (c <= '9')){
	cleanValue[pos++] = ascii[highDigit+(c-'0')];
	/* printf("highDigit %x (c-0) %x\n",highDigit,(c-'0')); */
	state = URL_PARM_NORMAL;
      } else if ((c >= 'a') && (c <= 'f')){
	cleanValue[pos++] = ascii[highDigit+10+(c-'a')];
	state = URL_PARM_NORMAL;
      } else if ((c >= 'A') && (c <= 'F')){
	cleanValue[pos++] = ascii[highDigit+10+(c-'A')];
	state = URL_PARM_NORMAL;
      } else if (c == '%'){
	cleanValue[pos++] = '%';
        cleanValue[pos++] = numChar;
	state = URL_PARM_PERCENT;
      } else{
	cleanValue[pos++] = '%';
        cleanValue[pos++] = numChar;
	cleanValue[pos++] = c;
	state = URL_PARM_NORMAL;
      }
      break;
    }
  }
  cleanValue[pos] = 0;
  /* free(value); */
  return cleanValue;
}


static int getBitsForChar(char c){
  if (c >= 'a' && c <= 'i'){
    return (c - 'a') + 26;
  } else if (c >= 'j' && c <= 'r'){
    return (c - 'j') + 35;
  } else if (c >= 's' && c <= 'z'){
    return (c - 's') + 44;
  } else if (c >= 'A' && c <= 'I'){
    return (c - 'A') + 0;
  } else if (c >= 'J' && c <= 'R'){
    return (c - 'J') + 9;
  } else if (c >= 'S' && c <= 'Z'){
    return (c - 'S') + 18;
  } else if (c >= '0' && c <= '9'){
    return (c - '0') + 52;
  } else if (c == '+'){
    return 62;
  } else if (c == '/'){
    return 63;
  } else{
    return -1;
  }
}

int decodeBase64(char *s, char *result){
  int sLen = strlen(s);
  int numGroups = sLen / 4;
  int missingBytesInLastGroup = 0;
  int numFullGroups = numGroups;
  int inCursor = 0, outCursor = 0;
  int i;
    
  if (4 * numGroups != sLen){
    printf("non 4-mult\n");
    return -1;
  }
    
  if (sLen != 0){
    if (s[sLen - 1] == '='){
      missingBytesInLastGroup++;
      numFullGroups--;
    }
    if (s[sLen - 2] == '='){
      missingBytesInLastGroup++;
    }
  }
    
  /* Translate all full groups from base64 to byte array elements */
  for (i = 0; i < numFullGroups; i++){
    int ch0 =getBitsForChar(s[inCursor++]);
    int ch1 =getBitsForChar(s[inCursor++]);
    int ch2 =getBitsForChar(s[inCursor++]);
    int ch3 =getBitsForChar(s[inCursor++]);
    result[outCursor++] = (char) ((ch0 << 2) | (ch1 >> 4));
    result[outCursor++] = (char) ((ch1 << 4) | (ch2 >> 2));
    result[outCursor++] = (char) ((ch2 << 6) | ch3);
  }
  
  /* Translate partial group, if present */
  if (missingBytesInLastGroup != 0){
    int ch0 =getBitsForChar(s[inCursor++]);
    int ch1 =getBitsForChar(s[inCursor++]);
    result[outCursor++] = (char) ((ch0 << 2) | (ch1 >> 4));
    
    if (missingBytesInLastGroup == 1){
      int ch2 =getBitsForChar(s[inCursor++]);
      result[outCursor++] = (char) ((ch1 << 4) | (ch2 >> 2));
    }
  }

  return outCursor;  
}

static char binToB64[] ={0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,
			 0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x61,0x62,0x63,0x64,0x65,0x66,
			 0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,0x70,0x71,0x72,0x73,0x74,0x75,0x76,
			 0x77,0x78,0x79,0x7A,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x2B,0x2F};

static char binToEB64[] ={0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
                          0xd8,0xd9,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0x81,0x82,0x83,0x84,0x85,0x86,
                          0x87,0x88,0x89,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0xa2,0xa3,0xa4,0xa5,
                          0xa6,0xa7,0xa8,0xa9,0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0x4e,0x61};
                          

char *encodeBase64(ShortLivedHeap *slh, const char buf[], int size, int *resultSize, int useEbcdic){
  int   allocSize = BASE64_ENCODE_SIZE(size)+1;  /* +1 for null term */
  char *result = (slh ? SLHAlloc(slh,allocSize) : safeMalloc31(allocSize,"BASE64"));
  if (result){
    encodeBase64NoAlloc(buf, size, result, resultSize, useEbcdic);
    return result;
  } else{
    *resultSize = 0;
    return NULL;
  }
}

void encodeBase64NoAlloc(const char buf[], int size, char result[], int *resultSize,
                         int useEbcdic){
  const char *data = (char*)buf;
  char equalsChar = (useEbcdic ? '=' : 0x3D);
  char *translation = (useEbcdic ? binToEB64 : binToB64);
    int numFullGroups = size / 3;
    int numBytesInPartialGroup = size - 3 * numFullGroups;
    int inCursor = 0;
    char *resPtr = result;
#ifdef DEBUG
    printf("num in full groups %d num in partial %d\n",numFullGroups,numBytesInPartialGroup);
#endif
    for (int i = 0; i < numFullGroups; i++){
      int byte0 = data[inCursor++] & 0xff;
      int byte1 = data[inCursor++] & 0xff;
      int byte2 = data[inCursor++] & 0xff;
      *resPtr++ = (char)(translation[byte0 >> 2]); 
      *resPtr++ = (char)(translation[(byte0 << 4) & 0x3f | (byte1 >> 4)]); 
      *resPtr++ = (char)(translation[(byte1 << 2) & 0x3f | (byte2 >> 6)]); 
      *resPtr++ = (char)(translation[byte2 & 0x3f]); 
    }

    if (numBytesInPartialGroup != 0){
      int byte0 = data[inCursor++] & 0xff;
      *resPtr++ = (char)(translation[byte0 >> 2]); 
      if (numBytesInPartialGroup == 1){
        *resPtr++ = (char)(translation[(byte0 << 4) & 0x3f]); 
        *resPtr++ = equalsChar; 
        *resPtr++ = equalsChar; 
      }
      else{
        int byte1 = data[inCursor++] & 0xff;
        *resPtr++ = (char)(translation[(byte0 << 4) & 0x3f | (byte1 >> 4)]); 
        *resPtr++ = (char)(translation[(byte1 << 2) & 0x3f]); 
        *resPtr++ = equalsChar; 
      }
    }
    *resultSize = resPtr - result;
    result[*resultSize] = 0;
}

/*
 * Assumes "EBCDIC base64" on EBCDIC platforms
 */
int base64ToBase64url(char *s) {
  int i = 0;
  unsigned int c;

  while ((c = s[i]) != '\0') {
    switch (c) {
    case '+':
      s[i] = '-';
      break;
    case '/':
      s[i] = '_';
      break;
    }
    i++;
  }
  while ((i > 0) && s[i - 1] == '=') {
    s[--i] = '\0';
  }
  return i;
}

/*
 * Assumes "EBCDIC base64" on EBCDIC platforms
 */
int base64urlToBase64(char *s, int bufSize) {
  int i = 0;
  unsigned int c;
  int rc = 0;

  while (((c = s[i]) != '\0') && (i < bufSize)) {
    switch (c) {
    case '-':
      s[i] = '+';
      break;
    case '_':
      s[i] = '/';
      break;
    }
    i++;
  }
  switch (i % 4) {
    case 2:
      if (i >= bufSize - 2) {
        rc = -2;
        break;
      } else {
        s[i++] = '=';
        rc++;
        /* fall through */
      }
    case 3:
      if (i >= bufSize - 1) {
        rc = -1;
        break;
      } else {
        s[i++] = '=';
        rc++;
        /* fall through */
      }
    case 0:
      s[i++] = '\0';
      break;
    default:
      rc = -3;
  }
  return rc;
}


char *destructivelyUnasciify(char *s){
  int i;
  for (i=0; s[i] != '\0'; i++){
    char c = s[i];
    if (c == 0x24){
      s[i] = 0x5b;
    } else if (c == 0x25){
      s[i] = 0x6c;
    } else{
      s[i] = ascii[c];
    }
  }
  return s;
}

int ASCIIDecodeTable [256] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'00' - x'0f' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'10' - x'1f' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'20' - x'2f' */
                              -1, -1, 26, 27, 28, 29, 30, 31, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'30' - x'3f' */
                              -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  /* x'40' - x'4f' */
                              15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,  /* x'50' - x'5f' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'60' - x'6f' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'70' - x'7f' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'80' - x'8f' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'90' - x'9f' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'a0' - x'af' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'b0' - x'bf' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'c0' - x'cf' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'd0' - x'df' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'e0' - x'ef' */
                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}; /* x'f0' - x'ff' */
int EBCDICDecodeTable [256] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'00' - x'0f' */
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'10' - x'1f' */
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'20' - x'2f' */
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'30' - x'3f' */
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'40' - x'4f' */
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'50' - x'5f' */
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'60' - x'6f' */
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'70' - x'7f' */
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'80' - x'8f' */
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'90' - x'9f' */
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'a0' - x'af' */
                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* x'b0' - x'bf' */
                               -1,  0,  1,  2,  3,  4,  5,  6,  7,  8, -1, -1, -1, -1, -1, -1,  /* x'c0' - x'cf' */
                               -1,  9, 10, 11, 12, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1,  /* x'd0' - x'df' */
                               -1, -1, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1,  /* x'e0' - x'ef' */
                               -1, -1, 26, 27, 28, 29, 30, 31, -1, -1, -1, -1, -1, -1, -1, -1}; /* x'f0' - x'ff' */

char ASCIIEncodeTable [32] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
                              0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
                              0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
                              0x59, 0x5a, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37};

char EBCDICEncodeTable [32] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                               'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                               'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                               'Y', 'Z', '2', '3', '4', '5', '6', '7'};

int base32Decode (int alphabet,
                  char *input,
                  char *output,
                  int *outputLength,
                  int useEBCDIC) {

  int i = 0;
  int inputLength = strlen(input);
  int numFullGroups = inputLength / 8;
  int outputIndex = 0;
  int *decodeTable = NULL;
  
  int ch0 = 0;
  int ch1 = 0;
  int ch2 = 0;
  int ch3 = 0;
  int ch4 = 0;
  int ch5 = 0;
  int ch6 = 0;
  int ch7 = 0;

  if (alphabet != RFC4648) {
    return INVALID_ALPHABET;
  }
  if (inputLength %8 != 0) {
    return INVALID_DECODE_SIZE;
  }
  if ((((inputLength + 7) / 8) * 5) > *outputLength) {
    return DECODE_LENGTH_TOO_SMALL;
  }
  if (useEBCDIC) {
    decodeTable = EBCDICDecodeTable;
  } else {
    decodeTable = ASCIIDecodeTable;
  }
  for (i = 0;
       i < numFullGroups;
       i++)
  {
    char equals = useEBCDIC ? '=' : 0x3d;
    ch0 = (*input == equals ? -2 : decodeTable [*input++]);
    ch1 = (*input == equals ? -2 : decodeTable [*input++]);
    ch2 = (*input == equals ? -2 : decodeTable [*input++]);
    ch3 = (*input == equals ? -2 : decodeTable [*input++]);
    ch4 = (*input == equals ? -2 : decodeTable [*input++]);
    ch5 = (*input == equals ? -2 : decodeTable [*input++]);
    ch6 = (*input == equals ? -2 : decodeTable [*input++]);
    ch7 = (*input == equals ? -2 : decodeTable [*input++]);
    if (ch0 == -1 ||
        ch1 == -1 ||
        ch2 == -1 ||
        ch3 == -1 ||
        ch4 == -1 ||
        ch5 == -1 ||
        ch6 == -1 ||
        ch7 == -1) {
      return DECODE_CHAR_INVALID;
    }
    if (ch0 == -2) {
      break;
    }
    output [outputIndex++] = (char) ((ch0 << 3) | (ch1 >> 2));
    if (ch2 == -2) {
      break;
    }
    output [outputIndex++] = (char) ((ch1 << 6) | (ch2 << 1) | (ch3 >> 4) & 0xff);
    if (ch4 == -2) {
      break;
    }
    output [outputIndex++] = (char) ((ch3 << 4) | (ch4 >> 1) & 0xff);
    if (ch5 == -2) {
      break;
    }
    output [outputIndex++] = (char) ((ch4 << 7) | (ch5 << 2) | (ch6 >> 3) & 0xff);
    if (ch7 == -2) {
      break;
    }
    output [outputIndex++] = (char) ((ch6 << 5) | (ch7) & 0xff);
  }
  *outputLength = outputIndex;
  return 0;
}

int base32Encode (int alphabet,
                  char *input,
                  int inputLength,
                  char *output,
                  int *outputLength,
                  int useEBCDIC) {

  int i = 0;
  int numFullGroups = inputLength / 5;
  int numCharsLeft = inputLength % 5;
  int outputIndex = 0;
  char *encodeTable = NULL;

  int byte0 = 0;
  int byte1 = 0;
  int byte2 = 0;
  int byte3 = 0;
  int byte4 = 0;
  
  if (alphabet != RFC4648) {
    return INVALID_ALPHABET;
  }

  if (((((inputLength + 4) / 5) * 8) + 1) > *outputLength) {
    return ENCODE_LENGTH_TOO_SMALL;
  }
  if (useEBCDIC) {
    encodeTable = EBCDICEncodeTable;
  } else {
    encodeTable = ASCIIEncodeTable;
  }
  for (i = 0;
       i < numFullGroups;
       i++)
  {
    byte0 = *input++ & 0xff;
    byte1 = *input++ & 0xff;
    byte2 = *input++ & 0xff;
    byte3 = *input++ & 0xff;
    byte4 = *input++ & 0xff;
    output [outputIndex++] = (char)(encodeTable [(byte0 >> 3) & 0x1f]); 
    output [outputIndex++] = (char)(encodeTable [(byte0 << 2) & 0x1f | (byte1 >> 6)]); 
    output [outputIndex++] = (char)(encodeTable [(byte1 >> 1) & 0x1f]); 
    output [outputIndex++] = (char)(encodeTable [(byte1 << 4) & 0x1f | (byte2 >> 4)]); 
    output [outputIndex++] = (char)(encodeTable [(byte2 << 1) & 0x1f | (byte3 >> 7)]); 
    output [outputIndex++] = (char)(encodeTable [(byte3 >> 2) & 0x1f]); 
    output [outputIndex++] = (char)(encodeTable [(byte3 << 3) & 0x1f | (byte4 >> 5)]); 
    output [outputIndex++] = (char)(encodeTable [(byte4 & 0x1f)]); 
  }
  if (numCharsLeft) {
    int byte0 = *input++;
    char equals = useEBCDIC ? '=' : 0x3d;
    output [outputIndex++] = (char)(encodeTable [(byte0 >> 3) & 0x1f]);
    if (numCharsLeft == 1) {
      output [outputIndex++] = (char)(encodeTable [(byte0 << 2) & 0x1f]);
      output [outputIndex++] = equals;
      output [outputIndex++] = equals;
      output [outputIndex++] = equals;
      output [outputIndex++] = equals;
      output [outputIndex++] = equals;
      output [outputIndex++] = equals;
    }
    else if (numCharsLeft == 2) {
      int byte1 = *input++;
      output [outputIndex++] = (char)(encodeTable [(byte0 << 2) & 0x1f | (byte1 >> 6)]);
      output [outputIndex++] = (char)(encodeTable [(byte1 >> 1) & 0x1f]);
      output [outputIndex++] = (char)(encodeTable [(byte1 << 4) & 0x1f]);
      output [outputIndex++] = equals;
      output [outputIndex++] = equals;
      output [outputIndex++] = equals;
      output [outputIndex++] = equals;
    }
    else if (numCharsLeft == 3) {
      int byte1 = *input++;
      int byte2 = *input++;
      output [outputIndex++] = (char)(encodeTable [(byte0 << 2) & 0x1f | (byte1 >> 6)]);
      output [outputIndex++] = (char)(encodeTable [(byte1 >> 1) & 0x1f]);
      output [outputIndex++] = (char)(encodeTable [(byte1 << 4) & 0x1f | (byte2 >> 4)]);
      output [outputIndex++] = (char)(encodeTable [(byte2 << 1) & 0x1f]);
      output [outputIndex++] = equals;
      output [outputIndex++] = equals;
      output [outputIndex++] = equals;
    }
    else if (numCharsLeft == 4) {
      int byte1 = *input++;
      int byte2 = *input++;
      int byte3 = *input++;
      output [outputIndex++] = (char)(encodeTable [(byte0 << 2) & 0x1f | (byte1 >> 6)]); 
      output [outputIndex++] = (char)(encodeTable [(byte1 >> 1) & 0x1f]); 
      output [outputIndex++] = (char)(encodeTable [(byte1 << 4) & 0x1f | (byte2 >> 4)]); 
      output [outputIndex++] = (char)(encodeTable [(byte2 << 1) & 0x1f | (byte3 >> 7)]); 
      output [outputIndex++] = (char)(encodeTable [(byte3 >> 2) & 0x1f]); 
      output [outputIndex++] = (char)(encodeTable [(byte3 << 3) & 0x1f]); 
      output [outputIndex++] = equals;
    }
  }
  output [outputIndex++] = 0;
  *outputLength = outputIndex;
  return 0;
}


ListElt *cons(void *data, ListElt *list){
  ListElt *newList = (ListElt*)safeMalloc(sizeof(ListElt),"ListElt");
  newList->data = data;
  newList->next = list;
  return newList;
}

ListElt *cons64(void *data, ListElt *list){
  ListElt *newList = (ListElt*)safeMalloc(sizeof(ListElt),"ListElt");
  newList->data = data;
  newList->next = list;
  return newList;
}


static ShortLivedHeap *makeShortLivedHeapInternal(int blockSize, int maxBlocks, int is64){
  ShortLivedHeap *heap = (ShortLivedHeap*)safeMalloc(sizeof(ShortLivedHeap),"ShortLivedHeap");
  memcpy(heap->eyecatcher,"SLH SLH ",8);
  
  heap->is64 = is64;
  heap->activeBlock = NULL;
  heap->blockChain = NULL;
  heap->blockCount = 0;
  heap->blockSize = blockSize;
  heap->maxBlocks = maxBlocks;
  return heap;
}

static void reportSLHFailure(ShortLivedHeap *slh, int size){
  printf("could not get any memory for SLH extension, extension size was 0x%x\n",size);
  ListElt *chain = slh->blockChain;
  int i = 0;
  while (chain){
    char *data = chain->data-4;
    int size = *((int*)data);
    printf("  segment %d at 0x%p, size = 0x%x\n",i,data,size+4);
    chain = chain->next;
    i++;
  }
}

ShortLivedHeap *makeShortLivedHeap(int blockSize, int maxBlocks){
#ifdef __ZOWE_64
  return makeShortLivedHeapInternal(blockSize,maxBlocks,TRUE);
#else
  return makeShortLivedHeapInternal(blockSize,maxBlocks,FALSE);
#endif
}

ShortLivedHeap *makeShortLivedHeap64(int blockSize, int maxBlocks){
  return makeShortLivedHeapInternal(blockSize,maxBlocks,TRUE);
}

char *SLHAlloc(ShortLivedHeap *slh, int size){
  /* expand for fullword alignment */
  int rem = size & 0x7;
  if (rem != 0){
    size += (8-rem);
  }
  char *data;
  /* 
     printf("slh=0x%p\n",slh);fflush(stdout);
     printf("SLHAlloc me=0x%p size=%d bc=%d\n",slh,size,slh->blockCount);fflush(stdout);
  */
  int remainingHeapBytes = (slh->blockSize * (slh->maxBlocks - slh->blockCount));
  if (size > remainingHeapBytes){
    printf("SLH at 0x%p cannot allocate above block size %d > %d mxbl %d bkct %d bksz %d\n",
	   slh,size,remainingHeapBytes,slh->maxBlocks,slh->blockCount,slh->blockSize);
    fflush(stdout);
    char *mem = (char*)0;
    mem[0] = 13;
    return NULL;
  } else if (size > slh->blockSize){
    char *bigBlock = (slh->is64 ? 
                      safeMalloc64(size+4,"SLH Oversize Extend") :
                      safeMalloc31(size+4,"SLH Oversize Extend"));
    if (bigBlock == NULL){
      reportSLHFailure(slh,size);
    }
    int *sizePtr = (int*)bigBlock;
    *sizePtr = size;
    bigBlock += 4;
    if (slh->blockChain){
      ListElt *blockTail = slh->blockChain->next;
      ListElt *newChainElement = (slh->is64 ?
                                  cons64(bigBlock,blockTail) :
                                  cons(bigBlock,blockTail));
      slh->blockChain->next = newChainElement;
    } else{
      slh->blockChain = cons(bigBlock,NULL);
      slh->activeBlock = bigBlock;
      slh->bytesRemaining = 0;
    }
    slh->blockCount++;
    return bigBlock;
  }

  if ((slh->activeBlock == NULL) ||
      (slh->bytesRemaining < size)){
    char *data = (slh->is64 ?
                  safeMalloc64(slh->blockSize+4,"SLH Extend") :
                  safeMalloc31(slh->blockSize+4,"SLH Extend") );
    if (data == NULL){
      reportSLHFailure(slh,size);
    }
    int *sizePtr = (int*)data;
    *sizePtr = slh->blockSize;
    data += 4;
    slh->activeBlock = data;
    slh->blockChain = (slh->is64 ?
                       cons64(data,slh->blockChain) :
                       cons(data,slh->blockChain) );
    slh->bytesRemaining = slh->blockSize;
    slh->blockCount++;
  }
  slh->bytesRemaining -= size;
  data = slh->activeBlock;
  slh->activeBlock += size;
  return (char *)data;
  }

void SLHFree(ShortLivedHeap *slh){
  ListElt *chain = slh->blockChain;
  while (chain){
    ListElt *elt = chain;
    char *data = chain->data-4;
    int size = *((int*)data);
    /* printf("SLHFree %d bytes\n",size); */
    if (slh->is64){
      safeFree64(data,size+4);
    } else{
      safeFree31(data,size+4);
    }
    chain = chain->next;
    safeFree((char*)elt,sizeof(ListElt));
  }
  safeFree((char*)slh,sizeof(ShortLivedHeap));
}

char *noisyMalloc(int size){
  char *data = safeMalloc(size,"NoisyMalloc");
  if (data == 0){
    printf("malloc failure for allocation of %d bytes\n",size);
    fflush(stdout);
  }
  return data;
}

StringList *makeStringList(ShortLivedHeap *slh){
  StringList *list = NULL;

  /* printf("makeStringList slh=0x%x size=%d\n",slh,sizeof(StringList));fflush(stdout); */
  list = (StringList*)SLHAlloc(slh,sizeof(StringList));
  list->slh = slh;
  list->count = 0;
  list->totalSize = 0;
  list->head = NULL;
  list->tail = NULL;
  /* printf("returning string list 0x%x\n",list); */
  return list;
}

int stringListLength(StringList *list){
  if (list->head){
    StringListElt *elt = list->head;
    int res = 0;
    while (elt){
      res ++;
      elt = elt->next;
    }
    return res;
  } else{
    return 0;
  }
}

int stringListContains(StringList *list, char *s){
  if (list->head){
    StringListElt *elt = list->head;
    while (elt){
      if (!strcmp(elt->string,s)){
	return 1;
      }
      elt = elt->next;
    }
    return 0;
  } else{
    return 0;
  }
}

char *stringListLast(StringList *list){
  if (list->head){
    StringListElt *elt = list->head;
    StringListElt *last = NULL;
    do{
      last = elt;
      elt = elt->next;
    } while (elt);
    return last->string;
  } else{
    return NULL;
  }
}

int addToStringListUnique(StringList *list, char *s){
  if (!stringListContains(list,s)){
    addToStringList(list,s);
    return 1;
  } else{
    return 0;
  }
}

void addToStringList(StringList *list, char *s){
  StringListElt *elt = (StringListElt*)SLHAlloc(list->slh,sizeof(StringListElt));
  elt->string = s;
  elt->next = NULL;
  list->count++;
  list->totalSize += strlen(s);
  /* 
     printf("addToStringList %s len=%d total=%d count=%d\n",
     s,strlen(s),list->totalSize,list->count);
     */
  fflush(stdout);
  if (list->head){
    list->tail->next = elt;
    list->tail = elt;
  } else{
    list->tail = elt;
    list->head = elt;
  }
}

StringListElt *firstStringListElt(StringList *list){
  return list->head;
}

char *stringListPrint(StringList *list, int start, int max, char *separator, char quoteChar){
  int separatorLen = strlen(separator);
  char *out = NULL;
  int pos = 0;
  int count = 0;
  StringListElt *elt = list->head;
  int allocSize = list->totalSize + (list->count * (separatorLen + 2 * (quoteChar ? 1 : 0))) + 1;
  int i;

  /* printf("stringListPrint totalSize = %d listCount=%d buffer size %d, max=%d, slh=0x%x\n",
     list->totalSize,list->count,allocSize,max,list->slh); */
  out = SLHAlloc(list->slh, allocSize);
  memcpy(out,"                        ",20);
  for (i=0; (i<start && elt); i++){
    elt = elt->next;
  }
  while (elt && (count < max)){
    int eltLen = strlen(elt->string);
    if (quoteChar){
      out[pos] = quoteChar;
      memcpy(out+pos+1,elt->string,eltLen);
      out[pos+eltLen+1] = quoteChar;
      pos += (eltLen+2);
    } else{
      memcpy(out+pos,elt->string,eltLen);
      pos += eltLen;
    }
    if (elt->next && (count+1 < max)){
      memcpy(out+pos,separator,separatorLen);
      pos += separatorLen;
    }
    count++;
    elt = elt->next;
  }
  /* printf("string list formats to %d chars \n",pos); */
  out[pos] = 0;
  return out;
}

char *stringConcatenate(ShortLivedHeap *slh, char *s1, char *s2){
  int l1 = strlen(s1);
  int l2 = strlen(s2);
  char *s = SLHAlloc(slh,l1+l2+1);
  memcpy(s,s1,l1);
  memcpy(s+l1,s2,l2);
  s[l1+l2] = 0;
  return s;
}

static int readBufferCharacter(CharStream *s, int trace){
  if (s->byteCount >= s->size){
    return EOF;
  } else{
    char *buffer = (char*)(s->internalStream);
    if (trace){
      printf("readBufferChar %s\n",buffer);
    }
    int c = (int)(buffer[s->byteCount++]);
    return c;
  }
}

static int getBufferPosition(CharStream *s){
  return s->byteCount;
}

static int eofBuffer(CharStream *s){
  return s->byteCount >= s->size;
}

CharStream *makeBufferCharStream(char *buffer, int len, int trace){
  CharStream *s = (CharStream*)safeMalloc(sizeof(CharStream),"CharStream");

  if (trace){
    printf("mbcs in %d\n",len);
  }
  s->byteCount = 0;
  s->size = len;
  s->internalStream = buffer;
  s->readMethod = readBufferCharacter;
  s->positionMethod = getBufferPosition;
  s->closeMethod = NULL;
  if (trace){
    printf("mbcs out 0x%p, '%.*s'\n",s,len,buffer);
  }
  return s;
}

int charStreamPosition(CharStream *s){
  if (s->positionMethod){
    return (s->positionMethod)(s);
  } else{
    return -1;
  }
}

int charStreamGet(CharStream *s, int trace){
  return (s->readMethod)(s, trace);
}

int charStreamEOF(CharStream *s){
  return (s->eofMethod)(s);
}

int charStreamClose(CharStream *s){
  if (s->closeMethod){
    return (s->closeMethod)(s);
  } else {
    return 0;
  }
}

void charStreamFree(CharStream *s){
  safeFree((char*)s,sizeof(CharStream));
}

/* Some common purpose routines for processing string and numberic data */

/* This function is used to pad char strings with spaces to the specified length.
 * The input parameters are:
 * - a pointer to the modified string data,
 * - data length (a limit of filling),
 * - a flag which determines should the terminating null also be substituted by space,
 * - a flag which determines should the empty strings of the specified length be filled by
 * 0's or should be left as is.
 * The function returns the count of the symbols substituted by spaces.
 * It will return 0 if the passed string pointer equals NULL. */

int padWithSpaces(char *str,
                  int actualLength,
                  int handleNullSymbols,
                  int substEmptyStrings) {

  if (str == NULL) {
    return 0;
  }

  int spaceCount = 0;

  int length;
  int isStringInfinite = 1;
  // Check for the "infinite" strings
  for (int i = 0; i < actualLength; i++) {
    if (*(str+i) == '\0') {
      isStringInfinite = 0;
      length = i;
      break;
    }
  }
  if (isStringInfinite) return 0;

  if (length == 0 && substEmptyStrings == NOT_SUBST_EMPTY) {
    return 0;
  }
  int border = length;
    if (handleNullSymbols == LEAVE_NULL) {
      border++;
    }
  for (int i = border; i < actualLength; i++) {
    memset(str + i, ' ', 1);
    spaceCount++;
  }

  return spaceCount;

}

/* This function replaces the termination null symbols in a char string for spaces
 * up to the specified length. */
int replaceTerminateNulls(char *str, int actualLength) {

  if (str == NULL) {
      return 0;
  }

  int replaceCount = 0;

  for (int i = actualLength - 1; i >= 0; i--) {
    if (*(str + i) == '\0') {
      *(str + i) = ' ';
      replaceCount++;
      // If the next symbol is not NULL, break the cycle.
      if (i != 0 && *(str - actualLength + i) != '\0') break;
    }
    else break;
  }

  return replaceCount;

}

/* This routine allows to convert from int in decimal representation to string.
 * For this version, the number of symbols in the output must be specified
 * to add zeroes to the time and date properly.
 * *string - the pointer to the first element of an output string array,
 * ptrSize - number of digits to convert (to add the leading zeros),
 * input - input integer number.
 */
void convertIntToString(char *string, int ptrSize, int input) {

  unsigned int rem = input;
  for (int i = 0; i < ptrSize; i++) {
    int digit = rem % 10;
    char value = (char)digit + '0';
    memset(string + ptrSize - 1 - i, value, 1);
    rem = rem / 10;
  }
  return;

}

/* This function unpacks sort-of-packed decimal (without 0xC at the end of the input number).
 * Example: 0x98 -> decimal 98. The parameter "digit count" specifies the size of the input in digits.*/
unsigned int hexToDec(unsigned int hex, int digitCount) {

  unsigned int result = 0;

  unsigned int mask = 0x0000000F << (4 * (digitCount - 1));
  for (int i = 0; i < digitCount; i++) {
    //My favorite part
    int digit = ( hex & ( mask >> 4 * i ) ) >> ((digitCount - 1 - i)*4);
    result = result * 10 + digit;
  }
  return result;
}

/* This function converts a decimal number to the meaningful part of a packed decimal.
 * This is an inverse function for hexToDec.  */
unsigned int decToHex(unsigned int dec, int digitCount) {

  unsigned int result = 0;

  unsigned int rem = dec;
  unsigned int pow16 = 1;
  for (int i = 0; i < digitCount; i++) {
    int digit = rem % 10;
    result += digit * pow16;
    pow16 *= 16;
    rem = rem / 10;

  }
  return result;

}

/*
 * This function checks if one character sequence equals, greater
 * or less than another one.
 * It is used to match SMF records and VTS entries with each other by name etc.
 * If the sequences match, 0 is returned, otherwise - 1 if the first sequence
 * is later in alphabetical order, -1 - if the first sequence stands
 * earlier alphabetically.
 * If one of the pointers equals 0, the function returns 4.
 */

int compareSequences(const char *firstSequence,
                     const char *secondSequence,
                     int compareLength) {

  if (firstSequence == NULL || secondSequence == NULL) return 4;

  for (int i = 0; i < compareLength; i++) {
    if (firstSequence[i] > secondSequence[i]) {
      return 1;
    }
    else if (firstSequence[i] < secondSequence[i]) {
      return -1;
    }
  }

  return 0;
}

void convertUnixToISO(int unixTime, ISOTime *timeStamp) {
  char rawTime[15];
  char year[5];
  char month[3];
  char day[3];
  char hour[3];
  char minute[3];
  char second[3];

  unixToTimestamp(unixTime, rawTime);
  rawTime[14] = '\0';
  memcpy(year, rawTime, 4);
  year[4] = '\0';
  memcpy(month, rawTime+4, 2);
  month[2] = '\0';
  memcpy(day, rawTime+6, 2);
  day[2] = '\0';
  memcpy(hour, rawTime+8, 2);
  hour[2] = '\0';
  memcpy(minute, rawTime+10, 2);
  minute[2] = '\0';
  memcpy(second, rawTime+12, 2);
  second[2] = '\0';

  sprintf(timeStamp->data, "%s-%s-%sT%s:%s:%s", year, month, day, hour, minute, second);
}

int decimalToOctal(int decimal) {
  int octalArray[3];

  int i = 0;
  while (decimal != 0) {
    octalArray[i] = decimal % 8;
    decimal /= 8;
    i++;
  }

  int k = 0;
  for (int j = i - 1; j >= 0; j--) {
    k = 10 * k + octalArray[j];
  }

  return k;
}

#define MAX_WILDCARDS 4

#ifdef MACTH_WITH_WILD_CARD_DEBUG_OK
#define debugPrintf(formatString, ...) \
 printf(formatString, ##__VA_ARGS__)
#else
#define debugPrintf(formatString, ...)
#endif

static int incrementPlaceValues(int *placeValues,
                                int lim, int digits);


int matchWithWildcards(char *pattern, int patternLen,
                       char *s, int len, int flags){

  int wildcardPositions[MAX_WILDCARDS];
  int wildcardState[MAX_WILDCARDS];
  int fixedWidths[MAX_WILDCARDS+1];
  int wildcardCount = 0;
  int ignoreCase = flags & STRLIKE_IGNORE_CASE;
  char zeroOrMoreChar = '*';
  char exactlyOneChar = '%';
  char spaceChar = ' ';
  int allMatch = TRUE;
  int patternPos = 0;
  int pos = 0;
  int currentFixedWidth = 0;
  int totalFixedWidth = 0;
  int lengthWithoutTailJunk = len;

  if (flags & STRLIKE_UTF8_PATTERN){
    if (flags & STRLIKE_MIMIC_SQL){
      zeroOrMoreChar = (char)0x25;
      exactlyOneChar = (char)0x5F;
      spaceChar = (char)0x20;
    } else{
      zeroOrMoreChar = (char)0x2A;
      exactlyOneChar = (char)0x25;
      spaceChar = (char)0x20;
    }
  } else if (flags & STRLIKE_MIMIC_SQL){
    zeroOrMoreChar = '%';
    exactlyOneChar = '_';
    spaceChar = ' ';
  }


  /* count wildcards and identify special cases
     also, if no wilcards, just do the match */
  while (patternPos < patternLen){
    char c = pattern[patternPos];
    debugPrintf("loop one patternChar = %c wildCount=%d curFixWidth=%d totalFix=%d patPos=%d patLen=%d\n",
           c,wildcardCount,currentFixedWidth,totalFixedWidth,patternPos,patternLen);
    if (c == zeroOrMoreChar){
      if (wildcardCount == MAX_WILDCARDS){
        return 0;
      } else{
        wildcardPositions[wildcardCount] = patternPos;

        fixedWidths[wildcardCount] = currentFixedWidth;
        totalFixedWidth += currentFixedWidth;
        currentFixedWidth = 0;
        wildcardState[wildcardCount++] = 0;
      }
    } else {
      currentFixedWidth++;
      if (c != exactlyOneChar &&
          c != s[pos]){
        allMatch = FALSE;
        if (wildcardCount == 0){
          return FALSE;
        }
      }
      pos++;
    }
    patternPos++;
  }
  fixedWidths[wildcardCount] = patternLen - (wildcardCount + totalFixedWidth);
  wildcardState[wildcardCount] = 0;


  int i = 0;
  debugPrintf("after analysis wildCount=%d \n",wildcardCount);
  debugPrintf("  wild positions ");
  for (i=0; i<wildcardCount; i++){
    debugPrintf("%d ",wildcardPositions[i]);
  }
  debugPrintf("\n");
  debugPrintf("  fixed width section lengths ");
  for (i=0; i<=wildcardCount; i++){
    debugPrintf("%d ",fixedWidths[i]);
  }
  debugPrintf("\n");

  if (wildcardCount == 0){
    if (allMatch){
      if (patternLen == len){
        return TRUE;
      } else{
        return FALSE;
      }
    } else{
      return FALSE;
    }

  } else if ((wildcardCount == 1) &&                         /* The "ends with a star" case  */
             (wildcardPositions[1] == (patternLen-1))){
    debugPrintf("END WITH STAR!!\n");
    return (allMatch ? TRUE : FALSE);
  }

  /* if no special cases applied, perform general algorithm */
  int firstSectionLength = wildcardPositions[0];
  int longestPossibleWildcardMatch = len-totalFixedWidth;

  debugPrintf("firstSectionLength=%d totalFixed=%d longestPossibleWildcardMatch=%d\n",
              firstSectionLength,totalFixedWidth,longestPossibleWildcardMatch);

  do{
    int matchLength = totalFixedWidth;
    for (i=0; i<wildcardCount; i++){
      debugPrintf("%d ",wildcardState[i]);
      matchLength += wildcardState[i];
    }
    debugPrintf("\n");

    if (matchLength > len){ /* too much fixed and wildcard length to match */
      continue;
    }

    pos = firstSectionLength;;       /* never work on first fixed section again */
    patternPos = firstSectionLength;
    int wildcardIndex = 0;

    while ((pos < len) &&
           (patternPos < patternLen)){
      char c = pattern[patternPos];
      debugPrintf("  patPos=%d c=%c pos=%d wildcardIndex=%d\n",patternPos,c,pos,wildcardIndex);
      if (c == zeroOrMoreChar){
        pos += wildcardState[wildcardIndex++]; /* this "skips" the characters */
      } else if (c == exactlyOneChar){
        pos ++;
      } else if (c == s[pos]){
        pos++;
      } else{
        break; /* no chance of matching with this wilcard length combo */
      }
      patternPos++;
    }
    debugPrintf("after while patPos=%d pos=%d wildcardIndex=%d\n",patternPos,pos,wildcardIndex);
    if (patternPos == patternLen) {
      if (flags & STRLIKE_ALLOW_TAIL_JUNK) {
        int allJunk = TRUE;
        while (pos < len){
          if (s[pos] > spaceChar){
            allJunk = FALSE;
            break;
          }
          pos++;
        }
        if (allJunk){
          return TRUE;
        }
      } else if (pos == len){
        return TRUE;
      }
    } else if ((patternPos+1 == patternLen) &&
               (wildcardIndex+1 == wildcardCount)){
      return TRUE;
    }
  } while (wildcardCount == 1 ?
           (++wildcardState[0] <= longestPossibleWildcardMatch) :
           incrementPlaceValues(wildcardState,longestPossibleWildcardMatch,wildcardCount));

  return 0;
}

static int incrementPlaceValues(int *placeValues,
                                int lim, int digits){
  int leastSignificantColumn = digits-1;
  int canIncrement = TRUE;
  int i;


  if ((placeValues[leastSignificantColumn] + 1) > lim){
    if (leastSignificantColumn == 0){
      return FALSE;
    } else{
      placeValues[leastSignificantColumn] = 0;
      for (i=leastSignificantColumn-1; i>=0; i--){
        /* tracePrintf("inc p value loop i=%d lsC=%d\n",i,leastSignificantColumn); */
        if ((placeValues[i] + 1) <= lim){
          placeValues[i]++;
          break;
        } else{
          if (i == 0){
            canIncrement = FALSE;
            break;
          } else{
            placeValues[i] = 0;
          }
        }
      }
    }
  } else{
    placeValues[leastSignificantColumn]++;
  }
  return canIncrement;
}

bool stringIsDigit(const char * str){
  bool returnValue = TRUE;
  for (int i = 0; i < strlen(str); i ++) {
    if (!isdigit(str[i])) {
      returnValue = FALSE;
      break;
    }
  }
  return returnValue;
}

/* Library does not support reverse string-string */
const char* strrstr(const char * base, const char * find) {
  const char * returnPtr = NULL;
  const char * newPtr = base;
  while ((strstr(newPtr, find)) != NULL) {
    returnPtr = strstr(newPtr, find);
    newPtr = returnPtr + 1;
  }
  return returnPtr;
}

/* trimRight removes whitespace from the end of a string. */
void trimRight(char *str, int length) {
  int i;
  for (i = length - 1; i >= 0; i--) {
    if (str[i] != ' ') {
      break;
    }
    str[i] = '\0';
  }
}

#ifdef __ZOWE_OS_ZOS
int isLowerCasePasswordAllowed(){
  RCVT* rcvt = getCVT()->cvtrac;
  return (RCVTFLG3_BIT_RCVTPLC & (rcvt->rcvtflg3)); /* if lower-case pw allowed */
}
#else
int isLowerCasePasswordAllowed(){
  return TRUE;
}
#endif

bool isPassPhrase(const char *password) {
  return strlen(password) > 8;
}



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

