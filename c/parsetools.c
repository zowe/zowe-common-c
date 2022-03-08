#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "parsetools.h"
#include "json.h"

/*

  clang -I../h -I../platform/windows -D_CRT_SECURE_NO_WARNINGS -Dstrdup=_strdup -DYAML_DECLARE_STATIC=1 -Wdeprecated-declarations --rtlib=compiler-rt -o parsetools.exe parsetools.c timeutls.c utils.c alloc.c

 Grammar
  (PHRASEREF n)
  (TOKENREF n)
  (ALT n)
  (SEQ n)
  (STAR )
  (PLUS n)

 */

static int cp1047to1208[256] = {
  0x00, 0x01, 0x02, 0x03, 0x9c, 0x09, 0x86, 0x7f, 0x97, 0x8d, 0x8e, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 
  0x10, 0x11, 0x12, 0x13, 0x9d, 0x0a, 0x08, 0x87, 0x18, 0x19, 0x92, 0x8f, 0x1c, 0x1d, 0x1e, 0x1f, 
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x17, 0x1b, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x05, 0x06, 0x07, 
  0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04, 0x98, 0x99, 0x9a, 0x9b, 0x14, 0x15, 0x9e, 0x1a, 
  0x20, 0xa0, 0xe2, 0xe4, 0xe0, 0xe1, 0xe3, 0xe5, 0xe7, 0xf1, 0xa2, 0x2e, 0x3c, 0x28, 0x2b, 0x7c, 
  0x26, 0xe9, 0xea, 0xeb, 0xe8, 0xed, 0xee, 0xef, 0xec, 0xdf, 0x21, 0x24, 0x2a, 0x29, 0x3b, 0x5e, 
  0x2d, 0x2f, 0xc2, 0xc4, 0xc0, 0xc1, 0xc3, 0xc5, 0xc7, 0xd1, 0xa6, 0x2c, 0x25, 0x5f, 0x3e, 0x3f, 
  0xf8, 0xc9, 0xca, 0xcb, 0xc8, 0xcd, 0xce, 0xcf, 0xcc, 0x60, 0x3a, 0x23, 0x40, 0x27, 0x3d, 0x22, 
  0xd8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xab, 0xbb, 0xf0, 0xfd, 0xfe, 0xb1, 
  0xb0, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0xaa, 0xba, 0xe6, 0xb8, 0xc6, 0xa4, 
  0xb5, 0x7e, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0xa1, 0xbf, 0xd0, 0x5b, 0xde, 0xae, 
  0xac, 0xa3, 0xa5, 0xb7, 0xa9, 0xa7, 0xb6, 0xbc, 0xbd, 0xbe, 0xdd, 0xa8, 0xaf, 0x5d, 0xb4, 0xd7, 
  0x7b, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0xad, 0xf4, 0xf6, 0xf2, 0xf3, 0xf5, 
  0x7d, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0xb9, 0xfb, 0xfc, 0xf9, 0xfa, 0xff, 
  0x5c, 0xf7, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0xb2, 0xd4, 0xd6, 0xd2, 0xd3, 0xd5, 
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0xb3, 0xdb, 0xdc, 0xd9, 0xda, 0x9f
};

bool testCharProp(int ccsid, int c, int prop){
  switch(ccsid){
  case IBM1047:
    return (charProps[cp1047to1208[c]] & prop) != 0;
  case UTF8:
    return (charProps[c] & prop) != 0;
  default:
    printf("*** PANIC *** Unsupported CCSID = %d\n",ccsid);
    return false;
  }
}

#define MAX_DEPTH 200


typedef struct SavedState_tag {
  GRule *rule;
  int    index;
  int    pos;
  GRule *parent;
  int    indexInParent;
  /* AST building */
  SyntaxNode *node;
  int    nodeChildCount;

} SavedState;

static void setParseState(SavedState *state,
                          GRule *rule,
                          int index,
                          int pos,
                          GRule *parent,
                          int indexInParent){
  state->rule = rule;
  state->index = index;
  state->pos = pos;
  state->parent = parent;
  state->indexInParent = indexInParent;
}

static int countRuleSet(GRule *rules){
  int count = 0;
  while (rules[count].type != G_END){
    count++;
  }
  return count;
}

