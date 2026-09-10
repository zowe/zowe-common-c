/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/


#include <yaml.h> 
   /* #include <yaml_private.h> */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "json.h"
#include "jsonschema.h"
#include "yaml2json.h"

#ifdef __ZOWE_OS_ZOS

int __atoe_l(char *bufferptr, int leng);
int __etoa_l(char *bufferptr, int leng);

#endif


#if defined(__ZOWE_OS_ZOS)
#  define SOURCE_CODE_CHARSET CCSID_IBM1047
#else
#  define SOURCE_CODE_CHARSET CCSID_UTF_8
#endif


static int convertToNative(char *buf, size_t size) {
#ifdef __ZOWE_OS_ZOS
  return __atoe_l(buf, size);
#endif
  return 0;
}

static int convertFromNative(char *buf, size_t size) {
#ifdef __ZOWE_OS_ZOS
  return __etoa_l(buf, size);
#endif
  return 0;
}

/* The key is still in the YAML input encoding; convert a copy for the message */
static void warnKeyTooLong(const yaml_char_t *value) {
  char shown[MAX_JSON_KEY + 1];
  memcpy(shown, value, MAX_JSON_KEY);
  shown[MAX_JSON_KEY] = 0;
  convertToNative(shown, MAX_JSON_KEY);
  printf("*** WARNING *** key too long '%s...'\n", shown);
}

static char *tokenTypeName(yaml_token_type_t type){
  switch (type){
  case YAML_NO_TOKEN: return "NO_TOKEN";
  case YAML_STREAM_START_TOKEN: return "STREAM_START";
  case YAML_STREAM_END_TOKEN: return "STREAM_END";
  case YAML_VERSION_DIRECTIVE_TOKEN: return "VERSION_DIRECTIVE";
  case YAML_TAG_DIRECTIVE_TOKEN: return "TAG_DIRECTIVE";
  case YAML_DOCUMENT_START_TOKEN: return "DOCUMENT_START";
  case YAML_DOCUMENT_END_TOKEN: return "DOCUMENT_END";
  case YAML_BLOCK_SEQUENCE_START_TOKEN: return "BLOCK_SEQUENCE_START";
  case YAML_BLOCK_MAPPING_START_TOKEN: return "BLOCK_MAPPING_START";
  case YAML_BLOCK_END_TOKEN: return "BLOCK_END";
  case YAML_FLOW_SEQUENCE_START_TOKEN: return "FLOW_SEQUENCE_START";
  case YAML_FLOW_SEQUENCE_END_TOKEN: return "FLOW_SEQUENCE_END";
  case YAML_FLOW_MAPPING_START_TOKEN: return "FLOW_MAPPING_START";
  case YAML_FLOW_MAPPING_END_TOKEN: return "FLOW_MAPPING_END";
  case YAML_BLOCK_ENTRY_TOKEN: return "BLOCK_ENTRY";
  case YAML_FLOW_ENTRY_TOKEN: return "FLOW_ENTRY";
  case YAML_KEY_TOKEN: return "KEY";
  case YAML_VALUE_TOKEN: return "VALUE";
  case YAML_ALIAS_TOKEN: return "ALIAS";
  case YAML_ANCHOR_TOKEN: return "ANCHOR";
  case YAML_TAG_TOKEN: return "TAG";
  case YAML_SCALAR_TOKEN: return "SCALAR";
  default:
    {
      printf("unknown token seen with type = %d\n",(int)type);
      return "UNKNOWN_TOKEN";
    }
  }

}

static char *getScalarStyleName(yaml_scalar_style_t style){
  switch (style){
  case YAML_ANY_SCALAR_STYLE: return "any";
  case YAML_PLAIN_SCALAR_STYLE: return "plain";
  case YAML_SINGLE_QUOTED_SCALAR_STYLE: return "single";
  case YAML_DOUBLE_QUOTED_SCALAR_STYLE: return "double";
  case YAML_LITERAL_SCALAR_STYLE: return "literal";
  case YAML_FOLDED_SCALAR_STYLE: return "folded";
  default: return "UNKNOWN_STYLE";
  }
}

static int yamlReadHandler(void *data, unsigned char *buffer, size_t size, size_t *sizeRead) {
  FILE *fp = data;
  int rc = 1;
  size_t bytesRead = fread(buffer, 1, size, fp);
  if (bytesRead > 0) {
    if (convertFromNative((char *)buffer, bytesRead) == -1) {
    fprintf (stderr, "failed to convert yaml input - %s\n", strerror(errno));
      rc = 0;
    }
  }
  if (ferror(fp)) {
    fprintf (stderr, "failed to read yaml input - %s\n", strerror(errno));
    rc = 0;
  }
  *sizeRead = bytesRead;
  return rc;
}

static void decodeParserError(yaml_parser_t *parser, char *errorBuf, size_t errorBufSize, const char *filename) {
  /*
   * Convert diagnostics (problem and context) from ascii to ebcdic so that the error buffer that is returned from this function
   * is in native codepage.
   */
  size_t problemLen = strlen(parser->problem);
  char *problemNative = safeMalloc(problemLen + 1, "parser problem");
  if (problemNative) {
    memset(problemNative, 0, problemLen + 1);
    strcpy(problemNative, parser->problem);
    convertToNative(problemNative, problemLen);
  }
  size_t contextLen = 0;
  char *contextNative = NULL;
  if (parser->context) {
    contextLen = strlen(parser->context);
    contextNative = safeMalloc(contextLen + 1, "parser context");
    if (contextNative) {
      memset(contextNative, 0, contextLen + 1);
      strcpy(contextNative, parser->context);
      convertToNative(contextNative, contextLen);
    }
  }
  switch (parser->error) {
    case YAML_MEMORY_ERROR:
      snprintf(errorBuf, errorBufSize, "Couldn't allocate enough memory to process file '%s'.", filename);
      break;
    case YAML_READER_ERROR: {
      if (parser->problem_value != -1) {
        snprintf(errorBuf,
                 errorBufSize,
                 "Couldn't read file '%s': %s, #%x at %zu.",
                 filename, problemNative, parser->problem_value, parser->problem_offset);
      } else {
        snprintf(errorBuf, errorBufSize, "Couldn't read file %s: %s at %zu.", filename, problemNative, parser->problem_offset);
      }
      break;
    }
    case YAML_SCANNER_ERROR:
      if (parser->context) {
        snprintf(errorBuf, errorBufSize,
                "Couldn't scan file '%s': %s at line %d, column %d, "
                "%s at line %d, column %d.",
                filename, contextNative,
                (int)parser->context_mark.line+1, (int)parser->context_mark.column+1,
                problemNative, (int)parser->problem_mark.line+1,
                (int)parser->problem_mark.column+1);
      } else {
        snprintf(errorBuf, errorBufSize,
                "Couldn't scan file '%s': %s at line %d, column %d.",
                filename, problemNative, (int)parser->problem_mark.line+1,
                (int)parser->problem_mark.column+1);
      }
      break;
      case YAML_PARSER_ERROR:
        if (parser->context) {
          snprintf(errorBuf, errorBufSize,
                  "Couldn't parse file '%s': %s at line %d, column %d, "
                  "%s at line %d, column %d.",
                  filename, contextNative,
                  (int)parser->context_mark.line+1, (int)parser->context_mark.column+1,
                  problemNative, (int)parser->problem_mark.line+1,
                  (int)parser->problem_mark.column+1);
        } else {
          snprintf(errorBuf, errorBufSize,
                   "Couldn't parse file '%s': %s at line %d, column %d.",
                   filename, problemNative, (int)parser->problem_mark.line+1,
                   (int)parser->problem_mark.column+1);
        }
        break;
      default:
        snprintf(errorBuf, errorBufSize, "Couldn't process file '%s' because of an unknown error type '%d'.", filename, parser->error);
  }
  /*
   * We've already copied the information into the errorBuffer so we can free these now.
   */
  safeFree(problemNative, problemLen);
  if (contextNative) {
    safeFree(contextNative, contextLen);
  }
}

