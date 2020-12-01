

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __UTILS__
#define __UTILS__   1

#ifndef __ZOWETYPES__
#include "zowetypes.h"
#endif

#include "utilsopt.h"

/**  \file
 *   \brief Various utilities.
 */

#if defined(__cplusplus)                                                        
extern "C" {                                                                    
#endif                                                                          

#ifndef __LONGNAME__
#define strcopy_safe STRCPSAF
#define indexOf INDEXOF
#define indexOfString IDXSTR
#define lastIndexOfString LIDXSTR
#define indexOfStringInsensitive IDXSTRNS
#endif

char * strcopy_safe(char * dest, const char * source, int dest_size);

int indexOf(char *str, int len, char c, int startPos);
int lastIndexOf(const char *str, int len, char c);
int indexOfString(char *str, int len, char *searchString, int startPos);
int lastIndexOfString(char *str, int len, char *searchString);
int indexOfStringInsensitive(char *str, int len, char *searchString, int startPos);

/* max() is not a standard macro.  Windows introduces this and they are wrong */

#ifdef max
#undef max
#undef min
#endif

#define max(x,y) ((x > y) ? x : y)
#define min(x,y) ((x < y) ? x : y)

typedef struct token_struct{
  int start; /* inclusive */
  int end;   /* exclusive */
} token;

int isZeros(char *data, int offset, int length);

int isBlanks(char *data, int offset, int length);

int hasText(char* data, int offset, int length);

int parseInt(const char *str, int start, int end);

#ifndef __LONGNAME__
#define parseInitialInt PSINTINT
#define nullTerminate NULLTERM
#define tknGetDecimal TKGTDCML
#define tknGetQuoted TKGTQOTD
#define tknGetAlphanumeric TKGTANUM
#define tknGetStandard TKGTSTND
#define tknGetNonWhitespace TKGTNWSP
#define tknGetTerminating TKGTTERM
#define tknTextEquals TKTXEQLS
#define freeToken FREETKN
#define tknLength TKNLNGTH
#endif

int parseInitialInt(const char *str, int start, int end);

int nullTerminate(char *str, int len);

/* Basic CP500 alphanumeric character predicate
   Doesn't care about any fancy languages */
int isCharAN(char c);

token* tknGetDecimal(char *str, int len, int start);
token* tknGetQuoted(char *str, int len, int start, char quote, char escape);
                                                           
token* tknGetAlphanumeric(char *str, int len, int start);

/* alphanumeric plus hash dollar and at sign */
token* tknGetStandard(char *str, int len, int start);

token* tknGetNonWhitespace(char *str, int len, int start);

token*  tknGetTerminating(char *str, int len,
                          char *match, int start);

int tknTextEquals(token *t, char *str, char *match);

void freeToken(token *t);
char *tknText(token *t, char *str);               
int tknInt(token *t, char *str); 
int tknLength(token *t);

#ifndef __LONGNAME__
#define dumpbuffer DMPBUFFR
#define dumpbufferA DMPBUFFA
#define simpleHexFill SMPHXFIL
#define simpleHexPrint SMPHXPRT
#define simpleHexPrintLower SMPHXPRL
#define dumpbuffer2 DMPBFFR2
#define dumpBufferToStream DMPBFFRS
#define compareIgnoringCase CMPIGNCS
#define strupcase STRUPCAS
#endif

/**
 * A utility to dump hex data to stdout 
 */

void dumpbuffer(const char *buffer, int length);

/** 
 * A utility to dump data to a FILE* pointer 
 */

void dumpBufferToStream(const char *buffer, int length, /* FILE* */void *traceOut);

/**
 *   A utility to generate precisely formatted hex strings.
 */

void dumpbufferA(const char *buffer, int length);

int hexFill(char *buffer, int offset, int prePad, int formatWidth, int postPad, int value);

/**
 *   A utility that is a convenience function to format a hex strings from integers.
 * 
 *   The value is left-padded with 0's
 */

char *simpleHexFill(char *buffer, int formatWidth, int value);
char *simpleHexPrint(char *buffer, char *data, int len);
char *simpleHexPrintLower(char *buffer, char *data, int len);
void hexdump(char *buffer, int length, int nominalStartAddress, int formatWidth, char *pad1, char *pad2);
void dumpbuffer2(char *buffer, int length);
int compareIgnoringCase(char *s1, char *s2, int len);
char *strupcase(char *s);

typedef struct ListElt_tag {
  char *data;
  struct ListElt_tag *next;
} ListElt;

ListElt *cons(void *data, ListElt *list);

/**
 *    \brief  The ShortLivedHeap is a memory manager for large numbers of small allocations followed by a single bulk deallocation,
 *
 *    There are many situations in programming (e.g. compilers, parsers, etc) that build large complex data structures with 
 *    reference counts per object that are > 1.    This can solve many leak problems and performance problems that are seen when
 *    calling malloc and free zillions of times.  
 */

typedef struct ShortLivedHeap_tag{
  char eyecatcher[8];
  char *activeBlock;
  ListElt *blockChain;
  int is64;
  int bytesRemaining;
  int maxBlocks;
  int blockCount;
  int blockSize;
} ShortLivedHeap;

#ifndef __LONGNAME__
#define makeShortLivedHeap MAKESLH
#define makeShortLivedHeap64 MAKSLH64
#define noisyMalloc NYMALLOC
#define decodeBase32 DECODB32
#define encodeBase32 ENCODB32
#define decodeBase64 DECODB64
#define encodeBase64 ENCODB64
#define cleanURLParamValue CLNURLPV
#define percentEncode PCTENCOD
#define destructivelyUnasciify DSTUNASC
#endif

/**
 *    \brief  makes a short lived heap.
 *
 *    The short lived heap will manage blockSize * maxBlocks bytes of data.
 */

ShortLivedHeap *makeShortLivedHeap(int blockSize, int maxBlocks);
ShortLivedHeap *makeShortLivedHeap64(int blockSize, int maxBlocks);

/**
 *    \brief   This is the "malloc" of a short lived heap.
 *
 *    If the size is greater than the blockSize of the ShortLivedHeap a new block will be added to the heap
 *    with exactly this size.  
 */

char *SLHAlloc(ShortLivedHeap *slh, int size);

/**
 *    \brief   This will reclaim the whole heap.
 *
 *    Make sure that the application has copied what it needs from this heap before freeing!! 
 */

void SLHFree(ShortLivedHeap *slh);

char *cleanURLParamValue(ShortLivedHeap *slh, char *value);
int percentEncode(char *value, char *buffer, int len);

#define BASE64_ENCODE_SIZE(SZ) (2 + 4 * ((SZ + 2) / 3))

int decodeBase64(char *s, char *result);
char *encodeBase64(ShortLivedHeap *slh, const char buf[], int size, int *resultSize,
                   int useEbcdic);
void encodeBase64NoAlloc(const char buf[], int size, char result[], int *resultSize,
                         int useEbcdic);
/*
 * Assumes "EBCDIC base64" on EBCDIC platforms
 */
int base64ToBase64url(char *s);

/*
 * Assumes "EBCDIC base64" on EBCDIC platforms
 */
int base64urlToBase64(char *s, int bufSize);

char *destructivelyUnasciify(char *s);

int base32Decode (int alphabet,
                  char *input,
                  char *output,
                  int *outputLength,
                  int useEBCDIC);
int base32Encode (int alphabet,
                  char *input,
                  int inputLength,
                  char *output,
                  int *outputLength,
                  int useEBCDIC);
#define RFC4648   0

#define INVALID_ALPHABET         1
#define INVALID_DECODE_SIZE      2
#define DECODE_LENGTH_TOO_SMALL  3
#define DECODE_CHAR_INVALID      4
#define INVALID_ENCODE_LENGTH    2
#define ENCODE_LENGTH_TOO_SMALL  3

char *noisyMalloc(int size);

typedef struct StringListElt_tag{
  char *string;
  struct StringListElt_tag *next;
} StringListElt;

/**
 *   \brief A simple utility struct for holding lists of strings
 *   
 *   This is a headed, singly-linked list which allocates elements in an associated ShortLivedHeap. It has
 *   a tail pointer for fast insertions.  
 */

typedef struct StringList_tag{
  ShortLivedHeap *slh;
  int count;
  int totalSize;
  StringListElt *head;
  StringListElt *tail;
} StringList;

#ifndef __LONGNAME__
#define makeStringList MAKESLST
#define stringListLength SLSTLEN
#define stringListPrint SLSTPRNT
#define stringListContains SLSTCTNS
#define stringListLast SLSTLAST
#define addToStringListUnique ADSLSTUQ
#define addToStringList ADSLST
#define firstStringListElt SLSTELT1
#define stringConcatenate STRCNCAT
#endif

/**
 *    \brief  Makes a StringList.
 */

StringList *makeStringList(ShortLivedHeap *slh);

/**   \brief  Current member count of string list */

int stringListLength(StringList *list);
char *stringListPrint(StringList *list, int start, int max, char *separator, char quoteChar);

/**   \brief  Membership test for string in a String in a String List */

int stringListContains(StringList *list, char *s);
char *stringListLast(StringList *list);
int addToStringListUnique(StringList *list, char *s);

/**
 *   \brief   Tail-insertion to string list.  No uniqueness check.
 */

void addToStringList(StringList *list, char *s);


/**
 *   \brief   Peeks at first element in list.
 */

StringListElt *firstStringListElt(StringList *list);
char *stringConcatenate(ShortLivedHeap *slh, char *s1, char *s2);

typedef struct CharStream_tag{
  void *internalStream;
  int byteCount;
  int size; /* -1 if not known */
  int (*readMethod)(struct CharStream_tag *, int trace);
  int (*positionMethod)(struct CharStream_tag *);
  int (*eofMethod)(struct CharStream_tag *);
  int (*closeMethod)(struct CharStream_tag *);
} CharStream;

#ifndef __LONGNAME__
#define makeBufferCharStream MKBFCHST
#define charStreamPosition   CSTRPOSN
#define charStreamGet        CSTRGET
#define charStreamEOF        CSTREOF
#define charStreamClose      CSTRCLOS
#define charStreamFree       CSTRFREE
#endif

CharStream *makeBufferCharStream(char *buffer, int len, int trace);
int charStreamPosition(CharStream *s);
int charStreamGet(CharStream *s, int trace);
int charStreamEOF(CharStream *s);
int charStreamClose(CharStream *s);
void charStreamFree(CharStream *s);

/* Defines for padWithSpaces */
#define LEAVE_NULL 0 // Leave terminate symbols unchanged
#define SUBST_NULL 1 // Change terminate symbols for spaces (for ctMsg)

#define NOT_SUBST_EMPTY 0
#define SUBST_EMPTY 1

/* Defines for compareSequences */
#define SEQ_EQUAL 0
#define SEQ_MORE 1
#define SEQ_LESS -1
#define SEQ_ERROR 4

#define padWithSpaces padwspcs
#define replaceTerminateNulls rpltrmnl
#define convertIntToString cnvintst
#define compareSequences compseqs

int padWithSpaces(char *str,
                  int actualLength,
                  int handleNullSymbols,
                  int substEmptyStrings);
int replaceTerminateNulls(char *str, int actualLength);
void convertIntToString(char *string, int ptrSize, int input);
unsigned int hexToDec(unsigned int hex, int digitCount);
unsigned int decToHex(unsigned int dec, int digitCount);
int compareSequences(const char *firstSequence,
                     const char *secondSequence,
                     int compareLength);

typedef struct ISOTime_tag {
  char data[20];
} ISOTime;

int decimalToOctal(int decimal); /* Converts output of fileUnixMode to octal format */

void convertUnixToISO(int unixTime, ISOTime *timeStamp); /* Converts output of fileInfoUnixCreationTime to ISO Format Timestamp */

#define STRLIKE_IGNORE_CASE      0x01
#define STRLIKE_ALLOW_TAIL_JUNK  0x02
#define STRLIKE_UTF8_PATTERN     0x04
#define STRLIKE_MIMIC_SQL        0x08

int matchWithWildcards(char *pattern, int patternLen,
                       char *s, int len, int flags);

bool stringIsDigit(const char * str);

#ifndef METTLE
const char* strrstr(const char * base, const char * find);
#endif

void trimRight(char *str, int length);

#if defined(__cplusplus)                                                        
}           /* end of extern "C" */
#endif                                                                          

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

