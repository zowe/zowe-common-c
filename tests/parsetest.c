#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "json.h"
#include "parsetools.h"

/*

  clang -I../h -I../platform/windows -D_CRT_SECURE_NO_WARNINGS -Dstrdup=_strdup -DYAML_DECLARE_STATIC=1 -Wdeprecated-declarations --rtlib=compiler-rt -o parsetest.exe parsetest.c ../c/parsetools.c ../c/json.c ../c/winskt.c ../c/xlate.c ../c/charsets.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc.c

 Grammar
  (PHRASEREF n)
  (TOKENREF n)
  (ALT n)
  (SEQ n)
  (STAR )
  (PLUS n)

 */

static int myNextToken(AbstractTokenizer *tokenizer, char *s, int len, int pos, int *nextPos){
  if (pos < len){
    char c = s[pos];
    *nextPos = (pos+1);
    return c&0xFF;
  } else {
    *nextPos = pos;
    return EOF_TOKEN;
  }
}

static char *myTokenIDName(int id){
  // leaky
  switch(id){
  case EOF_TOKEN: return "<EOF>";
  case NO_VALID_TOKEN: return "<BADTOKEN>";
  default:
    {
      char *buffer = safeMalloc(8,"Token ID");
      snprintf(buffer,8,"<%c>",id);
      return buffer;
    }
  }
}

#define  TOP  (FIRST_GRULE_ID+0)
#define  R1  (FIRST_GRULE_ID+1)
#define  R2  (FIRST_GRULE_ID+2)
#define  EE (FIRST_GRULE_ID+3)
#define  EEE  (FIRST_GRULE_ID+4)
#define  BBB  (FIRST_GRULE_ID+5)

static GRuleSpec testSet1[] = {
  { G_SEQ, TOP, .sequence = { (int)'A', R1, (int)'D', (int)EOF_TOKEN, G_END} },
  { G_ALT, R1, .alternates = { (int)'B', (int)'C', G_END}},
  { G_END }
};


static GRuleSpec testSet2[] = {
  { G_SEQ, TOP, .sequence = { (int)'A', R1, (int)'D', (int)EOF_TOKEN, G_END} },
  { G_ALT, R1, .alternates = { EEE, EE, G_END}},
  { G_SEQ, EEE, .sequence = { (int)'E', (int)'E', (int)'E', G_END}},
  { G_SEQ, EE, .sequence = { (int)'E', (int)'E', G_END}},
  { G_END }
};

static GRuleSpec testSet3[] = {
  { G_SEQ, TOP, .sequence = { (int)'A', R1, (int)'D', (int)EOF_TOKEN, G_END} },
  { G_STAR, R1, .star = R2 },
  { G_ALT, R2, .alternates = { BBB, EE, G_END}},
  { G_SEQ, BBB, .sequence = { (int)'B', (int)'B', (int)'B', G_END}},
  { G_SEQ, EE, .sequence = { (int)'E', (int)'E', G_END}},
  { G_END }
};

static char *getTestSetRuleName(int id){
  switch (id){
  case TOP: return "TOP";
  case R1: return "R1";
  case R2: return "R2";
  case EE: return "EE";
  case EEE: return "EEE";
  case BBB: return "BBB";
  default:
    return "UNKNOWN_RULE";
  }
}



static int old_main(int argc, char **argv){
  AbstractTokenizer tokenizer;
  tokenizer.lowTokenID = 1;
  tokenizer.highTokenID = 127;
  tokenizer.nextToken = myNextToken;
  tokenizer.getTokenIDName = myTokenIDName;
  GParseContext *ctx = gParse(testSet3,TOP,&tokenizer,argv[1],strlen(argv[1]),getTestSetRuleName);
  return 0;
}

static void tokenTest(JQTokenizer *jqt){
  AbstractTokenizer *tokenizer = (AbstractTokenizer*)jqt;
  printf("length = %d\n",jqt->length);
  while (true){
    int tokenID = jqtToken(jqt);
    printf("token id=%d, %s pos is now=%d\n",tokenID,getJTokenName(tokenID),jqt->pos);
    printf("  from %d to %d\n",tokenizer->lastTokenStart,tokenizer->lastTokenEnd);
    if (tokenID == NO_VALID_TOKEN){
      printf("bad token seen near %d\n",jqt->pos);
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

static char *getExprRuleName(int id){
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
  { G_ALT, PLUS_MINUS, .alternates = { JTOKEN_PLUS, JTOKEN_DASH, G_END }},
  { G_SEQ, TERM, .sequence = { FACTOR, TERM_TAIL, G_END }},
  { G_STAR, TERM_TAIL, .star = TERM_MULT_DIV },
  { G_SEQ,  TERM_MULT_DIV, .sequence = { STAR_SLASH, FACTOR, G_END}},
  { G_ALT, STAR_SLASH, .alternates = { JTOKEN_STAR, JTOKEN_SLASH, G_END }},
  { G_ALT, FACTOR, .alternates = { JTOKEN_IDENTIFIER, JTOKEN_INTEGER, G_END }}, /* add expression recursion later */
  { G_END }
};


static void parseTest1(JQTokenizer *jqt){
  AbstractTokenizer *tokenizer = (AbstractTokenizer*)jqt;
  tokenizer->lowTokenID = FIRST_REAL_TOKEN_ID;
  tokenizer->highTokenID = LAST_JTOKEN_ID;
  tokenizer->nextToken = getNextJToken;
  tokenizer->getTokenIDName = getJTokenName;
  GParseContext *ctx = gParse(expressionGrammar,TOP,tokenizer,jqt->data,jqt->length,getExprRuleName);
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
  JQTokenizer jqt;
  memset(&jqt,0,sizeof(JQTokenizer));
  jqt.data = argv[2];
  jqt.length = strlen(jqt.data);
  jqt.ccsid = 1208;
  if (!strcmp(command,"tokenize")){
    printf("tokenize...\n");
    tokenTest(&jqt);
  } else if (!strcmp(command,"parse")){
    printf("parse...\n");
    parseTest1(&jqt);
  } else {
    printf("bad command: %s\n",command);
  }
  return 0;
}


