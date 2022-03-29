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
#include <metal/stdbool.h>  
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
#include <sys/types.h> 
#include <stdint.h> 
#include <stdbool.h> 
#include <errno.h>
#include <setjmp.h>
#include <math.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "bpxnet.h"
#include "unixfile.h"
#include "json.h"
#include "parsetools.h"
#include "microjq.h"


/* 
   Grammar 

       Expr = Operation (PIPE* Operation)*

       Opertion = 
          Literal
          CreateArray
          CreateObject
          Filter

       Literal
          Number
          String  dquote and squote'd
       
       CreateArray
          LBRACK Expr (COMMA Expr) RBRACK

       CreateObject
          LPAREN KeyValue (COMMA KeyValue) RPAREN 

       KeyValue
          Key COLON Expression
 
       Key Identifier


   Test Filters
    '.results[] | {name, age}'

   Real JQ 

   set JQ="c:\temp"
   %JQ%\jq-win64 <args>...
   
 */


#define  TOP                (FIRST_GRULE_ID+0)
#define  EXPR               (FIRST_GRULE_ID+1)
#define  EXPR_TAIL          (FIRST_GRULE_ID+2)
#define  EXPR_PIPE_OP       (FIRST_GRULE_ID+3)
#define  OPERATION          (FIRST_GRULE_ID+4)
#define  CREATE_ARRAY       (FIRST_GRULE_ID+5)
#define  CREATE_ARRAY_TAIL  (FIRST_GRULE_ID+6)
#define  COMMA_EXPR         (FIRST_GRULE_ID+7)
#define  CREATE_OBJECT      (FIRST_GRULE_ID+8)
#define  CREATE_OBJECT_TAIL (FIRST_GRULE_ID+9)
#define  COMMA_KEY_VALUE    (FIRST_GRULE_ID+10)
#define  KEY_VALUE          (FIRST_GRULE_ID+11)
#define  FILTER             (FIRST_GRULE_ID+12)
#define  TRAVERSAL          (FIRST_GRULE_ID+13)
#define  MORE_TRAVERSALS    (FIRST_GRULE_ID+14)
#define  INDEX              (FIRST_GRULE_ID+15)
#define  PICK               (FIRST_GRULE_ID+16)
#define  EXPLODE            (FIRST_GRULE_ID+17)
#define  CONTEXT            (FIRST_GRULE_ID+18)

static char *getJQRuleName(int id){
  switch (id){
  case TOP: return "TOP";
  case EXPR: return "EXPR";
  case EXPR_TAIL: return "EXPR_TAIL";
  case EXPR_PIPE_OP: return "EXPR_PIPE_OP";
  case OPERATION: return "OPERATION";
  case CREATE_ARRAY: return "CREATE_ARRAY";
  case CREATE_ARRAY_TAIL: return "CREATE_ARRAY_TAIL";
  case COMMA_EXPR: return "COMMA_EXPR";
  case CREATE_OBJECT: return "CREATE_OBJECT";
  case CREATE_OBJECT_TAIL: return "CREATE_OBJECT_TAIL";
  case COMMA_KEY_VALUE: return "COMMA_KEY_VALUE";
  case KEY_VALUE: return "KEY_VALUE";
  case FILTER: return "FILTER";
  case TRAVERSAL: return "TRAVERSAL";
  case MORE_TRAVERSALS: return "MORE_TRAVERSALS";
  case INDEX: return "INDEX";
  case PICK: return "PICK";
  case EXPLODE: return "EXPLODE";
  case CONTEXT: return "CONTEXT";
  default:
    return "UNKNOWN_RULE";
  }
}