yaml_document_t *readYAML2(const char *filename, char *errorBuf, size_t errorBufSize, bool *wasMissing){
  FILE *file = NULL;
  yaml_document_t *document = NULL;
  yaml_parser_t parser = {0};
  bool done = false;

  do {
    if (!(document = (yaml_document_t*)safeMalloc(sizeof(yaml_document_t), "YAML Doc"))) {
      snprintf(errorBuf, errorBufSize, "failed to alloc memory for YAML doc");
      break;
    }
    memset(document, 0, sizeof(yaml_document_t));
    if (!(file = fopen(filename, "r"))) {
      snprintf(errorBuf, errorBufSize, "failed to read '%s' - %s", filename, strerror(errno));
      *wasMissing = true;
      break;
    }
    if (!yaml_parser_initialize(&parser)) {
      snprintf(errorBuf, errorBufSize, "failed to initialize YAML parser");
      break;
    }
    yaml_parser_set_input(&parser, yamlReadHandler, file);
    if (!yaml_parser_load(&parser, document)) {
      decodeParserError(&parser, errorBuf, errorBufSize, filename);
      break;
    }
    if (!yaml_document_get_root_node(document)){
      snprintf(errorBuf, errorBufSize, "failed to get root node in YAML '%s'", filename);
      break;
    }
    done = true;
  } while (0);

  if (!done && document) {
    yaml_document_delete(document);
    safeFree((char*)document, sizeof(yaml_document_t));
    document = NULL;
  }
  if (file) {
    fclose(file);
  }
  yaml_parser_delete(&parser);
  return document;
}

yaml_document_t *readYAML(const char *filename, char *errorBuf, size_t errorBufSize) {
  bool wasMissing = false;
  return readYAML2(filename,errorBuf,errorBufSize,&wasMissing);
}

static void indent(int x){
  for (int i=0; i<x; i++){
    printf(" ");
  }
}

static char *getSequenceStyleName(yaml_sequence_style_t style){
  switch (style){
  case YAML_ANY_SEQUENCE_STYLE: return "any";
  case YAML_BLOCK_SEQUENCE_STYLE: return "block";
  case YAML_FLOW_SEQUENCE_STYLE: return "flow";
  default: return "unknown";
  }
}

#define SCALAR_SIZE_LIMIT 1024

static void printYamlScalar(const yaml_node_t *node, bool eol) {
  size_t dataLen = node->data.scalar.length;
  int printLength = dataLen > SCALAR_SIZE_LIMIT ? 40 : (int)dataLen;
  char val[printLength + 1];
  snprintf(val, printLength + 1, "%.*s", printLength, node->data.scalar.value);
  convertToNative(val, printLength);
  printf("%s%s", val, eol ? "\n" : "");
}


static void pprintYAML1(yaml_document_t *doc, yaml_node_t *node, int depth){
  switch (node->type){
  case YAML_NO_NODE:
    {
      indent(depth);
      printf("NoNode\n");
    }
    break;
  case YAML_SCALAR_NODE:
    {
      indent(depth);
      size_t dataLen = node->data.scalar.length;
      printf("Scalar: (len=%d)", (int)node->data.scalar.length);
      printYamlScalar(node, true);
    }
    break;
  case YAML_SEQUENCE_NODE:
    {
      yaml_node_item_t *item;
      indent(depth);printf("SequenceStart (%s)\n",getSequenceStyleName(node->data.sequence.style));
      for (item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
        yaml_node_t *nextNode = yaml_document_get_node(doc, *item);
        if (nextNode){
          pprintYAML1(doc,nextNode,depth+2);
        } else {
          indent(depth+4);
          printf("dead end item\n");
        }
      }
      indent(depth);printf("SequenceEnd\n");
    }
    break;
  case YAML_MAPPING_NODE:
   {
    yaml_node_pair_t *pair;
    indent(depth);printf("MapStart\n");
    for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
      yaml_node_t *keyNode = yaml_document_get_node(doc, pair->key);
      /* printf("keyNode 0x%p\n",keyNode); */
      if (keyNode) {
        if (keyNode->type != YAML_SCALAR_NODE){
          printf("*** UNEXPECTED NON SCALAR KEY ***\n");
        } else {
          indent(depth+2);
          size_t dataLen = keyNode->data.scalar.length;
          int printLength = dataLen > SCALAR_SIZE_LIMIT ? 40 : (int)dataLen;
          /* printf("printLength=%d dataLen=%lld\n",printLength,dataLen); */
          printYamlScalar(keyNode, false);
          printf(":\n");
        }
      } else {
        printf("dead end key\n");
      }
      yaml_node_t *valueNode = yaml_document_get_node(doc, pair->value);
      if (valueNode){
        pprintYAML1(doc,valueNode,depth+4);
      } else{
        printf("dead end value\n");
      }
    }
    indent(depth);printf("MapEnd\n");
  }
    break;
  default:
    printf("unexpected yaml node type %d\n",node->type);
    break;
  }
}
  
void pprintYAML(yaml_document_t *document){
  pprintYAML1(document,yaml_document_get_root_node(document),0);
}

/* this needs to be smarter some day
   Refer to Yaml spec http://yaml.org/spec/1.2-old/spec.html#id2805071
 */

static bool isSyntacticallyInteger(yaml_char_t *data, int length){
  if (length == 0){
    /* printf("empty string not integer\n"); */
    return false;
  }
  for (int i=0; i<length; i++){
    yaml_char_t c = data[i];
    if (c >= '0' && c <= '9'){
      
    } else {
      /* printf("%s is NOT an integer\n",data); */
      return false;
    }     
  }
  /* printf("%s is an integer\n",data); */
  return true;
}

static bool isSyntacticallyBool(yaml_char_t *data, int length){
  if ((length == 4) &&
      (!memcmp(data,"true",4) ||
       !memcmp(data,"True",4) ||
       !memcmp(data,"TRUE",4))){
    return true;
  } else if ((length == 5) &&
             (!memcmp(data,"false",5) ||
              !memcmp(data,"False",5) ||
              !memcmp(data,"FALSE",5))){
    return true;
  } else {
    return false;
  }
}

static bool isSyntacticallyNull(yaml_char_t *data, int length){
  if (length == 0 ) {
    return true;
  } else if ((length == 1) &&
      !memcmp(data,"~",1)){
    return true;
  } else if ((length == 4) &&
             (!memcmp(data,"null",4) ||
              !memcmp(data,"Null",4) ||
              !memcmp(data,"NULL",4))){
    return true;
  } else {
    return false;
  }
}

static bool isSyntacticallyTemplate(yaml_char_t *data, int length){
  if (strstr((char*)data,"${{")){
    return true;
  } else {
    return false;
  }
}

static int64_t readInt(yaml_char_t *data, int length, bool *valid){
  int64_t val64 = 0;
  bool allDecimal = true;
  
  for (int i=0; i<length; i++){
    yaml_char_t c = data[i];
    if (c >= '0' && c <= '9'){
      val64 = (10 * val64) + (c-'0');
    } else {
      allDecimal = false;
    }
  }

  *valid = allDecimal;
  return val64;
}

static bool readBool(yaml_char_t *data, int length, bool *valid){
  if ((length == 4) &&
      (!memcmp(data,"true",4) ||
       !memcmp(data,"True",4) ||
       !memcmp(data,"TRUE",4))){
    *valid = true;
    return true;
  } else if ((length == 5) &&
             (!memcmp(data,"false",5) ||
              !memcmp(data,"False",5) ||
              !memcmp(data,"FALSE",5))){
    *valid = true;
    return false;
  } else {
    *valid = false;
    return false;
  }
}


#define JSON_FAIL_NOT_HANDLED  100
#define JSON_FAIL_BAD_INTEGER  104
#define JSON_FAIL_BAD_BOOL     108
#define JSON_FAIL_BAD_NULL     112
#define JSON_FAIL_BAD_TEMPLATE 116

static char *extractString(JsonBuilder *b, char *s, char *e){
  int len = e-s;
  char *copy = SLHAlloc(b->parser.slh,len+1);
  memcpy(copy,s,len);
  copy[len] = 0;
  return copy;
}

static void addPlusIfNecessary(jsonPrinter *p, int sourceCCSID, bool *firstPtr){
  bool first = *firstPtr;
  if (!first){
    jsonWriteParseably(p," + ",3,false,false,sourceCCSID);
  }
  *firstPtr = false;
}

