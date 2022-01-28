

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
#include <metal/string.h>
#include <metal/stdlib.h>
#include <metal/stdarg.h>  
#include "metalio.h"

#else

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>  

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "collections.h"
#include "textmatch.h"

/* 
    c89 -DNOIBMHTTP=1 -D_EXT '-Wc,langlvl(extc99),gonum' -I ../h -o textmatch textmatch.c collections.c utils.c alloc.c

    HERE
    0.2) general char sets
    0.3) negation in char classes and PERL abbrevs
    0.4) more funny PERL chars and the "." operator
    4) stops/lookaheads, binds, longest match, etc
    4.1) stream vs. array
    4.1) partial vs. whole vs. anchored matches.
    4.2) switching the charset other ebcdics, acsii, or UTF8
    5) general quicksort, mergesort
    6) testing matcher
    7) minimization
    8) re-testing
   10) page/line predicates.
   11) worry about carriage control
   11.1) can the "$" operator be a sloppy newline that accounts for machine/ANSI/none CC modes

   Example Patterns for QA

     USA-SOCIAL-SECURITY = "\d{3}( |-)?\d{2}( |-)?\d{4}"
     USA_CANADA-PHONENUM = "((\(\d{3}\))|\d{3}) ?\d{3}( |-)?\d{4}"
     USA-DATE-WORDS      = "(Jan(uary)?|Feb(uary)?|Mar(ch)?|Apr(il)?|May|Jun(e)?|Jul(y)?|Aug(ust)),? ([0-3]\d),? \d{4}"
     


   should matcher be a VARCHAR or a CLOB, is there a varchar with >2 byte length header?

   CHARSET CODE PAGE:  Default 037, 1140+euro 1047

   MATCH IN PAGE(*|s,*) IN LINE(*,*|111) <perl-pattern>

   VALUE match, date ranges, currency amounts.

   TEMPLATE Variables "&C~" , also JOBNAME, JOBID

            Pattern    "&1~"

   are template fixed width, makes sense for JCL,
   do we do substitution exactly 
     & is blank in banner, so "&C  ~" is a 4 char subsitution.

   WTO limited to 10 lines, what's the line separator?
     line types 
     single-line 126,
     multi  34 or 70
     type C,D,E (see WTO in assember services manual)

   ACTION 
     (WTO|MVSCOMMAND|JCL|SAVECONDITION|HIGHLIGHT|INDEX(ha?|SMF|EMAIL|TWEET))
     - additional args per type
         WTO route code.
	     descriptor code
             line types???
     - JOB cards.
     - PROCS,
     - other stuff 
  
   PARMs 
     max ACTIONs per page/per sysout
     
   IMPERSONATION, can the job specify USER=

 */

#define ESCAPE '\\'

#define TEXT_EXPR_SEQ    1
#define TEXT_EXPR_ALT    2
#define TEXT_EXPR_CHAR   3
#define TEXT_EXPR_STAR   4
#define TEXT_EXPR_PLUS   5
#define TEXT_EXPR_QMARK  6
#define TEXT_EXPR_BIND   7
#define TEXT_EXPR_PREDEFINED_CLASS 8
#define TEXT_EXPR_REPETITION 9

#define UNLIMITED_REPETITION -1

#define TEXT_CHARSET_037           0x0037
#define TEXT_CHARSET_500           0x0500
#define TEXT_CHARSET_1047          0x1047
#define TEXT_CHARSET_ISO_8859_1  0x885901
#define TEXT_CHARSET_BINARY    0x01010101
#define TEXT_CHARSET_ASCII        0xA5CII
#define TEXT_CHARSET_UTF8         0x00008
#define TEXT_CHARSET_UTF16        0x00016

typedef struct TextExpr_tag{
  int type;
  /* matching arguments */
  char c;
  int predefinedClass;
  char *data;
  int minValue;
  int maxValue;       /* -1 for open interval on right */
  char *bindingName;  /* bind name or null */
  int bindingNumber;
  int terminalStateNumber;
  struct TextExpr_tag *firstChild;
  struct TextExpr_tag *nextSibling;
} TextExpr;

static TextExpr* getNthSubExpression(TextExpr *expr, int n){
  TextExpr *child = expr->firstChild;
  if (n == 0){
    return child;
  } else{
    int i;
    for (i=0; child && (i<n); i++){
      child = child->nextSibling;
    }
    return child;
  }
}

static void setTerminalStateNumber(TextExpr *expr, int stateNumber){
  expr->terminalStateNumber = stateNumber;
  TextExpr *child = expr->firstChild;
  while (child){
    setTerminalStateNumber(child,stateNumber);
    child = child->nextSibling;
  }
}

static int textExprEquals(TextExpr *a, TextExpr *b){
  if (a == NULL || b == NULL){
    return FALSE;
  }
  if (a->type == b->type){
    switch (a->type){
    case TEXT_EXPR_REPETITION:
      if ((a->minValue != b->minValue) ||
	  (a->maxValue != b->maxValue)){
	return FALSE;
      }                    /* SWITCH/CASE fall-through by design */
    case TEXT_EXPR_SEQ:
    case TEXT_EXPR_ALT:
    case TEXT_EXPR_STAR:
    case TEXT_EXPR_PLUS:
    case TEXT_EXPR_QMARK:
      {
	TextExpr *childA = a->firstChild;
	TextExpr *childB = b->firstChild;
	while (childA){
	  if (childB == NULL){
	    return FALSE;
	  } else{
	    if (!textExprEquals(childA,childB)){
	      return FALSE;
	    }
	  }
	  childA = childA->nextSibling;
	  childB = childB->nextSibling;
	}
	return (childB == NULL);
      }
    case TEXT_EXPR_CHAR:
      return (a->c == b->c);
    case TEXT_EXPR_PREDEFINED_CLASS:
      return ((a->predefinedClass == b->predefinedClass) &&
	      (a->data ? 
	       (b->data ? !strcmp(a->data,b->data) : FALSE) :
	       (b->data ? FALSE : TRUE)));
    case TEXT_EXPR_BIND:
      return !strcmp(a->bindingName,b->bindingName);
    default:
      return FALSE;
    }
  } else{
    return FALSE;
  }
}



static void noteParseError(TextContext *context, char *formatString, ...){
  char *errorBuffer = SLHAlloc(context->slh,512);
  va_list argPointer;
  va_start(argPointer,formatString);
  vsnprintf(errorBuffer,512,formatString,argPointer);
  context->error = errorBuffer;
  va_end(argPointer);
}

#define NDFA_VECTOR_SIZE 2048

/* only for NDFA */
#define TEXT_TEST_EPSILON 1
#define TEXT_TEST_CHAR    2
#define TEXT_TEST_ALPHA             3
#define TEXT_TEST_ALPHALOWER        4
#define TEXT_TEST_ALPHAUPPER        5
#define TEXT_TEST_ALPHANUMERIC      6
#define TEXT_TEST_ALPHANUMERICUPPER 7
#define TEXT_TEST_ALPHANUMERICLOWER 8
#define TEXT_TEST_DIGIT             9
#define TEXT_TEST_HEXDIGIT         10
#define TEXT_TEST_HEXDIGITUPPER    11
#define TEXT_TEST_HEXDIGITLOWER    12
#define TEXT_TEST_BOL              15
#define TEXT_TEST_EOL              16
#define TEXT_TEST_DOT              17
#define TEXT_TEST_WHITESPACE       18
#define TEXT_TEST_BLANK            19
#define TEXT_TEST_WORDCHAR         20
#define TEXT_TEST_CHARSET          21
#define TEXT_TEST_NEGATIVE_CHARSET 22
#define TEXT_TEST_ALPHANUMERIC_OR_NATIONAL 23

static char *textTestDescription(Transition *t, char *buffer){
  switch (t->test){
  case TEXT_TEST_EPSILON:
    sprintf(buffer,"epsilon");
    break;
  case TEXT_TEST_CHAR:
    sprintf(buffer,"== (0x%x) '%c'",t->c,t->c);
    break;
  case TEXT_TEST_ALPHA:
    sprintf(buffer,"isAlpha(c)");
    break;
  case TEXT_TEST_ALPHALOWER:
    sprintf(buffer,"isLower(c)");
    break;
  case TEXT_TEST_ALPHAUPPER:
    sprintf(buffer,"isUpper(c)");
    break;
  case TEXT_TEST_ALPHANUMERIC:
    sprintf(buffer,"isAlphaNumeric(c)");
    break;
  case TEXT_TEST_ALPHANUMERICLOWER:
    sprintf(buffer,"isAlphaNumericLower(c)");
    break;
  case TEXT_TEST_ALPHANUMERICUPPER:
    sprintf(buffer,"isAlphaNumericUpper(c)");
    break;
  case TEXT_TEST_WORDCHAR:
    sprintf(buffer,"isWordChar(c)");
  case TEXT_TEST_HEXDIGIT:
    sprintf(buffer,"isDigit(c)");
    break;
  case TEXT_TEST_HEXDIGITLOWER:
    sprintf(buffer,"isHexLower(c)");
    break;
  case TEXT_TEST_HEXDIGITUPPER:
    sprintf(buffer,"isHexUpper(c)");
    break;
  case TEXT_TEST_DIGIT:
    sprintf(buffer,"isDigit(c)");
    break;
  case TEXT_TEST_BOL:
    sprintf(buffer,"isBOL(c)");
    break;
  case TEXT_TEST_EOL:
    sprintf(buffer,"isEOL(c)");
    break;
  case TEXT_TEST_WHITESPACE:
    sprintf(buffer,"isWhitespace(c)");
    break;
  case TEXT_TEST_BLANK:
    sprintf(buffer,"isBlank(c)");
    break;
  case TEXT_TEST_CHARSET:
    sprintf(buffer,"isInCharset(c,\"%s\")",t->data);
    break;
  case TEXT_TEST_NEGATIVE_CHARSET:
    sprintf(buffer,"isNotInCharset(c,\"%s\")",t->data);
    break;
  case TEXT_TEST_DOT:
    sprintf(buffer,"anyChar(c) (aka '.')");
    break;
  case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
    sprintf(buffer,"isAlphanumericOrNational(c)");
    break;
  default:
    sprintf(buffer,"unknown test %d\n",t->test);
    break;
  }
  return buffer;
}

static int dfaStateHash(void *k){
  DFAState *state = (DFAState*)k;
  int i;
  int hash=0;
  
  for (i=0; i<state->ndfaStateCount; i++){
    hash = (hash<<4) | state->ndfaStates[i];
   
  }
  return (hash & 0x7fffffff);
}

static int dfaStateCompare(void *k1, void *k2){
  DFAState *state1 = (DFAState*)k1;
  DFAState *state2 = (DFAState*)k2;

  if (state1->ndfaStateCount != state2->ndfaStateCount){
    return 0;
  } else{
    int i;
    int len = state1->ndfaStateCount;
    for (i=0; i<len; i++){
      if (state1->ndfaStates[i] != state2->ndfaStates[i]){
	return 0;
      }
    }
    return 1;
  }
}

static void addDFAState(DFA *dfa, DFAState *state){
  if (dfa->stateCount == dfa->stateVectorSize){
    int newSize = 2 * dfa->stateVectorSize;
    DFAState **newVector = (DFAState**)safeMalloc(sizeof(DFAState*)*newSize,"DFA");
    memcpy(newVector,dfa->states,newSize/2*sizeof(DFAState*));
    dfa->stateVectorSize = newSize;
    safeFree((char*)dfa->states,newSize/2*sizeof(DFAState*));
    dfa->states = newVector;
  }
  dfa->states[dfa->stateCount++] = state;
}

static int nextChar(TextContext *context, int end, int *escaped, char *caller){
  char *s = context->s;
  context->lastPos = context->pos;
  if (context->pos == end){
    if (context->parseTrace){
      printf("nextChar(from %s) pos=%d: EOF\n",caller,context->pos);
    }
    return -1;
  }
  int c = s[context->pos++];
  if (c == ESCAPE){
    if (context->pos == end){
      if (context->parseTrace){
	printf("nextChar(from %s) pos=%d, end=%d: EOF after escape\n",caller,context->pos,end);
      }
      return -1;
    } else{
      *escaped = TRUE;
      c = s[context->pos++];
    }
  }
  if (context->parseTrace){
    printf("nextChar(from %s) pos=%d: c=%c\n",caller,context->pos,c);
  }
  return c;
}

static void unreadLastChar(TextContext *context){
  context->pos = context->lastPos;
}