static bool matchToken(AbstractTokenizer *tokenizer,
                       char *s, int len,
                       int pos, int *nextPos,
                       int desiredTokenID,
                       int *tokenIDPtr){
  int tokenID = tokenizer->nextToken(tokenizer,s,len,pos,nextPos);
  *tokenIDPtr = tokenID;
  return (tokenID == desiredTokenID);
}

static bool isTokenReference(AbstractTokenizer *tokenizer, int id){
  return ((id == EOF_TOKEN) ||
          (id >= tokenizer->lowTokenID && id <= tokenizer->highTokenID));
}

static GRule *getRule(GRule *ruleSet, int count, int id){
  /* printf("get rule %d\n",id); */
  fflush(stdout);
  if (id < FIRST_GRULE_ID){
    printf("rule id %d lower than min %d\n",id,FIRST_GRULE_ID);
    return NULL;
  }
  for (int i=0; i<count; i++){
    /* printf("want id=%d vs %d of 0x%p\n",id,ruleSet[i].id,&ruleSet[i]); */
    if (ruleSet[i].id == id){
      GRule *rule = &ruleSet[i];
      /* printf("found rule 0x%p\n",rule); */
      return rule;
    }
  }
  printf("undefined rule ID = %d\n",id);
  return NULL;
}


/* This makes a doubly-linked tree (like the HTML Node Tree)
   out of the flag GRuleSpec array.   And counts and verifies a few 
   things.
 */

static bool setRuleRefArray(ShortLivedHeap *slh,
                            AbstractTokenizer *tokenizer,
                            GRule *rule,
                            GRule *ruleSet,
                            int    ruleCount,
                            int   *indices){
  rule->refs = (GRuleRef*)SLHAlloc(slh,rule->subCount*sizeof(GRuleRef));
  for (int i=0; i<rule->subCount; i++){
    int index = indices[i];
    /* printf("setRRA i=%d\n",i); */
    GRuleRef *ref = &rule->refs[i];
    ref->isTokenRef = isTokenReference(tokenizer,index);
    if (ref->isTokenRef){
      ref->tokenID = index;
    } else {
      GRule *referent = getRule(ruleSet,ruleCount,index);
      if (referent == NULL){
        return false;
      }
      ref->parent = rule;
      ref->rule = referent;
      ref->indexInParent = index;
    }
  }
  return true;
}

static GRule *makeGRules(GRuleSpec *ruleSpecs, AbstractTokenizer *tokenizer, ShortLivedHeap *slh, int *ruleCountPtr){
  int ruleCount = 0;
  while (ruleSpecs[ruleCount].type != G_END){
    ruleCount++;
  }
  *ruleCountPtr = ruleCount;
  GRule *rules = (GRule*)SLHAlloc(slh,ruleCount*sizeof(GRule));
  int r;
  for (r=0; r<ruleCount; r++){
    GRuleSpec *spec = &ruleSpecs[r];
    GRule *rule = &rules[r];
    rule->type = spec->type;
    rule->id = spec->id;
    /* printf("rule at 0x%p with id=%d\n",rule,rule->id); */
  }
  for (r=0; r<ruleCount; r++){
    GRuleSpec *spec = &ruleSpecs[r];
    GRule *rule = &rules[r];
    switch (rule->type){
    case G_SEQ:
      {
        int subCount = 0;
        while (spec->sequence.subs[subCount] != G_END) subCount++;
        rule->subCount = subCount;
        if (!setRuleRefArray(slh,tokenizer,rule,rules,ruleCount,&spec->sequence.subs[0])){
          return NULL;
        }
      }
      break;
    case G_ALT:
      {
        int subCount = 0;
        while (spec->alternates.subs[subCount] != G_END) subCount++;
        rule->subCount = subCount;
        if (!setRuleRefArray(slh,tokenizer,rule,rules,ruleCount,&spec->alternates.subs[0])){
          return NULL;
        }
      }
      break;
    case G_STAR:
      {
        int subCount = 1;
        rule->subCount = subCount;
        if (!setRuleRefArray(slh,tokenizer,rule,rules,ruleCount,&spec->star.sub)){
          return NULL;
        }
      }
      break;
    default:
      printf("*** PANIC unhandled rule type in compilation %d\n",rule->type);
    }
  }
  return rules;
}