static int buildTemplateJSON(JsonBuilder *b, Json *parent, char *parentKey,
                             char *nativeValue, int valueLength){
  char *tail = nativeValue;
  JsonBuffer *buffer = makeJsonBuffer();
  int sourceCCSID = CCSID_UTF_8;
  /* printf ("buildTemplateJSON nativeValue '%.*s' sourceCodeCharset %d\n", valueLength, nativeValue, sourceCCSID);*/
  jsonPrinter *p = makeBufferJsonPrinter(sourceCCSID,buffer);
  int status = 0;
  bool first = true;
  while (true){
    char *nextExpr = strstr(tail,"${{");
    
    if (nextExpr){
      if (nextExpr > tail){
        char *frag = extractString(b,tail,nextExpr);
        /* printf("frag = '%s'\n",frag); */
        addPlusIfNecessary(p,sourceCCSID,&first);
        jsonWriteParseably(p,frag,strlen(frag),true,false,sourceCCSID);
      }
      char *end = strstr(nextExpr,"}}");
      if (end){
        char *exprText = extractString(b,nextExpr+3,end);
        tail = end+2;
        addPlusIfNecessary(p,sourceCCSID,&first);
        jsonWriteParseably(p,exprText,strlen(exprText),false,false,sourceCCSID);
      } else {
        status = JSON_FAIL_BAD_TEMPLATE;
        break;
      }
    } else {
      char *lastFrag = extractString(b,tail,nativeValue+valueLength);
      /* printf("lastFrag = '%s'\n",lastFrag); */
      if (strlen(lastFrag) > 0){
        addPlusIfNecessary(p,sourceCCSID,&first);
        jsonWriteParseably(p,lastFrag,strlen(lastFrag),true,false,sourceCCSID);
      }
      break;
    }
  }
  if (status == 0){
    Json *object = jsonBuildObject(b,parent,parentKey,&status);
    if (!status){
      jsonBuildString(b,object,ZOWE_INTERNAL_TYPE,ZOWE_UNEVALUATED,strlen(ZOWE_UNEVALUATED),&status);
      if (!status){
        jsonBufferTerminateString(buffer);
        char *sourceCode = jsonBufferCopy(buffer);
        if (b->traceLevel >= 1){
          printf("embedded source code is (tl=%d): %s\n",b->traceLevel,sourceCode);
          dumpbuffer(sourceCode,strlen(sourceCode));
        }
        jsonBuildString(b,object,"source",sourceCode,strlen(sourceCode),&status);
      }
    }
  }
  freeJsonPrinter(p);
  freeJsonBuffer(buffer);
  return status;
}





static Json *yaml2JSON1(JsonBuilder *b, Json *parent, char *parentKey,
                        yaml_document_t *doc, yaml_node_t *node, int depth){
  int buildStatus = 0;
  switch (node->type){
  case YAML_NO_NODE:
    {
      printf("*** WARNING *** NoNode\n");
      return NULL;
    }
  case YAML_SCALAR_NODE:
    {
      size_t dataLen = node->data.scalar.length;
      int valueLength = (int)dataLen;
      if (dataLen > MAX_JSON_STRING){
        printf("*** WARNING *** oversize JSON string!\n");
        valueLength = MAX_JSON_STRING;
      }
      // Negative numbers, hexadecimal
      yaml_scalar_style_t style = node->data.scalar.style;
      switch (style){
      case YAML_ANY_SCALAR_STYLE:
      case YAML_PLAIN_SCALAR_STYLE:
      case YAML_SINGLE_QUOTED_SCALAR_STYLE:
      case YAML_DOUBLE_QUOTED_SCALAR_STYLE:
      case YAML_LITERAL_SCALAR_STYLE:
      case YAML_FOLDED_SCALAR_STYLE:
      {
        char nativeValue[valueLength+1];
        char *tag = (char*)node->tag;
        int tagLen = strlen(tag);
        char nativeTag[tagLen + 1];
        snprintf(nativeValue, valueLength+1, "%.*s", valueLength, (const char *)node->data.scalar.value);
        snprintf(nativeTag, tagLen + 1, "%.*s", tagLen, tag);
        convertToNative(nativeValue, valueLength);
        convertToNative(nativeTag, tagLen);
        if (b->traceLevel >= 2){
          printf("tag = %s scalarStyle=%s\n",nativeTag,getScalarStyleName(node->data.scalar.style));
        }
        Json *scalar = NULL;
        // HERE, make test with float, int, bool, null, ddate
        if (!strcmp(nativeTag,YAML_NULL_TAG) ||
            (!strcmp(nativeTag,YAML_STR_TAG) &&
             (style == YAML_PLAIN_SCALAR_STYLE) &&
             isSyntacticallyNull((yaml_char_t*)nativeValue,valueLength))){
          scalar = jsonBuildNull(b,parent,parentKey,&buildStatus);
        } else if (!strcmp(nativeTag,YAML_INT_TAG) ||
                   (!strcmp(nativeTag,YAML_STR_TAG) &&
                    (style == YAML_PLAIN_SCALAR_STYLE) &&
                    isSyntacticallyInteger((yaml_char_t*)nativeValue,valueLength))){
          bool valid;
          int64_t x = readInt((yaml_char_t*)nativeValue,valueLength,&valid);
          if (valid){
            scalar = jsonBuildInt64(b,parent,parentKey,x,&buildStatus);
          } else {
            buildStatus = JSON_FAIL_BAD_INTEGER;
          }
        } else if (!strcmp(nativeTag,YAML_BOOL_TAG) ||
                   (!strcmp(nativeTag,YAML_STR_TAG) &&
                    (style == YAML_PLAIN_SCALAR_STYLE) &&
                    isSyntacticallyBool((yaml_char_t*)nativeValue,valueLength))){
          bool valid;
          bool x = readBool((yaml_char_t*)nativeValue,valueLength,&valid);
          if (valid){
            scalar = jsonBuildBool(b,parent,parentKey,x,&buildStatus);
          } else {
            buildStatus = JSON_FAIL_BAD_BOOL;
          }
        } else if (!strcmp(nativeTag,YAML_STR_TAG)){
          /*
            Must be plain
            ${{ <javaScript }} (otherText  ${{ }})*
            how to be integer??
          */
          if ((style == YAML_PLAIN_SCALAR_STYLE) &&
              isSyntacticallyTemplate((yaml_char_t*)nativeValue,valueLength)){
            buildStatus = buildTemplateJSON(b,parent,parentKey,nativeValue,valueLength);
          } else if (style == YAML_SINGLE_QUOTED_SCALAR_STYLE &&
                     isSyntacticallyTemplate((yaml_char_t*)nativeValue,valueLength)){
            buildStatus = JSON_FAIL_NOT_HANDLED;
          } else if (style == YAML_DOUBLE_QUOTED_SCALAR_STYLE &&
                     isSyntacticallyTemplate((yaml_char_t*)nativeValue,valueLength)){
            buildStatus = buildTemplateJSON(b,parent,parentKey,nativeValue,valueLength);
          } else {
            scalar = jsonBuildString(b,parent,parentKey,nativeValue,valueLength,&buildStatus);
          }
        } else if (!strcmp(nativeTag,YAML_FLOAT_TAG)){
          printf("*** Warning don't know how to handle float yet\n");
          buildStatus = JSON_FAIL_NOT_HANDLED;
        } else if (!strcmp(nativeTag,YAML_TIMESTAMP_TAG)){
          printf("*** Warning don't know how to handle timestamp yet\n");
          buildStatus = JSON_FAIL_NOT_HANDLED;
        }
        if (buildStatus){
          printf("*** WARNING *** Failed to add property/scalar err=%d\n",buildStatus);
        }
        return scalar;
      }
      default:
        printf("*** WARNING *** - unknown scalar style %d",node->data.scalar.style);
        return NULL;
      }
    }
  case YAML_SEQUENCE_NODE:
    {
      yaml_node_item_t *item;
      Json *jsonArray = jsonBuildArray(b,parent,parentKey,&buildStatus);
      if (jsonArray){
        for (item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
          yaml_node_t *nextNode = yaml_document_get_node(doc, *item);
          if (nextNode){
            yaml2JSON1(b,jsonArray,NULL,doc,nextNode,depth+2);
          } else {
            printf("*** WARNING *** dead end item\n");
          }
        }
        return jsonArray;
      } else {
        printf("*** WARNING *** Failed to add json array err=%d\n",buildStatus);
        return NULL;
      }
    }
  case YAML_MAPPING_NODE:
    {
      yaml_node_pair_t *pair;
      Json *jsonObject = jsonBuildObject(b,parent,parentKey,&buildStatus);
      if (jsonObject){
        for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
          yaml_node_t *keyNode = yaml_document_get_node(doc, pair->key);
          /* printf("keyNode 0x%p\n",keyNode); */
          char *key = NULL;
          int keyLength;
          if (keyNode) {
            if (keyNode->type != YAML_SCALAR_NODE){
              printf("*** WARNING *** Non Scalar key\n");
            } else {
              size_t dataLen = keyNode->data.scalar.length;
              keyLength = (int)dataLen;
              if (keyLength > MAX_JSON_KEY){
                warnKeyTooLong(keyNode->data.scalar.value);
              } else {
                key = (char*)keyNode->data.scalar.value;
              }
            }
          } else {
            printf("*** WARNING *** dead end key\n");
          }
          if (key){
            char *keyNative = jsonBuildKey(b, key, keyLength);
            convertToNative(keyNative, keyLength);
            yaml_node_t *valueNode = yaml_document_get_node(doc, pair->value);
            if (valueNode){
              /* Duplicate mapping key handling (zowe/zowe-common-c#581). Left
                 unchecked, a YAML mapping with a repeated key (e.g. two top-level
                 `zowe:` blocks) produced a JSON object with duplicate properties.
                 Consumers then diverged: the C accessors return the first match
                 while the QuickJS template evaluator (used by the launcher) sees
                 the last -- so `configmgr` and `launcher` evaluated the same YAML
                 differently. Emit a single-valued key so every consumer agrees.
                 PROTOTYPE SEMANTICS = FIRST-WINS (keep first, ignore later dupes);
                 warn so a likely config mistake is never silent. Alternatives for
                 the squad: last-wins (jsonObjectRemoveNode then add), deep-merge
                 (jsonMerge), or hard error. */
              if (jsonObjectHasKey(jsonAsObject(jsonObject), keyNative)){
                fprintf(stderr,
                        "*** WARNING *** duplicate key '%s' in YAML mapping; keeping"
                        " the first value and ignoring later ones (zowe-common-c#581)\n",
                        keyNative);
              } else {
                yaml2JSON1(b,jsonObject,keyNative,doc,valueNode,depth+2);
              }
            } else{
              printf("*** WARNING *** dead end value\n");
            }
          }
        }
        return jsonObject;
      } else {
        printf("*** WARNING *** Failed to add json object err=%d\n",buildStatus);
        return NULL;
      }
    }
    break;
  default:
    printf("*** WARNING *** unexpected yaml node type %d\n",node->type);
    return NULL;
  }
}

