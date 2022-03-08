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

/*
  clang -I %QJS%/porting -I%YAML%/include -I../platform/windows -I %QJS% -I ..\h -Wdeprecated-declarations -D_CRT_SECURE_NO_WARNINGS -o microjq.exe microjq.c parsetools.c ../c/json.c ../c/xlate.c ../c/charsets.c ../c/winskt.c ../c/logging.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc.c
 */

#define STATE_INITIAL    0
#define STATE_INTEGER    1
#define STATE_IDENTIFIER 2
#define STATE_SQUOTE     3
#define STATE_DQUOTE     4

#define TOKEN_IDENTIFIER    FIRST_REAL_TOKEN_ID
#define TOKEN_INTEGER       (FIRST_REAL_TOKEN_ID+1)
#define TOKEN_DQUOTE_STRING FIRST_REAL_TOKEN_ID+2
#define TOKEN_SQUOTE_STRING FIRST_REAL_TOKEN_ID+3
#define TOKEN_LPAREN        FIRST_REAL_TOKEN_ID+4
#define TOKEN_RPAREN        FIRST_REAL_TOKEN_ID+5
#define TOKEN_LBRACK        FIRST_REAL_TOKEN_ID+6
#define TOKEN_RBRACK        FIRST_REAL_TOKEN_ID+7
#define TOKEN_LBRACE        FIRST_REAL_TOKEN_ID+8
#define TOKEN_RBRACE        FIRST_REAL_TOKEN_ID+9
#define TOKEN_DOT           FIRST_REAL_TOKEN_ID+10
#define TOKEN_COMMA         FIRST_REAL_TOKEN_ID+11
#define TOKEN_VBAR          FIRST_REAL_TOKEN_ID+12
#define TOKEN_COLON         FIRST_REAL_TOKEN_ID+13
#define TOKEN_QMARK         FIRST_REAL_TOKEN_ID+14
#define TOKEN_PLUS          FIRST_REAL_TOKEN_ID+15
#define TOKEN_DASH          FIRST_REAL_TOKEN_ID+16
#define TOKEN_STAR          FIRST_REAL_TOKEN_ID+17
#define TOKEN_SLASH         FIRST_REAL_TOKEN_ID+18
#define TOKEN_PERCENT       FIRST_REAL_TOKEN_ID+19

#define LAST_TOKEN_ID TOKEN_PERCENT /* KEEP ME UPDATED!!! - whenever a new token ID is added*/


typedef struct JQParser_tag {
  AbstractTokenizer tokenizer; /* low token, high token */
  int ccsid;
  int tokenState; /*  = STATE_INITIAL; */
  char *data;
  int   length;
  int   pos;
} JQParser;

static char *getTokenName(int id){
  switch (id){
  case EOF_TOKEN: return "EOF";
  case TOKEN_IDENTIFIER: return "ID";
  case TOKEN_INTEGER: return "INTEGER";
  case TOKEN_DQUOTE_STRING: return "DQUOTE_STRING";
  case TOKEN_SQUOTE_STRING: return "SQUOTE_STRING";
  case TOKEN_LPAREN: return "LPAREN";
  case TOKEN_RPAREN: return "RPAREN";
  case TOKEN_LBRACK: return "LBRACK";
  case TOKEN_RBRACK: return "RBRACK";
  case TOKEN_LBRACE: return "LBRACE";
  case TOKEN_RBRACE: return "RBRACE";
  case TOKEN_DOT: return "DOT";
  case TOKEN_COMMA: return "COMMA";
  case TOKEN_VBAR: return "VBAR";
  case TOKEN_COLON: return "COLON";
  case TOKEN_QMARK: return "QMARK";
  case TOKEN_PLUS: return "PLUS";
  case TOKEN_DASH: return "DASH";
  case TOKEN_STAR: return "STAR";
  case TOKEN_SLASH: return "SLASH";
  case TOKEN_PERCENT: return "PERCENT";
  default:return "BAD_TOKEN";
  }
}

static bool isLetter(JQParser *jqp, int c){
  return testCharProp(jqp->ccsid,c,CPROP_LETTER);
}

static bool isDigit(JQParser *jqp, int c){
  return testCharProp(jqp->ccsid,c,CPROP_DIGIT);
}

static bool isWhite(JQParser *jqp, int c){
  return testCharProp(jqp->ccsid,c,CPROP_WHITE);
}