static GRuleSpec jqGrammar[] = {
  { G_SEQ,  TOP,  .sequence = {  EXPR, EOF_TOKEN, G_END} },
  { G_SEQ,  EXPR, .sequence = {  OPERATION, EXPR_TAIL, G_END }},
  { G_STAR, EXPR_TAIL, .star = EXPR_PIPE_OP },
  { G_SEQ,  EXPR_PIPE_OP, .sequence = { JTOKEN_VBAR, OPERATION, G_END }},
  { G_ALT,  OPERATION, .alternates = { JTOKEN_INTEGER,
                                      /* JTOKEN_DQUOTE_STRING,
                                       JTOKEN_SQUOTE_STRING,
                                       CREATE_ARRAY,
                                       CREATE_OBJECT, --> how are these distinguishable from traversals??  */
                                       FILTER,
                                       G_END }},
  { G_SEQ, CREATE_ARRAY, .sequence = { JTOKEN_LBRACK, EXPR, CREATE_ARRAY_TAIL, JTOKEN_RBRACK, G_END }},
  { G_STAR, CREATE_ARRAY_TAIL, .star = COMMA_EXPR },
  { G_SEQ, COMMA_EXPR, .sequence = { JTOKEN_COMMA, EXPR, G_END }},
  { G_SEQ, CREATE_OBJECT, .sequence = { JTOKEN_LBRACE, KEY_VALUE, CREATE_OBJECT_TAIL, JTOKEN_RBRACE, G_END }},
  { G_STAR, CREATE_OBJECT_TAIL, .star = COMMA_KEY_VALUE },
  { G_SEQ, COMMA_KEY_VALUE, .sequence = {JTOKEN_COMMA, KEY_VALUE, G_END }},
  { G_SEQ, KEY_VALUE, .sequence = { JTOKEN_IDENTIFIER, JTOKEN_COLON, EXPR, G_END }},
  { G_SEQ, FILTER, .sequence = { TRAVERSAL, MORE_TRAVERSALS, G_END }},
  { G_ALT, TRAVERSAL, .alternates = { INDEX, PICK, EXPLODE, CONTEXT, G_END }},
  { G_STAR, MORE_TRAVERSALS, .star = TRAVERSAL },
  { G_SEQ, INDEX, .sequence = { JTOKEN_LBRACK, JTOKEN_INTEGER, JTOKEN_RBRACK, G_END}},
  { G_SEQ, PICK, .sequence = { JTOKEN_DOT, JTOKEN_IDENTIFIER, G_END }},
  { G_SEQ, EXPLODE, .sequence = { JTOKEN_LBRACK, JTOKEN_RBRACK, G_END }},
  { G_SEQ, CONTEXT, .sequence = { JTOKEN_DOT, G_END}},
  { G_END }
};


Json *parseJQ(JQTokenizer *jqt, ShortLivedHeap *slh, int traceLevel){
  AbstractTokenizer *tokenizer = (AbstractTokenizer*)jqt;
  tokenizer->lowTokenID = FIRST_REAL_TOKEN_ID;
  tokenizer->highTokenID = LAST_JTOKEN_ID;
  tokenizer->nextToken = getNextJToken;
  tokenizer->getTokenIDName = getJTokenName;
  tokenizer->getTokenJsonType = getJTokenJsonType;
  GParseContext *ctx = gParse(jqGrammar,TOP,tokenizer,jqt->data,jqt->length,getJQRuleName,traceLevel);
  if (ctx->status > 0){
    return gBuildJSON(ctx,slh);
  } else {
    return NULL;
  }
}

#define JQ_ERROR_MAX 1024



typedef struct JQEvalContext_tag {
  FILE   *out;
  jmp_buf recoveryData;
  int     flags;
  int     traceLevel;
  int     errorCode;
  char   *errorMessage;
  int     errorMessageLength;
  jsonPrinter *printer;
  FILE   *traceOut;
} JQEvalContext;

#define JQ_SUCCESS 0
#define JQ_MISSING_PROPERTY 8
#define JQ_BAD_PROPERTY_TYPE 12
#define JQ_NOT_AN_OBJECT 16
#define JQ_NOT_AN_ARRAY 20
#define JQ_UNEXPECTED_OPERATION 24
#define JQ_INTERNAL_NULL_POINTER 28
#define JQ_NON_INTEGER_ARRAY_INDEX_NOT_YET_SUPPORTED 32
#define JQ_ARRAY_INDEX_OUT_OF_BOUNDS 36

static void jqEvalThrow(JQEvalContext *ctx, int errorCode, char *formatString, ...){
  va_list argPointer;
  char *text = safeMalloc(JQ_ERROR_MAX,"ErrorBuffer");
  va_start(argPointer,formatString);
  vsnprintf(text,JQ_ERROR_MAX,formatString,argPointer);
  va_end(argPointer);
  ctx->errorCode = errorCode;
  ctx->errorMessage = text;
  ctx->errorMessageLength = JQ_ERROR_MAX;
  longjmp(ctx->recoveryData,1);
}