typedef struct ByteOutputStream_tag{
  int   size;
  int   capacity;
  int   chunkSize;
  char *data;
} ByteOutputStream;

ByteOutputStream *makeByteOutputStream(int chunkSize){
  ByteOutputStream *bos = (ByteOutputStream*)safeMalloc(sizeof(ByteOutputStream),"ByteOutputStream");
  bos->size = 0;
  bos->capacity = chunkSize;
  bos->chunkSize = chunkSize;
  bos->data = safeMalloc(chunkSize,"ByteOutputStream Chunk");
  return bos;
}

int bosWrite(ByteOutputStream *bos, char *data, int dataSize){
  if (bos->size+dataSize > bos->capacity){
    int extendSize = (bos->chunkSize > dataSize) ? bos->chunkSize : dataSize;
#ifdef NDEBUG
    printf("bos extend currSize=0x%x dataSize=0x%x chunk=0x%x extend=0x%x\n",
           bos->size,dataSize,bos->chunkSize,extendSize);
#endif
    int newCapacity = bos->capacity + extendSize;
    char *newData = safeMalloc(newCapacity,"BOS extend");
    memcpy(newData,bos->data,bos->size);
    safeFree(bos->data,bos->capacity);
    bos->data = newData;
    bos->capacity = newCapacity;
  }
  memcpy(bos->data+bos->size,data,dataSize);
  bos->size += dataSize;
  return bos->size;
}

int bosAppendString(ByteOutputStream *bos, char *s){
  return bosWrite(bos,s,strlen(s));
}

int bosAppendChar(ByteOutputStream *bos, char c){
  return bosWrite(bos,&c,1);
}

char *bosNullTerminateAndUse(ByteOutputStream *bos){
  char c = 0;
  bosWrite(bos,&c,1);
  return bos->data;
}

char *bosUse(ByteOutputStream *bos){
  return bos->data;
}

void bosReset(ByteOutputStream *bos){
  bos->size = 0;
}

void bosFree(ByteOutputStream *bos, bool freeBuffer){
  if (freeBuffer){
    safeFree(bos->data,bos->capacity);
  }
  safeFree((char*)bos,sizeof(ByteOutputStream));
}

/* Friday, expand it */

Json *yaml2JSON(yaml_document_t *document, ShortLivedHeap *slh){
  JsonBuilder *builder = makeJsonBuilder(slh);
  Json *json = yaml2JSON1(builder,NULL,NULL,document,yaml_document_get_root_node(document),0);
  freeJsonBuilder(builder,false);
  return json;
}


static void yaml2JS1(ByteOutputStream *bos,
                     yaml_document_t *doc, yaml_node_t *node, int depth,
                     int traceLevel){
  int buildStatus = 0;
  switch (node->type){
  case YAML_NO_NODE:
    {
      printf("*** WARNING *** NoNode\n");
      return;
    }
  case YAML_SCALAR_NODE:
    {
      size_t dataLen = node->data.scalar.length;
      int valueLength = (int)dataLen;
      if (dataLen > MAX_JSON_STRING){
        printf("*** WARNING *** oversize JSON string!\n");
        valueLength = MAX_JSON_STRING;
      }
      // Negative numbers, hexadecimal
      yaml_scalar_style_t style = node->data.scalar.style;
      switch (style){
      case YAML_ANY_SCALAR_STYLE:
      case YAML_PLAIN_SCALAR_STYLE:
      case YAML_SINGLE_QUOTED_SCALAR_STYLE:
      case YAML_DOUBLE_QUOTED_SCALAR_STYLE:
      case YAML_LITERAL_SCALAR_STYLE:
      case YAML_FOLDED_SCALAR_STYLE:
      {
        char nativeValue[valueLength+1];
        char *tag = (char*)node->tag;
        int tagLen = strlen(tag);
        char nativeTag[tagLen + 1];
        snprintf(nativeValue, valueLength+1, "%.*s", valueLength, (const char *)node->data.scalar.value);
        snprintf(nativeTag, tagLen + 1, "%.*s", tagLen, tag);
        convertToNative(nativeValue, valueLength);
        convertToNative(nativeTag, tagLen);
        if (traceLevel >= 2){
          printf("tag = %s scalarStyle=%s\n",nativeTag,getScalarStyleName(node->data.scalar.style));
        }
        Json *scalar = NULL;
        /* HERE, make test with float, int, bool, null, date */
        if (!strcmp(nativeTag,YAML_NULL_TAG)){
        } else if (!strcmp(nativeTag,YAML_NULL_TAG)){
          /* Json *scalar = jsonBuildNull(b,parent,parentKey,&buildStatus); */
        } else if (!strcmp(nativeTag,YAML_BOOL_TAG)){
          /* Json *scalar = jsonBuildBool(b,parent,parentKey,"FOO",3,&buildStatus); */
        } else if (!strcmp(nativeTag,YAML_INT_TAG) ||
                   (!strcmp(nativeTag,YAML_STR_TAG) &&
                    (style == YAML_PLAIN_SCALAR_STYLE) &&
                    isSyntacticallyInteger((yaml_char_t*)nativeValue,valueLength))){
          bool valid;
          int64_t x = readInt((yaml_char_t*)nativeValue,valueLength,&valid);
          if (valid){
            /* this is not yet done */
          } else {
            buildStatus = JSON_FAIL_BAD_INTEGER;
          }
          /* Json *scalar = jsonBuildInt(b,parent,parentKey,"FOO",3,&buildStatus); */
        } else if (!strcmp(nativeTag,YAML_STR_TAG)){
          printf("%*.*s\n",valueLength,valueLength,nativeValue);
        } else if (!strcmp(nativeTag,YAML_FLOAT_TAG)){
          printf("*** Warning don't know how to handle float yet\n");
          buildStatus = JSON_FAIL_NOT_HANDLED;
        } else if (!strcmp(nativeTag,YAML_TIMESTAMP_TAG)){
          printf("*** Warning don't know how to handle timestamp yet\n");
          buildStatus = JSON_FAIL_NOT_HANDLED;
        }
        if (buildStatus){
          printf("*** WARNING *** Failed to add property/scalar err=%d\n",buildStatus);
        }
        return;
      }
      default:
        printf("*** WARNING *** - unknown scalar style %d",node->data.scalar.style);
        return;
      }
    }
  case YAML_SEQUENCE_NODE:
    {
      yaml_node_item_t *item;
      printf("[\n");
      for (item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
        yaml_node_t *nextNode = yaml_document_get_node(doc, *item);
        if (nextNode){
          yaml2JS1(bos,doc,nextNode,depth+2,traceLevel);
        } else {
          printf("*** WARNING *** dead end item\n");
        }
      }
      printf("]\n");
    }
  case YAML_MAPPING_NODE:
    {
      yaml_node_pair_t *pair;
      /* Json *jsonObject = jsonBuildObject(b,parent,parentKey,&buildStatus); */
      for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
        yaml_node_t *keyNode = yaml_document_get_node(doc, pair->key);
        /* printf("keyNode 0x%p\n",keyNode); */
        char *key = NULL;
        int keyLength;
        if (keyNode) {
          if (keyNode->type != YAML_SCALAR_NODE){
            printf("*** WARNING *** Non Scalar key\n");
          } else {
            size_t dataLen = keyNode->data.scalar.length;
            keyLength = (int)dataLen;
            if (keyLength > MAX_JSON_KEY){
              warnKeyTooLong(keyNode->data.scalar.value);
            } else {
                key = (char*)keyNode->data.scalar.value;
            }
          }
        } else {
          printf("*** WARNING *** dead end key\n");
        }
        if (key){
          char keyNative[keyLength+1];
          memcpy(keyNative,key,keyLength);
          keyNative[keyLength] = 0;
          convertToNative(keyNative, keyLength);
          yaml_node_t *valueNode = yaml_document_get_node(doc, pair->value);
          if (valueNode){
            printf("%s\n",keyNative);
            yaml2JS1(bos,doc,valueNode,depth+2,traceLevel);
          } else{
            printf("*** WARNING *** dead end value\n");
          }
        }
      }
      return;
    }
    break;
  default:
    printf("*** WARNING *** unexpected yaml node type %d\n",node->type);
    return;
  }
}

