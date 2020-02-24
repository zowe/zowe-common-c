

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
#include "metalio.h"
#include "qsam.h"
#else

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#endif


#ifndef NOIBMHTTP
  #include <HTAPI.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif
#include "xml.h"
#include "logging.h"

/** worry about turning ebcdic into UTF-8 on the fly */

#define TRUE 1
#define FALSE 0

static int traceXML = TRUE;

int setXMLTrace(int trace){
  int was = traceXML;
  traceXML = trace;
  return was;
}

static void writeFully(xmlPrinter *p, char *text, int len){
  int bytesWritten = 0;
  int loopCount = 0;
  int returnCode = 0;
  int reasonCode = 0;

  if (p->isCustom){
    p->customWriteFully(p,text,len);
  } else if (p->isHTTP){
    long rc;
    unsigned long ullen = (unsigned long)len;

#ifndef NOIBMHTTP
    HTTPD_write(p->handle,(unsigned char *)text,&ullen,&rc);
#endif
  } else{
    while (bytesWritten < len){
#ifndef METTLE
      int newWriteReturn = write(p->s,text+bytesWritten,len-bytesWritten);
#else
      int newWriteReturn = socketWrite(p->s,text+bytesWritten,len-bytesWritten,
				       &returnCode,&reasonCode);
#endif
      /* printf("writeReturn %d\n",newWriteReturn);fflush(stdout); */
      if (newWriteReturn == -1){
	break;
      }
      bytesWritten += newWriteReturn;
      loopCount++;
      if (loopCount > 100) break;
    }
  }
  /* printf("done write fully\n");fflush(stdout); */
}

void writeByte(xmlPrinter *p, char c){
  if (p->isCustom){
    /* printf("writeByte custom 0x%x \n",p->customWriteByte); */
    p->customWriteByte(p,c);
  }

  char buffer[4];
  buffer[0] = c;
  int returnCode = 0;
  int reasonCode = 0;

  if (p->isHTTP){
    long rc;
    unsigned long ullen = 1;

#ifndef NOIBMHTTP
    HTTPD_write(p->handle,(unsigned char *)buffer,&ullen,&rc);
#endif
  } else{
#ifndef METTLE
    write(p->s,buffer,1);
#else
    socketWrite(p->s,buffer,1,
		&returnCode,&reasonCode);
#endif
  }
}

static xmlPrinter *xmlPrintInternal(xmlPrinter *p, char *text, int len, int newLine, int literal){
  if (p->isOpen){
    writeByte(p,'>');
    p->isOpen = 0;
    p->inAttrs = 0;
  }
    
  if (text != NULL){
    if (literal){
      writeFully(p,text,len);
    } else {
      int i;

      for (i=0; i<len; i++){
        char c = text[i];
        switch (c){
	case '<':
          writeFully(p,"&lt;",4);
	  break;
	case '>':
          writeFully(p,"&gt;",4);
          break;
        case '&':
          writeFully(p,"&amp;",5);
          break;
        default:
          writeByte(p,c);
	}
      }
    }
  }
    
  if (newLine){
    writeByte(p,'\n');
  }
    
  p->inContent = TRUE;
  return p;
}


xmlPrinter *xmlPrint(xmlPrinter *p, char *string){
  xmlPrintInternal(p,string,strlen(string),FALSE,FALSE);
  return p;
}

xmlPrinter *xmlPrintPartial(xmlPrinter *p, char *string, int start, int end){
  if (start < end){
    xmlPrintInternal(p,string+start,end-start,FALSE,FALSE);
  }
  return p;
}

xmlPrinter *xmlPrintln(xmlPrinter *p, char *string){
  xmlPrintInternal(p,string,strlen(string),TRUE,FALSE);
  return p;
}

char *itoa(int x, char *buffer, int radix);

xmlPrinter *xmlPrintInt(xmlPrinter *p, int x){
  char buffer[100];
  int len = sprintf(buffer,"%d",x);
  
  xmlPrintInternal(p,buffer,len,FALSE,FALSE);
  return p;
}

xmlPrinter *xmlPrintBoolean(xmlPrinter *p, int x){
  char *s = x ? "TRUE" : "FALSE";
  xmlPrintInternal(p,s,strlen(s),FALSE,FALSE);
  return p;
}

void xmlIndent(xmlPrinter *p){
  if (p->prettyPrint){
    int i = p->stackSize;
    for (i=p->stackSize; i>0; i--){
       writeFully(p,p->indentString,p->indentStringLen);
    }
  }
}


static xmlPrinter *makeXmlPrinterInternal(int isCustom, 
                                          int stream, 
                                          char *xmlDeclaration,
					  void (*writeFullyMethod)(xmlPrinter *, char *, int), 
                                          void (*writeByteMethod)(xmlPrinter *,char),
					  void *object){
  xmlPrinter *p = (xmlPrinter*)safeMalloc(sizeof(xmlPrinter),"xmlPrinter");
  
  p->isHTTP = 0;
  p->isCustom = isCustom;
  p->s = stream;
  p->prettyPrint = TRUE;
  p->lineBetweenElements = FALSE;
  p->wrapAttributes = TRUE;
  p->stackSize = 0;

  p->isOpen = FALSE;
  p->inAttrs = FALSE;
  p->inContent = FALSE;
  p->firstChild = FALSE;
  p->indentString = "  ";
  p->indentStringLen = 2;
  p->asciify = FALSE;
  p->customWriteFully = writeFullyMethod;
  p->customWriteByte = writeByteMethod;
  p->customObject = object;

  if (xmlDeclaration != NULL){
    xmlPrintInternal(p,xmlDeclaration,strlen(xmlDeclaration),TRUE,TRUE);
  }
  zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "makeXMLPrinter custom=%d full 0x%x byte 0x%x\n",
	 p->isCustom,p->customWriteFully,p->customWriteByte);
  return p; 
}