static int jqpToken(JQParser *jqp){
  /* belt *AND* suspenders */
  AbstractTokenizer *tokenizer = (AbstractTokenizer*)jqp;
  if (jqp->pos >= jqp->length){
    tokenizer->lastTokenStart = jqp->pos; /* 0-length token */
    tokenizer->lastTokenEnd = jqp->pos;
    return EOF_TOKEN;
  }
  int tokenID = NO_VALID_TOKEN;
  while(tokenID == NO_VALID_TOKEN){
    int c = -1; // EOF by defaultx
    int charPos = jqp->pos;
    if (jqp->pos < jqp->length){
      c = (int)jqp->data[jqp->pos++];
    }
    /* printf("at pos=%d, c=0x%x state=%d\n",jqp->pos,c,jqp->tokenState);  */
    switch (jqp->tokenState){
    case STATE_INITIAL:
      if (c == -1){
        tokenID = EOF_TOKEN;
        tokenizer->lastTokenStart = jqp->length;
      } else if (isLetter(jqp,c) ||
                 (c == UNI_DOLLAR) ||
                 (c == UNI_UNDER)){
        jqp->tokenState = STATE_IDENTIFIER;
        tokenizer->lastTokenStart = charPos;
      } else if (isDigit(jqp,c)){
        jqp->tokenState = STATE_INTEGER;
        tokenizer->lastTokenStart = charPos;
      } else if (isWhite(jqp,c)){
        // just roll forward 
      } else if (c == UNI_DQUOTE){
        jqp->tokenState = STATE_DQUOTE;
        tokenizer->lastTokenStart = charPos;
      } else if (c == UNI_SQUOTE){
        jqp->tokenState = STATE_SQUOTE;
        tokenizer->lastTokenStart = charPos;
      } else {
        /* Single-char-punc tokens */
        switch (c){
        case UNI_LPAREN: tokenID = TOKEN_LPAREN; break;
        case UNI_RPAREN: tokenID = TOKEN_RPAREN; break;
        case UNI_LBRACK: tokenID = TOKEN_LBRACK; break;
        case UNI_RBRACK: tokenID = TOKEN_RBRACK; break;
        case UNI_LBRACE: tokenID = TOKEN_LBRACE; break;
        case UNI_RBRACE: tokenID = TOKEN_RBRACE; break;
        case UNI_DOT: tokenID = TOKEN_DOT; break;
        case UNI_COMMA: tokenID = TOKEN_COMMA; break;
        case UNI_COLON: tokenID = TOKEN_COLON; break;
        case UNI_VBAR: tokenID = TOKEN_VBAR; break;
        case UNI_QMARK: tokenID = TOKEN_QMARK; break;
        case UNI_PLUS: tokenID = TOKEN_PLUS; break;
        case UNI_DASH: tokenID = TOKEN_DASH; break;
        case UNI_STAR: tokenID = TOKEN_STAR; break;
        case UNI_SLASH: tokenID = TOKEN_SLASH; break;
        case UNI_PERCENT: tokenID = TOKEN_PERCENT; break;
        default:
          /* unhandled char, kill the parse */
          return NO_VALID_TOKEN; 
        }
        tokenizer->lastTokenStart = charPos;
      }
      break;
    case STATE_INTEGER:
      if (!isDigit(jqp,c)){
        if (c != -1) jqp->pos--; /* nothing to push back */
        tokenID = TOKEN_INTEGER;
      }
      break;
    case STATE_IDENTIFIER:
      if (isLetter(jqp,c) ||
          (c == UNI_DOLLAR) ||
          (c == UNI_UNDER)){
        // accumulate
      } else {
        if (c != -1) jqp->pos--; /* nothing to push back */
        tokenID = TOKEN_IDENTIFIER;
      }
      break;
    case STATE_DQUOTE:
      if (c == -1){ /* EOF in string */
        return NO_VALID_TOKEN;
      } else if (c == UNI_DQUOTE){
        tokenID = TOKEN_DQUOTE_STRING;
      }
      break;
    case STATE_SQUOTE:
      if (c == -1){ /* EOF in string */
        return NO_VALID_TOKEN;
      } else if (c == UNI_SQUOTE){
        tokenID = TOKEN_SQUOTE_STRING;
      }
    default:
      printf("*** PANIC *** internal error unknown tokenizer state = %d\n",jqp->tokenState);
      return NO_VALID_TOKEN;
      // unknow state, kill the parse
    }
    if (tokenID != NO_VALID_TOKEN){
      tokenizer->lastTokenEnd = jqp->pos;
      jqp->tokenState = STATE_INITIAL;
      return tokenID;
    } 
  }
  return 0;
}

int getNextToken(AbstractTokenizer *tokenizer, char *s, int len, int pos, int *nextPos){
  JQParser *jqp = (JQParser*)tokenizer;
  jqp->pos = pos;
  int id = jqpToken(jqp);
  *nextPos = jqp->pos;
  return id;
}

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