/* this is not fully done, and might not be needed */
static void yaml2JS(yaml_document_t *document, ShortLivedHeap *slh){
  ByteOutputStream *bos = makeByteOutputStream(0x1000);
  yaml2JS1(bos,document,yaml_document_get_root_node(document),0,1);
}

static char YAML_NULL_TAG_ASCII[23] ={0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x6e, 0x75, 0x6c, 0x6c,  0x00};

static char YAML_BOOL_TAG_ASCII[23] ={0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x6f, 0x6f, 0x6c,  0x00};

static char YAML_STR_TAG_ASCII[22] ={0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x73, 0x74, 0x72,  0x00};

static char YAML_INT_TAG_ASCII[22] ={0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x69, 0x6e, 0x74,  0x00};

static char YAML_FLOAT_TAG_ASCII[24] ={0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x66, 0x6c, 0x6f, 0x61, 0x74,  0x00};

static char YAML_SEQ_TAG_ASCII[22] ={0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x73, 0x65, 0x71,  0x00};

static char YAML_MAP_TAG_ASCII[22] ={0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x6d, 0x61, 0x70,  0x00};

static int emitScalar(yaml_emitter_t *emitter, char *scalar, char *tag, int style){
  yaml_event_t event;
  int sLen = strlen(scalar);
  char asciiScalar[sLen+1];
  memcpy(asciiScalar,scalar,sLen+1);
  convertFromNative(asciiScalar,sLen);
  yaml_scalar_event_initialize(&event, NULL, (yaml_char_t *)YAML_STR_TAG_ASCII,
                               (yaml_char_t*)asciiScalar, sLen,
                               1, 1, style);
  if (yaml_emitter_emit(emitter, &event)){
    return YAML_SUCCESS;
  } else {
    return YAML_GENERAL_FAILURE+1;
  }
}

static int writeJsonAsYaml1(yaml_emitter_t *emitter, Json *json){
  yaml_event_t event;
  char scalarBuffer[MAX_ACCESS_PATH];
  if (jsonIsArray(json)){
    JsonArray *array = jsonAsArray(json);
    int elementCount = jsonArrayGetCount(array);

    yaml_sequence_start_event_initialize(&event, NULL, (yaml_char_t *)YAML_SEQ_TAG_ASCII,
                                         1, YAML_ANY_SEQUENCE_STYLE);
    if (!yaml_emitter_emit(emitter, &event)) return YAML_GENERAL_FAILURE+2;
    
    for (int i=0; i<elementCount; i++){
      Json *itemValue = jsonArrayGetItem(array,i);
      int subStatus = writeJsonAsYaml1(emitter, itemValue);
      if (subStatus){
        return subStatus;
      }
    }

    yaml_sequence_end_event_initialize(&event);
    if (!yaml_emitter_emit(emitter, &event)) return YAML_GENERAL_FAILURE+3;
    return YAML_SUCCESS;
  } else if (jsonIsObject(json)){
    int propertyCount = 0;
    JsonObject *object = jsonAsObject(json);
    JsonProperty *property = jsonObjectGetFirstProperty(object);

    yaml_mapping_start_event_initialize(&event, NULL, (yaml_char_t *)YAML_MAP_TAG_ASCII,
                                        1, YAML_ANY_MAPPING_STYLE);
    if (!yaml_emitter_emit(emitter, &event)) return YAML_GENERAL_FAILURE+4;

    while (property){
      propertyCount++;
      char *propertyName = jsonPropertyGetKey(property);
      Json *propertyValue = jsonPropertyGetValue(property);
      int pLen = strlen(propertyName);
      char asciiName[pLen+1];
      memcpy(asciiName,propertyName,pLen+1);
      convertFromNative(asciiName,pLen);
      yaml_scalar_event_initialize(&event, NULL, (yaml_char_t *)YAML_STR_TAG_ASCII,
                                   (yaml_char_t *)asciiName, pLen,
                                   1, 0, YAML_PLAIN_SCALAR_STYLE);
      if (!yaml_emitter_emit(emitter, &event)) return YAML_GENERAL_FAILURE+5;

      int subStatus = writeJsonAsYaml1(emitter,propertyValue);
      if (subStatus){
        return subStatus;
      }
      property = jsonObjectGetNextProperty(property);
    }
    yaml_mapping_end_event_initialize(&event);
    if (!yaml_emitter_emit(emitter, &event)) return YAML_GENERAL_FAILURE+6;
    return YAML_SUCCESS;
  } else if (jsonIsInt64(json)){
#ifdef __ZOWE_OS_WINDOWS
    sprintf(scalarBuffer,"%lld",jsonAsInt64(json));
#else
    sprintf(scalarBuffer,"%ld",jsonAsInt64(json));
#endif
    return emitScalar(emitter,scalarBuffer,YAML_INT_TAG_ASCII, YAML_PLAIN_SCALAR_STYLE);
  } else if (jsonIsDouble(json)){
    sprintf(scalarBuffer,"%f",jsonAsDouble(json));
    return emitScalar(emitter,scalarBuffer,YAML_FLOAT_TAG_ASCII, YAML_PLAIN_SCALAR_STYLE);
  } else if (jsonIsNumber(json)){
    sprintf(scalarBuffer,"%d",jsonAsNumber(json));
    return emitScalar(emitter,scalarBuffer,YAML_INT_TAG_ASCII, YAML_PLAIN_SCALAR_STYLE);
  } else if (jsonIsString(json)){
    sprintf(scalarBuffer,"%s",jsonAsString(json));
    return emitScalar(emitter,scalarBuffer,YAML_STR_TAG_ASCII, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
  } else if (jsonIsBoolean(json)){
    return emitScalar(emitter,(jsonAsBoolean(json) ? "true" : "false"),YAML_BOOL_TAG_ASCII, YAML_PLAIN_SCALAR_STYLE);
  } else if (jsonIsNull(json)){
    return emitScalar(emitter,"null",YAML_NULL_TAG_ASCII, YAML_PLAIN_SCALAR_STYLE);
  } else {
    printf("PANIC: unexpected json type %d\n",json->type);
    return YAML_GENERAL_FAILURE+7;
  }
}

static int emitYaml(yaml_emitter_t *emitter, Json *json){
  yaml_event_t event;
  yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
  if (!yaml_emitter_emit(emitter, &event)) goto error;
  
  yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
  if (!yaml_emitter_emit(emitter, &event)){
    printf("failed at doc start\n");
    goto error;
  }
  
  int status = writeJsonAsYaml1(emitter,json);
  if (status){
    printf("failed at write %d\n",status);
    goto error;
  }
  
  yaml_document_end_event_initialize(&event, 0);
  if (!yaml_emitter_emit(emitter, &event)){
    printf("failed at doc end\n");
    goto error;
  }
  
  yaml_stream_end_event_initialize(&event);
  if (!yaml_emitter_emit(emitter, &event)) {
    printf("failed at end\n");
    goto error;
  }
  
  yaml_emitter_delete(emitter);
  return YAML_SUCCESS;
 error:
  yaml_emitter_delete(emitter);
  return YAML_GENERAL_FAILURE+8;
}

/* yaml_write_handler_t *handler, void *data);
   typedef int yaml_write_handler_t(void *data, unsigned char *buffer, size_t size); */

static int yamlHandlerCallback(void *context, unsigned char *buffer, size_t size){
  ByteOutputStream *baos = (ByteOutputStream*)context;
  bosWrite(baos,(char *)buffer,(int)size);
  return 1;
}
                               

int json2Yaml2Buffer(Json *json, char **buffer, int *bufferLen){
  ByteOutputStream *baos = makeByteOutputStream(0x1000);
  yaml_emitter_t emitter;
  yaml_emitter_initialize(&emitter);
  yaml_emitter_set_output(&emitter,yamlHandlerCallback,baos);

  int emitStatus = emitYaml(&emitter,json);
  if (emitStatus){
    bosFree(baos,true);
  } else {
    *buffer = bosNullTerminateAndUse(baos);
    *bufferLen = baos->size;
    bosFree(baos,false);
  }
  return emitStatus;
}

int json2Yaml2File(Json *json, FILE *out){
  yaml_emitter_t emitter;
  yaml_emitter_initialize(&emitter);
  yaml_emitter_set_output_file(&emitter, out);
  return emitYaml(&emitter,json);
}

/* =====================================================================
   Comment-preserving YAML round-trip support
   =====================================================================

   Architecture:
   1. scanYamlComments() reads the raw YAML file line-by-line and
      extracts comment text with line numbers (native codepage).
   2. yaml2JSONWithComments() builds the Json tree (via existing yaml2JSON1)
      then walks the YAML document + Json tree in parallel to attach
      comments to JsonProperty / Json nodes using start_mark line numbers.
   3. The custom YAML writer (json2Yaml2BufferWithComments /
      json2Yaml2FileWithComments) emits YAML text directly (no libyaml
      emitter) so that it can interleave comment lines.
   ===================================================================== */

/* ---------- Comment data structures ---------- */

typedef struct YamlComment_tag {
  int  line;       /* 0-based line number in the source file */
  char *text;      /* comment body (after '#', trimmed, native encoding) */
  int  isInline;   /* 1 = after content on same line, 0 = full-line comment */
  int  column;     /* column of '#' in original line (for alignment) */
  int  isBlank;    /* 1 = blank line (separator), 0 = comment */
} YamlComment;

struct YamlCommentList_tag {
  YamlComment *comments;
  int count;
  int capacity;
};

/* ---------- Comment scanner ---------- */

static void addComment(YamlCommentList *list, int line, const char *text, int isInline, int column, int isBlank) {
  if (list->count >= list->capacity) {
    int newCap = list->capacity ? list->capacity * 2 : 64;
    YamlComment *newArr = (YamlComment*)safeMalloc(newCap * sizeof(YamlComment), "YamlComment array");
    if (list->comments) {
      memcpy(newArr, list->comments, list->count * sizeof(YamlComment));
      safeFree((char*)list->comments, list->capacity * sizeof(YamlComment));
    }
    list->comments = newArr;
    list->capacity = newCap;
  }
  YamlComment *c = &list->comments[list->count++];
  c->line = line;
  c->isInline = isInline;
  c->column = column;
  c->isBlank = isBlank;
  int len = strlen(text);
  c->text = safeMalloc(len + 1, "comment text");
  memcpy(c->text, text, len + 1);
}

/*
 * Find the start of a comment on a line.
 * Returns pointer to the '#' character, or NULL if no comment.
 * Handles single-quoted and double-quoted strings.
 */
static const char *findCommentStart(const char *line) {
  int inSingle = 0, inDouble = 0;
  const char *p = line;
  while (*p) {
    if (*p == '\'' && !inDouble) {
      inSingle = !inSingle;
    } else if (*p == '"' && !inSingle) {
      inDouble = !inDouble;
    } else if (*p == '\\' && inDouble && *(p+1)) {
      p++; /* skip escaped character */
    } else if (*p == '#' && !inSingle && !inDouble) {
      /* In YAML, '#' is a comment only if preceded by whitespace or at start of line */
      if (p == line || *(p-1) == ' ' || *(p-1) == '\t') {
        return p;
      }
    }
    p++;
  }
  return NULL;
}

/* Trim leading whitespace from a string, return pointer into same buffer */
static const char *trimLeading(const char *s) {
  while (*s == ' ' || *s == '\t') s++;
  return s;
}

/* Trim trailing whitespace/newline in place */
static void trimTrailing(char *s) {
  int len = strlen(s);
  while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\n' || s[len-1] == '\r')) {
    s[--len] = '\0';
  }
}