xmlPrinter *makeXmlPrinter(int stream, char *xmlDeclaration){
  xmlPrinter *p = makeXmlPrinterInternal(FALSE,stream,xmlDeclaration,NULL,NULL,NULL);
  return p;
}

xmlPrinter *makeCustomXmlPrinter(char *xmlDeclaration,
				 void (*writeFullyMethod)(xmlPrinter *, char *,int), 
                                 void (*writeByteMethod)(xmlPrinter *, char),
				 void *object){
  xmlPrinter *p = makeXmlPrinterInternal(TRUE,0,xmlDeclaration,writeFullyMethod,writeByteMethod,object);
  return p;
}

xmlPrinter *makeHttpXmlPrinter(unsigned char *handle, char *xmlDeclaration){
  xmlPrinter *p = (xmlPrinter*)safeMalloc(sizeof(xmlPrinter),"xmlPrinter");
  
  if (p == NULL){
    zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_SEVERE, "malloc fail in makeHttpXMLPrinter...\n");
    fflush(stdout);
  }

  p->isHTTP = 1;
  p->isCustom = FALSE;
  p->handle = handle;
  p->prettyPrint = TRUE;
  p->lineBetweenElements = FALSE;
  p->wrapAttributes = TRUE;
  p->stackSize = 0;

  p->isOpen = FALSE;
  p->inAttrs = FALSE;
  p->inContent = FALSE;
  p->firstChild = FALSE;
  p->indentString = "  ";
  p->indentStringLen = 2;
  p->asciify = FALSE;

  if (xmlDeclaration != NULL){
    xmlPrintInternal(p,xmlDeclaration,strlen(xmlDeclaration),TRUE,TRUE);
  }
  return p; 
}

xmlPrinter *xmlStart(xmlPrinter *p, char *elementName){
  if (p->isOpen){
    writeByte(p,'>');
    writeByte(p,'\n');
  }

  if (p->lineBetweenElements && !p->firstChild && p->stackSize == 1) {
    writeByte(p,'\n');
  }
  
  xmlIndent(p);
  writeByte(p,'<');
  writeFully(p,elementName,strlen(elementName));
  if (p->stackSize + 1 >= XML_STACK_LIMIT){
    zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_WARNING, "XML Printer stack limit exceeded\n");
    fflush(stdout);
  }
  if (p->stackSize > 15 || p->stackSize < 0){
    zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_WARNING, "deep /negative XML stack size %d seen while starting level for %s\n",p->stackSize,elementName);
    fflush(stdout);
  }
  p->stack[p->stackSize++] = elementName;
  p->isOpen = TRUE;
  p->inAttrs = FALSE;
  p->firstChild = TRUE;
  
  return p;
  
}


xmlPrinter *xmlEnd(xmlPrinter *p){
  char *elementName = p->stack[--(p->stackSize)];
    
  if (p->isOpen){
    writeFully(p,"/>",2);
    p->isOpen = FALSE;
    p->inAttrs = FALSE;
  }
  else{
    if (p->inContent){
      p->inContent = FALSE;
    }
    else{
      xmlIndent(p);
    }
    
    writeFully(p,"</",2);
    writeFully(p,elementName,strlen(elementName));
    writeByte(p,'>');
    writeByte(p,'\n');
  }
  
  p->firstChild = FALSE;
  return p;
}

void xmlClose(xmlPrinter *p){
  while( p->stackSize )
    xmlEnd(p);

  if( !p->isHTTP && !p->isCustom)
#ifndef METTLE
    close(p->s);
#endif

  safeFree((void*)p,sizeof(xmlPrinter));
}


xmlPrinter *xmlAddTextElement(xmlPrinter *p, char *elementName, char *string, int len){
  xmlStart(p,elementName);
  xmlPrintInternal(p,string,len,FALSE,FALSE);
  xmlEnd(p);
  return p;
}

xmlPrinter *xmlAddString(xmlPrinter *p, char *elementName, char *string){
  xmlStart(p,elementName);
  xmlPrintInternal(p,string,strlen(string),FALSE,FALSE);
  xmlEnd(p);
  return p;
}

xmlPrinter *xmlAddCData(xmlPrinter *p, char *elementName, char *data){
  xmlStart(p,elementName);
  xmlPrintInternal(p,"<![CDATA[",9,FALSE,TRUE);
  xmlPrintInternal(p,data,strlen(data),FALSE,TRUE);
  xmlPrintInternal(p,"]]>",3,FALSE,TRUE);
  xmlEnd(p);
  return p;
}

xmlPrinter *xmlAddBooleanElement(xmlPrinter *p, char *elementName, int truthValue){
  xmlStart(p,elementName);
  xmlPrintBoolean(p,truthValue);
  xmlEnd(p);
  return p;
}

xmlPrinter *xmlAddIntElement(xmlPrinter *p, char *elementName, int x){
  xmlStart(p,elementName);
  xmlPrintInt(p,x);
  xmlEnd(p);
  return p;
}