/*

  HERE
  0) specs could be arrays of strings for simpler parsing
  maybe
  0.5) factor to parse test and jqtest
  0.6) separate out the JLExer
  1) write microJQ grammar 
  2) start pushing builder operations corresponding to loops, trees
  3) tree construction comes from temporarily associating text ranges and token ID's with this node tree
  can tree construction go direct to json
  can all steps be a set of JSON Build calls that are kept in a list,
  and then backtracked to on earlier places in list, and then finally played forward to make the JSON Tree
  4) JSON program can be written to take this output and test it in Node.js
  5) bring it into QuickJS as evaluator
  6) or just interpret it straight up
  7) Trace improvements:
     backtrack uniqueID and establishment trace
     backtrack printf enhance to be specific


     build steps
    
    SEQ 
      kv pairs - (uniquified names of subs -> json)
   
    ALT 
      {
         indicator:  <nameOfToken>
         value:      <json>
      }
  
    STAR
      [ <JSON> ]

    TOKEN 
      jsonValuizer 
        makeString ( various escaping rules, like handling \r \u, etc )
        makeInteger
        makeBoolean
        makeNull
      but can make anything!!
        

*/

typedef struct GContinuation_tag {
  struct GContinuation_tag *previous;
#ifdef METTLE
#error "need setjmp"
#else
  jmp_buf     resumeData;
#endif
  int indexInRule;
  int position;
  int depth;
  int buildStepHWM;
} GContinuation;

static char *getBuildStepTypeName(int type){
  switch (type){
  case BUILD_OBJECT: return "Object";
  case BUILD_ARRAY: return "Array";
  case BUILD_STRING: return "String";
  case BUILD_INT: return "int";
  case BUILD_INT64: return "int64";
  case BUILD_DOUBLE: return "double";
  case BUILD_BOOL: return "boolean";
  case BUILD_NULL: return "null";
  case BUILD_KEY: return "key";
  case BUILD_POP: return "pop()";
  case BUILD_ALT: return "altWrap()";
  default: return "UnknownBuildStep";
  }

}

#define GPARSE_SUCCESS(x) ((x)>=0)
#define GPARSE_FAIL (-1)
#define setResumePoint(cPtr) setjmp((cPtr)->resumeData)

static void setStepRef(GBuildStep *step, GRuleRef *ref){
  step->tokenID = (ref->isTokenRef ? ref->tokenID : NO_VALID_TOKEN);
  step->rule = ref->rule;
}

static void setAltChoice(GBuildStep *step, GRuleRef *ref){
  /* printf("setAltChoice isTokenRef=%d, tokenID=%d\n",ref->isTokenRef,ref->tokenID); */
  step->altChoice = ref;
}

static GBuildStep *makeBuildStep(GParseContext *ctx, int type, GRuleRef *ref, GRule *rule){
  if (ctx->buildStepCount >= ctx->buildStepsSize){
    printf("**** PANIC **** out of build steps\n");
    printf("**** PANIC **** out of build steps\n");
    printf("**** PANIC **** out of build steps\n");
    printf("**** PANIC **** out of build steps\n");
    return NULL;
  }
  GBuildStep *step = &ctx->buildSteps[ctx->buildStepCount++];
  memset(step,0,sizeof(GBuildStep));
  step->type = type;
  if (ref){
    setStepRef(step,ref);
  } else if (rule){
    step->tokenID = NO_VALID_TOKEN;
    step->rule = rule;
  } else {
    step->tokenID = NO_VALID_TOKEN;
    step->rule = NULL;
  }
  step->valueStart = STEP_UNDEFINED_POSITION;
  step->valueEnd = STEP_UNDEFINED_POSITION;
  return step;
}

static void buildTokenValue(GParseContext *ctx, GRuleRef *ref, int lastTokenStart, int lastTokenEnd){
  GBuildStep *step = makeBuildStep(ctx,BUILD_STRING,ref,NULL);
  step->valueStart = lastTokenStart;
  step->valueEnd = lastTokenEnd;
}

static void buildSeqElement(GParseContext *ctx, GRuleRef *ref){
  makeBuildStep(ctx,BUILD_KEY,ref,NULL);
}

static GBuildStep *buildAltValue(GParseContext *ctx, GRule *rule){
  return makeBuildStep(ctx,BUILD_ALT,NULL,rule);
}

static void buildObject(GParseContext *ctx, GRule *rule){
  makeBuildStep(ctx,BUILD_OBJECT,NULL,rule);
}

static void buildArray(GParseContext *ctx, GRule *rule){
  makeBuildStep(ctx,BUILD_ARRAY,NULL,rule);
}