YamlCommentList *scanYamlComments(const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) return NULL;

  YamlCommentList *list = (YamlCommentList*)safeMalloc(sizeof(YamlCommentList), "YamlCommentList");
  memset(list, 0, sizeof(YamlCommentList));

  char lineBuf[4096];
  int lineNum = 0;
  while (fgets(lineBuf, sizeof(lineBuf), fp)) {
    trimTrailing(lineBuf);
    const char *trimmed = trimLeading(lineBuf);

    if (*trimmed == '#') {
      /* Full-line comment */
      const char *body = trimmed + 1;
      if (*body == ' ') body++;
      addComment(list, lineNum, body, 0, (int)(trimmed - lineBuf), 0);
    } else if (*trimmed != '\0') {
      /* Non-empty, non-comment-only line: check for inline comment */
      const char *hashPos = findCommentStart(lineBuf);
      if (hashPos) {
        const char *body = hashPos + 1;
        if (*body == ' ') body++;
        addComment(list, lineNum, body, 1, (int)(hashPos - lineBuf), 0);
      }
    }
    lineNum++;
  }

  fclose(fp);
  return list;
}

void freeYamlComments(YamlCommentList *list) {
  if (!list) return;
  for (int i = 0; i < list->count; i++) {
    if (list->comments[i].text) {
      safeFree(list->comments[i].text, strlen(list->comments[i].text) + 1);
    }
  }
  if (list->comments) {
    safeFree((char*)list->comments, list->capacity * sizeof(YamlComment));
  }
  safeFree((char*)list, sizeof(YamlCommentList));
}

/* ---------- Comment lookup helpers ---------- */

/*
 * Concatenate all full-line comments (isInline==0) whose line numbers
 * fall in (afterLine, beforeLine).  Returns a safeMalloc'd string with
 * newline separators, or NULL if none found.
 */
static char *gatherBeforeComments(YamlCommentList *list, int afterLine, int beforeLine) {
  if (!list) return NULL;
  /* First pass: measure total length */
  int totalLen = 0;
  int found = 0;
  for (int i = 0; i < list->count; i++) {
    YamlComment *c = &list->comments[i];
    if (!c->isInline && c->line > afterLine && c->line < beforeLine) {
      totalLen += strlen(c->text) + 1; /* +1 for newline */
      found++;
    }
  }
  if (!found) return NULL;

  char *result = safeMalloc(totalLen + 1, "before comments");
  char *p = result;
  for (int i = 0; i < list->count; i++) {
    YamlComment *c = &list->comments[i];
    if (!c->isInline && c->line > afterLine && c->line < beforeLine) {
      int len = strlen(c->text);
      memcpy(p, c->text, len);
      p += len;
      *p++ = '\n';
    }
  }
  /* Overwrite last newline with null terminator */
  if (p > result) {
    *(p - 1) = '\0';
  } else {
    *p = '\0';
  }
  return result;
}

/*
 * Find the inline comment for a specific line.
 * Returns pointer to the comment text (owned by the list), or NULL.
 */
static char *getInlineCommentForLine(YamlCommentList *list, int line) {
  if (!list) return NULL;
  for (int i = 0; i < list->count; i++) {
    YamlComment *c = &list->comments[i];
    if (c->isInline && c->line == line) {
      return c->text;
    }
  }
  return NULL;
}

/*
 * Find the inline comment column for a specific line.
 * Returns the column of '#' in the original source, or 0 if not found.
 */
static int getInlineCommentColumnForLine(YamlCommentList *list, int line) {
  if (!list) return 0;
  for (int i = 0; i < list->count; i++) {
    YamlComment *c = &list->comments[i];
    if (c->isInline && c->line == line) {
      return c->column;
    }
  }
  return 0;
}

/*
 * Find the first full-line comment line number in the file
 * (used for document-level comment detection).
 */
static int getFirstContentLine(YamlCommentList *list) {
  if (!list || list->count == 0) return 0;
  /* Return the line number of the first comment, if it's a full-line comment at line 0 */
  return 0;
}

/* ---------- Comment attachment ---------- */

/*
 * Walk the YAML document tree and the Json tree in parallel,
 * attaching comments from the YamlCommentList to Json nodes.
 * prevLine tracks the end of the previous sibling for before-comment ranges.
 */