/*
int xml_main(int argc, char **argv){
  if (argc != 2){
    printf("Usage:  xml <filename>\n");
  } else {
    FILE *fp = NULL;
    int fd = 0;
    xmlPrinter *p = NULL;

    fp = fopen(argv[1],"w");
    p = makeXmlPrinter(fileno(fp),NULL);
    printf("fd = %d\n",fd);
    fflush(stdout);

    xmlStart(p,"OUTER");
    printf("OUTER done\n");fflush(stdout);
    xmlStart(p,"INNER");
    printf("INNER done\n");fflush(stdout);
    xmlPrint(p,"cool stuff");
    printf("PRINT done\n");fflush(stdout);
    xmlEnd(p);
    xmlAddTextElement(p,"TEXT","Text This!",10);
    xmlAddIntElement(p,"INT",1234);
    xmlAddBooleanElement(p,"BOOL",TRUE);
    xmlEnd(p);
    printf("END2 done\n");fflush(stdout);
    xmlClose(p);
    printf("REALLY DONE\n");fflush(stdout);
  }

} 
*/

/* parsing stuff */
/*

 */

static char *xmlTokenTypeNames[10] = { "broken", "start-open", "end-open", "close", "empty-close", "pcdata", "id", "whitespace",
				    "attr-equal", "attr-value" };

static int parseTrace = TRUE;

int setXMLParseTrace(int trace){
  int was = parseTrace;
#ifndef METTLE
  parseTrace = trace;
#endif
  return was;
}

static ByteArrayOutputStream *makeBAOSWithSize(int extraSize){
  ByteArrayOutputStream *baos = (ByteArrayOutputStream*)safeMalloc(sizeof(ByteArrayOutputStream)+extraSize,"ByteArrayOutputStream");
  baos->pos = 0;
  baos->extraSize= extraSize;
  return baos;
} 

ByteArrayOutputStream *makeBAOS(){
  return makeBAOSWithSize(0);
}

static void freeBAOS(ByteArrayOutputStream *baos){
  int extraSize = baos->extraSize;
  safeFree((void*)baos,sizeof(ByteArrayOutputStream)+extraSize);
}

static void writeBAOS(ByteArrayOutputStream *baos, int b){
  if (baos->pos == (BAOS_MAX + baos->extraSize)){
    zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_SEVERE, "PANIC, baos out of room\n");
  }
  baos->bytes[baos->pos++] = (char)b;
}

static void resetBAOS(ByteArrayOutputStream *baos){
  baos->pos = 0;
}

static void syntaxError(XmlParser *p, char *formatString, ...){
  char buffer[2048];
  va_list argPointer;
  va_start(argPointer,formatString);
  vsprintf(buffer,formatString,argPointer);
  va_end(argPointer);
  
  zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_SEVERE, "SYNTAX ERROR line=%d %s\n",(p ? p->lineNumber : -1) ,buffer);
}

static char *getBAOSBytes(XmlParser *p, ByteArrayOutputStream *baos){
  char *data = SLHAlloc(p->slh,baos->pos+1);
  int i,len = baos->pos+1;
  memcpy(data,baos->bytes,baos->pos);
  data[baos->pos] = 0;
  return data;
}

#ifndef METTLE 

XmlParser *makeXmlParser(FILE *in) {
  XmlParser *parser = (XmlParser*)safeMalloc(sizeof(XmlParser),"XML Parser");
  parser->slh = makeShortLivedHeap(65536,1000);
  parser->lineNumber = 0;
  parser->in = in;
  parser->pushback = -1;
  parser->tokenState = XMLTOKEN_STATE_NORMAL;
  parser->tokenBytes = makeBAOS();
  parser->hasPushback = FALSE;
  parser->stringInput = NULL;
  return parser;  
}

#else

#define READER_STATE_INITIAL 0
#define READER_STATE_LINE_MIDDLE 1
#define READER_STATE_NEWLINE 2

#define DEFAULT_QSAM_BUFFER_SIZE 1024

XmlParser *makeXMLParserUSS(UnixFile *file){
  XmlParser *parser = (XmlParser*)safeMalloc(sizeof(XmlParser),"XML Parser");
  parser->slh = makeShortLivedHeap(65536,1000);
  parser->lineNumber = 0;
  parser->file = file;
  parser->eof = 0;
  parser->pushback = -1;
  parser->tokenState = XMLTOKEN_STATE_NORMAL;
  parser->tokenBytes = makeBAOS();
  parser->hasPushback = FALSE;
  parser->stringInput = NULL;
  zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "about to return UnixFile based parser 0x%x with SLH at 0x%x and lineBuffer at 0x%x\n",parser,parser->slh,parser->lineBuffer);
  return parser;    
}

XmlParser *makeXmlParser(char *dcb){
  XmlParser *parser = (XmlParser*)safeMalloc(sizeof(XmlParser),"XML Parser");
  parser->slh = makeShortLivedHeap(65536,1000);
  parser->lineNumber = 0;
  parser->dcb = dcb;
  parser->linePos = 0;
  parser->eof = 0;
  parser->lineBuffer = safeMalloc(DEFAULT_QSAM_BUFFER_SIZE,"XML Line Buffer");
  parser->lineLength = 0;
  parser->readState = READER_STATE_INITIAL;
  parser->pushback = -1;
  parser->tokenState = XMLTOKEN_STATE_NORMAL;
  parser->tokenBytes = makeBAOS();
  parser->hasPushback = FALSE;
  parser->stringInput = NULL;
  zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "about to return DCB based parser 0x%x with SLH at 0x%x and lineBuffer at 0x%x\n",parser,parser->slh,parser->lineBuffer);
  return parser;  
}

void setLineBufferSize(XmlParser *p, int size){
  // safeFree(p->lineBuffer,DEFAULT_QSAM_BUFFER_SIZE);
  p->lineBuffer = safeMalloc(size,"Larger XML QSAM buffer size");
  p->tokenBytes = makeBAOSWithSize(size);
}

#endif