static void buildPop(GParseContext *ctx){
  makeBuildStep(ctx,BUILD_POP,NULL,NULL);
}

static void indent(int depth){
  for (int i=0; i<depth; i++) printf("  ");
}

static int parseDispatch(GParseContext *ctx, GRule *rule, int startPos, int depth, GContinuation *resumePoint);

static void backtrack(GParseContext *ctx, GContinuation *continuation){
  printf("-------> Backtracking... build step back from %d to %d\n",
         ctx->buildStepCount,
         continuation->buildStepHWM);
  fflush(stdout);
  longjmp(continuation->resumeData,1);
}

static int runTokenMatch(GParseContext *ctx, GRuleRef *ref, int pos, int depth){
  AbstractTokenizer *tokenizer = ctx->tokenizer;
  int nextPos = 0;
  int tokenID = 0;
  indent(depth);
  printf("runTokenMatch trying id=0x%x, (%s) pos=%d\n",ref->tokenID,tokenizer->getTokenIDName(ref->tokenID),pos);
  fflush(stdout);
  bool matched = matchToken(tokenizer,ctx->s,ctx->len,pos,&nextPos,ref->tokenID,&tokenID);
  if (matched){
    buildTokenValue(ctx,ref,tokenizer->lastTokenStart,tokenizer->lastTokenEnd);
    return nextPos;
  } else if (tokenID == NO_VALID_TOKEN){
    printf("*** Tokenizer failed, parse must stop\n");
    printf("*** Tokenizer failed, parse must stop\n");
    printf("*** Tokenizer failed, parse must stop\n");
    printf("*** Tokenizer failed, parse must stop\n");
    /* should throw to outer error handler */
    return 0x7FFFFFFF;
  } else {
    return GPARSE_FAIL;
  }
}

static char *ruleName(GParseContext *ctx, GRule *rule){
  return ctx->ruleNamer(rule->id);
}

static int runSeq(GParseContext *ctx, GRule *rule, int startPos, int depth, GContinuation *resumePoint){
  indent(depth);
  printf("runSeq (%s) at pos=%d, resume=0x%p\n",ruleName(ctx,rule),startPos,resumePoint);
  fflush(stdout);
  int pos = startPos;
  for (int index=0; index<rule->subCount; index++){
    GRuleRef *ref = &rule->refs[index];
    buildSeqElement(ctx,ref);
    int matchResult =
      (ref->isTokenRef ?
       runTokenMatch(ctx,ref,pos,depth) :
       parseDispatch(ctx,ref->rule,pos,depth,resumePoint));
    indent(depth+1);
    printf("seq submatch index=%d result=%d\n",index,matchResult);
    if (GPARSE_SUCCESS(matchResult)){
      pos = matchResult;
    } else if (resumePoint){
      backtrack(ctx,resumePoint);
    } else {
      return GPARSE_FAIL;
    }
  }
  return pos;
}

static int runAlt(GParseContext *ctx, GRule *rule, int startPos, int depth, GContinuation *resumePoint){
  indent(depth);
  printf("runAlt (%s) at pos=%d, resume=0x%p\n",ruleName(ctx,rule),startPos,resumePoint);
  fflush(stdout);
  GBuildStep *altStep = buildAltValue(ctx,rule);
  GContinuation continuation;
  continuation.previous = resumePoint;
  continuation.indexInRule = 0;
  continuation.position = startPos;
  continuation.depth = depth;
  continuation.buildStepHWM = ctx->buildStepCount;
  int isResumption = setResumePoint(&continuation);
  int index = (isResumption ? continuation.indexInRule : 0);
  int pos = (isResumption ? continuation.position : startPos);
  ctx->buildStepCount = continuation.buildStepHWM;
  for (; index<rule->subCount; index++){
    GRuleRef *ref = &rule->refs[index];
    setAltChoice(altStep,ref);
    continuation.indexInRule = index+1;
    if (ref->isTokenRef){
      int matchResult = runTokenMatch(ctx,ref,pos,depth);
      if (GPARSE_SUCCESS(matchResult)){
        indent(depth+1);
        printf("alt token success matchResult=%d\n",matchResult);
        return matchResult;
      } else if (index+1 < rule->subCount){
        // just roll forward
        indent(depth+1);
        printf("alt token roll forward\n");
      } else if (resumePoint){
        indent(depth+1);
        printf("alt token backtrack\n");
        backtrack(ctx,resumePoint);
      } else {
        indent(depth+1);
        printf("alt token fail\n");
        return GPARSE_FAIL;
      }
    } else {
      GContinuation *failContinuation = ((index + 1 < rule->subCount) ?
                                         &continuation :
                                         resumePoint);
      int parseResult = parseDispatch(ctx,ref->rule,pos,depth,failContinuation);
      indent(depth+1);
      printf("alt subrule res = %d\n",parseResult);
      return parseResult; // we never loop except when hitting a token 
    }
  }
  indent(depth+1);
  printf("out of ALT's\n");
  return GPARSE_FAIL;
}