static void tokenTest(JQParser *jqp){
  AbstractTokenizer *tokenizer = (AbstractTokenizer*)jqp;
  printf("length = %d\n",jqp->length);
  while (true){
    int tokenID = jqpToken(jqp);
    printf("token id=%d, %s pos is now=%d\n",tokenID,getTokenName(tokenID),jqp->pos);
    printf("  from %d to %d\n",tokenizer->lastTokenStart,tokenizer->lastTokenEnd);
    if (tokenID == NO_VALID_TOKEN){
      printf("bad token seen near %d\n",jqp->pos);
      break;
    } else if (tokenID == EOF_TOKEN){
      printf("EOF Seen\n");
      break;
    }
  }
}

#define  TOP           (FIRST_GRULE_ID+0)
#define  EXPR          (FIRST_GRULE_ID+1)
#define  EXPR_TAIL     (FIRST_GRULE_ID+2)
#define  EXPR_ADD_SUB  (FIRST_GRULE_ID+3)
#define  PLUS_MINUS    (FIRST_GRULE_ID+4)
#define  TERM          (FIRST_GRULE_ID+5)
#define  TERM_TAIL     (FIRST_GRULE_ID+6)
#define  TERM_MULT_DIV (FIRST_GRULE_ID+7)
#define  STAR_SLASH    (FIRST_GRULE_ID+8)
#define  FACTOR        (FIRST_GRULE_ID+9)

static char *getRuleName(int id){
  switch (id){
  case TOP: return "TOP";
  case EXPR: return "EXPR";
  case EXPR_TAIL: return "EXPR_TAIL";
  case EXPR_ADD_SUB: return "EXPR_ADD_SUB";
  case PLUS_MINUS: return "PLUS_MINUS";
  case TERM: return "TERM";
  case TERM_TAIL: return "TERM_TAIL";
  case TERM_MULT_DIV: return "TERM_MULT_DIV";
  case STAR_SLASH: return "STAR_SLASH";
  case FACTOR: return "FACTOR";
  default:
    return "UNKNOWN_RULE";
  }
}

static GRuleSpec expressionGrammar[] = {
  { G_SEQ, TOP,  .sequence = {  EXPR, EOF_TOKEN, G_END} },
  { G_SEQ, EXPR, .sequence = {  TERM, EXPR_TAIL, G_END }},
  { G_STAR, EXPR_TAIL, .star = EXPR_ADD_SUB },
  { G_SEQ,  EXPR_ADD_SUB, .sequence = { PLUS_MINUS, TERM, G_END }},
  { G_ALT, PLUS_MINUS, .alternates = { TOKEN_PLUS, TOKEN_DASH, G_END }},
  { G_SEQ, TERM, .sequence = { FACTOR, TERM_TAIL, G_END }},
  { G_STAR, TERM_TAIL, .star = TERM_MULT_DIV },
  { G_SEQ,  TERM_MULT_DIV, .sequence = { STAR_SLASH, FACTOR, G_END}},
  { G_ALT, STAR_SLASH, .alternates = { TOKEN_STAR, TOKEN_SLASH, G_END }},
  { G_ALT, FACTOR, .alternates = { TOKEN_IDENTIFIER, TOKEN_INTEGER, G_END }}, /* add expression recursion later */
  { G_END }
};


static void parseTest1(JQParser *jqp){
  AbstractTokenizer *tokenizer = (AbstractTokenizer*)jqp;
  tokenizer->lowTokenID = FIRST_REAL_TOKEN_ID;
  tokenizer->highTokenID = LAST_TOKEN_ID;
  tokenizer->nextToken = getNextToken;
  tokenizer->getTokenIDName = getTokenName;
  GParseContext *ctx = gParse(expressionGrammar,TOP,tokenizer,jqp->data,jqp->length,getRuleName);
  printf("parse ctx = 0x%p\n",ctx);
  if (ctx->status > 0){
    #ifdef __ZOWE_OS_WINDOWS
    int stdoutFD = _fileno(stdout);
#else
    int stdoutFD = STDOUT_FILENO;
#endif
    ShortLivedHeap *slh = makeShortLivedHeap(0x10000, 100);
    Json *tree = gBuildJSON(ctx,slh);
    if (tree){
      jsonPrinter *p = makeJsonPrinter(stdoutFD);
      jsonEnablePrettyPrint(p);
      printf("parse result as json\n");
      jsonPrint(p,tree);
    }
  }
}


int main(int argc, char **argv){
  char *command = argv[1];
  char *filter = argv[2];
  char *filename = argv[3];
  JQParser jqp;
  memset(&jqp,0,sizeof(JQParser));
  jqp.data = argv[2];
  jqp.length = strlen(jqp.data);
  jqp.ccsid = 1208;
  if (!strcmp(command,"tokenize")){
    printf("tokenize...\n");
    tokenTest(&jqp);
  } else if (!strcmp(command,"parse")){
    printf("parse...\n");
    parseTest1(&jqp);
  } else {
    printf("bad command: %s\n",command);
  }
  return 0;
}