XmlParser *makeXmlStringParser(char *s, int len){
  XmlParser *parser = (XmlParser*)safeMalloc(sizeof(XmlParser),"XML Parser");
  parser->slh = makeShortLivedHeap(65536,1000);
  parser->lineNumber = 0;
  parser->in = NULL;
  parser->pushback = -1;
  parser->tokenState = XMLTOKEN_STATE_NORMAL;
  parser->tokenBytes = makeBAOS();
  parser->hasPushback = FALSE;
  parser->stringInput = s;
  parser->stringInputPos = 0;
  parser->stringInputLen = len;
  return parser;  
}

void freeXmlParser(XmlParser *p){
  freeBAOS(p->tokenBytes);
  SLHFree(p->slh);
  safeFree((void*)p,sizeof(XmlParser));
}

static XMLToken *makeXMLToken(XmlParser *parser, int type){
  /* should really be pooled */
  XMLToken *t = (XMLToken*)SLHAlloc(parser->slh,sizeof(XMLToken));
  t->type = type;
  t->lineNumber = 0;
  t->bytes = NULL;
  return t;
}

void printXMLToken(XMLToken *t){
  zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "<XMLToken %s text=\"%s\">",
	 (t->type >= 0 ? xmlTokenTypeNames[t->type] : "EOF"),
	 (t->bytes != NULL ? t->bytes : ""));
}

static void pushback(XmlParser *p, int b) {
  p->hasPushback = TRUE;
  p->pushback = b;
}

#ifndef METTLE 
static int safeRead(XmlParser *p) {
  if (p->hasPushback) {
    p->hasPushback = FALSE;
    return p->pushback;
  } else {
    int c = 0;
    if (p->stringInput) {
      if (p->stringInputPos == p->stringInputLen){
	c = -1;
      } else{
	c = p->stringInput[p->stringInputPos++];
      }
    } else{
      c = fgetc(p->in);

      
    }
    if (c == 0x15){
      p->lineNumber++;
      if (traceXML) {
        zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "read line number %d\n",p->lineNumber);
      }
    } else{
      if (traceXML) {
        zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "read '%c' on line %d\n",c,p->lineNumber);
      }
    }
    return c;
  }
}


#else

static int sanityCheck(XmlParser *p, char *site){
  if ( (p->lineLength > 1800) && (p->lineBuffer[0x720] != 0x40)){
    zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_SEVERE, "sanity check failed at %s\n",site);
    return 0;
  } else{
    return 1;
  }
}

static int safeReadUnix(XmlParser *p){
  if (p->hasPushback) {
    p->hasPushback = FALSE;
    return p->pushback;
  } else {
    int c = 0;
    if (p->stringInput) {
      if (p->stringInputPos == p->stringInputLen){
	c = -1;
      } else{
	c = p->stringInput[p->stringInputPos++];
      }
    } else{
      int returnCode;
      int reasonCode;
      c = fileGetChar(p->file,&returnCode,&reasonCode);
    }
    if (c == 0x15){
      p->lineNumber++;
      zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "read line number %d\n",p->lineNumber);
    } else{
      zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "read '%c' on line %d\n",c,p->lineNumber);
    }
    return c;
  }
}

static int safeRead1(XmlParser *p) {
  if (p->file || p->stringInput){ /* USS/OMVS File or reading from String */
    return safeReadUnix(p);
  }

  if (p->hasPushback) {
    p->hasPushback = FALSE;
    return p->pushback;
  } else if (p->stringInput){
    int c = 0;
    if (p->stringInputPos == p->stringInputLen){
      c = -1;
    } else{
      c = p->stringInput[p->stringInputPos++];
    }
    if (c == 0x15){
      p->lineNumber++;
    }
    return c;
  } else {
    switch (p->readState){
    case READER_STATE_INITIAL:
      p->eof = getline(p->dcb,p->lineBuffer,&(p->lineLength));
      zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "getline1(), length=%d\n", p->lineLength);
      dumpbuffer(p->lineBuffer,p->lineLength);
      p->readState = READER_STATE_LINE_MIDDLE;
      p->linePos = 1;
      return p->lineBuffer[0];
      break;
    case READER_STATE_LINE_MIDDLE:
      if (p->linePos < p->lineLength){
        int b = p->lineBuffer[p->linePos++];
        return b;
      } else{
	p->readState = READER_STATE_NEWLINE;
	return 0x15;
      }
      break;
    case READER_STATE_NEWLINE:
      if (p->eof){
	return -1;
      } else{
	p->eof = getline(p->dcb,p->lineBuffer,&(p->lineLength));
        zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "getline2() length=%d\n",p->lineLength);
        dumpbuffer(p->lineBuffer,p->lineLength);
        p->lineNumber++;
	p->readState = READER_STATE_LINE_MIDDLE;
	p->linePos = 1;
	return (p->eof ? -1 : p->lineBuffer[0]);
      }
      break;
    default:
      zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "unknown state\n");
      break;
    }
  }
}

static int safeRead(XmlParser *p) {
  int c = safeRead1(p);
  if (c == 0x15){
    zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "newline to %d\n",p->lineNumber);
  } else{
    zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "'%c' seen on line number %d\n",c,p->lineNumber);
  }
  return c;
}

#endif 

#define CHAR_META_CONTROL   0x01
#define CHAR_META_ALPHA    0x02
#define CHAR_META_UPPER    0x04  
#define CHAR_META_LOWER    0x08
#define CHAR_META_DIGIT    0x10
#define CHAR_META_HEXDIGIT 0x20
#define CHAR_META_WHITESPACE 0x40
#define CHAR_META_BLANK      0x80

/* this is a very vanilla ebcdic 37-500-1047 ish */