// can easily by plus or repeat, or opt
static int runStar(GParseContext *ctx, GRule *rule, int startPos, int depth, GContinuation *resumePoint){
  indent(depth);
  printf("runStar (%s) at pos=%d, resume=0x%p\n",ruleName(ctx,rule),startPos,resumePoint);
  fflush(stdout);
  GContinuation continuation;
  continuation.previous = resumePoint;
  continuation.indexInRule = 0;
  continuation.position = startPos;
  continuation.depth = depth;
  continuation.buildStepHWM = ctx->buildStepCount;
  GRuleRef *ref = &rule->refs[0];
  int isResumption = setResumePoint(&continuation);
  if (isResumption){
    ctx->buildStepCount = continuation.buildStepHWM;
    return continuation.position; // still always a success
  } else {
    /* this is a "greedy" star */
    while (true){
      int matchResult =
        (ref->isTokenRef ?
         runTokenMatch(ctx,ref,continuation.position,depth) :
         parseDispatch(ctx,ref->rule,continuation.position,depth,&continuation));
      if (GPARSE_SUCCESS(matchResult)){
        continuation.position = matchResult;
        continuation.buildStepHWM = ctx->buildStepCount;
        continuation.indexInRule++;
        // keep looping
      } else {
        return continuation.position; // it's always a success, even if zero matched
      }
    }
  }
}

static int parseDispatch(GParseContext *ctx, GRule *rule, int startPos, int depth, GContinuation *resumePoint){
  switch (rule->type){
  case G_SEQ:
    {
      buildObject(ctx,rule);
      int res = runSeq(ctx,rule,startPos,depth+1,resumePoint);
      buildPop(ctx);
      return res;
    }
  case G_ALT:
    {
      int res = runAlt(ctx,rule,startPos,depth+1,resumePoint);
      buildPop(ctx);
      return res;
    }
  case G_STAR:
    {
      buildArray(ctx,rule);
      int res = runStar(ctx,rule,startPos,depth+1,resumePoint);
      buildPop(ctx);
      return res;
    }
  default:
    printf("*** PANIC *** Unhandled ruleType = %d\n",rule->type);
    return GPARSE_FAIL; /* should bounce to "total failure setjmp" */
  }
}

void showBuildSteps(GParseContext *ctx){
  for (int i=0; i<ctx->buildStepCount; i++){
    GBuildStep *step = &ctx->buildSteps[i];
    if (step->type == BUILD_ALT){
      bool isToken = (step->tokenID != NO_VALID_TOKEN);
      int markingID = (step->tokenID != NO_VALID_TOKEN) ? step->tokenID : step->rule->id;
      printf("Step(%d): ALT choice byToken=%d id=%d\n",i,isToken,markingID);
    } else if (step->tokenID != NO_VALID_TOKEN){
      printf("Step(%d) %s: token %s text from %d to %d\n",
             i,
             getBuildStepTypeName(step->type),
             ctx->tokenizer->getTokenIDName(step->tokenID),
             step->valueStart,step->valueEnd);
    } else if (step->rule){
      printf("Step(%d) %s: %s\n",i,getBuildStepTypeName(step->type),ctx->ruleNamer(step->rule->id));
    } else {
      printf("Step(%d) %s\n",i,getBuildStepTypeName(step->type));
    }
  }
}