static Json *getJQProperty(JQEvalContext *ctx, JsonObject *value, char *key, bool isRequired){
  if (value == NULL){
    jqEvalThrow(ctx,JQ_INTERNAL_NULL_POINTER,"NULL pointer seen when looking for key = '%s'",key);
    return NULL;
  }
  Json *propertyValue = jsonObjectGetPropertyValue(value,key);
  if (isRequired && (propertyValue == NULL)){
    jqEvalThrow(ctx,JQ_MISSING_PROPERTY,"Missing required property %s",key);
  }
  return propertyValue;
}

static int getJQInt(JQEvalContext *ctx, JsonObject *value, char *key, bool isRequired, int defaultValue){
  Json *intProperty = getJQProperty(ctx,value,key,isRequired);
  if (jsonIsNumber(intProperty)){
    return jsonAsNumber(intProperty);
  } else if (isRequired){
    jqEvalThrow(ctx,JQ_BAD_PROPERTY_TYPE,"Property '%s' is not int",key);
    return 0; /* unreachable */
  } else {
    return defaultValue;
  }
}

static JsonObject *getJQObject(JQEvalContext *ctx, JsonObject *value, char *key, bool isRequired){
  Json *p = getJQProperty(ctx,value,key,isRequired);
  if (jsonIsObject(p)){
    return jsonAsObject(p);
  } else if (isRequired){
    jqEvalThrow(ctx,JQ_BAD_PROPERTY_TYPE,"Property '%s' is not object",key);
    return NULL; /* unreachable */
  } else {
    return NULL;
  }
}

static char *getJQString(JQEvalContext *ctx, JsonObject *value, char *key, bool isRequired){
  Json *p = getJQProperty(ctx,value,key,isRequired);
  if (jsonIsString(p)){
    return jsonAsString(p);
  } else if (isRequired){
    jqEvalThrow(ctx,JQ_BAD_PROPERTY_TYPE,"Property '%s' is not string",key);
    return NULL; /* unreachable */
  } else {
    return NULL;
  }
}

static JsonArray *getJQArray(JQEvalContext *ctx, JsonObject *value, char *key, bool isRequired){
  Json *p = getJQProperty(ctx,value,key,isRequired);
  if (jsonIsArray(p)){
    return jsonAsArray(p);
  } else if (isRequired){
    jqEvalThrow(ctx,JQ_BAD_PROPERTY_TYPE,"Property '%s' is not array",key);
    return NULL; /* unreachable */
  } else {
    return NULL;
  }
}

static JsonObject *jqCastToObject(JQEvalContext *ctx, Json *json){
  if (json == NULL){
    jqEvalThrow(ctx,JQ_INTERNAL_NULL_POINTER,"NULL pointer seen when casting to JsonObject");
    return NULL;
  }
  if (jsonIsObject(json)){
    return jsonAsObject(json);
  } else {
    jqEvalThrow(ctx,JQ_NOT_AN_OBJECT,"Attempt to use value as object that is not an object");
    return NULL; /* unreachable */
  }
}

static JsonArray *jqCastToArray(JQEvalContext *ctx, Json *json){
  if (jsonIsArray(json)){
    return jsonAsArray(json);
  } else {
    jqEvalThrow(ctx,JQ_NOT_AN_ARRAY,"Attempt to use value as array that is not an array");
    return NULL; /* unreachable */
  }
}

static void jqPrint(JQEvalContext *ctx, Json *json){
  if (jsonIsString(json) && ctx->flags & JQ_FLAG_RAW_STRINGS){
    char *s = jsonAsString(json);
    fprintf(ctx->out,"%s",s);
  } else {
    jsonPrint(ctx->printer,json);
  }
}

