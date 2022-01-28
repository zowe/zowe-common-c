

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __TEXTMATCH__
#define __TEXTMATCH__ 1


#define NO_MATCH (-1)
#define BAD_PATTERN (-2)


typedef struct Transition_tag{
  char eyecatcher[4];
  int test;
  char c;
  char res1;
  short res2;
  char *data;    /* NULL for predefined, set of chars for CHARSET  */
  void *nextState;
  int bindingStart;
  struct Transition_tag *sibling;
} Transition;

typedef struct IntListMember_tag{
  int x;
  struct IntListMember_tag *next;
} IntListMember;


#define FA_INITIAL  1
#define FA_TERMINAL 2

typedef struct NDFAState_tag{
  int index;
  int flags;
  Transition *first;
  int visited;
  int bindingStart;
  int bindingEnd;
  int bindingClearFrom;
  int bindingClearTo;
  int terminalIndex;
} NDFAState;

typedef struct EClosure_tag{
  int stackSize;
  int stackPtr;
  int *stack;
  int memberCount;
  IntListMember *stateIndices;
} EClosure;


typedef struct TextContext_tag{
  ShortLivedHeap *slh;
  int patternCount;
  char **patterns;
  char *s; /* what we currently are parsing */
  int pos;
  int lastPos;
  int highestNDFAIndex;
  int ndfaStateVectorSize;
  int parseTrace;
  int compileTrace;
  NDFAState **ndfaStates;
  int closureStackSize;
  int closureStackPtr;
  int *closureStack;
  int highestDFAIndex;
  int bindingCount; /* incremented during parse */
  int highestBindingIndex; /* incremented during buildNDFA */
  int highestTerminalIndex;
  int *minimumBindingLengths;
  int *terminalBindingOffsets;
  hashtable *closureToDFAStateTable;
  fixedBlockMgr *intSetManager;
  int maxDFAStates;
  char *error; /* SLH allocated */
  int retainedClosures;
  int droppedClosures;
  int experimental;
} TextContext;

typedef struct DFAState_tag{
  char eyecatcher[4];
  int index;
  int flags;
  int terminalIndex; /* 0 for not a terminal */
  int ndfaStateCount;
  int *ndfaStates;
  int marked;
  IntListMember *bindingStarts;
  IntListMember *bindingEnds;
  IntListMember *bindingClearFroms;
  IntListMember *bindingClearTos;
  IntListMember *terminalIndices;
  Transition *firstTransition;
} DFAState;

typedef struct DFATerminal_tag{
  int type;
} DFATerminal;

typedef struct DFA_tag{
  int stateCount;
  int stateVectorSize;
  DFAState **states;
  int terminalCount;
  int bindingCount;
  int *minimumBindingLengths;
  int *terminalBindingOffsets; /* equal in size of NDFA terminal state count */
} DFA;

/* DFA is thread safe and read only.
   Matcher is not thread safe and has context of one match in progress */

typedef struct DFABinding_tag{
  int start;
  int end;
  int nextOpenStart;
} DFABinding;

typedef struct DFAMatcher_tag{
  DFA *dfa;
  int totalSize;
  int longestMatch;
  int bindingCount;    /* Group 0, the whole pattern, the rest are for each parenthesis, left to right */
  int trace;
  DFAState *matchedState;
  DFABinding firstBinding;  
} DFAMatcher;

#define getDFABinding GTDFABND

void pprintIntSet(IntListMember *set, char *prefix);
TextContext *makeRegexCompilationContext(int maxPatterns);
int addRegexPattern(TextContext *context, char *pattern);
DFA *compileRegex(TextContext *context);
int dfaMatch(DFAMatcher *matcher, char *s, int len, int matchWholeString);
DFAMatcher *makeDFAMatcher(DFA *dfa);
void freeDFAMatcher(DFAMatcher *matcher);
void dumpBindings(DFAMatcher *matcher);
DFABinding *getDFABinding(DFAMatcher *matcher, int index);

#endif
