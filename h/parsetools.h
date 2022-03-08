/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_PARSETOOLS__
#define __ZOWE_PARSETOOLS__ 1

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "json.h"

/* Common Tokenization/Lexing Infrastucture */

#define EOF_TOKEN -1
#define NO_VALID_TOKEN 0
#define FIRST_REAL_TOKEN_ID 1


#define CPROP_ALPHA   0x0001
#define CPROP_DIGIT   0x0002
#define CPROP_HEX     0x0004
#define CPROP_CONTROL 0x0008
#define CPROP_LETTER  0x0010
#define CPROP_LOWER   0x0020
#define CPROP_WHITE   0x0040

static int charProps[256] =  {
  CPROP_CONTROL, /* 0x0 */
  CPROP_CONTROL, /* 0x1 */
  CPROP_CONTROL, /* 0x2 */
  CPROP_CONTROL, /* 0x3 */
  CPROP_CONTROL, /* 0x4 */
  CPROP_CONTROL, /* 0x5 */
  CPROP_CONTROL, /* 0x6 */
  CPROP_CONTROL, /* 0x7 */
  CPROP_CONTROL, /* 0x8 */
  CPROP_CONTROL  |CPROP_WHITE, /* 0x9 */
  CPROP_CONTROL  |CPROP_WHITE, /* 0xa */
  CPROP_CONTROL  |CPROP_WHITE, /* 0xb */
  CPROP_CONTROL  |CPROP_WHITE, /* 0xc */
  CPROP_CONTROL  |CPROP_WHITE, /* 0xd */
  CPROP_CONTROL, /* 0xe */
  CPROP_CONTROL, /* 0xf */
  CPROP_CONTROL, /* 0x10 */
  CPROP_CONTROL, /* 0x11 */
  CPROP_CONTROL, /* 0x12 */
  CPROP_CONTROL, /* 0x13 */
  CPROP_CONTROL, /* 0x14 */
  CPROP_CONTROL, /* 0x15 */
  CPROP_CONTROL, /* 0x16 */
  CPROP_CONTROL, /* 0x17 */
  CPROP_CONTROL, /* 0x18 */
  CPROP_CONTROL, /* 0x19 */
  CPROP_CONTROL, /* 0x1a */
  CPROP_CONTROL, /* 0x1b */
  CPROP_CONTROL  |CPROP_WHITE, /* 0x1c */
  CPROP_CONTROL  |CPROP_WHITE, /* 0x1d */
  CPROP_CONTROL  |CPROP_WHITE, /* 0x1e */
  CPROP_CONTROL  |CPROP_WHITE, /* 0x1f */
  CPROP_WHITE, /* 0x20 */
  0, /* 0x21 */
  0, /* 0x22 */
  0, /* 0x23 */
  0, /* 0x24 */
  0, /* 0x25 */
  0, /* 0x26 */
  0, /* 0x27 */
  0, /* 0x28 */
  0, /* 0x29 */
  0, /* 0x2a */
  0, /* 0x2b */
  0, /* 0x2c */
  0, /* 0x2d */
  0, /* 0x2e */
  0, /* 0x2f */
  CPROP_DIGIT  |CPROP_HEX, /* 0x30 */
  CPROP_DIGIT  |CPROP_HEX, /* 0x31 */
  CPROP_DIGIT  |CPROP_HEX, /* 0x32 */
  CPROP_DIGIT  |CPROP_HEX, /* 0x33 */
  CPROP_DIGIT  |CPROP_HEX, /* 0x34 */
  CPROP_DIGIT  |CPROP_HEX, /* 0x35 */
  CPROP_DIGIT  |CPROP_HEX, /* 0x36 */
  CPROP_DIGIT  |CPROP_HEX, /* 0x37 */
  CPROP_DIGIT  |CPROP_HEX, /* 0x38 */
  CPROP_DIGIT  |CPROP_HEX, /* 0x39 */
  0, /* 0x3a */
  0, /* 0x3b */
  0, /* 0x3c */
  0, /* 0x3d */
  0, /* 0x3e */
  0, /* 0x3f */
  0, /* 0x40 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER, /* 0x41 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER, /* 0x42 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER, /* 0x43 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER, /* 0x44 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER, /* 0x45 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER, /* 0x46 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x47 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x48 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x49 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x4a */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x4b */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x4c */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x4d */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x4e */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x4f */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x50 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x51 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x52 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x53 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x54 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x55 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x56 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x57 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x58 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x59 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0x5a */
  0, /* 0x5b */
  0, /* 0x5c */
  0, /* 0x5d */
  0, /* 0x5e */
  0, /* 0x5f */
  0, /* 0x60 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER  |CPROP_LOWER, /* 0x61 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER  |CPROP_LOWER, /* 0x62 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER  |CPROP_LOWER, /* 0x63 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER  |CPROP_LOWER, /* 0x64 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER  |CPROP_LOWER, /* 0x65 */
  CPROP_ALPHA  |CPROP_HEX  |CPROP_LETTER  |CPROP_LOWER, /* 0x66 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x67 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x68 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x69 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x6a */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x6b */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x6c */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x6d */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x6e */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x6f */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x70 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x71 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x72 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x73 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x74 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x75 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x76 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x77 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x78 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x79 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0x7a */
  0, /* 0x7b */
  0, /* 0x7c */
  0, /* 0x7d */
  0, /* 0x7e */
  CPROP_CONTROL, /* 0x7f */
  CPROP_CONTROL, /* 0x80 */
  CPROP_CONTROL, /* 0x81 */
  CPROP_CONTROL, /* 0x82 */
  CPROP_CONTROL, /* 0x83 */
  CPROP_CONTROL, /* 0x84 */
  CPROP_CONTROL, /* 0x85 */
  CPROP_CONTROL, /* 0x86 */
  CPROP_CONTROL, /* 0x87 */
  CPROP_CONTROL, /* 0x88 */
  CPROP_CONTROL, /* 0x89 */
  CPROP_CONTROL, /* 0x8a */
  CPROP_CONTROL, /* 0x8b */
  CPROP_CONTROL, /* 0x8c */
  CPROP_CONTROL, /* 0x8d */
  CPROP_CONTROL, /* 0x8e */
  CPROP_CONTROL, /* 0x8f */
  CPROP_CONTROL, /* 0x90 */
  CPROP_CONTROL, /* 0x91 */
  CPROP_CONTROL, /* 0x92 */
  CPROP_CONTROL, /* 0x93 */
  CPROP_CONTROL, /* 0x94 */
  CPROP_CONTROL, /* 0x95 */
  CPROP_CONTROL, /* 0x96 */
  CPROP_CONTROL, /* 0x97 */
  CPROP_CONTROL, /* 0x98 */
  CPROP_CONTROL, /* 0x99 */
  CPROP_CONTROL, /* 0x9a */
  CPROP_CONTROL, /* 0x9b */
  CPROP_CONTROL, /* 0x9c */
  CPROP_CONTROL, /* 0x9d */
  CPROP_CONTROL, /* 0x9e */
  CPROP_CONTROL, /* 0x9f */
  0, /* 0xa0 */
  0, /* 0xa1 */
  0, /* 0xa2 */
  0, /* 0xa3 */
  0, /* 0xa4 */
  0, /* 0xa5 */
  0, /* 0xa6 */
  0, /* 0xa7 */
  0, /* 0xa8 */
  0, /* 0xa9 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xaa */
  0, /* 0xab */
  0, /* 0xac */
  0, /* 0xad */
  0, /* 0xae */
  0, /* 0xaf */
  0, /* 0xb0 */
  0, /* 0xb1 */
  0, /* 0xb2 */
  0, /* 0xb3 */
  0, /* 0xb4 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xb5 */
  0, /* 0xb6 */
  0, /* 0xb7 */
  0, /* 0xb8 */
  0, /* 0xb9 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xba */
  0, /* 0xbb */
  0, /* 0xbc */
  0, /* 0xbd */
  0, /* 0xbe */
  0, /* 0xbf */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xc0 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xc1 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xc2 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xc3 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xc4 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xc5 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xc6 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xc7 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xc8 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xc9 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xca */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xcb */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xcc */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xcd */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xce */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xcf */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xd0 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xd1 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xd2 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xd3 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xd4 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xd5 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xd6 */
  0, /* 0xd7 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xd8 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xd9 */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xda */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xdb */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xdc */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xdd */
  CPROP_ALPHA  |CPROP_LETTER, /* 0xde */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xdf */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xe0 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xe1 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xe2 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xe3 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xe4 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xe5 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xe6 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xe7 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xe8 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xe9 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xea */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xeb */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xec */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xed */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xee */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xef */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xf0 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xf1 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xf2 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xf3 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xf4 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xf5 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xf6 */
  0, /* 0xf7 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xf8 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xf9 */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xfa */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xfb */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xfc */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xfd */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER, /* 0xfe */
  CPROP_ALPHA  |CPROP_LETTER  |CPROP_LOWER /* 0xff */
};