static char ebcdicMetaData[256] = 
{ /* 0     1     2     3     4     5     6     7     8     9     A     B     C     D    E      F   */
  0x01, 0x01, 0x01, 0x01, 0x01, 0xC1, 0x01, 0x01, 0x01, 0x01, 0x01, 0x41, 0x41, 0x41, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x41, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x41, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 0x40's */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 0x80's */
  0x00, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x26, 0x26, 0x26, 0x26, 0x26, 0x26, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 0xC0's */
  0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   /* 0xF0's */
};

static int isAlpha(char c){
  char meta = ebcdicMetaData[(int)c];
  return meta & CHAR_META_ALPHA;
}

static int isAlphaNumeric(char c){
  char meta = ebcdicMetaData[(int)c];
  return meta & (CHAR_META_ALPHA | CHAR_META_DIGIT);
}

static int isDigit(char c){
  return (c >= 0xF0 && c <= 0xF9);
}

static int isWhitespace(char c){
  char meta = ebcdicMetaData[(int)c];
  return meta & CHAR_META_WHITESPACE;
}

XMLToken *readCommentTail(XmlParser *p){
  int hyphenCount = 0;
  while (TRUE){
    int b = safeRead(p);

    switch (hyphenCount){
    case 0:
      if (b == '-'){
	hyphenCount = 1;
      }
      break;
    case 1:
      if (b == '-'){
	hyphenCount = 2;
      }
      break;
    case 2:
      if (b == '>'){
	goto done;
      } else{
	return NULL;
      }
      break;
    default:
      return NULL;
      break;
    }
  }
done:
  return makeXMLToken(p,XMLTOKEN_COMMENT);
}

XMLToken *getXMLToken(XmlParser *p) {
  int b;

  resetBAOS(p->tokenBytes);
  b = safeRead(p);
  if (b == -1) {
    return makeXMLToken(p,XMLTOKEN_EOF);
  } else if ((char)b == '<') {
    p->tokenState = XMLTOKEN_STATE_TAG;
    b = safeRead(p);
    if ((char)b == '/') {
      return makeXMLToken(p,XMLTOKEN_END_OPEN);
    } else if ((char)b == '!'){
      b =  safeRead(p);
      XMLToken *comment = NULL;
      int badCommentSyntax = TRUE;
      if (b == '-'){
	b = safeRead(p);
	if (b == '-'){
	  badCommentSyntax = FALSE;
	  comment = readCommentTail(p);
	}
      }
      if (badCommentSyntax){
	p->tokenizationFailureReason = "Bad Comment Tag";
	return NULL;
      } else if (comment == NULL){
	p->tokenizationFailureReason = "Bad Comment Termination";
	return NULL;
      } else{
	return comment;
      }
    } else {
      pushback(p,b);
      XMLToken *tt = makeXMLToken(p,XMLTOKEN_START_OPEN);
      return tt;
    }
  } else if (((char)b == '/') && (p->tokenState == XMLTOKEN_STATE_TAG)){
    p->tokenState = XMLTOKEN_STATE_NORMAL;
    b = safeRead(p);
    if ((char)b == '>') {
      return makeXMLToken(p,XMLTOKEN_EMPTY_CLOSE);
    } else {
      return NULL;
    }
  } else if ((char)b == '>') {
    p->tokenState = XMLTOKEN_STATE_NORMAL;
    return makeXMLToken(p,XMLTOKEN_CLOSE);
  } else if (p->tokenState == XMLTOKEN_STATE_TAG) {
    char c = (char)b;
    if (isAlpha(c) || (c == '_') || (c == ':')) {
      writeBAOS(p->tokenBytes,b);
      while ((b = safeRead(p)) > 0) {
	c = (char)b;
	if (isAlpha(c) ||
	    isDigit(c) ||
	    (c == '_') || (c == ':') || (c == '.') || (c == '-')) {
	  writeBAOS(p->tokenBytes,b);
	} else {
	  XMLToken *t;
	  pushback(p,b);
	  t = makeXMLToken(p,XMLTOKEN_ID);
	  t->bytes = getBAOSBytes(p,p->tokenBytes);
	  return t;
	}
      }
      return makeXMLToken(p,b);
    } if (c == '=') {
      return makeXMLToken(p,XMLTOKEN_ATTR_EQUAL);
    } if (c == '"' || c == '\'') {
      char firstChar = c;
      while ((b = safeRead(p)) > 0) {
	c = (char)b;
	if (c == firstChar) {
	  XMLToken *t = makeXMLToken(p,XMLTOKEN_ATTR_VALUE);
	  t->bytes = getBAOSBytes(p,p->tokenBytes);
	  return t;            
	} else {
	  writeBAOS(p->tokenBytes,b);
	}
      }
      return NULL;
    } else if (isWhitespace(c)) {
      while ((b = safeRead(p)) > 0) {
	c = (char)b;
	if (isWhitespace(c)) {
	  writeBAOS(p->tokenBytes,b);
	} else {
	  XMLToken *t;
	  pushback(p,b);
	  t = makeXMLToken(p,XMLTOKEN_WHITESPACE);
	  t->bytes = getBAOSBytes(p,p->tokenBytes);
	  return t;            
	}
      }
      return makeXMLToken(p,b);
    } else {
      zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_WARNING, "help unknown token\n");
      return NULL;
    }
  } else { /* NOT TAG MODE*/
    writeBAOS(p->tokenBytes,b);
    while ((b = safeRead(p)) > 0) {
      if ((char)b == '<') {
	XMLToken *t;
	pushback(p,b);
	t = makeXMLToken(p,XMLTOKEN_PCDATA);
	t->bytes = getBAOSBytes(p,p->tokenBytes);
	return t;
      } else if ((char)b == '&'){
	switch(p->tokenState){
	case XMLTOKEN_STATE_NORMAL:
	  p->charSequence[0] = '&';
	  p->charSequencePos = 1;
	  p->tokenState = XMLTOKEN_STATE_CHAR;
	  break;
	default:
	  return makeXMLToken(p,XMLTOKEN_BROKEN);
	}
      } else if (((char)b == ';') && (p->tokenState == XMLTOKEN_STATE_CHAR)){
	p->charSequence[p->charSequencePos] = 0;
	if (!strcmp(p->charSequence,"&lt")){
	  writeBAOS(p->tokenBytes,'<');
	  p->tokenState = XMLTOKEN_STATE_NORMAL;
	} else if (!strcmp(p->charSequence,"&gt")){
	  writeBAOS(p->tokenBytes,'>');
	  p->tokenState = XMLTOKEN_STATE_NORMAL;
	} else if (!strcmp(p->charSequence,"&amp")){
	  writeBAOS(p->tokenBytes,'&');
	  p->tokenState = XMLTOKEN_STATE_NORMAL;
	} else{
	  return makeXMLToken(p,XMLTOKEN_BROKEN);
	}
      } else if (p->tokenState == XMLTOKEN_STATE_CHAR){
	p->charSequence[p->charSequencePos++] = (char)b;
      } else {
	writeBAOS(p->tokenBytes,b);
      }
    }
    return makeXMLToken(p,b);
  }
}