static void evalTraversal(JQEvalContext *ctx, Json *value, Json *filter, int index, int moreCount){
  JsonObject *filterObject = jqCastToObject(ctx,filter);
  if (ctx->traceLevel >= 1){
    fprintf(ctx->traceOut,"evalTraversal, index=%d mcount=%d\n",index,moreCount);
  }
  Json *firstTraversal = getJQProperty(ctx,filterObject,"TRAVERSAL",true);
  JsonArray *moreTraversals = getJQArray(ctx,filterObject,"MORE_TRAVERSALS",true);
  Json *traversal = (index == 0 ) ? firstTraversal : jsonArrayGetItem(moreTraversals,index-1);
  JsonObject *traversalObject = jqCastToObject(ctx,traversal);
  int traversalType = getJQInt(ctx,traversalObject,"altID",true,0);
  Json *traversalDetails = getJQProperty(ctx,traversalObject,"value",true);
  switch (traversalType){
  case INDEX:
    {
      JsonObject *detailsObject = jqCastToObject(ctx,traversalDetails);
      int arrayIndex = getJQInt(ctx,detailsObject,"INTEGER",false,-1);
      if (arrayIndex == -1){        
        jqEvalThrow(ctx,JQ_NON_INTEGER_ARRAY_INDEX_NOT_YET_SUPPORTED,"Index must be integer");
      } else {
        JsonArray *valueArray = jqCastToArray(ctx,value);
        if (arrayIndex >= 0 || arrayIndex < jsonArrayGetCount(valueArray)){
          Json *valueForIndex = jsonArrayGetItem(valueArray,arrayIndex);
          if (index+1 <= moreCount){
            evalTraversal(ctx,valueForIndex,filter,index+1,moreCount);
          } else {
            jqPrint(ctx,valueForIndex);
          }
        } else {
          jqEvalThrow(ctx,JQ_ARRAY_INDEX_OUT_OF_BOUNDS,"% is not in size of array",arrayIndex);
        }
      }
    }
    break;
  case PICK:
    {
      JsonObject *detailsObject = jqCastToObject(ctx,traversalDetails);
      char *identifier = getJQString(ctx,detailsObject,"ID",true);
      JsonObject *valueObject = jqCastToObject(ctx,value);
      Json *valueForID = getJQProperty(ctx,valueObject,identifier,true);
      if (index+1 <= moreCount){
        evalTraversal(ctx,valueForID,filter,index+1,moreCount);
      } else {
        jqPrint(ctx,valueForID);
      }
    }
    break;
  case EXPLODE:
    {
      JsonArray *valueArray = jqCastToArray(ctx,value);
      int valueCount = jsonArrayGetCount(valueArray);
      for (int v=0; v<valueCount; v++){
        jsonPrinterReset(ctx->printer);
        Json *element = jsonArrayGetItem(valueArray,v);
        if (index+1 <= moreCount){
          evalTraversal(ctx,element,filter,index+1,moreCount);
        } else {
          jqPrint(ctx,element);
        }
        fprintf(ctx->out,"\n");
      }
    }
    break;
  case CONTEXT:
    if (index+1 <= moreCount){
      evalTraversal(ctx,value,filter,index+1,moreCount);
    } else {
      jqPrint(ctx,value);
    }
    break;
  default:
    jqEvalThrow(ctx,JQ_UNEXPECTED_OPERATION,"unexpeted operation %d",traversalType);
  }
}

static void evalFilter(JQEvalContext *ctx, Json *value, Json *filter){
  JsonObject *filterObject = jqCastToObject(ctx,filter);
  JsonArray *moreTraversals = getJQArray(ctx,filterObject,"MORE_TRAVERSALS",true);
  int moreCount = jsonArrayGetCount(moreTraversals);
  evalTraversal(ctx,value,filter,0,moreCount);
}

static void evalOperation(JQEvalContext *ctx, Json *value, Json *operation){
  JsonObject *operationObject = jqCastToObject(ctx,operation);
  int altID = getJQInt(ctx,operationObject,"altID",true,-1);
  switch (altID){
  case FILTER:
    evalFilter(ctx,value,getJQProperty(ctx,operationObject,"value",true));
    break;
  default:
    jqEvalThrow(ctx,JQ_UNEXPECTED_OPERATION,"unexpected operation %d",altID);
  } 
}

static void evalJQExpr(JQEvalContext *ctx, Json *value, Json *expr){
  JsonObject *exprObject = jqCastToObject(ctx,expr);
  Json *operation = getJQProperty(ctx,exprObject,"OPERATION",true);
  Json *exprTail = getJQProperty(ctx,exprObject,"EXPR_TAIL",true);
  evalOperation(ctx,value,operation);
  /* ignoring tail for now */
}