static TextExpr *makeTextExpr(TextContext *context, int type){
  TextExpr *expr = (TextExpr*)SLHAlloc(context->slh,sizeof(TextExpr));
  memset(expr,0,sizeof(TextExpr));
  expr->type = type;
  return expr;
}

static TextExpr *nthTailExpr(TextContext *context, TextExpr *expr, int n){
  TextExpr *tailStart = getNthSubExpression(expr,n);
  TextExpr *newExpr = makeTextExpr(context,expr->type);
  newExpr->firstChild = tailStart;
  return newExpr;
}

static void addChild(TextExpr *parent, TextExpr *newChild){
  if (parent->firstChild == NULL){
    parent->firstChild = newChild;
  } else{
    TextExpr *child = parent->firstChild;
    while (child->nextSibling){
      child = child->nextSibling;
    }
    child->nextSibling = newChild;
  }
}

static void wrapLastChild(TextExpr *parent, TextExpr *wrapper){
  TextExpr *child = parent->firstChild;
  if (parent->firstChild->nextSibling){
    TextExpr *penultimate = NULL;
    while (child->nextSibling){
      penultimate = child;
      child = child->nextSibling;
    }
    wrapper->firstChild = child;
    penultimate->nextSibling = wrapper;
  } else{
    wrapper->firstChild = parent->firstChild;
    parent->firstChild = wrapper;
  }
}

static TextExpr *shallowCloneTextExpr(TextContext *context, TextExpr *expr){
  TextExpr *clone = makeTextExpr(context,expr->type);
  clone->c = expr->c;
  clone->data = expr->data;
  clone->firstChild = expr->firstChild;
  clone->predefinedClass = expr->predefinedClass;
  clone->bindingName= expr->bindingName;
  return clone;
}

static TextExpr *getLastChild(TextExpr *parent){
  TextExpr *child = parent->firstChild;
  while (child->nextSibling){
    child = child->nextSibling;
  }
  return child;
}

static NDFAState *makeNDFAState(TextContext *context, int flags){
  NDFAState *state = (NDFAState*)SLHAlloc(context->slh,sizeof(NDFAState));
  memset(state,0,sizeof(NDFAState));
  state->index = context->highestNDFAIndex++;
  state->flags = flags;
  if (flags & FA_TERMINAL){
    state->terminalIndex = context->highestTerminalIndex++;
  }
  context->ndfaStates[state->index] = state;
  // printf ("End of make, State[1]=0x%x, State[1]->first=0x%x\n", context->ndfaStates[1], context->ndfaStates[1]->first);
  return state;
}

static Transition *addNDFATransition(TextContext *context, NDFAState *from, NDFAState *to, int test, char *data){
  Transition *t = (Transition*)SLHAlloc(context->slh,sizeof(Transition));
  memset(t,0,sizeof(Transition));
  memcpy(t->eyecatcher,"TRAN",4);
  t->test = test;
  t->data = data;
  t->nextState = to;
  if (from->first){
    Transition *child = from->first;
    while (child->sibling){
      child = child->sibling;
    }
    child->sibling = t;
  } else{
    from->first = t;
  } 
  return t;
}

static void epsilonLink(TextContext *context, NDFAState *from, NDFAState *to){
  addNDFATransition(context,from,to,TEXT_TEST_EPSILON,NULL);
}

static void charLink(TextContext *context, NDFAState *from, NDFAState *to, char c){
  if (c == 0){
    if (context->compileTrace){
    printf("ALARM: charLink char=0\n");
  }
  }
  Transition *t = addNDFATransition(context,from,to,TEXT_TEST_CHAR,NULL);
  t->c = c;
}

static void predefinedClassLink(TextContext *context, NDFAState *from, NDFAState *to, int testType){
  addNDFATransition(context,from,to,testType,NULL);
}

/* when walking and expression, what are the true parent sequences

   if PARENT=NULL
        SEQ->SELF
        ALT->NULL
        SPQ->NULL
        BND->NULL
        CHR-><n/a>

      PARENT=SOME-SEQ
        SEQ->PARENT
        ALT->NULL
        SPQ->NULL
        BND->PARENT
        CHR->n/a

        */

static void buildNDFA(TextContext *context, TextExpr *expr, NDFAState *entryState, NDFAState *exitState,
                      TextExpr *parentSequence){
  TextExpr *child;
  NDFAState *prev;
  NDFAState *next;
  switch (expr->type){
  case TEXT_EXPR_SEQ:
    prev = entryState;
    for (child=expr->firstChild; child; child=child->nextSibling){
      next = (child->nextSibling ? makeNDFAState(context,0) : exitState);
      buildNDFA(context,child,prev,next,(parentSequence ? parentSequence : expr));
      prev = next;
    }
    break;
  case TEXT_EXPR_ALT:
    for (child=expr->firstChild; child; child=child->nextSibling){
      prev = makeNDFAState(context,0);
      epsilonLink(context,entryState,prev);
      buildNDFA(context,child,prev,exitState,NULL);
    }
    break;
  case TEXT_EXPR_STAR:
  case TEXT_EXPR_PLUS:
  case TEXT_EXPR_QMARK:
    prev = makeNDFAState(context,0);
    next = makeNDFAState(context,0);
    epsilonLink(context,entryState,prev);
    epsilonLink(context,next,exitState);
    int bindingIndexBefore = context->highestBindingIndex;
    buildNDFA(context,expr->firstChild,prev,next,NULL);
    if (context->compileTrace){
      printf("NDFA state %d should clear bindings >= %d\n",prev->index,bindingIndexBefore);
    }
    prev->bindingClearFrom = bindingIndexBefore;
    /* '?' and '+' operators are degenerate cases of the graph for '*' */
    if (expr->type != TEXT_EXPR_QMARK){
      epsilonLink(context,next,prev);            /* the back link */
    }
    if (expr->type != TEXT_EXPR_PLUS){
      epsilonLink(context,entryState,exitState); /* the 0 occurrence link link */
    }
    break;  
  case TEXT_EXPR_BIND:
    prev = makeNDFAState(context,0);
    next = makeNDFAState(context,0);
    prev->bindingStart = context->highestBindingIndex;
    int minimumBindingLength = computeMinimumBindingLength(expr->firstChild);
    if (context->compileTrace){
      printf("bind (%d) expr min Length=%d\n",context->highestBindingIndex,minimumBindingLength);
    }
    next->bindingEnd = context->highestBindingIndex++;
    context->minimumBindingLengths[prev->bindingStart] = minimumBindingLength; 
    epsilonLink(context,entryState,prev);
    epsilonLink(context,next,exitState);
    buildNDFA(context,expr->firstChild,prev,next,parentSequence);
    break;
  case TEXT_EXPR_CHAR:
    charLink(context,entryState,exitState,expr->c);
    break;
  case TEXT_EXPR_PREDEFINED_CLASS:
    predefinedClassLink(context,entryState,exitState,expr->predefinedClass);
    break;
  default:
    printf("panic unsupported text expr in expr 0x%x\n",expr);
  }
}

/* the beginnings of a binding measurement thing */
 
static int computeMinimumBindingLength(TextExpr *expr){
  switch (expr->type){
  case TEXT_EXPR_SEQ:
    {
      int count = 0;
      int i;
      TextExpr *child = expr->firstChild;
      while (child){
        count += computeMinimumBindingLength(child);
        child = child->nextSibling;
      }
      return count;
    }
  case TEXT_EXPR_CHAR:
  case TEXT_EXPR_PREDEFINED_CLASS:
    return 1;
  case TEXT_EXPR_ALT:
    /* max of above */
    { 
      int minLength = 0x7FFFFFFF;
      TextExpr *child = expr->firstChild;
      while (child){
        int childLength = computeMinimumBindingLength(child);
        if (childLength < minLength){
          minLength = childLength;
        }
        child = child->nextSibling;
      }
      return minLength;
    }
  case TEXT_EXPR_PLUS:
    return computeMinimumBindingLength(expr->firstChild);
  default: /* QMARK, STAR */
    return 0;
  } 
}

static int countBindings(TextExpr *expr){
  switch (expr->type){ 
  case TEXT_EXPR_SEQ:
  case TEXT_EXPR_ALT:
    {
      int count = 0;
      int i;
      TextExpr *child = expr->firstChild;
      while (child){
        count += countBindings(child);
        child = child->nextSibling;
      }
      return count;
    }
  case TEXT_EXPR_CHAR:
  case TEXT_EXPR_PREDEFINED_CLASS:
    return 0;
  case TEXT_EXPR_BIND:
    return 1+countBindings(expr->firstChild);
  case TEXT_EXPR_PLUS:
  case TEXT_EXPR_QMARK:
  case TEXT_EXPR_STAR:
    return countBindings(expr->firstChild);
  default:
    // printf("**** Unexpected Text Expr seen %d ****\n",expr->type);
    return 0;
  } 
}

/*
  Aho and Ullman use specialized ndfa: only single non-epsilon transition per state.
  
  0     1     2     3
   a->0  b->2  b->3  FINAL
   a->1
   b->0

   ambiguity closure

   

 */

static int intSetContains(IntListMember *set, int x){
  IntListMember *member = set;
  while (member){
    if (member->x == x){
      return TRUE;
    }
    member = member->next;
  }
  return FALSE;
}

static IntListMember *addToIntSet(TextContext *context, IntListMember *set, int x, 
				  int isPermanent){
  IntListMember *newMember = (IntListMember*)(isPermanent ? 
					      safeMalloc(sizeof(IntListMember),"IntListMember"):
					      fbMgrAlloc(context->intSetManager));
					      /* SLHAlloc(context->slh,sizeof(IntListMember)) ); */
  memset(newMember,0,sizeof(IntListMember));
  newMember->x = x;

  if (set){
    IntListMember *prev = NULL;
    IntListMember *member = set;
    int linked = FALSE;
    while (member){
      if (x < member->x){
	if (prev){
	  prev->next = newMember;
	} else{
	  set = newMember;
	}
	newMember->next = member;
	linked = TRUE;
	break;
      }
      prev = member;
      member = member->next;
    }
    if (!linked){
      prev->next = newMember;
    }
    return set;
  } else{
    return newMember;
  }
}

static void recycleIntSet(TextContext *context, IntListMember *set){
  fixedBlockMgr *mgr = context->intSetManager;
  while (set){
    IntListMember *next = set->next;
    fbMgrFree(mgr,set);
    set = next;
  }
}

static void freeIntSet(IntListMember *set){
  while (set){
    IntListMember *next = set->next;
    safeFree((char*)set,sizeof(IntListMember));
    set = next;
  }
}

void pprintIntSet(IntListMember *set, char *prefix){
  IntListMember *member = set;
  printf("%s{ ",prefix);
  while (member){
    /* printf("member loop 0x%x\n",member);fflush(stdout); */
    
    printf("%d%s",member->x,(member->next ? ", " : ""));
    member = member->next;
  }
  printf("}\n");
  fflush(stdout);
}


static int eClosureContains(EClosure *eclosure, int x){
  return intSetContains(eclosure->stateIndices,x);
}

static void pprintEClosure(EClosure *closure){
  pprintIntSet(closure->stateIndices,"EClosure: ");
}

static void addToEClosure(TextContext *context, EClosure *eclosure, int x){
  eclosure->stateIndices = addToIntSet(context,eclosure->stateIndices,x,FALSE);
  eclosure->memberCount++;
}

static void recycleEClosure(TextContext *context, EClosure *eclosure){
  recycleIntSet(context,eclosure->stateIndices);
  eclosure->stateIndices = NULL;
}

static EClosure *createEClosure(TextContext *context){
  EClosure *closure = (EClosure*)SLHAlloc(context->slh,sizeof(EClosure));
  memset(closure,0,sizeof(EClosure));
  context->closureStackPtr = 0;
  return closure;
} 

static void addEClosureSeed(TextContext *context, EClosure *closure, int stateIndex){
  context->closureStack[context->closureStackPtr++] = stateIndex;
  addToEClosure(context,closure,stateIndex);
}