XMLToken *getTokenNoWS(XmlParser *p) {

  XMLToken *t;
  while (1) {
    t = getXMLToken(p);
    if (traceXML){
      zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "NO WS loopB type=%s, %s\n",
	     xmlTokenTypeNames[t->type],
	     (t->type == XMLTOKEN_ID ? t->bytes : ""));
    }
    if ((t->type != XMLTOKEN_WHITESPACE) &&
	(t->type != XMLTOKEN_COMMENT)){
      return t;
    }
  }
}

XMLNode *makeXMLNode(int type, char *name) {
  XMLNode *node = (XMLNode*)safeMalloc(sizeof(XMLNode),"XML Node");
  node->type = type;
  node->name = name;
  node->childCount = 0;
  node->children = NULL;
  node->attributeCount = 0;
  node->attributes = NULL;
  node->nextSibling = NULL;
  node->prevSibling = NULL;
  node->parent = NULL;
  return node;
}
void addChild(XMLNode *node, XMLNode *child) {
  int prevCount = node->childCount;
  if (node->children == NULL) {
    node->children = (XMLNode**)safeMalloc(4*sizeof(XMLNode*),"XML Node Array");
    node->children[0] = child;
    child->parent = node;
    node->childCount = 1;
    node->childrenLength = 4;
  } else if (node->childCount == node->childrenLength) {
    XMLNode** newChildren = (XMLNode**)safeMalloc(node->childCount * 2 * sizeof(XMLNode*),"XML Node Array Extension");
    memcpy(newChildren,node->children,node->childCount * sizeof(XMLNode*));
    safeFree((void*)node->children,node->childCount*sizeof(XMLNode*));
    newChildren[node->childCount++] = child;
    child->parent = node;
    node->children = newChildren;
    node->childrenLength++;
  } else {
    node->children[node->childCount++] = child;
    child->parent = node;
  }
  if (prevCount > 0){
    XMLNode *lastChild = node->children[prevCount-1];
    child->prevSibling = lastChild;
    lastChild->nextSibling = child;
  }
}

void addAttribute(XMLNode *node, char *attrName, char *attrValue) {
  XMLNode *attrNode = makeXMLNode(NODE_ATTRIBUTE,attrName);
  attrNode->value = attrValue;
  if (node->attributes == NULL) {
    node->attributes = (XMLNode**)safeMalloc(4*sizeof(XMLNode*),"XML Attributes");
    node->attributes[0] = attrNode;
    attrNode->parent = node;
    node->attributeCount = 1;
  } else if (node->attributeCount == node->attributesLength) {
    XMLNode **newAttributes = (XMLNode**)safeMalloc(node->attributeCount * 2 * sizeof(XMLNode*),"XML Attributes Extension");
    memcpy(newAttributes,node->attributes,node->attributeCount * sizeof(XMLNode*));
    safeFree((void*)node->attributes,node->attributeCount*sizeof(XMLNode*));
    newAttributes[node->attributeCount++] = attrNode;
    attrNode->parent = node;
    node->attributes = newAttributes;
  } else {
    node->attributes[node->attributeCount++] = attrNode;
    attrNode->parent = node;
  }
}

void pprintNode2(XMLNode *node, int indent) {
  int a,c,i;
  switch (node->type) {
  case NODE_TEXT:
    for (i=0; i<indent; i++) zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, " ");
    zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "#text: '%s'\n",node->value);
    break;
  case NODE_ELEMENT:
    for (i=0; i<indent; i++) zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, " ");
    zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "<%s cc=%d",node->name,node->childCount);
    for (a=0; a<node->attributeCount; a++) {
      pprintNode2(node->attributes[a],indent);
    }
    if (node->childCount == 0) {
      zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "/>\n");
    } else {
      zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, ">\n");
      for (c=0; c<node->childCount; c++) {
	pprintNode2(node->children[c],indent+2);
      }
      for (i=0; i<indent; i++) zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, " ");
      zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "</%s>\n",node->name);
    }
    break;
  case NODE_ATTRIBUTE:
    zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, " %s=\"%s\"",node->name,node->value);
  }
}

void pprintNode(XMLNode *node) {
  if (node->type != NODE_ELEMENT) {
    syntaxError(NULL,"not an element %s",node->type);
  }
  pprintNode2(node,2);
}

#define NODE_DEPTH_LIMIT 100

