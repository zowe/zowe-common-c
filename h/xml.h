

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __XML__
#define __XML__   1

#include "alloc.h"
#include "utils.h"
#ifdef METTLE 
#include "unixfile.h"
#endif

/** \file
 *  \brief xml.h is an implementation of an efficient low-level XML writer and parser.
 *
 *  The XML handling here is designed for high-speed streaming output behavior
 *  and chunked input and output.   
 */


#define STANDARD_XML_DECLARATION "<?xml version=\"1.0\" encoding = \"UTF-8\"?>"

#define XML_STACK_LIMIT 100

typedef struct xmlPrinter_tag {
  int isHTTP;
  int isCustom;
  int s;  /* file or stream descriptor */
  unsigned char *handle;
  char *stack[XML_STACK_LIMIT];
  int stackSize;

  int prettyPrint;
  int lineBetweenElements;
  int wrapAttributes;

  int isOpen;
  int inAttrs;
  int inContent;
  int firstChild;
  int asciify;

  char *indentString;
  int indentStringLen;

  void (*customWriteFully)(struct xmlPrinter_tag *p, char *text, int len);
  void (*customWriteByte)(struct xmlPrinter_tag *p, char c);
  void *customObject;
} xmlPrinter;

xmlPrinter *makeHttpXmlPrinter(unsigned char *handle, char *xmlDeclaration);
xmlPrinter *makeCustomXmlPrinter(char *xmlDeclaration,
				 void (*writeFullyMethod)(struct xmlPrinter_tag *, char *, int), 
                                 void (*writeByteMethod)(struct xmlPrinter_tag *, char),
				 void *object);

xmlPrinter *xmlStart(xmlPrinter *p, char *elementName);
xmlPrinter *xmlEnd(xmlPrinter *p);
void xmlClose(xmlPrinter *p);

xmlPrinter *xmlPrintInt(xmlPrinter *p, int x);
xmlPrinter *xmlPrintBoolean(xmlPrinter *p, int x);
xmlPrinter *xmlPrint(xmlPrinter *p, char *string);
xmlPrinter *xmlPrintln(xmlPrinter *p, char *string);
xmlPrinter *xmlPrintPartial(xmlPrinter *p, char *string, int start, int end);
xmlPrinter *xmlAddTextElement(xmlPrinter *p, char *elementName, char *string, int len);
xmlPrinter *xmlAddCData(xmlPrinter *p, char *elementName, char *data);
xmlPrinter *xmlAddString(xmlPrinter *p, char *elementName, char *string);
xmlPrinter *xmlAddBooleanElement(xmlPrinter *p, char *elementName, int truthValue);
xmlPrinter *xmlAddIntElement(xmlPrinter *p, char *elementName, int x);


#define BAOS_MAX 8192

typedef struct ByteArrayOutputStream_tag{
  int pos;
  int extraSize;
  char bytes[BAOS_MAX];
} ByteArrayOutputStream;

#define XMLTOKEN_BROKEN      0
#define XMLTOKEN_START_OPEN  1
#define XMLTOKEN_END_OPEN    2
#define XMLTOKEN_CLOSE       3
#define XMLTOKEN_EMPTY_CLOSE 4
#define XMLTOKEN_PCDATA      5
#define XMLTOKEN_ID          6
#define XMLTOKEN_WHITESPACE  7
#define XMLTOKEN_ATTR_EQUAL  8
#define XMLTOKEN_ATTR_VALUE  9
#define XMLTOKEN_COMMENT    10
#define XMLTOKEN_EOF        (-1)

typedef struct XMLToken_tag{
  int type;
  int lineNumber;
  char *bytes;
} XMLToken;

#define XMLTOKEN_STATE_NORMAL 0
#define XMLTOKEN_STATE_TAG    1
#define XMLTOKEN_STATE_CHAR   2

#define XMLPARSER_CHAR_SEQUENCE_MAX 10

typedef struct XmlParser_tag{
  int tokenState;
  ShortLivedHeap *slh;
#ifndef METTLE
  FILE *in;
#else 
  void *in;
  char *dcb;
  UnixFile *file;
  int eof;
  char *lineBuffer;
  int readState;
  int linePos;
  int lineLength;
#endif
  ByteArrayOutputStream *tokenBytes;
  char charSequence[XMLPARSER_CHAR_SEQUENCE_MAX];
  int charSequencePos;
  int lineNumber;
  int pushback;
  int hasPushback;
  char *stringInput;
  int stringInputPos;
  int stringInputLen;
  char *tokenizationFailureReason;
} XmlParser;
  
#define NODE_ELEMENT   1
#define NODE_ATTRIBUTE 2
#define NODE_TEXT      3

typedef struct XMLNode_tag {
  int type;
  char *name;
  char *value; /* for text and attributes */
  struct XMLNode_tag *parent;
  struct XMLNode_tag *prevSibling;
  struct XMLNode_tag *nextSibling;
  int childCount; /* = 0; */
  int childrenLength;
  struct XMLNode_tag **children;
  int attributeCount; /*  = 0;*/
  int attributesLength;
  struct XMLNode_tag **attributes;
} XMLNode;

#ifndef METTLE
XmlParser *makeXmlParser(FILE *in);
#else
XmlParser *makeXmlParser(char *dcb);
void setLineBufferSize(XmlParser *p, int size);
#endif


XmlParser *makeXmlStringParser(char *s, int len);
XMLNode *parseXMLNode(XmlParser *parser);
int setXMLParseTrace(int trace);
int setXMLTrace(int trace);
XMLToken *getTokenNoWS(XmlParser *parser);

/* DOM-like function that skips junk text nodes */
XMLNode *firstRealChild(XMLNode *node);

/* DOM-like function that skips junk text nodes */
XMLNode *nextRealSibling(XMLNode *node);

/* search children of node for child node with given tag */
XMLNode *firstChildWithTag(XMLNode *node, char *tag);

/* Text from node.  Must copy because text storage is owned by XmlParser */
char *nodeText(XMLNode *node);

/* Convenience function for reaching into a specific child a getting the text in it */
char *textFromChildWithTag(XMLNode *node, char *tag);

/* access attribute text from nodes by name.  Text must be copied after XmlParser freed */
char *getAttribute(XMLNode *node, char *attributeName);

/* Convenience Function for extracting integer from text of child node with given tag */
int intFromChildWithTag(XMLNode *node, char *tag, int *value);

#endif



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