Json *gBuildJSON(GParseContext *ctx, ShortLivedHeap *slh){
  if (slh == NULL){
    slh = ctx->slh;
  }
  JsonBuilder *b = makeJsonBuilder(slh);
  int sp = 0;
  Json **parentStack = (Json**)SLHAlloc(ctx->slh,MAX_DEPTH*sizeof(Json*));
  Json *currentParent = NULL;
  char *currentKey = NULL;
  for (int i=0; i<ctx->buildStepCount; i++){
    printf("build step %d\n",i);
    GBuildStep *step = &ctx->buildSteps[i];
    int errorCode = 0;
    switch (step->type){
    case BUILD_OBJECT:
      {
        Json *o = jsonBuildObject(b,currentParent,currentKey,&errorCode);
        parentStack[sp++] = currentParent;
        currentParent = o;
         currentKey = NULL;
      }
      break;
    case BUILD_ARRAY:
      {
        Json *a = jsonBuildArray(b,currentParent,currentKey,&errorCode);
        parentStack[sp++] = currentParent;
        currentParent = a;
        currentKey = NULL;
      }
      break;
    case BUILD_POP:
      {
        currentParent = parentStack[--sp];
        currentKey = NULL;
      }
      break;
    case BUILD_KEY:
      {
        /* unless EOF */
        switch (step->tokenID){
        case NO_VALID_TOKEN:
          currentKey = ctx->ruleNamer(step->rule->id);
          break;
        case EOF_TOKEN:
          currentKey = "<EOF>";
          break;
        default:
          ctx->tokenizer->getTokenIDName(step->tokenID);
          break;
        }
      }
      break;
    case BUILD_ALT:
      {
        Json *alt = jsonBuildObject(b,currentParent,currentKey,&errorCode);
        parentStack[sp++] = currentParent;
        currentParent = alt;
        GRuleRef *choiceRef = step->altChoice;
        int choiceID = (choiceRef->isTokenRef ? choiceRef->tokenID : choiceRef->rule->id);
        jsonBuildInt(b,alt,"altID",choiceID,&errorCode);
        currentKey = "value";
       }
      break;
    case BUILD_STRING:
      if (currentKey == NULL){
        printf("no parent for string!!!\n");
      } else {
        int len = step->valueEnd - step->valueStart;
        jsonBuildString(b,currentParent,currentKey,ctx->s+step->valueStart,len,&errorCode);
        currentKey = NULL; /* because was consumed */
      }
      break;
    case BUILD_INT64:
    case BUILD_INT:
    case BUILD_DOUBLE:
    case BUILD_BOOL:
    case BUILD_NULL:
      printf("unhandled JSON value type for build type %d\n",step->type);
      break;
    default:
      printf("*** PANIC *** unhandled JSON builder step type %d\n",step->type);
      break;
    }
    /*
       } else if (step->type 
       } else if (step->tokenID != NO_VALID_TOKEN){
       } else if (step->rule){
       printf("Step %s: %s\n",getBuildStepTypeName(step->type),ctx->ruleNamer(step->rule->id));
       } else {
      printf("Step %s\n",getBuildStepTypeName(step->type));
      }
     */
    if (errorCode){
      printf("last build step failed with code = %d\n",errorCode);
    }
  }
  printf("final sp = %d\n",sp);
  Json *result = b->root;
  freeJsonBuilder(b,false);
  return result;
}

GParseContext *gParse(GRuleSpec *ruleSet, int topRuleID, AbstractTokenizer *tokenizer, char *s, int len,
                     char *(*ruleNamer)(int id)){
  ShortLivedHeap *slh = makeShortLivedHeap(0x10000, 100);
  GParseContext *ctx = (GParseContext *)SLHAlloc(slh,sizeof(GParseContext));
  memset(ctx,0,sizeof(GParseContext));
  ctx->tokenizer = tokenizer;
  ctx->s = s;
  ctx->len = len;
  ctx->ruleNamer = ruleNamer;
  ctx->slh = slh;
  ctx->buildStepsSize = MAX_BUILD_STEPS;
  ctx->buildSteps = (GBuildStep*)SLHAlloc(slh,ctx->buildStepsSize*sizeof(GBuildStep));
  int ruleSetSize = 0;
  GRule *rules = makeGRules(ruleSet,tokenizer,slh,&ruleSetSize);
  ctx->rules = rules;
  GRule *topRule = getRule(rules,ruleSetSize,topRuleID);
  if (topRule->type == G_SEQ){
    int parseResult = parseDispatch(ctx,topRule,0,0,NULL);
    printf("parseResult=%d\n",parseResult);
    ctx->status = parseResult;
    showBuildSteps(ctx);
    return ctx;
  } else {
    printf("top rule must be G_SEQ\n");
    ctx->status = -1;
    return ctx;
  }
}