#define STATE_ELEMENT 1
#define STATE_ATTRIBUTES 2
#define STATE_ELEMENTS 3
#define STATE_DONE 4

void showStack(XmlParser *p, XMLNode** stack, int stackPointer) {
  int i;
  for (i=stackPointer-1; i>=0; i--) {
    zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "%s ",stack[i]->name);
  }
  zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "\n");
}

static char *stateNames[5] ={ "UNKNOWN", "ELEMENT", "ATTRIBUTES", "ELEMENT", "DONE"};

static char *tokenTypeNames[11] = 
{ "EOF", "BROKEN", "START_OPEN", "END_OPEN", "CLOSE", "EMPTY_CLOSE", "PCDATA", "ID", "WHITESPACE", 
  "ATTR_EQUAL", "ATTR_VALUE"};

XMLNode *parseXMLNode(XmlParser *p) {
  XMLNode **stack = (XMLNode**)safeMalloc(NODE_DEPTH_LIMIT*sizeof(XMLNode*),"XML Node Stack");
  int stackPointer = 0;
  XMLNode *currentNode = NULL;
  int state = STATE_ELEMENT;
  XMLToken *pushbackToken = NULL;
  while (1) {
    XMLToken *firstToken;
    if (pushbackToken != NULL) {
      firstToken = pushbackToken;
      pushbackToken = NULL;
    } else {
      firstToken = getTokenNoWS(p);
    }
    if (traceXML){
      zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "Parse Loop top state %d '%s'tokenType=%d '%s'\n",
	     state,stateNames[state],firstToken->type,tokenTypeNames[firstToken->type]);
      showStack(p,stack,stackPointer);
    }
    switch (state) {
    case STATE_ELEMENT:
      if (firstToken->type == XMLTOKEN_START_OPEN) {
	XMLToken *idToken = getTokenNoWS(p);
	XMLToken *nextToken = getTokenNoWS(p);
	XMLNode *newElement = NULL;
	if ((idToken == NULL) || (idToken->type != XMLTOKEN_ID)) {
	  syntaxError(p,"expected id, got %s for %s",xmlTokenTypeNames[idToken->type],idToken->bytes);
	  return NULL;
	}
	newElement = makeXMLNode(NODE_ELEMENT,idToken->bytes);
	  
	if (currentNode != NULL) {
	  stack[stackPointer++] = currentNode;
	  addChild(currentNode,newElement);
	  currentNode = newElement;
	} else {
            currentNode = newElement;
	} 
	if (nextToken == NULL) {
	  syntaxError(p,"unexpected input NULL");
	  return NULL;
	  } else if (nextToken->type == XMLTOKEN_ID) {
	    /* go for attribute list */
	    state = STATE_ATTRIBUTES;
            pushbackToken = nextToken;
	  } else if (nextToken->type == XMLTOKEN_CLOSE) {
	    state = STATE_ELEMENTS;
	  } else if (nextToken->type == XMLTOKEN_EMPTY_CLOSE) {
            if (stackPointer == 0) {
              state = STATE_DONE; /* currentNode is set */
            } else {
              currentNode = stack[--stackPointer];
	      state = STATE_ELEMENTS; /* continue to look for more elements */
	    }
	  } else {
	    syntaxError(p,"unexpected input, type=%s",xmlTokenTypeNames[nextToken->type]);
	    return NULL;
	  }
	} else {
	  syntaxError(p,"expected tag start %s\n",xmlTokenTypeNames[firstToken->type]);
	  return NULL;
	}
	break;
      case STATE_ATTRIBUTES:
        {
          XMLToken *idToken = firstToken;
          XMLToken *equalsToken = getTokenNoWS(p);
          XMLToken *valueToken = getTokenNoWS(p);
          XMLToken *nextToken = getTokenNoWS(p);
	  if (traceXML){
        zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "ATTR< equalsToken=%s valueToken=%s nextToken=%s\n",
		   equalsToken,valueToken,nextToken);
	  }
          if (equalsToken->type != XMLTOKEN_ATTR_EQUAL) {
	    syntaxError(p,"expected '=', got %s",xmlTokenTypeNames[equalsToken->type]);
            return NULL;
          } else if (valueToken->type != XMLTOKEN_ATTR_VALUE) {
            syntaxError(p,"expected attr value string, got %s",xmlTokenTypeNames[valueToken->type]);
            return NULL;
          } 
          addAttribute(currentNode,idToken->bytes,valueToken->bytes);
          if (nextToken->type == XMLTOKEN_CLOSE) {
            if (currentNode != NULL) {
              
            }
            state = STATE_ELEMENTS;
          } else if (nextToken->type == XMLTOKEN_EMPTY_CLOSE) {
            if (stackPointer == 0) {
              state = STATE_DONE;
            } else {
              currentNode = stack[--stackPointer];
              state = STATE_ELEMENTS;
            }
          } else if (nextToken->type == XMLTOKEN_ID) {
            pushbackToken = nextToken;
          } else {
	    syntaxError(p,"unexpected attribute close type=%s",xmlTokenTypeNames[nextToken->type]);
            return NULL;
          }
        }
        break;
      case STATE_ELEMENTS:
	if (firstToken->type == XMLTOKEN_END_OPEN) {
	  XMLToken *idToken = getTokenNoWS(p);
	  XMLToken *nextToken = getTokenNoWS(p);
	  if ((idToken == NULL) || (idToken->type != XMLTOKEN_ID)) {
	    syntaxError(p,"expected id, %s",xmlTokenTypeNames[idToken->type]);
	    return NULL;
	  } else if ((nextToken == NULL) || (nextToken->type != XMLTOKEN_CLOSE)) {
	    syntaxError(p,"expected close %s",xmlTokenTypeNames[nextToken->type]);
	    return NULL;
	  }
	  
	  if (currentNode == NULL) {
	    syntaxError(p,"how can there be no current node");
	  } else {
	    char *closeName = idToken->bytes;
	    if (strcmp(closeName,currentNode->name)) {
	      syntaxError(p,"tag name mismatch, expecting %s at line %d\n",
			  xmlTokenTypeNames[currentNode->type],p->lineNumber);
	      /* " got "+idToken); */
	      return NULL;
	    }
	  }
	  if (stackPointer == 0) {
	    state = STATE_DONE;
	  } else {
	    currentNode = stack[--stackPointer]; 
	    /* state remains STATE_ELEMENT, but accumulate to parent */
	  }
	} else if (firstToken->type == XMLTOKEN_START_OPEN) {
	  state = STATE_ELEMENT;
	  pushbackToken = firstToken;
	} else if (firstToken->type == XMLTOKEN_PCDATA) {
	  XMLNode *textNode = makeXMLNode(NODE_TEXT,"#text");
	  addChild(currentNode,textNode);
	  textNode->value = firstToken->bytes;
	} else {
	  syntaxError(p,"expecting element, text, or close, got %s",xmlTokenTypeNames[firstToken->type]);
          return NULL;
	}
	break;
      case STATE_DONE:
	if (firstToken->type == XMLTOKEN_EOF) {
	  return currentNode;
	} else {
	  syntaxError(p,"junk after document, type=%s",xmlTokenTypeNames[firstToken->type]);
#ifdef METTLE
      zowelog(NULL, LOG_COMP_XML, ZOWE_LOG_INFO, "junk linePos = 0x%x len=0x%x\n",p->linePos,p->lineLength);
	  dumpbuffer(p->lineBuffer,1024);
#endif
          return NULL;
	}
      }
    }
  }