static void attachComments(Json *json, yaml_document_t *doc, yaml_node_t *yamlNode,
                           YamlCommentList *comments, int *prevLine) {
  if (!json || !yamlNode || !comments) return;

  switch (yamlNode->type) {
  case YAML_MAPPING_NODE: {
    JsonObject *obj = jsonAsObject(json);
    JsonProperty *prop = jsonObjectGetFirstProperty(obj);
    yaml_node_pair_t *pair = yamlNode->data.mapping.pairs.start;

    while (prop && pair < yamlNode->data.mapping.pairs.top) {
      yaml_node_t *keyNode = yaml_document_get_node(doc, pair->key);
      yaml_node_t *valNode = yaml_document_get_node(doc, pair->value);
      int keyLine = (int)keyNode->start_mark.line;

      /* Before-comments: full-line comments between previous item and this key */
      char *before = gatherBeforeComments(comments, *prevLine, keyLine);
      if (before) {
        jsonPropertySetBeforeComment(prop, before);
      }

      /* Inline comment: on the key's line (most YAML configs put
         the value on the same line as the key for scalars) */
      int inlineLine = keyLine;
      if (valNode && valNode->type == YAML_SCALAR_NODE) {
        /* If scalar value is on a different line, use value's line */
        int valLine = (int)valNode->start_mark.line;
        if (valLine == keyLine) {
          inlineLine = keyLine;
        } else {
          inlineLine = valLine;
        }
      }
      char *inl = getInlineCommentForLine(comments, inlineLine);
      if (inl) {
        jsonPropertySetInlineComment(prop, inl);
        jsonPropertySetInlineCommentColumn(prop, getInlineCommentColumnForLine(comments, inlineLine));
      }

      /* Advance prevLine and recurse into non-scalar values */
      if (valNode) {
        if (valNode->type == YAML_SCALAR_NODE) {
          *prevLine = (int)valNode->end_mark.line;
        } else {
          /* For the first child inside a mapping/sequence, prevLine is the key line */
          int childPrev = keyLine;
          attachComments(jsonPropertyGetValue(prop), doc, valNode, comments, &childPrev);
          *prevLine = childPrev;
        }
      }

      prop = jsonObjectGetNextProperty(prop);
      pair++;
    }
    break;
  }
  case YAML_SEQUENCE_NODE: {
    JsonArray *arr = jsonAsArray(json);
    int count = jsonArrayGetCount(arr);
    yaml_node_item_t *item = yamlNode->data.sequence.items.start;
    int i = 0;

    while (i < count && item < yamlNode->data.sequence.items.top) {
      yaml_node_t *itemNode = yaml_document_get_node(doc, *item);
      Json *element = jsonArrayGetItem(arr, i);
      int itemLine = (int)itemNode->start_mark.line;

      char *before = gatherBeforeComments(comments, *prevLine, itemLine);
      if (before) {
        jsonSetBeforeComment(element, before);
      }

      if (itemNode->type == YAML_SCALAR_NODE) {
        char *inl = getInlineCommentForLine(comments, itemLine);
        if (inl) {
          jsonSetInlineComment(element, inl);
          jsonSetInlineCommentColumn(element, getInlineCommentColumnForLine(comments, itemLine));
        }
        *prevLine = (int)itemNode->end_mark.line;
      } else {
        int childPrev = itemLine;
        attachComments(element, doc, itemNode, comments, &childPrev);
        *prevLine = childPrev;
      }

      item++;
      i++;
    }
    break;
  }
  default:
    break;
  }
}

/*
 * Attach the document-level comment (full-line comments at the very
 * top of the file, before the root node starts).
 */
static void attachDocumentComment(Json *json, yaml_document_t *doc, YamlCommentList *comments) {
  if (!json || !jsonIsObject(json) || !comments) return;
  yaml_node_t *root = yaml_document_get_root_node(doc);
  if (!root) return;
  int rootLine = (int)root->start_mark.line;
  if (rootLine <= 0) return;

  char *docComment = gatherBeforeComments(comments, -1, rootLine);
  if (docComment) {
    JsonObject *obj = jsonAsObject(json);
    jsonObjectSetDocumentComment(obj, docComment);
  }
}

Json *yaml2JSONWithComments(yaml_document_t *document, YamlCommentList *comments, ShortLivedHeap *slh) {
  /* Build the JSON tree using the existing converter */
  Json *json = yaml2JSON(document, slh);
  if (!json || !comments) return json;

  /* Attach comments from the scanned list */
  yaml_node_t *root = yaml_document_get_root_node(document);
  if (root) {
    attachDocumentComment(json, document, comments);
    int prevLine = (int)root->start_mark.line - 1;
    attachComments(json, document, root, comments, &prevLine);

    /* Attach trailing comments (after the last node) */
    if (jsonIsObject(json)) {
      int lastCommentLine = -1;
      for (int i = 0; i < comments->count; i++) {
        if (comments->comments[i].line > lastCommentLine) {
          lastCommentLine = comments->comments[i].line;
        }
      }
      /* Gather full-line comments after the last content line */
      char *trailer = gatherBeforeComments(comments, prevLine, lastCommentLine + 1);
      if (trailer) {
        jsonObjectSetTrailerComment(jsonAsObject(json), trailer);
      }
    }
  }
  return json;
}

/* ---------- Custom YAML writer with comments ---------- */

static void yamlWriteIndent(ByteOutputStream *bos, int indentLevel) {
  for (int i = 0; i < indentLevel; i++) {
    bosAppendChar(bos, ' ');
  }
}

static void yamlWriteCommentLines(ByteOutputStream *bos, const char *comment, int indentLevel) {
  if (!comment) return;
  const char *p = comment;
  while (*p) {
    yamlWriteIndent(bos, indentLevel);
    bosAppendString(bos, "# ");
    const char *eol = strchr(p, '\n');
    if (eol) {
      bosWrite(bos, (char*)p, (int)(eol - p));
      bosAppendChar(bos, '\n');
      p = eol + 1;
    } else {
      bosAppendString(bos, (char*)p);
      bosAppendChar(bos, '\n');
      break;
    }
  }
}

static void yamlWriteInlineComment(ByteOutputStream *bos, const char *comment,
                                    YamlWriteOptions *opts, int originalColumn) {
  if (!comment) return;
  /* Compute actual column from last newline in buffer */
  int currentColumn = 0;
  for (int k = bos->size - 1; k >= 0; k--) {
    if (bos->data[k] == '\n') {
      currentColumn = bos->size - k - 1;
      break;
    }
    if (k == 0) {
      currentColumn = bos->size;  /* no newline found, entire buffer is one line */
    }
  }
  if (opts && opts->commentAlignMode == JSON_COMMENT_ALIGN_FIXED) {
    int target = opts->commentPadWidth > 0 ? opts->commentPadWidth : 40;
    while (currentColumn < target) {
      bosAppendChar(bos, ' ');
      currentColumn++;
    }
    bosAppendString(bos, "# ");
  } else if (opts && opts->commentAlignMode == JSON_COMMENT_ALIGN_ORIGINAL) {
    /* Restore original column from stored inlineCommentColumn */
    if (originalColumn > currentColumn) {
      while (currentColumn < originalColumn) {
        bosAppendChar(bos, ' ');
        currentColumn++;
      }
      bosAppendString(bos, "# ");
    } else {
      bosAppendString(bos, " # ");
    }
  } else {
    bosAppendString(bos, " # ");
  }
  bosAppendString(bos, (char*)comment);
}

/* Forward declaration for recursive calls */
static void yamlWriteValue(ByteOutputStream *bos, Json *json, int indentLevel, YamlWriteOptions *opts);

static int yamlNeedsQuoting(const char *s) {
  if (!s || !*s) return 1;
  /* Check for special YAML values that need quoting */
  if (!strcmp(s, "true") || !strcmp(s, "false") ||
      !strcmp(s, "null") || !strcmp(s, "~") ||
      !strcmp(s, "yes") || !strcmp(s, "no") ||
      !strcmp(s, "on") || !strcmp(s, "off") ||
      !strcmp(s, "True") || !strcmp(s, "False") ||
      !strcmp(s, "TRUE") || !strcmp(s, "FALSE") ||
      !strcmp(s, "Yes") || !strcmp(s, "No") ||
      !strcmp(s, "ON") || !strcmp(s, "OFF")) {
    return 1;
  }
  /* Check for characters that need quoting */
  const char *p = s;
  if (*p == '-' || *p == '?' || *p == ':' || *p == '{' || *p == '}' ||
      *p == '[' || *p == ']' || *p == ',' || *p == '&' || *p == '*' ||
      *p == '#' || *p == '!' || *p == '|' || *p == '>' || *p == '\'' ||
      *p == '"' || *p == '%' || *p == '@' || *p == '`') {
    return 1;
  }
  while (*p) {
    if (*p == ':' || *p == '#') return 1;
    p++;
  }
  return 0;
}