#define IBM1047 1047
#define UTF8 1208

#define UNI_TAB     0x09
#define UNI_LF      0x0A
#define UNI_CR      0X0D
#define UNI_SPACE   0x20
#define UNI_BANG    0x21
#define UNI_DQUOTE  0x22
#define UNI_HASH    0x23
#define UNI_DOLLAR  0x24
#define UNI_PERCENT 0x25
#define UNI_AMP     0x26
#define UNI_SQUOTE  0x27
#define UNI_LPAREN  0x28
#define UNI_RPAREN  0x29
#define UNI_STAR    0x2A
#define UNI_PLUS    0x2B
#define UNI_COMMA   0x2C
#define UNI_DASH    0x2D
#define UNI_DOT     0x2E
#define UNI_SLASH   0x2F
#define UNI_COLON   0x3A
#define UNI_SEMI    0x3B
#define UNI_LESS    0x3C
#define UNI_EQUAL   0x3D
#define UNI_GREATER 0x3E
#define UNI_QMARK   0x3F
#define UNI_AT      0x40
#define UNI_LBRACK 0x5B
#define UNI_BSLASH 0x5C
#define UNI_RBRACK 0x5D
#define UNI_CARET  0x5E
#define UNI_UNDER  0x5F
#define UNI_BQUOTE 0x60
#define UNI_LBRACE 0x7B
#define UNI_VBAR   0x7C
#define UNI_RBRACE 0x7D
#define UNI_TILDE  0x7E