static void finishEClosure(TextContext *context, EClosure *closure){
  int *stack = context->closureStack;
  int stackPtr= context->closureStackPtr;
  /*
    printf("before EClosure seeds are: ");
    int i; 
    for (i=0; i<context->closureStackPtr; i++){
      printf("%d ",stack[i]);
    }
    printf("\n");
    printf("yielding closure: ");
    */
  while (stackPtr > 0){
    // printf("stackPtr is now %d top element is %d\n",stackPtr,stack[stackPtr-1]); 
    int x = stack[--stackPtr];
    // printf("context->ndfaStates=0x%x x=%d\n",context->ndfaStates,x); 
    NDFAState *state = context->ndfaStates[x];
    Transition *t = state->first;
    while (t){
      /* printf ("t=0x%x, t->test=0x%x, t->sibling=0x%x\n",
              t, t->test, t->sibling); */
      if (t->test == TEXT_TEST_EPSILON){
	NDFAState *nextState = t->nextState;
	if (!eClosureContains(closure,nextState->index)){
	  stack[stackPtr++] = nextState->index;
	  addToEClosure(context,closure,nextState->index);
          // printf("%d ",nextState->index); 
	}
      }
      t = t->sibling;
    }
  }
  // printf("\n"); 
}

static EClosure *simpleClosure(TextContext *context, int x){
  EClosure *closure = createEClosure(context);
  addEClosureSeed(context,closure,x);
  finishEClosure(context,closure);
  return closure;
}

#define SET_COMPARISON_UNKNOWN      0
#define SET_COMPARISON_DISJOINT     1
#define SET_COMPARISON_A_SUB_B      2
#define SET_COMPARISON_B_SUB_A      3 
#define SET_COMPARISON_INTERSECTING 4
#define SET_COMPARISON_EQUIVALENT   5

#define CHAR_META_CONTROL   0x01
#define CHAR_META_ALPHA    0x02
#define CHAR_META_UPPER    0x04  
#define CHAR_META_LOWER    0x08
#define CHAR_META_DIGIT    0x10
#define CHAR_META_HEXDIGIT 0x20
#define CHAR_META_WHITESPACE 0x40
#define CHAR_META_BLANK      0x80
#define CHAR_META_NATIONAL   0x100

/* this is a very vanilla ebcdic 37-500-1047 ish */

static int ebcdicMetaData[256] =
{ /* 0     1     2     3     4     5     6     7     8     9     A     B     C     D    E      F   */
  0x01, 0x01, 0x01, 0x01, 0x01, 0xC1, 0x01, 0x01, 0x01, 0x01, 0x01, 0x41, 0x41, 0x41, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x41, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x41, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 0x40's */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x100, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x100, 0x100, 0x00, 0x00, 0x00,
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
  int meta = ebcdicMetaData[(int)c];
  return meta & CHAR_META_ALPHA;
}

static int isAlphaNumeric(char c){
  int meta = ebcdicMetaData[(int)c];
  return meta & (CHAR_META_ALPHA | CHAR_META_DIGIT);
}

static int isAlphaNumericUpper(char c){
  int meta = ebcdicMetaData[(int)c];
  return meta & (CHAR_META_UPPER | CHAR_META_DIGIT);
}

static int isAlphaNumericLower(char c){
  int meta = ebcdicMetaData[(int)c];
  return meta & (CHAR_META_LOWER | CHAR_META_DIGIT);
}

static int isWordChar(char c){
  int meta = ebcdicMetaData[(int)c];
  return ((meta & (CHAR_META_ALPHA | CHAR_META_DIGIT)) ||
	  c == '_');
}

static int isUpper(char c){
  int meta = ebcdicMetaData[(int)c];
  return meta & CHAR_META_UPPER;
}

static int isLower(char c){
  int meta = ebcdicMetaData[(int)c];
  return meta & CHAR_META_LOWER;
}

static int isWhitespace(char c){
  int meta = ebcdicMetaData[(int)c];
  return meta & CHAR_META_WHITESPACE;
}

static int isBlank(char c){
  int meta = ebcdicMetaData[(int)c];
  return meta & CHAR_META_BLANK;
}

static int isDigit(char c){
  return (c >= 0xF0 && c <= 0xF9);
}

static int isHexDigit(char c){
  int meta = ebcdicMetaData[(int)c];
  return meta & CHAR_META_HEXDIGIT;
}

static int isHexUpper(char c){
  int meta = ebcdicMetaData[(int)c];
  return meta & (CHAR_META_HEXDIGIT & CHAR_META_UPPER);
}

static int isHexLower(char c){
  int meta = ebcdicMetaData[(int)c];
  return meta & (CHAR_META_HEXDIGIT & CHAR_META_LOWER);
}

static int isAlphaNumericOrNational(char c){
  int meta = ebcdicMetaData[(int)c];
  return meta & (CHAR_META_UPPER | CHAR_META_DIGIT | CHAR_META_NATIONAL);
}

