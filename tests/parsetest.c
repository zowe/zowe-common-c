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



int main(int argc, char **argv){
  AbstractTokenizer tokenizer;
  tokenizer.lowTokenID = 1;
  tokenizer.highTokenID = 127;
  tokenizer.nextToken = myNextToken;
  tokenizer.getTokenIDName = myTokenIDName;
  GParseContext *ctx = gParse(testSet3,TOP,&tokenizer,argv[1],strlen(argv[1]),getTestSetRuleName);
  return 0;
}


/*
  seq( A E* D )
  
  A, eps, D=E, fail
     E* bears fruit until the first time it doesn't match one more 
*/