static void yamlWriteScalar(ByteOutputStream *bos, Json *json) {
  char buf[256];
  if (jsonIsString(json)) {
    char *s = jsonAsString(json);
    if (yamlNeedsQuoting(s)) {
      bosAppendChar(bos, '"');
      /* Simple escaping for common cases */
      char *p = s;
      while (*p) {
        if (*p == '"') {
          bosAppendString(bos, "\\\"");
        } else if (*p == '\\') {
          bosAppendString(bos, "\\\\");
        } else if (*p == '\n') {
          bosAppendString(bos, "\\n");
        } else {
          bosAppendChar(bos, *p);
        }
        p++;
      }
      bosAppendChar(bos, '"');
    } else {
      bosAppendString(bos, s);
    }
  } else if (jsonIsInt64(json)) {
#ifdef __ZOWE_OS_WINDOWS
    sprintf(buf, "%lld", jsonAsInt64(json));
#else
    sprintf(buf, "%ld", jsonAsInt64(json));
#endif
    bosAppendString(bos, buf);
  } else if (jsonIsNumber(json)) {
    sprintf(buf, "%d", jsonAsNumber(json));
    bosAppendString(bos, buf);
  } else if (jsonIsDouble(json)) {
    sprintf(buf, "%g", jsonAsDouble(json));
    bosAppendString(bos, buf);
  } else if (jsonIsBoolean(json)) {
    bosAppendString(bos, jsonAsBoolean(json) ? "true" : "false");
  } else if (jsonIsNull(json)) {
    bosAppendString(bos, "null");
  }
}

static void yamlWriteMapping(ByteOutputStream *bos, Json *json, int indentLevel, YamlWriteOptions *opts) {
  JsonObject *obj = jsonAsObject(json);
  const char *docComment = jsonObjectGetDocumentComment(obj);
  if (docComment && indentLevel == 0) {
    yamlWriteCommentLines(bos, docComment, 0);
  }

  JsonProperty *prop = jsonObjectGetFirstProperty(obj);
  while (prop) {
    const char *beforeComment = jsonPropertyGetBeforeComment(prop);
    if (beforeComment) {
      yamlWriteCommentLines(bos, beforeComment, indentLevel);
    }

    yamlWriteIndent(bos, indentLevel);
    bosAppendString(bos, jsonPropertyGetKey(prop));
    bosAppendChar(bos, ':');

    Json *value = jsonPropertyGetValue(prop);
    if (jsonIsObject(value) || jsonIsArray(value)) {
      const char *inl = jsonPropertyGetInlineComment(prop);
      yamlWriteInlineComment(bos, inl, opts, jsonPropertyGetInlineCommentColumn(prop));
      bosAppendChar(bos, '\n');
      yamlWriteValue(bos, value, indentLevel + 2, opts);
    } else {
      bosAppendChar(bos, ' ');
      yamlWriteScalar(bos, value);
      const char *inl = jsonPropertyGetInlineComment(prop);
      yamlWriteInlineComment(bos, inl, opts, jsonPropertyGetInlineCommentColumn(prop));
      bosAppendChar(bos, '\n');
    }

    prop = jsonObjectGetNextProperty(prop);
  }

  /* Emit trailer comment at root level */
  if (indentLevel == 0) {
    const char *trailer = jsonObjectGetTrailerComment(obj);
    if (trailer) {
      bosAppendChar(bos, '\n');
      yamlWriteCommentLines(bos, trailer, 0);
    }
  }
}

static void yamlWriteSequence(ByteOutputStream *bos, Json *json, int indentLevel, YamlWriteOptions *opts) {
  JsonArray *arr = jsonAsArray(json);
  int count = jsonArrayGetCount(arr);
  for (int i = 0; i < count; i++) {
    Json *element = jsonArrayGetItem(arr, i);

    const char *beforeComment = jsonGetBeforeComment(element);
    if (beforeComment) {
      yamlWriteCommentLines(bos, beforeComment, indentLevel);
    }

    yamlWriteIndent(bos, indentLevel);
    bosAppendString(bos, "- ");

    if (jsonIsObject(element)) {
      /* Write first property on same line as '- ', rest indented */
      JsonObject *obj = jsonAsObject(element);
      JsonProperty *first = jsonObjectGetFirstProperty(obj);
      if (first) {
        const char *bcom = jsonPropertyGetBeforeComment(first);
        if (bcom) {
          /* Before-comment for first property goes above the '- ' line.
             Already printed if it was in the element's beforeComment.
             For safety, print it here. */
        }
        bosAppendString(bos, jsonPropertyGetKey(first));
        bosAppendChar(bos, ':');
        Json *fval = jsonPropertyGetValue(first);
        if (jsonIsObject(fval) || jsonIsArray(fval)) {
          const char *inl = jsonPropertyGetInlineComment(first);
          yamlWriteInlineComment(bos, inl, opts, jsonPropertyGetInlineCommentColumn(first));
          bosAppendChar(bos, '\n');
          yamlWriteValue(bos, fval, indentLevel + 4, opts);
        } else {
          bosAppendChar(bos, ' ');
          yamlWriteScalar(bos, fval);
          const char *inl = jsonPropertyGetInlineComment(first);
          yamlWriteInlineComment(bos, inl, opts, jsonPropertyGetInlineCommentColumn(first));
          bosAppendChar(bos, '\n');
        }
        /* Remaining properties at indentLevel + 2 */
        JsonProperty *rest = jsonObjectGetNextProperty(first);
        while (rest) {
          const char *bc = jsonPropertyGetBeforeComment(rest);
          if (bc) {
            yamlWriteCommentLines(bos, bc, indentLevel + 2);
          }
          yamlWriteIndent(bos, indentLevel + 2);
          bosAppendString(bos, jsonPropertyGetKey(rest));
          bosAppendChar(bos, ':');
          Json *rv = jsonPropertyGetValue(rest);
          if (jsonIsObject(rv) || jsonIsArray(rv)) {
            const char *inl = jsonPropertyGetInlineComment(rest);
            yamlWriteInlineComment(bos, inl, opts, jsonPropertyGetInlineCommentColumn(rest));
            bosAppendChar(bos, '\n');
            yamlWriteValue(bos, rv, indentLevel + 4, opts);
          } else {
            bosAppendChar(bos, ' ');
            yamlWriteScalar(bos, rv);
            const char *inl = jsonPropertyGetInlineComment(rest);
            yamlWriteInlineComment(bos, inl, opts, jsonPropertyGetInlineCommentColumn(rest));
            bosAppendChar(bos, '\n');
          }
          rest = jsonObjectGetNextProperty(rest);
        }
      } else {
        /* Empty object as array item */
        bosAppendString(bos, "{}\n");
      }
      /* Inline comment on the array element (the '- ' line) */
      const char *elemInl = jsonGetInlineComment(element);
      /* Already written via the first property; skip for now */
    } else if (jsonIsArray(element)) {
      bosAppendChar(bos, '\n');
      yamlWriteValue(bos, element, indentLevel + 2, opts);
    } else {
      yamlWriteScalar(bos, element);
      const char *inl = jsonGetInlineComment(element);
      yamlWriteInlineComment(bos, inl, opts, jsonGetInlineCommentColumn(element));
      bosAppendChar(bos, '\n');
    }
  }
}

static void yamlWriteValue(ByteOutputStream *bos, Json *json, int indentLevel, YamlWriteOptions *opts) {
  if (jsonIsObject(json)) {
    yamlWriteMapping(bos, json, indentLevel, opts);
  } else if (jsonIsArray(json)) {
    yamlWriteSequence(bos, json, indentLevel, opts);
  } else {
    yamlWriteIndent(bos, indentLevel);
    yamlWriteScalar(bos, json);
    bosAppendChar(bos, '\n');
  }
}

int json2Yaml2BufferWithComments(Json *json, char **buffer, int *bufferLen, YamlWriteOptions *opts) {
  ByteOutputStream *baos = makeByteOutputStream(0x1000);
  yamlWriteValue(baos, json, 0, opts);
  *buffer = bosNullTerminateAndUse(baos);
  *bufferLen = baos->size;
  bosFree(baos, false);
  return YAML_SUCCESS;
}

int json2Yaml2FileWithComments(Json *json, FILE *out, YamlWriteOptions *opts) {
  char *buffer = NULL;
  int bufferLen = 0;
  int status = json2Yaml2BufferWithComments(json, &buffer, &bufferLen, opts);
  if (status == YAML_SUCCESS && buffer) {
    fwrite(buffer, 1, bufferLen - 1, out); /* -1 to skip null terminator */
    safeFree(buffer, bufferLen);
  }
  return status;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