static int compareTransitions(Transition *a, Transition *b){
  char buffer[256];
  switch (a->test){
  case TEXT_TEST_CHAR:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (a->c == b->c ? SET_COMPARISON_EQUIVALENT : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_ALPHA:
      return (isAlpha(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_ALPHAUPPER:
      return (isUpper(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_ALPHALOWER:
      return (isLower(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_ALPHANUMERIC:
      return (isAlphaNumeric(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_ALPHANUMERICUPPER:
      return (isAlphaNumericUpper(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_ALPHANUMERICLOWER:
      return (isAlphaNumericLower(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_DIGIT:
      return (isDigit(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_HEXDIGIT:
      return (isHexDigit(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_HEXDIGITUPPER:
      return (isHexUpper(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_HEXDIGITLOWER:
      return (isHexLower(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_WHITESPACE:
      return (isWhitespace(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_WORDCHAR:
      return (isWordChar(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_DOT:
      return SET_COMPARISON_A_SUB_B;
    case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
      return (isAlphaNumericOrNational(a->c) ? SET_COMPARISON_A_SUB_B : SET_COMPARISON_DISJOINT);
    default:
      
      /* printf("*** PANIC no comparison for char to interesting test type, %s\n",textTestDescription(b,buffer)); */
      break;
    }
    break;
  case TEXT_TEST_ALPHA:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (isAlpha(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_DIGIT:
    case TEXT_TEST_WHITESPACE:
      return SET_COMPARISON_DISJOINT;
    case TEXT_TEST_HEXDIGIT:
    case TEXT_TEST_HEXDIGITUPPER:
    case TEXT_TEST_HEXDIGITLOWER:
    case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
      return SET_COMPARISON_INTERSECTING;
    }
    
    break;
  case TEXT_TEST_ALPHAUPPER:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (isUpper(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_DIGIT:
    case TEXT_TEST_WHITESPACE:
    case TEXT_TEST_ALPHALOWER:
    case TEXT_TEST_HEXDIGITLOWER:
      return SET_COMPARISON_DISJOINT;
    case TEXT_TEST_HEXDIGIT:
    case TEXT_TEST_HEXDIGITUPPER:
      return SET_COMPARISON_INTERSECTING;
    }
    break;
  case TEXT_TEST_ALPHALOWER:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (isLower(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_DIGIT:
    case TEXT_TEST_WHITESPACE:
    case TEXT_TEST_ALPHAUPPER:
    case TEXT_TEST_HEXDIGITUPPER:
    case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
      return SET_COMPARISON_DISJOINT;
    case TEXT_TEST_HEXDIGIT:
    case TEXT_TEST_HEXDIGITLOWER:
      return SET_COMPARISON_INTERSECTING;
    }
    break;
  case TEXT_TEST_ALPHANUMERIC:
  case TEXT_TEST_WORDCHAR:
    switch (b->test){
    case TEXT_TEST_CHAR:
      if (a->test == TEXT_TEST_ALPHANUMERIC){
	return (isAlphaNumeric(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
      } else{
	return (isWordChar(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
      }
    case TEXT_TEST_DIGIT:
    case TEXT_TEST_ALPHA:
    case TEXT_TEST_ALPHAUPPER:
    case TEXT_TEST_ALPHALOWER:
    case TEXT_TEST_HEXDIGIT:
    case TEXT_TEST_HEXDIGITUPPER:
    case TEXT_TEST_HEXDIGITLOWER:
      return SET_COMPARISON_B_SUB_A;
    case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
      return SET_COMPARISON_INTERSECTING;
    case TEXT_TEST_WHITESPACE:
      return SET_COMPARISON_DISJOINT;
    }
    break;
  case TEXT_TEST_ALPHANUMERICUPPER:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (isAlphaNumeric(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_DIGIT:
    case TEXT_TEST_ALPHA:
    case TEXT_TEST_ALPHAUPPER:
    case TEXT_TEST_HEXDIGITUPPER:
      return SET_COMPARISON_B_SUB_A;
    case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
      return SET_COMPARISON_A_SUB_B;
    case TEXT_TEST_HEXDIGIT:
    case TEXT_TEST_HEXDIGITLOWER:
      return SET_COMPARISON_INTERSECTING;
    case TEXT_TEST_ALPHALOWER:
    case TEXT_TEST_WHITESPACE:
      return SET_COMPARISON_DISJOINT;
    }
    break;
  case TEXT_TEST_ALPHANUMERICLOWER:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (isAlphaNumeric(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_DIGIT:
    case TEXT_TEST_ALPHA:
    case TEXT_TEST_ALPHALOWER:
    case TEXT_TEST_HEXDIGITLOWER:
      return SET_COMPARISON_B_SUB_A;
    case TEXT_TEST_HEXDIGIT:
    case TEXT_TEST_HEXDIGITUPPER:
      return SET_COMPARISON_INTERSECTING;
    case TEXT_TEST_ALPHAUPPER:
    case TEXT_TEST_WHITESPACE:
    case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
      return SET_COMPARISON_DISJOINT;
    }
    break;
  case TEXT_TEST_DIGIT:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (isDigit(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_DIGIT:
      return SET_COMPARISON_EQUIVALENT;
    case TEXT_TEST_WHITESPACE:
    case TEXT_TEST_ALPHA:
    case TEXT_TEST_ALPHAUPPER:
    case TEXT_TEST_ALPHALOWER:
      return SET_COMPARISON_DISJOINT;
    case TEXT_TEST_HEXDIGIT:
    case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
      return SET_COMPARISON_A_SUB_B;
    case TEXT_TEST_HEXDIGITUPPER:
    case TEXT_TEST_HEXDIGITLOWER:
      return SET_COMPARISON_INTERSECTING;
    }
    break;
  case TEXT_TEST_HEXDIGIT:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (isHexDigit(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_WHITESPACE:
      return SET_COMPARISON_DISJOINT;
    case TEXT_TEST_DIGIT:
      return SET_COMPARISON_B_SUB_A;
    case TEXT_TEST_ALPHANUMERIC:
    case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
      return SET_COMPARISON_A_SUB_B;
    }
    break;
  case TEXT_TEST_HEXDIGITUPPER:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (isHexUpper(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_ALPHALOWER:
    case TEXT_TEST_WHITESPACE:
      return SET_COMPARISON_DISJOINT;
    case TEXT_TEST_DIGIT:
      return SET_COMPARISON_B_SUB_A;
    case TEXT_TEST_ALPHA:
    case TEXT_TEST_ALPHAUPPER:
      return SET_COMPARISON_INTERSECTING;
    case TEXT_TEST_ALPHANUMERIC:
    case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
      return SET_COMPARISON_A_SUB_B;
    }
    break;
  case TEXT_TEST_HEXDIGITLOWER:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (isHexLower(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_ALPHAUPPER:
    case TEXT_TEST_WHITESPACE:
    case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
      return SET_COMPARISON_DISJOINT;
    case TEXT_TEST_DIGIT:
      return SET_COMPARISON_B_SUB_A;
    case TEXT_TEST_ALPHA:
    case TEXT_TEST_ALPHALOWER:
      return SET_COMPARISON_INTERSECTING;
    case TEXT_TEST_ALPHANUMERIC:
      return SET_COMPARISON_A_SUB_B;
    }
    break;
  case TEXT_TEST_WHITESPACE:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (isWhitespace(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
    default:
      return SET_COMPARISON_DISJOINT;
    }
    break;
  case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
    switch (b->test){
    case TEXT_TEST_CHAR:
      return (isAlphaNumericOrNational(b->c) ? SET_COMPARISON_B_SUB_A : SET_COMPARISON_DISJOINT);
    case TEXT_TEST_DIGIT:
    case TEXT_TEST_ALPHA:
    case TEXT_TEST_ALPHAUPPER:
    case TEXT_TEST_HEXDIGITUPPER:
    case TEXT_TEST_ALPHANUMERICUPPER:
      return SET_COMPARISON_B_SUB_A;
    case TEXT_TEST_HEXDIGIT:
    case TEXT_TEST_HEXDIGITLOWER:
      return SET_COMPARISON_INTERSECTING;
    case TEXT_TEST_ALPHALOWER:
    case TEXT_TEST_WHITESPACE:
      return SET_COMPARISON_DISJOINT;
    }
    break;
  case TEXT_TEST_DOT:
    switch (b->test){
    case TEXT_TEST_DOT:
      return SET_COMPARISON_EQUIVALENT;
    default:
      return SET_COMPARISON_B_SUB_A;
    }
  default:
    /* printf("*** PANIC no comparison for test type %d to any other type\n",a->test); */
    break;
  }
  if (b->test == TEXT_TEST_DOT){
    return SET_COMPARISON_A_SUB_B;
  } else if ((a->test == b->test) && (a->test != TEXT_TEST_CHAR)){
    return SET_COMPARISON_EQUIVALENT;
  } else{
  return SET_COMPARISON_UNKNOWN;
  }
}

static DFAState *makeDFAState(TextContext *context, EClosure *closure){
  DFAState *state = (DFAState*)safeMalloc(sizeof(DFAState),"DFAState");
  memset(state,0,sizeof(DFAState));
  memcpy(state->eyecatcher,"DFA ",4);
  state->index = context->highestDFAIndex++;
  int *ndfaStates = (int*)safeMalloc(sizeof(int*)*closure->memberCount,"DFA NDFA State Vector");
  int index = 0;
  IntListMember *member = closure->stateIndices;
  if (context->compileTrace > 0){
    printf("      making dfa state from ndfa states ");
  }
  while (member){
    ndfaStates[index++] = member->x;
    if (context->compileTrace > 0){
      printf("%d ",ndfaStates[index-1]);
    }
    member = member->next;
  }
  if (context->compileTrace > 0){
    printf("\n");
  }
  
  state->ndfaStateCount = closure->memberCount;
  state->ndfaStates = ndfaStates;
  int i;
  for (i=0; i<state->ndfaStateCount; i++){
    int ndfaIndex = ndfaStates[i];
    NDFAState *ndfaState = context->ndfaStates[ndfaIndex];
    if (ndfaState->bindingStart && !intSetContains(state->bindingStarts,ndfaState->bindingStart)){
      if (ndfaHasStateTransitionInsideDFA(ndfaState,state)){
        state->bindingStarts = addToIntSet(context,state->bindingStarts,ndfaState->bindingStart,TRUE);
      } else{
        if (context->compileTrace > 0){
          printf("avoiding adding binding of NDFA state %d to DFA state %d\n",ndfaState->index,state->index);
        }
      }
    }
    if (ndfaState->bindingEnd && !intSetContains(state->bindingEnds,ndfaState->bindingEnd)){
      state->bindingEnds = addToIntSet(context,state->bindingEnds,ndfaState->bindingEnd,TRUE);
    }
    if (ndfaState->bindingClearFrom && !intSetContains(state->bindingClearFroms,ndfaState->bindingClearFrom)){
      state->bindingClearFroms = addToIntSet(context,state->bindingClearFroms,ndfaState->bindingClearFrom,TRUE);
      state->bindingClearTos   = addToIntSet(context,state->bindingClearTos,  ndfaState->bindingClearTo,TRUE);
    }

    if (ndfaState->flags & FA_TERMINAL){
      state->flags |= FA_TERMINAL;
      state->terminalIndices = addToIntSet(context,state->terminalIndices,ndfaState->terminalIndex,TRUE);
    }
  }
  return state;
}

static int ndfaHasStateTransitionInsideDFA(NDFAState *ndfaState, DFAState *dfaState){
  Transition *t = ndfaState->first;
  while (t){
    if (t->nextState){
      int nextIndex = ((NDFAState*)t->nextState)->index;
      int i;
      for (i=0; i<dfaState->ndfaStateCount; i++){
        if (nextIndex == dfaState->ndfaStates[i]){
          return TRUE;
        }
      }
    }
    t = t->sibling;
  }
  return FALSE;
}

static int ndfaHasStateTransitionOutsideDFA(NDFAState *ndfaState, DFAState *dfaState){
  Transition *t = ndfaState->first;
  while (t){
    if (t->nextState){
      int nextIndex = ((NDFAState*)t->nextState)->index;
      int i;
      int found = FALSE;
      for (i=0; i<dfaState->ndfaStateCount; i++){
        if (nextIndex == dfaState->ndfaStates[i]){
          found = TRUE;
          break;
        }
      }
      if (!found){
        return TRUE;
      }
    }
    t = t->sibling;
  }
  return FALSE;
}


static void freeDFATransition(Transition *t){
  if (t->data){
    safeFree(t->data,strlen(t->data)+1);
  }
  memcpy(((char*)t)+4,"DEAD",4);
  safeFree((char*)t,sizeof(Transition));
}

static void freeDFATransitionChain(Transition* chain){
  while (chain){
    Transition *sibling = chain->sibling;
    freeDFATransition(chain);
    chain = sibling;
  }
}

static void freeDFAState(DFAState *state){
  freeDFATransitionChain(state->firstTransition);
  memcpy(((char*)state)+4,"DEAD",4);
  freeIntSet(state->bindingStarts);
  freeIntSet(state->bindingEnds);
  freeIntSet(state->bindingClearFroms);
  freeIntSet(state->bindingClearTos);
  freeIntSet(state->terminalIndices);
  safeFree((char*)state->ndfaStates,sizeof(int*)*state->ndfaStateCount);
  safeFree((char*)state,sizeof(DFAState));
}

static DFAState *getUnmarkedDFAState(DFA *dfa){
  int i;
  int len = dfa->stateCount;
  
  /* printf("getUnmarked, state count = %d\n",len); */
  for (i=0; i<len; i++){
    DFAState *state = dfa->states[i];
    /* printf("state = 0x%x, marked=%d\n",state,state->marked); */
    if (state->marked == FALSE){
      return state;
    }
  }
  return NULL;
}

static void markDFAState(DFAState *dfaState){
  dfaState->marked = TRUE;
}

static Transition *makeDFATransition(TextContext *context, DFAState *fromState, Transition *t, EClosure *toClosure){
  Transition *dfaTransition = (Transition*)safeMalloc(sizeof(Transition),"DFA Transition");
  memcpy(dfaTransition,t,sizeof(Transition));
  dfaTransition->nextState = NULL;
  dfaTransition->sibling = NULL;
  if (t->data){
    char *newData = safeMalloc(strlen(t->data)+1,"DFA Transition data");
    memcpy(newData,t->data,strlen(t->data)+1);
    dfaTransition->data = newData;
  }
  if (t->nextState){
    int i=0; 
    /* is this transition for one of our members that is a bindingStart
       It is already proven that this test is to some other closure/dfaState
       what we don't know is if the transition is from an ndfaState that had a binding start or not 
       */
    for (i=0; i<fromState->ndfaStateCount; i++){
      NDFAState *stateInClosure = context->ndfaStates[fromState->ndfaStates[i]];
      if (stateInClosure->bindingStart){
        if (context->compileTrace){
          printf("binding exists in from state %d for ndfa state %d\n",fromState->index,stateInClosure->index);
        }
        Transition *ndfaTransition = stateInClosure->first;
        int found = FALSE;
        while (ndfaTransition){
          if (ndfaTransition == t){
            found = TRUE;
            break;
          }
          ndfaTransition = ndfaTransition->sibling;
        }
        if (found){
          dfaTransition->bindingStart = stateInClosure->bindingStart; /* an out transition binding start */
        }
      }
    }
  }
  return dfaTransition;
}

/*
static void extendDFATransitionChain(Transition *chain, Transition *t){
  while (chain->sibling){
    chain = chain->sibling;
  }
  chain->sibling = makeDFATransition(t);
}
*/


/* here, dfaTransition bindning starts are the set union of binding starts of the ndfa states of the "toClosure" 
   1) see that the transition bindings starts can be made accurately.
   1.1) worry about updateTransitionChain second case 
   2) this does not assure that the initial dfa state will be noted well
      ".*(33) is bad because the initial state is a binding start
   2.1) or will the new way of marking only set binding starts for position i-1

   NEW IDEAS
   1) only add NDFA bindingStart to DFA if binding start has epsilon-transitions to something else in the closure
      That is if the outbound edge from the e-closure node goes through the bindingStart it is not "IN" the closure
   2) maintain earliest and latest binding start seen for an dfa binding start/end pair.  That is, make it a triple of 
      (greedyStart, minimalSizeMatchStart, end).  Will greedy always be the right answer?  Do we need minimal fixed
      length match to figure out.
   3) I think greedy plus the fixed length bind pattern hack will both be needed, but in some cases be 
      insufficient

*/

static void updateDFATransitionChain(TextContext *context, DFAState *fromState, DFAState *toState, Transition *ndfaTransition,
                                     EClosure *toClosure){
  Transition *dfaTransition = makeDFATransition(context,fromState,ndfaTransition,toClosure);
  char buffer[256];
  dfaTransition->nextState = toState;
  Transition *chain = fromState->firstTransition;
  if (chain){
    Transition *penultimate = NULL;
    while (chain){
      if ((chain->test == dfaTransition->test) &&
	  (chain->c == dfaTransition->c) &&
	  (!strcmp(chain->data,dfaTransition->data))){
	if (dfaTransition->nextState == chain->nextState){
	  if (context->compileTrace){
	    printf("INFO: repeated transition seen\n");fflush(stdout);
	  }
	} else{
	  if (context->compileTrace){
	    printf("WARNING or ERROR: conflict on repeated transition seen %s from %d to %d\n",
		   textTestDescription(ndfaTransition,buffer),fromState->index,toState->index);
	    fflush(stdout);
	  }
	}
	/* must clean up before return */
	freeDFATransition(dfaTransition);
	return;
      }
      chain = chain->sibling;
    }
    /* If not conflicts seen, insert the new test at the head if single char,
       else use A_SUB_B set comparison to make reasonable insert relative
       to partial ordering. */
    if (context->compileTrace > 1){
      printf("no conflicts seen adding transition for %s\n",textTestDescription(dfaTransition,buffer)); 
    }
    if (dfaTransition->test == TEXT_TEST_CHAR){
      /* just put it in front, all single-char tests are equivalent */
      dfaTransition->sibling = fromState->firstTransition;
      fromState->firstTransition = dfaTransition;
    } else{
      int inserted = FALSE;
      chain = fromState->firstTransition;
      while (chain){
	int comparison = compareTransitions(dfaTransition,chain);
	if (comparison == SET_COMPARISON_A_SUB_B){
	  if (penultimate){
	    penultimate->sibling = dfaTransition;
	    dfaTransition->sibling = chain;
	  } else{
	    dfaTransition->sibling = fromState->firstTransition;
	    fromState->firstTransition = dfaTransition;
	  }
	  inserted = TRUE;
	}
	penultimate = chain;
	chain = chain->sibling;
      }
      if (!inserted){
	penultimate->sibling = dfaTransition;
      } else{

      }
    }
  } else{
    fromState->firstTransition = dfaTransition;
  }
}

void dumpDFA(DFA *dfa, int showBindings){
  int i;
  char buffer[256];

  for (i=0; i<dfa->stateCount; i++){
    DFAState *state = dfa->states[i];
    printf("DFA STATE %d %s ndfaStates={ ",state->index,(state->flags & FA_TERMINAL ? "TERMINAL" : ""));
    int j;
    for (j=0; j<state->ndfaStateCount; j++){
      printf("%s%d",(j>0 ? ", " : ""),state->ndfaStates[j]);
    }
    printf("}\n");
    pprintIntSet(state->bindingStarts,"  Binding Starts");
    pprintIntSet(state->bindingEnds,"  Binding Ends");
    if (state->bindingClearFroms){
      pprintIntSet(state->bindingClearFroms,"  Binding Clear Froms  ");
      pprintIntSet(state->bindingClearTos,"  Binding Clear Tos  ");
    }
    Transition *t = state->firstTransition;
    while (t){
      DFAState *nextState = (DFAState*)t->nextState;
      printf("  %s to %d",textTestDescription(t,buffer),nextState->index);
      if (t->bindingStart){
        printf(" bindingStart=%d",t->bindingStart);
      }
      printf("\n");
      t = t->sibling;
    }
  }
  printf("Done with dumpDFA\n");
}

void freeDFA(DFA *dfa){
  int i;
  for (i=0; i<dfa->stateCount; i++){
    freeDFAState(dfa->states[i]);
  }
  safeFree((char*)dfa->states,sizeof(DFAState*)*dfa->stateVectorSize);
  if (dfa->minimumBindingLengths){
    safeFree((char*)dfa->minimumBindingLengths,dfa->bindingCount * sizeof(int*));
  }
  if (dfa->terminalBindingOffsets){
    safeFree((char*)dfa->terminalBindingOffsets,dfa->terminalCount * sizeof(int*));
  }
  safeFree((char*)dfa,sizeof(DFA));
}

static void dumpNDFA(TextContext *context);

#define DFA_STATE_VECTOR_SIZE 50

static DFA *constructDFA(TextContext *context){
  DFA *dfa = (DFA*)safeMalloc(sizeof(DFA),"DFA");
  EClosure *initialClosure = simpleClosure(context,0);
  DFAState *initialDFAState = makeDFAState(context,initialClosure);
  memset(dfa,0,sizeof(DFA));
  dfa->states = (DFAState**)safeMalloc(sizeof(DFAState*)*DFA_STATE_VECTOR_SIZE,"DFA State Vector");
  dfa->stateVectorSize = DFA_STATE_VECTOR_SIZE;
  DFAState *dfaState = NULL;
  char buffer[256];

  if (context->compileTrace > 1){
    dumpNDFA(context);
    pprintEClosure(initialClosure);
  }
  /* while unmarked DFA state */
  htPut(context->closureToDFAStateTable,initialDFAState,initialDFAState);
  addDFAState(dfa,initialDFAState);
  while (dfaState = getUnmarkedDFAState(dfa)){
    if (dfa->stateCount > context->maxDFAStates){
      noteParseError(context,"DFA was too complex");
      return NULL;
    }
    markDFAState(dfaState);
    int ndfaStateCount = dfaState->ndfaStateCount;
   
    int i;
    if (context->compileTrace > 1){
      printf("_______________________________________________\n");
      dumpDFA(dfa,FALSE);
      printf("\nbegin work on %d: {",dfaState->index);
      for (i=0; i<dfaState->ndfaStateCount; i++){
	printf("%s%d",(i>0 ? ", " : ""),dfaState->ndfaStates[i]);
      }
      printf("}\n");
      printf("_______________________________________________\n");
    }
    for (i=0; i<ndfaStateCount; i++){
      NDFAState *ndfaState = context->ndfaStates[dfaState->ndfaStates[i]];
      Transition *t = ndfaState->first;
      if (context->compileTrace > 1){
	printf("analyzing transitions of NDFA state %d\n",ndfaState->index);
      }

      while (t){
	if (t->test != TEXT_TEST_EPSILON){
	  EClosure *nextClosure = createEClosure(context);
	  /* Transition *newTransitionChain = makeDFATransition(t); */
	  int nextStateIndex = ((NDFAState*)t->nextState)->index;
	  addEClosureSeed(context,nextClosure,nextStateIndex);
	  if (context->compileTrace > 1){
	    printf("  analyzing transition %s of NDFA state %d in DFA state %d\n",
                   textTestDescription(t,buffer),ndfaState->index,dfaState->index);
	    printf("    starting seed is %d\n",nextStateIndex);
	  }
	  int j;
	  for (j=0; j<ndfaStateCount; j++){
	    NDFAState *otherState = context->ndfaStates[dfaState->ndfaStates[j]];
	    Transition *t1 = otherState->first;
	    while (t1){
	      if ((t1 != t) && (t1->test != TEXT_TEST_EPSILON)){
		int comparison = compareTransitions(t,t1);
		if ((comparison == SET_COMPARISON_A_SUB_B)||
		    (comparison == SET_COMPARISON_EQUIVALENT)){
		  int extraTarget = ((NDFAState*)t1->nextState)->index;
		  if (context->compileTrace > 1){
		    printf("    attaching next of test %s, -> %d\n",textTestDescription(t1,buffer),extraTarget);
		  }
		  addEClosureSeed(context,nextClosure,extraTarget);
		  /* the following would duplicate test in the transition chain */
		  /* extendDFATransitionChain(newTransitionChain,t1); */
		} else if (comparison == SET_COMPARISON_INTERSECTING){
		  if (context->compileTrace > 1){
		  printf("    WARNING overlapping tests\n");
		  }
		} else if (comparison == SET_COMPARISON_UNKNOWN){
		  char buffer1[256];
		  char buffer2[256];
		  if (context->compileTrace > 1){
		    printf("    WARNING undecidable tests comparison=%d, t=%s vs. t1=%s\n",
			   comparison,textTestDescription(t,buffer1),textTestDescription(t1,buffer2));
		  }
		}
	      }
	      t1 = t1->sibling;
	    }
	  }
	  finishEClosure(context,nextClosure);
	  DFAState *potentialNewState = makeDFAState(context,nextClosure);
	  DFAState *existingState = (DFAState*)htGet(context->closureToDFAStateTable,potentialNewState);
	  if (!existingState){
	    if (context->compileTrace > 1){
	      printf("  unique new state found with index %d\n",potentialNewState->index);
	    }
	    htPut(context->closureToDFAStateTable,potentialNewState,potentialNewState);
	    updateDFATransitionChain(context,dfaState,potentialNewState,t,nextClosure);
	    addDFAState(dfa,potentialNewState);
	    context->retainedClosures++;
	  } else{
	    if (context->compileTrace > 1){
	      printf("  previously existing state found\n");
	    }
	    /* freeDFATransitionChain(newTransitionChain); */
	    context->droppedClosures++;
	    recycleEClosure(context,nextClosure);
	    freeDFAState(potentialNewState);
	    updateDFATransitionChain(context,dfaState,existingState,t,nextClosure);
	  }
	  
	}
	t = t->sibling;
      }
    }
  }
  dfa->bindingCount = context->highestBindingIndex;
  return dfa;
}
  /*
     for each input in alpha (or tests)
       find all ndfa states for that input and make closure seed T
       make y closure of (T)
       check if y indexes a state in DFA already
       if not 
          add it as unmarked
	  add a transition on input char or function from prev state to new one

     a vs b comparisons
       DISJOINT
       EQUIVALENT
       A_SUB_B
       B_SUB_A
       NON_TRIVIAL_INTERSECTION

     for each transition T
        compare to all others T1
	   case DISJOINT
             do nothing
           case EQUIVALENT
             add next state of T1 to T
           case A_SUB_B 
             add next state of T1 to T
           case B_SUB_A
             example: T is alpha, T1 is 'q'
	     add nothing , nothing that if transition on B is chosen it is done ahead, and the transitions for A will be noted for it.
           case NON_TRIVIAL_INTERSECTION
             example: [A-Q] vs [H-Z]
             1) add nothing and warn of ambiguous grammar
	     2) refactor NDFA graph for no overlaps.

      1-finish intersection chart
      2-add perl-like char classes (easier than brack syntax)
      2-start comparing.
	     
  }
  */

static int findMatchingParen(char *s, int start, int end){  /* -1 for none */
  int i = start;
  int c;
  int followingEscape = 0;
  int level = 0;
  
  while (i<end){
    int c = s[i];
    /* printf("loop i=%d c=%c followingEscape=%d level=%d end=%d\n",i,c,followingEscape,level,end); */
    if (followingEscape){
      
      followingEscape = FALSE;
    } else if (c == ESCAPE){
      followingEscape = TRUE;
      i++;
      continue;
    } else{
      if (c == '('){
	level++;
      } else if (c == ')'){
	if (level == 0){
	  return i;
	} else{
	  level--;
	}
      }
    }
    i++;
  }
  return -1;
}

static int findMatchingClose(char *s, char openChar, char closeChar, int start, int end){  /* -1 for none */
  int i = start;
  int c;
  int followingEscape = 0;
  int level = 0;
  
  while (i<end){
    int c = s[i];
    /* printf("loop i=%d c=%c followingEscape=%d level=%d end=%d\n",i,c,followingEscape,level,end); */
    if (followingEscape){
      
      followingEscape = FALSE;
    } else if (c == ESCAPE){
      followingEscape = TRUE;
      i++;
      continue;
    } else{
      if (openChar == '[' ? 
	  ((c == '[') || (c == 0xBA)) :
	  (openChar == c)){
	level++;
      } else if (closeChar == ']' ?
		 ((c == ']') || (c == 0xBB)) :
		 (c == closeChar)){
	if (level == 0){
	  return i;
	} else{
	  level--;
	}
      }
    }
    i++;
  }
  return -1;
}

#define DEFAULT_PRECEDENCE 1

static int getTokenPrecedence(char c){
  switch (c){
  case '|':
    return 0;
  case '*':
  case '+':
    return 2;
  case '(':
    return 3;
  default:
    return DEFAULT_PRECEDENCE;
  }
}

/*

  aaab               (SEQ a a a b)

  a(b|c)             (SEQ a (ALT b c))

  a(b|(c|d)*)        (SEQ a (ALT b (STAR (ALT c d))))
  */

static void pprintTextExpr(TextExpr *expr, int depth);

static void pprintTextExprChildren(TextExpr *expr, int depth){
  TextExpr *child = expr->firstChild;
  while (child){
    pprintTextExpr(child,depth);
    child = child->nextSibling;
  }
}

static void pprintTextExpr(TextExpr *expr, int depth){
  int i;
  for (i=0; i<depth; i++){
    printf(" ");
  }
  switch (expr->type){
  case TEXT_EXPR_CHAR:
    printf("CHAR %c\n",expr->c);
    break;
  case TEXT_EXPR_ALT:
    printf("ALT  \n");
    pprintTextExprChildren(expr,depth+2);
    break;
  case TEXT_EXPR_SEQ:
    printf("SEQ  \n");
    pprintTextExprChildren(expr,depth+2);
    break;
  case TEXT_EXPR_STAR:
    printf("STAR \n");
    pprintTextExprChildren(expr,depth+2);
    break;
  case TEXT_EXPR_PLUS:
    printf("PLUS \n");
    pprintTextExprChildren(expr,depth+2);
    break;
  case TEXT_EXPR_QMARK:
    printf("OPTIONAL \n");
    pprintTextExprChildren(expr,depth+2);
    break;
  case TEXT_EXPR_PREDEFINED_CLASS:
    printf("PREDEFINED CHAR CLASS %d\n",expr->predefinedClass);
    break;
  case TEXT_EXPR_BIND:
    printf("BIND \n");
    pprintTextExprChildren(expr,depth+2);
    break;
  default:
    printf("WTF no pprintf for %d\n",expr->type);
    break;
  }
}

static TextExpr *makePredefinedClassExpr(TextContext *context, int test, int negated){
  if (negated){
    noteParseError(context,"Regular expression negation not yet supported");
    return NULL;
  } else{
    TextExpr *expr = makeTextExpr(context,TEXT_EXPR_PREDEFINED_CLASS);
    expr->predefinedClass = test;
    return expr;
  }
}

static TextExpr *parseCharClass(TextContext *context, int end){
  int escaped;
  int c = nextChar(context,end,&escaped,"char class negation");
  int negated = FALSE;
  if (c == '^'){
    negated=TRUE;
  } else{
    unreadLastChar(context);
  }
  int specLen = end-context->pos;
  char *spec = SLHAlloc(context->slh,specLen+1);
  memcpy(spec,context->s+context->pos,specLen);
  spec[specLen] = 0;
  if (!strcmp(spec,"A-Za-z") ||
      !strcmp(spec,"a-zA-Z") ||
      !strcmp(spec,":alpha:") ||
      !strcmp(spec,":ALPHA:")){
    /* alphabetical */
    return makePredefinedClassExpr(context,TEXT_TEST_ALPHA,negated);
  } else if (!strcmp(spec,"a-z") ||
	     !strcmp(spec,":LOWER:") ||
	     !strcmp(spec,":lower:")){
    /* alphabetical */
    return makePredefinedClassExpr(context,TEXT_TEST_ALPHALOWER,negated);
  } else if (!strcmp(spec,"A-Z") ||
	     !strcmp(spec,":UPPER:") ||
	     !strcmp(spec,":upper:")){
    /* alphabetical */
    return makePredefinedClassExpr(context,TEXT_TEST_ALPHAUPPER,negated);
  } else if (!strcmp(spec,":alnum:") ||
	     !strcmp(spec,":ALNUM:") ||
	     !strcmp(spec,"A-Za-z0-9") ||
	     !strcmp(spec,"a-zA-Z0-9") ||
	     !strcmp(spec,"A-Z0-9a-z") ||
	     !strcmp(spec,"a-z0-9A-Z") ||
	     !strcmp(spec,"0-9A-Za-z") ||
	     !strcmp(spec,"0-9a-zA-Z") ){
    /* alphanumeric */
    return makePredefinedClassExpr(context,TEXT_TEST_ALPHANUMERIC,negated);
  } else if (!strcmp(spec,"a-z0-9") ||
	     !strcmp(spec,"0-9a-z")){
    return makePredefinedClassExpr(context,TEXT_TEST_ALPHANUMERICLOWER,negated);
  } else if (!strcmp(spec,"A-Z0-9") ||
	     !strcmp(spec,"0-9A-Z")){
    return makePredefinedClassExpr(context,TEXT_TEST_ALPHANUMERICUPPER,negated);
  } else if (!strcmp(spec,"A-Fa-f0-9") ||
	     !strcmp(spec,"a-fA-F0-9") ||
	     !strcmp(spec,"A-F0-9a-f") ||
	     !strcmp(spec,"a-f0-9A-F") ||
	     !strcmp(spec,"0-9A-Fa-f") ||
	     !strcmp(spec,"0-9a-fA-F") ||
	     !strcmp(spec,":XDIGIT:")  ||
	     !strcmp(spec,":xdigit:") ){
    return makePredefinedClassExpr(context,TEXT_TEST_HEXDIGIT,negated);
  } else if (!strcmp(spec,"a-f0-9") ||
	     !strcmp(spec,"0-9a-f") ){
    return makePredefinedClassExpr(context,TEXT_TEST_HEXDIGITLOWER,negated);
  } else if (!strcmp(spec,"A-F0-9") ||
	     !strcmp(spec,"0-9A-F") ){
    return makePredefinedClassExpr(context,TEXT_TEST_HEXDIGITLOWER,negated);
  } else if (!strcmp(spec,"0-9") ||
	     !strcmp(spec,":DIGIT:") ||
	     !strcmp(spec,":digit:")){
    return makePredefinedClassExpr(context,TEXT_TEST_DIGIT,negated);
  } else if (!strcmp(spec,":word:") ||
	     !strcmp(spec,":WORD:")){
    return makePredefinedClassExpr(context,TEXT_TEST_WORDCHAR,negated);
  } else if (!strcmp(spec,":space:") ||
	     !strcmp(spec,":SPACE:")){
    return makePredefinedClassExpr(context,TEXT_TEST_WHITESPACE,negated);
  } else if (
      !strcmp(spec,"A-Z0-9@#$") ||
      !strcmp(spec,"A-Z0-9@$#") ||
      !strcmp(spec,"A-Z0-9#$@") ||
      !strcmp(spec,"A-Z0-9#@$") ||
      !strcmp(spec,"A-Z0-9$#@") ||
      !strcmp(spec,"A-Z0-9$@#") ||
      !strcmp(spec,"0-9A-Z@#$") ||
      !strcmp(spec,"0-9A-Z@$#") ||
      !strcmp(spec,"0-9A-Z#$@") ||
      !strcmp(spec,"0-9A-Z#@$") ||
      !strcmp(spec,"0-9A-Z$#@") ||
      !strcmp(spec,"0-9A-Z$@#") ||
      !strcmp(spec,"A-Z0-9@#\\$") ||
      !strcmp(spec,"A-Z0-9@\\$#") ||
      !strcmp(spec,"A-Z0-9#\\$@") ||
      !strcmp(spec,"A-Z0-9#@\\$") ||
      !strcmp(spec,"A-Z0-9\\$#@") ||
      !strcmp(spec,"A-Z0-9\\$@#") ||
      !strcmp(spec,"0-9A-Z@#\\$") ||
      !strcmp(spec,"0-9A-Z@\\$#") ||
      !strcmp(spec,"0-9A-Z#\\$@") ||
      !strcmp(spec,"0-9A-Z#@\\$") ||
      !strcmp(spec,"0-9A-Z\\$#@") ||
      !strcmp(spec,"0-9A-Z\\$@#")){
    return makePredefinedClassExpr(context,TEXT_TEST_ALPHANUMERIC_OR_NATIONAL,negated);
  } else{
    /* dis the general hyphen case, support bag-o-chars */
    noteParseError(context,"unsupported regular expression character class %s",spec);
    return NULL;
  }
}

#define REP_STATE_INITIAL 1
#define REP_STATE_DIGITS1 2
#define REP_STATE_COMMA   3
#define REP_STATE_DIGITS2 4
#define REP_STATE_FINAL   5

#define MAX_REPETITIONS 256

static TextExpr *parseRepetitionSpec(TextContext *context, int end){
  int i;
  int state = REP_STATE_INITIAL;
  int escaped = 0;
  int minValue = 0;
  int maxValue = UNLIMITED_REPETITION;
  if (context->pos == end){
    noteParseError(context,"empty regular expression repetition spec");
    return NULL;
  }

  while (context->pos < end){
    if (state == REP_STATE_FINAL){
      break;
    }
    int c = nextChar(context,end,&escaped,"repetition spec loop");
    if (context->parseTrace){
      printf("parse rep state=%d, c=%c\n",state,c);
      fflush(stdout);
    }
    switch (state){
    case REP_STATE_INITIAL:
      if (isDigit(c)){
	minValue = c-'0';
	state = REP_STATE_DIGITS1;
      } else{
	noteParseError(context,"bad regular expression repetition specifier");
	return NULL;
      }
      break;
    case REP_STATE_DIGITS1:
      if (c == ','){
	state = REP_STATE_COMMA;
      } else if (isDigit(c)){
	minValue = (minValue*10)+(c-'0');
      } else{
	noteParseError(context,"non-numeric repetition spec");
	return NULL;
      }
      break;
    case REP_STATE_COMMA:
      if (c == '}'){   /* min only */
	state = REP_STATE_FINAL;
      } else if (isDigit(c)){
	maxValue = c-'0';
	state = REP_STATE_DIGITS2;
      } else{
	noteParseError(context,"unexpected character in repetition spec: %c",c);
	return NULL;
      }
      break;
    case REP_STATE_DIGITS2:
      if (c == '}'){   /* done */
	state = REP_STATE_FINAL;
      } else if (isDigit(c)){
	maxValue = (maxValue*10)+(c-'0');
      } else{
	noteParseError(context,"non-numeric repitition spec");
	return NULL;
      }
      break;
    }
  }
  if (state == REP_STATE_DIGITS1){ /* max = min */
    maxValue = minValue;
  }
  if ((maxValue >= 0) && (maxValue < minValue)){
    noteParseError(context,"max repetitions must be greater or equal to min repetitions");
    return NULL;
  } else if (maxValue == 0 && minValue == 0){
    noteParseError(context,"zero repetitions specified");
    return NULL;
  } else if (maxValue > MAX_REPETITIONS){
    noteParseError(context,"max repetitions too high");
    return NULL;
  } else if (minValue < -2){
    noteParseError(context,"min repetitions out of range");
    return NULL;
  } else{
    TextExpr *repetitionExpr = makeTextExpr(context,TEXT_EXPR_REPETITION);
    repetitionExpr->minValue = minValue;
    repetitionExpr->maxValue = maxValue;
    if (context->parseTrace){
      printf("normal case min=%d max=%d\n",minValue,maxValue);
      fflush(stdout);
    }
    return repetitionExpr;
  }
}


static TextExpr *parseAlternation(TextContext *context, int end){
  TextExpr *altExpr = NULL;
  TextExpr *currentSeq = makeTextExpr(context,TEXT_EXPR_SEQ);
  while (TRUE){
    int escaped = FALSE;
    int c = nextChar(context,end,&escaped,"outer loop");
    if (c == -1){
      break;
    }
    if (escaped){
      /* all the special character cases, for PERL compatibility */
      if (c == 'd'){ 
	addChild(currentSeq,makePredefinedClassExpr(context,TEXT_TEST_DIGIT,FALSE));
      } else if (c == 'D'){ 
	addChild(currentSeq,makePredefinedClassExpr(context,TEXT_TEST_DIGIT,TRUE));
      } else if (c == 'h'){ 
	addChild(currentSeq,makePredefinedClassExpr(context,TEXT_TEST_HEXDIGIT,FALSE));
      } else if (c == 'H'){ 
	addChild(currentSeq,makePredefinedClassExpr(context,TEXT_TEST_HEXDIGIT,TRUE));
      } else if (c == 's'){ 
	addChild(currentSeq,makePredefinedClassExpr(context,TEXT_TEST_WHITESPACE,FALSE));
      } else if (c == 'S'){ 
	addChild(currentSeq,makePredefinedClassExpr(context,TEXT_TEST_WHITESPACE,TRUE));
      } else if (c == 'w'){ 
	addChild(currentSeq,makePredefinedClassExpr(context,TEXT_TEST_WORDCHAR,FALSE));
      } else if (c == 'W'){ 
	addChild(currentSeq,makePredefinedClassExpr(context,TEXT_TEST_WORDCHAR,TRUE));
      } else{
	TextExpr *charExpr = makeTextExpr(context,TEXT_EXPR_CHAR);
	charExpr->c = c;
	addChild(currentSeq,charExpr);
      }
    } else if (c == '|' || c == 0x6A){
      if (!altExpr){
	altExpr = makeTextExpr(context,TEXT_EXPR_ALT);
	addChild(altExpr,currentSeq);
      }
      currentSeq = makeTextExpr(context,TEXT_EXPR_SEQ);
      addChild(altExpr,currentSeq);
    } else if (c == '*' || c == '+' || c == '?'){
      /* if current sequence empty error, else replace last element with wrapped version of self */
      if (currentSeq->firstChild == NULL){
	noteParseError(context,"cannot apply postfix operator to empty sequence");
	return NULL;
      } else{
	TextExpr *wrapper = makeTextExpr(context,(c == '*' ? 
						  TEXT_EXPR_STAR :
						  (c == '+' ? TEXT_EXPR_PLUS : TEXT_EXPR_QMARK)));
	wrapLastChild(currentSeq,wrapper);
      }
    } else if (c == '{'){
      int closePos = findMatchingClose(context->s,'{','}',context->pos,end);
      if (closePos == -1){
	noteParseError(context,"unmatched { at position %d",context->pos);
	return NULL;
      } else if (currentSeq->firstChild == NULL){
	noteParseError(context,"cannot apply repetion operator to empty sequence");
	return NULL;
      } else{
	TextExpr *repSpec = parseRepetitionSpec(context,closePos);
	if (repSpec){
	  TextExpr *wrapper = NULL;
	  context->pos = closePos+1;
	  int i;
	  int requiredReps;
	  int optionalReps;
	  int done = FALSE;
	  if (repSpec->minValue == 1 && repSpec->maxValue == UNLIMITED_REPETITION){
	    wrapper = makeTextExpr(context,TEXT_EXPR_PLUS);
	    wrapLastChild(currentSeq,wrapper);
	    done = TRUE;
	  } else if (repSpec->minValue == 0){
	    if (repSpec->maxValue == UNLIMITED_REPETITION){ /* same as star */
	      wrapper = makeTextExpr(context,TEXT_EXPR_STAR);
	      wrapLastChild(currentSeq,wrapper);
	      done = TRUE;
	    } else if (repSpec->maxValue == 1){
	      wrapper = makeTextExpr(context,TEXT_EXPR_QMARK);
	      wrapLastChild(currentSeq,wrapper);
	      done = TRUE;
	    } else{
	      requiredReps = 0;
	      optionalReps = repSpec->maxValue-1;
	      wrapper = makeTextExpr(context,TEXT_EXPR_QMARK);
	      wrapLastChild(currentSeq,wrapper);
	    }
	  } else{
	    requiredReps = repSpec->minValue-1; /* don't wrap the one in the sequence already */
	    if (repSpec->maxValue == UNLIMITED_REPETITION){
	      optionalReps = UNLIMITED_REPETITION;
	    } else{
	      optionalReps = repSpec->maxValue-repSpec->minValue;
	    }
	    
	    if (context->parseTrace){
	      printf("reqReps=%d optReps = %d\n",requiredReps,optionalReps);
	      fflush(stdout);
	    }
	  }
	  if (!done){
	    TextExpr *lastChild = getLastChild(currentSeq);
	    for (i=0; i<requiredReps; i++){
	      addChild(currentSeq,shallowCloneTextExpr(context,lastChild));
	    }
	    if (context->parseTrace){
	      printf("after req rep loop\n");
	      fflush(stdout);
	    }
	    if (optionalReps == UNLIMITED_REPETITION){
	      if (context->parseTrace){
		printf("optional reps unlimited\n");
		fflush(stdout);
	      }
	      addChild(currentSeq,shallowCloneTextExpr(context,lastChild));
	      wrapper = makeTextExpr(context,TEXT_EXPR_STAR);
	      wrapLastChild(currentSeq,wrapper);
	    } else{
	      if (context->parseTrace){
		printf("optional rep loop to %d\n",optionalReps);
		fflush(stdout);
	      }
	      for (i=0; i<optionalReps; i++){
		addChild(currentSeq,shallowCloneTextExpr(context,lastChild));
		wrapper = makeTextExpr(context,TEXT_EXPR_QMARK);
		wrapLastChild(currentSeq,wrapper);
	      }
	    }
	    if (context->parseTrace){
	      printf("after opt rep star or loop\n");
	      fflush(stdout);
	    }
	  }
	} else{
	  /* The error will be noted by parseRepSpec */
	  return NULL;
	}      
      }
    } else if ((c == '[') || (c == 0xBA)){  /* CCSID 37 Variants */
      int closePos = findMatchingClose(context->s,'[',']',context->pos,end);
      if (closePos == -1){
	noteParseError(context,"unmatched [ at position %d",context->pos);
	return NULL;
      } else{
	TextExpr *subExpr = parseCharClass(context,closePos);
	if (subExpr){
	  context->pos = closePos+1;
	  addChild(currentSeq,subExpr);
	} else{
	  /* The error will be noted by parseCharClass */
	  return NULL;
	}
      }      
    } else if (c == '('){
      int closePos = findMatchingClose(context->s,'(',')',context->pos,end);
      if (closePos == -1){
	noteParseError(context,"unmatched ( at position %d",context->pos);
	return NULL;
      } else{
	TextExpr *subExpr = parseAlternation(context,closePos);
	if (subExpr){
	  TextExpr *bindingExpr = makeTextExpr(context,TEXT_EXPR_BIND);
	  addChild(bindingExpr,subExpr);
	  context->pos = closePos+1;
	  addChild(currentSeq,bindingExpr);
	} else{
	  /* The error will be noted by parse alternation */
	  return NULL;
	}
      }
    } else if (c == '.'){
      /* unescaped special cases */
      addChild(currentSeq,makePredefinedClassExpr(context,TEXT_TEST_DOT,FALSE));
    } else{
      TextExpr *charExpr = makeTextExpr(context,TEXT_EXPR_CHAR);
      charExpr->c = c;
      addChild(currentSeq,charExpr);
    }
  }
  return (altExpr ? altExpr : currentSeq);
}

static void dumpNDFA(TextContext *context){
  int i;
  char buffer[256];
  for (i=0; i<context->highestNDFAIndex; i++){
    NDFAState *state = context->ndfaStates[i];
    printf("NDFA STATE %d %s ",state->index,(state->flags & FA_TERMINAL ? "TERMINAL" : ""));
    if (state->bindingStart){
      printf("bindingStart=%d ",state->bindingStart);
    }
    if (state->bindingEnd){
      printf("bindingEnd=%d ",state->bindingEnd);
    }
    printf("\n");
    Transition *t = state->first;
    printf ("Transition=0x%x, state=0x%x\n", t, state);
    while (t){
      NDFAState *nextState = (NDFAState*)t->nextState;
      if (t->test == TEXT_TEST_EPSILON){
	printf("  epsilon to %d\n",nextState->index);
      } else if (t->test == TEXT_TEST_CHAR){
	printf("  char %c to %d\n",t->c,nextState->index);
      } else{
	printf("  %s to %d\n",textTestDescription(t,buffer),nextState->index);
      }
      t = t->sibling;
    }
  } 
}

static int matchTransition(Transition *t, char c){
  switch (t->test){
  case TEXT_TEST_CHAR:
    return (t->c == c);
  case TEXT_TEST_ALPHA:
    return isAlpha(c);
  case TEXT_TEST_ALPHALOWER:
    return isLower(c);
  case TEXT_TEST_ALPHAUPPER:
    return isUpper(c);
  case TEXT_TEST_ALPHANUMERIC:
    return isAlphaNumeric(c);
  case TEXT_TEST_ALPHANUMERICUPPER:
    return isAlphaNumericUpper(c);
  case TEXT_TEST_ALPHANUMERICLOWER:
    return isAlphaNumericLower(c);
  case TEXT_TEST_WORDCHAR:
    return isWordChar(c);
  case TEXT_TEST_DIGIT:
    return isDigit(c);
  case TEXT_TEST_HEXDIGIT:
    return isHexDigit(c);
  case TEXT_TEST_HEXDIGITLOWER:
    return isHexLower(c);
  case TEXT_TEST_HEXDIGITUPPER:
    return isHexUpper(c);
  case TEXT_TEST_BLANK:
    return isBlank(c);
  case TEXT_TEST_WHITESPACE:
    return isWhitespace(c);
  case TEXT_TEST_ALPHANUMERIC_OR_NATIONAL:
    return isAlphaNumericOrNational(c);
  case TEXT_TEST_DOT:
    return TRUE;
  default:
    printf("implement test type %d\n",t->test);
    return 0;
  }
}

DFAMatcher *makeDFAMatcher(DFA *dfa){
  int bindingCount = dfa->bindingCount;
  int totalSize = sizeof(DFAMatcher)+(sizeof(DFABinding)*(2*bindingCount));
  DFAMatcher *matcher = (DFAMatcher*)safeMalloc(totalSize,"DFA Matcher");
  memset(matcher,0,sizeof(DFAMatcher));
  matcher->dfa = dfa;
  matcher->totalSize = totalSize;
  matcher->bindingCount = bindingCount;
  /* there are two binding sets here, one for current work, and another for a save area for longest match */
  /*
  printf("matcher at 0x%x, sizeof=0x%x totalSize=0x%x binding count = %d\n",matcher,sizeof(DFAMatcher),totalSize,matcher->bindingCount); 
    */
  return matcher;
}

void freeDFAMatcher(DFAMatcher *matcher){
  safeFree((char*)matcher,matcher->totalSize);
}

static DFABinding *getBindings(DFAMatcher *matcher){
  return (&(matcher->firstBinding));
}

DFABinding *getDFABinding(DFAMatcher *matcher, int index){
  DFABinding *bindings = (DFABinding*)&(matcher->firstBinding);
  if (matcher->trace) {
    printf("getDFABinding: index=0x%x, matcher->bindingCount=0x%x\n",
           index, matcher->bindingCount);
  }
  if (index < matcher->bindingCount){
    DFABinding *binding = &(bindings[index]);
    return binding;
  } else{
    return NULL;
  }
}

static void old_updateBindings(DFAMatcher *matcher, DFAState *state, int position, int isStart, int specificBinding){
  DFABinding *bindings = (DFABinding*)&(matcher->firstBinding);
  IntListMember *set = (specificBinding ? NULL : (isStart ? state->bindingStarts : state->bindingEnds));
  /* hacky loop or singleton iteration.  God save me */
  while (specificBinding || set){
    int bindingIndex = specificBinding ? specificBinding : set->x;
    if (matcher->trace){ 
      printf("update binding %d of state=%d position=%d (%s)\n",bindingIndex,state->index,position,(isStart ? "START" : "END"));
      fflush(stdout);
    }
    DFABinding *binding = &(bindings[bindingIndex]);
    if (isStart){
      if (binding->nextOpenStart == -1){ /* keep leftmost start */
        binding->nextOpenStart = position;
      }
    } else{
      binding->start = binding->nextOpenStart;
      binding->end = position;
    }
    specificBinding = 0;
    set = set->next;
  }
}

static void updateBindings(DFAMatcher *matcher, DFAState *state, int position, int isStart, int specificBinding){
  DFABinding *bindings = (DFABinding*)&(matcher->firstBinding);
  IntListMember *set = (specificBinding ? NULL : (isStart ? state->bindingStarts : state->bindingEnds));
  /* hacky loop or singleton iteration.  God save me */
  while (specificBinding || set){
    int bindingIndex = specificBinding ? specificBinding : set->x;
    if (matcher->trace){ 
      printf("update binding %d of state=%d position=%d (%s)\n",bindingIndex,state->index,position,(isStart ? "START" : "END"));
      fflush(stdout);
    }
    DFABinding *binding = &(bindings[bindingIndex]);
    if (isStart){
      binding->nextOpenStart = position;
    } else{
      binding->start = binding->nextOpenStart;
      binding->end = position;
    }
    specificBinding = 0;
    if (set) {
      set = set->next;
    }
  }
}

static void clearSomeBindings(DFAMatcher *matcher, DFAState *state){
  DFABinding *bindings = (DFABinding*)&(matcher->firstBinding);
  IntListMember *fromSet = state->bindingClearFroms;
  IntListMember *toSet = state->bindingClearTos;
  while (fromSet){
    int from=fromSet->x;
    int to  =toSet->x;
    int i;
    for (i=from; i<to; i++){
      DFABinding *binding = &(bindings[i]);
      binding->nextOpenStart = -1;
      binding->start = -1;
    }
    fromSet = fromSet->next;
    toSet = toSet->next;
  }
}

static void applyMinimumBindingLengths(DFAMatcher *matcher, DFA *dfa){
  DFABinding *bindings = (DFABinding*)&(matcher->firstBinding);
  int i;
  for (i=0; i<matcher->bindingCount; i++){
    DFABinding *binding = &(bindings[i]);
    if (binding->end != -1) {                        /* is set */
      int minimum = dfa->minimumBindingLengths[i];
      int len = (binding->end - binding->start);
      if (len < minimum){
        binding->start = binding->end - minimum;
      }
    }
  }
}

static void saveBindingsForLongestMatch(DFAMatcher *matcher){
  DFABinding *workBindings = (DFABinding*)&(matcher->firstBinding);
  DFABinding *longestMatchBindings = workBindings+matcher->bindingCount;
  memcpy(longestMatchBindings,workBindings,sizeof(DFABinding)*(matcher->bindingCount));
}

static void restoreBindingsForLongestMatch(DFAMatcher *matcher){
  DFABinding *workBindings = (DFABinding*)&(matcher->firstBinding);
  DFABinding *longestMatchBindings = workBindings+matcher->bindingCount;
  memcpy(workBindings,longestMatchBindings,sizeof(DFABinding)*(matcher->bindingCount));
}

void dumpBindings(DFAMatcher *matcher){
  DFABinding *bindings = &(matcher->firstBinding);
  int i;
  for (i=0; i<= matcher->bindingCount; i++){
    DFABinding *binding = &(bindings[i]);
    if (binding->start >= 0 && binding->end >= 0){
      printf("    %d: (%d,%d)\n",i,binding->start,binding->end);
    }
  }
}

int dfaMatch(DFAMatcher *matcher, char *s, int len, int matchWholeString){
  DFA *dfa = matcher->dfa;
  DFAState *initialState = dfa->states[0];
  DFAState *state = initialState;
  int i;
  matcher->longestMatch = 0;
  DFABinding *bindings = &(matcher->firstBinding);
  for (i=0; i<= matcher->bindingCount; i++){
    DFABinding *binding = &(bindings[i]);
    binding->start = -1;
    binding->end = -1;
    binding->nextOpenStart = -1;
  }
  for (i=0; i<len; i++){
    if ((state == initialState) && (i == 0)){
      updateBindings(matcher,state,i,TRUE,0);
    }
    char c = s[i];
    if (matcher->trace){
      printf("i=%d c=%c in state %d\n",i,c,state->index);fflush(stdout);
    }
    Transition *t = state->firstTransition;
    DFAState *previousState = state;
    int matchedSomeTransition = FALSE;
    while (t){
      char buffer[256];
      if (matcher->trace){
	printf("match transition t=0x%x %s\n",t,textTestDescription(t,buffer));
	fflush(stdout);
      }
      if (matchTransition(t,c)){
        if (t->bindingStart){
          if (matcher->trace){
            printf("transition binding seen bindIndex=%d\n",t->bindingStart);
          }
          updateBindings(matcher,state,i,TRUE,t->bindingStart); 
        }
	state = (DFAState*)t->nextState;
        /* The theory is to clear some bindings on "backwards" branches 
           this is the first attempt */
        if (FALSE) { /* state->index < previousState->index){ */
          if (matcher->trace){
            printf("backwards-ish branch!\n");
          }
          if (state->bindingClearFroms){
            clearSomeBindings(matcher,state);
          }
        }
	matchedSomeTransition = TRUE;
	break;
      }
      t = t->sibling;
    }
    if (matcher->trace){
      printf("done with transitions\n");fflush(stdout);
    }
    if (matchedSomeTransition){
      updateBindings(matcher,state,i+1,FALSE,0);
      if (state->flags & FA_TERMINAL){
        matcher->matchedState = state;
        matcher->longestMatch = i+1;
        saveBindingsForLongestMatch(matcher);
      }
    } else {
      if (matcher->trace){
	printf("matching ended with longest match was %d\n",matcher->longestMatch);
      }
      break;
    }
  }
  if (matchWholeString){
    if (matcher->trace){
      printf("longest match was %d\n",matcher->longestMatch);
      fflush(stdout);
    }
    if (state->flags & FA_TERMINAL){
      matcher->matchedState = state;
      return i;
    } else{
      return 0;
    }
  } else{
    restoreBindingsForLongestMatch(matcher);
    applyMinimumBindingLengths(matcher,dfa);
    return matcher->longestMatch;
  }
}

TextContext *makeRegexCompilationContext(int maxPatterns){
  ShortLivedHeap *slh = makeShortLivedHeap(65536,1000);
  TextContext *context = (TextContext*)SLHAlloc(slh,sizeof(TextContext));
  memset(context,0,sizeof(TextContext));
  context->slh = slh;
  context->patterns = (char**)SLHAlloc(slh,maxPatterns*sizeof(char*));
  context->patternCount = 0;
  context->ndfaStateVectorSize = NDFA_VECTOR_SIZE;
  context->ndfaStates = (NDFAState**)SLHAlloc(slh,sizeof(NDFAState*)*NDFA_VECTOR_SIZE);
  context->closureStackSize = 1000;
  context->highestTerminalIndex = 0;
  context->highestBindingIndex = 1; /* 0 is reserved for the whole match */
  context->bindingCount = 1;        /* ditto */
  context->closureStack = (int*)SLHAlloc(context->slh,context->closureStackSize*sizeof(int*));
  context->closureToDFAStateTable = htCreate(257,dfaStateHash,dfaStateCompare,NULL,NULL);
  context->intSetManager = fbMgrCreate(sizeof(IntListMember),1024,context);
  context->maxDFAStates = 100000;
  return context;
}

void freeRegexCompilationContext(TextContext *context){
  if (context->parseTrace || context->compileTrace){
    printf("freeRegexContext 1 context=0x%x slh=0x%x\n",context,context->slh);
  }
  ShortLivedHeap *slh = context->slh;
  if (context->parseTrace || context->compileTrace){
  printf("freeRegexContext 2\n");
  }
  htDestroy(context->closureToDFAStateTable);
  fbMgrDestroy(context->intSetManager);
  if (context->parseTrace || context->compileTrace){
  printf("freeRegexContext 3\n");
  }
  SLHFree(slh);
}

int addRegexPattern(TextContext *context, char *pattern){
  context->patterns[context->patternCount++] = pattern;
}

typedef struct CommonSubPattern_tag{
  int len; /* length into pater */
  TextExpr *expr;
  int patternCount;
  int *patterns;
} CommonSubPattern;

static int findCommonSubExpressions(TextContext *context, TextExpr **expressions){
  int i,count = context->patternCount;

  if (expressions[0]->type != TEXT_EXPR_SEQ){
    return 0;
  }
  int l = 0;
  while (TRUE){
    int allMatchForPosition = TRUE;;
    TextExpr *firstSubExpression = getNthSubExpression(expressions[0],l);

    for (i=1; i<count; i++){
      TextExpr *expr = expressions[i];

      TextExpr *nthSubExpr = getNthSubExpression(expr,l);
      printf("comparing at %d first type=%d nth type=%d\n",
	     l,firstSubExpression->type,nthSubExpr->type);
      if ((expr->type != TEXT_EXPR_SEQ) ||
	  (firstSubExpression ? 
	   !textExprEquals(nthSubExpr,firstSubExpression) :
	   nthSubExpr == NULL)){
	allMatchForPosition = FALSE;
	break;
      }
    }
    if (allMatchForPosition && firstSubExpression){
      l++;
    } else{
      break;
    }
  }
  return l;
}



static int isBindingReachableFromState(TextContext *context, DFA *dfa, DFAState *startingState, int bindingNumber){
  int stackPtr = 0;
  int stackSize = context->closureStackSize;
  DFAState **stack = (DFAState**)safeMalloc(sizeof(DFAState*)*stackSize,"DFA State stack");
  int stateCount = dfa->stateCount;
  int i;
  DFAState **states = dfa->states;
  char buffer[256];

  /* marked will mean have you been placed in the stack ever */
  for (i=0; i<stateCount; i++){
    states[i]->marked = FALSE;
  }

  stack[stackPtr++] = startingState;
  startingState->marked = TRUE;
  while (stackPtr > 0){
    DFAState *state = stack[--stackPtr];
    /* printf("  popped index is %d with stackPtr %d\n",state->index,stackPtr); */
    if (state->index == 0){
      if (intSetContains(state->bindingStarts,bindingNumber)){
        return TRUE;
      }
    }
    Transition *t = state->firstTransition;
    while (t){
      /* printf("    look at transition t=0x%x %s\n",t,textTestDescription(t,buffer)); */
      if (t->bindingStart == bindingNumber){
        return TRUE;
      }
      DFAState *nextState = (DFAState*)t->nextState;
      if (nextState->marked == FALSE){
        stack[stackPtr++] = nextState;
        nextState->marked = TRUE;
      }
      t = t->sibling;
    }
  }
  return FALSE;
}

static void testBindings(TextContext *context, DFA *dfa){
  int i;
  int j;
  
  printf("Validating bindings for reachability, bindCount = %d\n",dfa->bindingCount);
  for (i=1; i< dfa->bindingCount; i++){
    for (j=0; j<dfa->stateCount; j++){
      DFAState *state = dfa->states[j];
      int reachable = isBindingReachableFromState(context,dfa,state,i);
      printf("binding %d reachable from state %d? %s\n",
             i,state->index,(reachable ? "YES" : "NO"));
    }
  }
}


DFA *compileRegex(TextContext *context){
  int i;
  NDFAState *initialState = makeNDFAState(context,FA_INITIAL);
  TextExpr **expressions = (TextExpr**)SLHAlloc(context->slh,context->patternCount*sizeof(TextExpr*));
  for (i=0; i<context->patternCount; i++){
    char *pattern = context->patterns[i];
    if (context->parseTrace){
      printf("about to parse pattern %d '%s'\n",i,pattern);
    }
    context->s = pattern;
    context->pos = 0;
    TextExpr *expr = parseAlternation(context,strlen(pattern));
    context->bindingCount += countBindings(expr);
    expressions[i] = expr;
    if (expr){
      if (context->compileTrace > 0){
	pprintTextExpr(expr,0);   
      }
      context->minimumBindingLengths = (int*)SLHAlloc(context->slh,context->bindingCount*sizeof(int*));
      memset((char*)(context->minimumBindingLengths),0,context->bindingCount*sizeof(int*));
      context->terminalBindingOffsets = (int*)SLHAlloc(context->slh,context->patternCount*sizeof(int*));
      memset((char*)(context->terminalBindingOffsets),0,context->patternCount*sizeof(int*));

    } else{
      return NULL;
    }
  }
  /* Let there be an equality predicate for TextExpr, TXEQ(a,b)
     let there be boolean needsExploration[<patternCount>]
     let there be set commonSubPatterns starting as empty
       CommonSubPattern:
          length
          TextExpr: common pieces
          IntList - indices sharing start
     for each pattern
       if (pattern.type = SEQ and patternLen > 1) THEN BEGIN 
         if (any(TXEQ(subPattern[0]),commonSubPatterns)){
	   addToIntList for specific common subPatterns.
         }
     while (any commonSubPattern member list cardinality > 1) {
       for subPattern members
         if (patternsequenceLength > subPatternLength) {
           getOrMakeNewSubPattern and move member from existing set to new set.
         }
         leave behind stragglers in copy or old commonSubPattern
       }
     }
     END
   */
  if (context->experimental && (context->patternCount > 0)){
    int l = findCommonSubExpressions(context,expressions);
    printf("expressions are common for length %d\n",l);fflush(stdout);
    if (l > 0){
      NDFAState *commonMidState = makeNDFAState(context,0);
      TextExpr **tails = (TextExpr**)SLHAlloc(context->slh,context->patternCount*sizeof(TextExpr*));
      TextExpr *bigAlt = makeTextExpr(context,TEXT_EXPR_ALT);
      for (i=0; i<context->patternCount; i++){
	tails[i] = nthTailExpr(context,expressions[i],l);
	/* addChild(bigAlt,tails[i]); */
      }
      TextExpr *sub = getNthSubExpression(expressions[0],l-1);
      sub->nextSibling = NULL; /* bigAlt; */
      TextExpr *commonPrefix = expressions[0];
      printf("about to pprint\n");fflush(stdout);
      pprintTextExpr(commonPrefix,0);
	fflush(stdout);

      buildNDFA(context,commonPrefix,initialState,commonMidState,NULL);
      for (i=0; i<context->patternCount; i++){
	printf("tail buildNDFA loop %d\n",i);fflush(stdout);
	NDFAState *terminalState = makeNDFAState(context,FA_TERMINAL);
	buildNDFA(context,tails[i],commonMidState,terminalState,NULL);
      }
	printf("___________________________________________\n");
	dumpNDFA(context);
	fflush(stdout);
	printf("___________________________________________\n");
      printf("about to compile simplified expression\n");fflush(stdout);
      }
    /* how to make separate state for each alt */
  } else{
    for (i=0; i<context->patternCount; i++){
      NDFAState *terminalState = makeNDFAState(context,FA_TERMINAL);
      int firstNDFAIndex = context->highestNDFAIndex;
      context->terminalBindingOffsets[i] = (context->highestBindingIndex - 1); /* because bindings are one-based */
      buildNDFA(context,expressions[i],initialState,terminalState,NULL);
      if (context->parseTrace){
        printf("maximum binding clear index for pattern %d\n",context->highestBindingIndex);
      }
      int n=0;
      for (n=firstNDFAIndex; n<context->highestNDFAIndex; n++){
        NDFAState *ndfaState = context->ndfaStates[n];
        if (ndfaState->bindingClearFrom){
          if (ndfaState->bindingClearFrom < context->highestBindingIndex){
            ndfaState->bindingClearTo = context->highestBindingIndex;
            if (context->parseTrace){
              printf("NDFA state %d should clear bindings from %d to %d (incl,excl)\n",
                     ndfaState->index,ndfaState->bindingClearFrom,ndfaState->bindingClearTo);
            }
          } else{
            /* no states to clear, kill it */
            ndfaState->bindingClearFrom = 0;
          }
        }
      }
    }
  }
  if (context->compileTrace > 0){
    fflush(stdout);
    printf("___________________________________________\n");
    dumpNDFA(context);
    fflush(stdout);
    printf("___________________________________________\n");
  }
//  printf ("textmatch compileTrace = %d\n", context->compileTrace);
  DFA *dfa = constructDFA(context);
  if (context->compileTrace > 0){
    if (dfa){
      printf("________FINAL DFA (stateCount=%d)_________\n",dfa->stateCount);
      dumpDFA(dfa,TRUE);
    } else{
      printf("dfa was too complex/ambiguous maxDFAStates=%d rc=%d dc=%d\n",
	     context->maxDFAStates,context->retainedClosures,context->droppedClosures);
    }
  }
  if (dfa){
    dfa->minimumBindingLengths = (int*)safeMalloc(dfa->bindingCount * sizeof(int*), "DFA min bindings");
    memcpy((char*)dfa->minimumBindingLengths,(char*)context->minimumBindingLengths,dfa->bindingCount * sizeof(int*));
    dfa->terminalCount = context->highestTerminalIndex;
    dfa->terminalBindingOffsets = (int*)safeMalloc(sizeof(int*)*context->highestTerminalIndex,"DFA terminal binding offsets");
    memcpy((char*)dfa->terminalBindingOffsets,(char*)context->terminalBindingOffsets,sizeof(int*)*context->highestTerminalIndex);
    int t;
    if (context->compileTrace){
      for (t = 0; t<context->highestTerminalIndex; t++){
        printf("terminal binding offset %d is %d\n",t,dfa->terminalBindingOffsets[t]);
      }
    }
  }
  return dfa;
}