int getSwitch(int argc, char**argv, char *switchName){
  int i;
  for (i=1; i<argc; i++){
    char *arg = argv[i];
    if (!strcmp(arg,switchName)){
      return 1;
    }
  }
  return 0;
}

/* DOM Utilities */

XMLNode *firstRealChild(XMLNode *node){
  if (node->childCount > 0){
    XMLNode *child = node->children[0];
    while (child != NULL){
      if (strcmp(child->name,"#text")){
	break;
      } else{
	child = child->nextSibling;
      }
    }
    return child;
  } else{
    return NULL;
  }
}

XMLNode *nextRealSibling(XMLNode *node){
  XMLNode *sib = node->nextSibling;
  while (sib != NULL && !strcmp(sib->name,"#text")){
    sib = sib->nextSibling;
  }
  return sib;
}

XMLNode *firstChildWithTag(XMLNode *node, char *tag){
  XMLNode *child = firstRealChild(node);
  while (child != NULL){
    if (!strcmp(child->name,tag)){
      /* this probably needs to go one deeper */
      return child;
    }
    child = nextRealSibling(child);
  }
  return NULL;
}

char *nodeText(XMLNode *node){
  if (node->childCount > 0){
    XMLNode *child = node->children[0];
    return child->value;
  } else{
    return NULL;
  }
}

char *textFromChildWithTag(XMLNode *node, char *tag){
  XMLNode *child = firstRealChild(node);
  while (child != NULL){
    if (!strcmp(child->name,tag)){
      if (child->childCount > 0){
	char *text = child->children[0]->value;
	return text;
      } else{
	return NULL;
      }
    }
    child = nextRealSibling(child);
  }
  return NULL;
}

int intFromChildWithTag(XMLNode *node, char *tag, int *value){
  char *text = textFromChildWithTag(node,tag);
  int len = strlen(text);
  int i;
  int x = 0;
  for (i=0; i<len; i++){
    char c = text[i];
    if (c < 0xF0 || c>0xF9){
      return 0;
    }
    x = (10 * x) + (text[i] - '0');
  }
  *value = x;
  return 1;
}

/* higher-order XML functions 
   mapChildren to list, make singly-linked list of structs from XML
   mapChildren apply function to all children
   */

typedef void *NodeFunction(XMLNode *n);

void *mapChildrenToList(XMLNode *node, NodeFunction *f, int chainPtrOffset){
  XMLNode *child = firstRealChild(node);
  void *listHead = NULL;
  void *listTail = NULL;
  while (child != NULL){
    void *thing = f(child);
    if (listHead){
      /* char *foo = ((char*)listTail)+4;*/
      char *tailAsCharPtr = (char*)listTail;
      *((int*)(tailAsCharPtr+chainPtrOffset)) = INT_FROM_POINTER(thing);
      listTail = thing;
    } else{
      listHead = thing;
      listTail = thing;
    }
  }
  return listHead;
}

char *getAttribute(XMLNode *node, char *attributeName){
  int i,attrCount = node->attributeCount;
  for (i=0; i<attrCount; i++){
    XMLNode *attribute = node->attributes[i];
    if (!strcmp(attribute->name,attributeName)){
      return attribute->value;
    }
  }
  return NULL;
}

int getBooleanAttribute(XMLNode *node, char *attributeName){
  char *value = getAttribute(node,attributeName);
  return ((value != NULL) && (!strcmp(value,"TRUE") ||
			      !strcmp(value,"true") ||
			      !strcmp(value,"True")));
}




/* DOM Style writing */


/* this routine will make a buffer using the given memory management interface */
#ifndef METTLE
int xmlWriteDocument(XMLNode node, char (*bufferAllocator)(int len), void (*bufferDeallocator)(void *buffer)){
  return 0;
}

char *xmlWriteDocumentToFile(XMLNode node, FILE *outputFilePointer){
  return 0;
}
#endif



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