typedef struct AbstractTokenizer_tag {
  int lowTokenID;
  int highTokenID;
  int lastTokenStart;
  int lastTokenEnd;
  int (*nextToken)(struct AbstractTokenizer_tag *tokenizer, char *s, int len, int pos, int *nextPos);
  char *(*getTokenIDName)(int tokenID);
} AbstractTokenizer;

bool testCharProp(int ccsid, int c, int prop);

/* Common Parsing infrastructure */

#define FIRST_GRULE_ID 10000

#define G_END 0
#define G_SEQ 1
#define G_ALT 2
#define G_STAR 3
#define G_PLUS 4

#define G_MAX_SUBS 12

typedef struct GRuleSpec_tag {
  int type;
  int id;
  union {
    struct { int subs[G_MAX_SUBS]; } sequence;
    struct { int subs[G_MAX_SUBS]; } alternates;
    struct { int sub; } star;
    struct { int sub; } plus; 
  };
} GRuleSpec;

typedef struct GRuleRef_tag {
  struct GRule_tag *parent;
  struct GRule_tag *rule;
  int indexInParent;
  int tokenID;
  bool isTokenRef;
} GRuleRef;

typedef struct GRule_tag {
  int type;
  int id;
  int subCount;
  GRuleRef *refs;
} GRule;



#define BUILD_OBJECT 1
#define BUILD_ARRAY 2
#define BUILD_STRING 3
#define BUILD_INT 4
#define BUILD_INT64 5
#define BUILD_DOUBLE 6
#define BUILD_BOOL 7
#define BUILD_NULL 8
#define BUILD_KEY 9
#define BUILD_POP 10
#define BUILD_ALT 11

#define MAX_BUILD_STEPS 10000 /* who in hell knows?  JK, actually should be a parameter to GParse() */
#define STEP_UNDEFINED_POSITION (-1)

typedef struct GBuildStep_tag {
  int type; /* BUILD_xxx above */
  GRule *rule;
  int tokenID;
  char *keyInParent;
  int   valueStart;  /* index into parse source */
  int   valueEnd;    /* index into parse source */
  GRuleRef *altChoice;
} GBuildStep;

typedef struct GParseContext_tag {
  AbstractTokenizer *tokenizer;
  GRule             *rules;
  char              *s;
  int                len;
  ShortLivedHeap    *slh;
  char              *(*ruleNamer)(int id);
  int                buildStepsSize;
  int                buildStepCount;
  GBuildStep        *buildSteps;
  int                status;
  int                errorPosition;
  char              *errorMessage;
} GParseContext;

typedef struct SyntaxNode_tag {
  int type;  /* owned by the user of this facility */
  int childCapacity;
  int childCount;
  struct SyntaxNode_tag **children;
} SyntaxNode;

typedef struct SyntaxTree_tag {
  ShortLivedHeap slh;
  SyntaxNode *topNode;
} SyntaxTree;

typedef struct GParseResult_tag {
  SyntaxTree *tree;
} GParseResult;

GParseContext *gParse(GRuleSpec *ruleSet, int topRuleID, AbstractTokenizer *tokenizer, char *s, int len,
                      char *(*ruleName)(int id));

Json *gBuildJSON(GParseContext *ctx, ShortLivedHeap *slh);

#endif
