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
#include "json.h"
#include "jsonschema.h"

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


yaml_document_t *readYAML(char *filename){
  FILE *file;
  yaml_parser_t parser;
  yaml_document_t *document = (yaml_document_t*)safeMalloc(sizeof(yaml_document_t),"YAML Doc");
  int done = 0;
  int count = 0;
  int error = 0;
  
   file = fopen(filename, "rb");
  assert(file);
  
  assert(yaml_parser_initialize(&parser));

  yaml_parser_set_input(&parser, yamlReadHandler, file);
  
  printf("before parser_load\n");fflush(stdout);
  
  if (!yaml_parser_load(&parser, document)) {

    printf("bad yaml load\n");fflush(stdout);
    error = 1;
  } else if (yaml_document_get_root_node(document)){

  } else {
    error = 1;
  }

  printf("before parser_delete\n");fflush(stdout);
  yaml_parser_delete(&parser);
  printf("after parser_delete\n");fflush(stdout);
  assert(!fclose(file));
          
  if (error){
    safeFree((char*)document,sizeof(yaml_document_t));
    document = NULL;
  }

  return document;
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
  printf("%s%c", val, eol ? '\n' : '');
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
      printf("Scalar: (len=%d)", node->data.scalar.length);
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

#define MAX_JSON_KEY 256
#define MAX_JSON_STRING 65536

/* this needs to be smarter some day */

static bool isSyntacticallyInteger(yaml_char_t *data, int length){
  if (length == 0){
    printf("empty string not integer\n");
    return false;
  }
  for (int i=0; i<length; i++){
    yaml_char_t c = data[i];
    if (c >= '0' && c <= '9'){
      
    } else {
      printf("%s is NOT an integer\n",data);
      return false;
    }     
  }
  printf("%s is an integer\n",data);
  return true;
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

#define JSON_FAIL_NOT_HANDLED 100
#define JSON_FAIL_BAD_INTEGER 104

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
        printf("tag = %s scalarStyle=%s\n",nativeTag,getScalarStyleName(node->data.scalar.style));
        Json *scalar = NULL;
        // HERE, make test with float, int, bool, null, ddate
        if (!strcmp(nativeTag,YAML_NULL_TAG)){
        } else if (!strcmp(nativeTag,YAML_NULL_TAG)){
          /* Json *scalar = jsonBuildNull(b,parent,parentKey,&buildStatus); */
        } else if (!strcmp(nativeTag,YAML_BOOL_TAG)){
          /* Json *scalar = jsonBuildBool(b,parent,parentKey,"FOO",3,&buildStatus); */
        } else if (!strcmp(nativeTag,YAML_INT_TAG) ||
                   (!strcmp(nativeTag,YAML_STR_TAG) &&
                    (style == YAML_PLAIN_SCALAR_STYLE) &&
                    isSyntacticallyInteger(nativeValue,valueLength))){
          bool valid;
          int64_t x = readInt(nativeValue,valueLength,&valid);
          if (valid){
            scalar = jsonBuildInt64(b,parent,parentKey,x,&buildStatus);
          } else {
            buildStatus = JSON_FAIL_BAD_INTEGER;
          }
          /* Json *scalar = jsonBuildInt(b,parent,parentKey,"FOO",3,&buildStatus); */
        } else if (!strcmp(nativeTag,YAML_STR_TAG)){
          scalar = jsonBuildString(b,parent,parentKey,nativeValue,valueLength,&buildStatus);
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
                printf("*** WARNING *** key too long '%*.*s...'\n",
                       MAX_JSON_KEY,MAX_JSON_KEY,keyNode->data.scalar.value);
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
              yaml2JSON1(b,jsonObject,keyNative,doc,valueNode,depth+2);
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

Json *yaml2JSON(yaml_document_t *document, ShortLivedHeap *slh){
  JsonBuilder *builder = makeJsonBuilder(slh);
  Json *json = yaml2JSON1(builder,NULL,NULL,document,yaml_document_get_root_node(document),0);
  freeJsonBuilder(builder,false);
  return json;
}