int evalJQ(Json *value, Json *jqTree, FILE *out, int flags, int traceLevel){
  JQEvalContext ctx;
  memset(&ctx,0,sizeof(JQEvalContext));
  ctx.out = out;
#ifdef __ZOWE_OS_WINDOWS
  int fd = _fileno(out);
#else
  int fd = fileno(out);
#endif
  ctx.printer = makeJsonPrinter(fd);
  if (flags & JQ_FLAG_PRINT_PRETTY){
    jsonEnablePrettyPrint(ctx.printer);
  }
  ctx.traceLevel = traceLevel;
  ctx.traceOut = stderr;
  ctx.flags = flags;
  if (setjmp(ctx.recoveryData) == 0) {  /* normal execution */
    evalJQExpr(&ctx,value,getJQProperty(&ctx,jqCastToObject(&ctx,jqTree),"EXPR",true));
    return JQ_SUCCESS;
  } else {
    fprintf(stderr,"uJQ error message: %s\n",ctx.errorMessage);
    return ctx.errorCode;
  }
}

/*
  Known examples from zowe-install-plugins

arguments: "-r .apimlServices.static.file"
arguments: "-r .apimlServices.static[].file"
arguments: "-r .appfwPlugins[0].path"
arguments: "-r .commands.configure"
arguments: "-r .commands.preConfigure"
arguments: "-r .commands.start"
arguments: "-r .commands.validate"
arguments: "-r .components.caching-service.storage.mode"
arguments: "-r .gatewaySharedLibs[0]"
arguments: "-r .zOSMF.host"
arguments: "-r .zOSMF.port"
arguments: "-r .zowe.launchScript.logLevel"
arguments: "-r .zowe.runtimeDirectory"
arguments: "-r .zowe.setup.certificate.dname.caCommonName"
arguments: "-r .zowe.setup.certificate.dname.commonName"
arguments: "-r .zowe.setup.certificate.dname.country"
arguments: "-r .zowe.setup.certificate.dname.locality"
arguments: "-r .zowe.setup.certificate.dname.org"
arguments: "-r .zowe.setup.certificate.dname.orgUnit"
arguments: "-r .zowe.setup.certificate.dname.state"
arguments: "-r .zowe.setup.certificate.importCertificateAuthorities"
arguments: "-r .zowe.setup.certificate.pkcs12.caAlias"
arguments: "-r .zowe.setup.certificate.pkcs12.caPassword"
arguments: "-r .zowe.setup.certificate.pkcs12.directory"
arguments: "-r .zowe.setup.certificate.pkcs12.import.alias"
arguments: "-r .zowe.setup.certificate.pkcs12.import.keystore"
arguments: "-r .zowe.setup.certificate.pkcs12.import.password"
arguments: "-r .zowe.setup.certificate.pkcs12.name"
arguments: "-r .zowe.setup.certificate.pkcs12.password"
arguments: "-r .zowe.setup.certificate.san"
arguments: "-r .zowe.setup.certificate.type"
arguments: "-r .zowe.setup.certificate.validity"
arguments: "-r .zowe.setup.certificate.zOSMF.ca"
arguments: "-r .zowe.setup.certificate.zOSMF.user"
arguments: "-r .zowe.setup.mvs.authLoadlib"
arguments: "-r .zowe.setup.mvs.authPluginLib"
arguments: "-r .zowe.setup.mvs.hlq"
arguments: "-r .zowe.setup.mvs.jcllib"
arguments: "-r .zowe.setup.mvs.parmlib"
arguments: "-r .zowe.setup.mvs.proclib"
arguments: "-r .zowe.setup.security.groups.admin"
arguments: "-r .zowe.setup.security.groups.stc"
arguments: "-r .zowe.setup.security.groups.sysProg"
arguments: "-r .zowe.setup.security.product"
arguments: "-r .zowe.setup.security.stcs.aux"
arguments: "-r .zowe.setup.security.stcs.xmem"
arguments: "-r .zowe.setup.security.stcs.zowe"
arguments: "-r .zowe.setup.security.users.aux"
arguments: "-r .zowe.setup.security.users.xmem"
arguments: "-r .zowe.setup.security.users.zowe"
arguments: "-r .zowe.verifyCertificates"
arguments: "-r .zowe.workspaceDirectory"
 */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
