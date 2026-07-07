

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
#include "metalio.h"

#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#endif /* METTLE */

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"

#define CHARSETNAME_SIZE 15

/*
 * getCharsetCode() maps charset name strings to CCSID integers.
 *
 * Name registry reference:
 *   https://www.iana.org/assignments/character-sets/character-sets.xhtml
 *
 * IBM codepages from that registry use the naming convention "IBM-NNNN"
 * where NNNN is the codepage number with no leading zeros (e.g. the IANA
 * name "IBM01140" is represented here as "IBM-1140", and "IBM037" as
 * "IBM-37").  Matching is case-insensitive (strupcase is applied first).
 */
int getCharsetCode(const char *charsetName) {
  char localArray[CHARSETNAME_SIZE + 1] = {0};

  /* Check for null pointer */
  if (charsetName == NULL) {
    return -1;
  }

  /* Make sure last element is 0 */
  if (strlen(charsetName) > CHARSETNAME_SIZE) {
    return -1;
  }
  strcpy( localArray, charsetName);
  strupcase (localArray);

  if ((!strcmp(localArray, "ISO-8859-1"))  ||
      (!strcmp(localArray, "ISO8859-1"))   ||
      (!strcmp(localArray, "IBM-819"))) {    /* IBM819 is an alias for ISO-8859-1 */
    return CCSID_ISO_8859_1;
  }
  else if (!strcmp(localArray, "UTF-8"))    { return CCSID_UTF_8;    }
  else if (!strcmp(localArray, "UTF-16"))   { return CCSID_UTF_16;   }
  else if (!strcmp(localArray, "UTF-16BE")) { return CCSID_UTF_16_BE; }
  else if (!strcmp(localArray, "UTF-16LE")) { return CCSID_UTF_16_LE; }
  /* IBM codepages from IANA character-sets registry (IBM-NNNN, no leading zeros) */
  else if (!strcmp(localArray, "IBM-37"))   { return 37;   }  /* IBM037  EBCDIC US/Canada */
  else if (!strcmp(localArray, "IBM-38"))   { return 38;   }  /* IBM038  EBCDIC INT */
  else if (!strcmp(localArray, "IBM-259"))  { return 259;  }  /* IBM-Symbols presentation set */
  else if (!strcmp(localArray, "IBM-273"))  { return 273;  }  /* IBM273  EBCDIC German */
  else if (!strcmp(localArray, "IBM-274"))  { return 274;  }  /* IBM274  EBCDIC Belgian */
  else if (!strcmp(localArray, "IBM-275"))  { return 275;  }  /* IBM275  EBCDIC Brazilian */
  else if (!strcmp(localArray, "IBM-277"))  { return 277;  }  /* IBM277  EBCDIC Danish/Norwegian */
  else if (!strcmp(localArray, "IBM-278"))  { return 278;  }  /* IBM278  EBCDIC Finnish/Swedish */
  else if (!strcmp(localArray, "IBM-280"))  { return 280;  }  /* IBM280  EBCDIC Italian */
  else if (!strcmp(localArray, "IBM-281"))  { return 281;  }  /* IBM281  EBCDIC Japanese-E */
  else if (!strcmp(localArray, "IBM-284"))  { return 284;  }  /* IBM284  EBCDIC Spanish */
  else if (!strcmp(localArray, "IBM-285"))  { return 285;  }  /* IBM285  EBCDIC UK */
  else if (!strcmp(localArray, "IBM-290"))  { return 290;  }  /* IBM290  EBCDIC Japanese Katakana */
  else if (!strcmp(localArray, "IBM-297"))  { return 297;  }  /* IBM297  EBCDIC French */
  else if (!strcmp(localArray, "IBM-367"))  { return 367;  }  /* IBM367  US-ASCII (alias) */
  else if (!strcmp(localArray, "IBM-420"))  { return 420;  }  /* IBM420  EBCDIC Arabic */
  else if (!strcmp(localArray, "IBM-423"))  { return 423;  }  /* IBM423  EBCDIC Greek */
  else if (!strcmp(localArray, "IBM-424"))  { return 424;  }  /* IBM424  EBCDIC Hebrew */
  else if (!strcmp(localArray, "IBM-437"))  { return 437;  }  /* IBM437  PC US */
  else if (!strcmp(localArray, "IBM-500"))  { return 500;  }  /* IBM500  EBCDIC International */
  else if (!strcmp(localArray, "IBM-775"))  { return 775;  }  /* IBM775  PC Baltic */
  else if (!strcmp(localArray, "IBM-838"))  { return 838;  }  /* IBM-Thai presentation set */
  else if (!strcmp(localArray, "IBM-850"))  { return 850;  }  /* IBM850  PC Multilingual */
  else if (!strcmp(localArray, "IBM-851"))  { return 851;  }  /* IBM851 */
  else if (!strcmp(localArray, "IBM-852"))  { return 852;  }  /* IBM852  PC Latin-2 */
  else if (!strcmp(localArray, "IBM-855"))  { return 855;  }  /* IBM855  PC Cyrillic */
  else if (!strcmp(localArray, "IBM-857"))  { return 857;  }  /* IBM857  PC Turkish */
  else if (!strcmp(localArray, "IBM-858"))  { return 858;  }  /* IBM00858 PC Multilingual+euro */
  else if (!strcmp(localArray, "IBM-860"))  { return 860;  }  /* IBM860  PC Portuguese */
  else if (!strcmp(localArray, "IBM-861"))  { return 861;  }  /* IBM861  PC Icelandic */
  else if (!strcmp(localArray, "IBM-862"))  { return 862;  }  /* IBM862  PC Hebrew */
  else if (!strcmp(localArray, "IBM-863"))  { return 863;  }  /* IBM863  PC Canadian French */
  else if (!strcmp(localArray, "IBM-864"))  { return 864;  }  /* IBM864  PC Arabic */
  else if (!strcmp(localArray, "IBM-865"))  { return 865;  }  /* IBM865  PC Nordic */
  else if (!strcmp(localArray, "IBM-866"))  { return 866;  }  /* IBM866  PC Russian */
  else if (!strcmp(localArray, "IBM-868"))  { return 868;  }  /* IBM868  PC Urdu */
  else if (!strcmp(localArray, "IBM-869"))  { return 869;  }  /* IBM869  PC Greek */
  else if (!strcmp(localArray, "IBM-870"))  { return 870;  }  /* IBM870  EBCDIC Latin-2 */
  else if (!strcmp(localArray, "IBM-871"))  { return 871;  }  /* IBM871  EBCDIC Icelandic */
  else if (!strcmp(localArray, "IBM-880"))  { return 880;  }  /* IBM880  EBCDIC Cyrillic */
  else if (!strcmp(localArray, "IBM-891"))  { return 891;  }  /* IBM891 */
  else if (!strcmp(localArray, "IBM-903"))  { return 903;  }  /* IBM903 */
  else if (!strcmp(localArray, "IBM-904"))  { return 904;  }  /* IBM904 */
  else if (!strcmp(localArray, "IBM-905"))  { return 905;  }  /* IBM905  EBCDIC Turkish */
  else if (!strcmp(localArray, "IBM-918"))  { return 918;  }  /* IBM918  EBCDIC Arabic-2 */
  else if (!strcmp(localArray, "IBM-924"))  { return 924;  }  /* IBM00924 EBCDIC Latin-9+euro */
  else if (!strcmp(localArray, "IBM-1026")) { return 1026; }  /* IBM1026 EBCDIC Turkish */
  else if ((!strcmp(localArray, "IBM-1047")) ||
           (!strcmp(localArray, "IBM1047"))) { return CCSID_IBM1047; } /* EBCDIC Latin-1/Open Sys */
  else if (!strcmp(localArray, "IBM-1140")) { return 1140; }  /* IBM01140 EBCDIC US+euro */
  else if (!strcmp(localArray, "IBM-1141")) { return 1141; }  /* IBM01141 EBCDIC German+euro */
  else if (!strcmp(localArray, "IBM-1142")) { return 1142; }  /* IBM01142 EBCDIC Danish/Norwegian+euro */
  else if (!strcmp(localArray, "IBM-1143")) { return 1143; }  /* IBM01143 EBCDIC Finnish/Swedish+euro */
  else if (!strcmp(localArray, "IBM-1144")) { return 1144; }  /* IBM01144 EBCDIC Italian+euro */
  else if (!strcmp(localArray, "IBM-1145")) { return 1145; }  /* IBM01145 EBCDIC Spanish+euro */
  else if (!strcmp(localArray, "IBM-1146")) { return 1146; }  /* IBM01146 EBCDIC UK+euro */
  else if (!strcmp(localArray, "IBM-1147")) { return 1147; }  /* IBM01147 EBCDIC French+euro */
  else if (!strcmp(localArray, "IBM-1148")) { return 1148; }  /* IBM01148 EBCDIC International+euro */
  else if (!strcmp(localArray, "IBM-1149")) { return 1149; }  /* IBM01149 EBCDIC Icelandic+euro */
  else {
    return -1;
  }
}

#if defined(__ZOWE_OS_ZOS) || defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)

bool isMultiByteCCSID(int ccsid) {
  switch (ccsid) {
    /* Unicode multi-byte encodings */
    case CCSID_UTF_8:      /* 1208 */
    case CCSID_UTF_16:     /* 1200 */
    case CCSID_UTF_16_BE:  /* 1201 */
    case CCSID_UTF_16_LE:  /* 1202 */
    /* EBCDIC MIX (SBCS+DBCS) code pages */
    case 930:   /* IBM930  - EBCDIC MIX Japanese */
    case 933:   /* IBM933  - EBCDIC MIX Korean */
    case 935:   /* IBM935  - EBCDIC MIX Simplified Chinese */
    case 937:   /* IBM937  - EBCDIC MIX Traditional Chinese */
    case 939:   /* IBM939  - EBCDIC MIX Japanese (Latin extension) */
    case 1364:  /* IBM1364 - EBCDIC MIX Korean */
    case 1388:  /* IBM1388 - EBCDIC MIX Simplified Chinese */
    case 1390:  /* IBM1390 - EBCDIC MIX Japanese */
    case 1399:  /* IBM1399 - EBCDIC MIX Japanese */
      return TRUE;
    default:
      return FALSE;
  }
}

int parseEncodingValue(const char *value) {
  char localArray[CHARSETNAME_SIZE + 1] = {0};

  if (value == NULL) {
    return -1;
  }

  /* "binary" is a special sentinel meaning CCSID_BINARY (0xFFFF) */
  if (strlen(value) <= CHARSETNAME_SIZE) {
    strcpy(localArray, value);
    strupcase(localArray);
    if (!strcmp(localArray, "BINARY")) {
      return (unsigned short)CCSID_BINARY;
    }
  }

  /* Try named charset first (e.g. "IBM-1047", "UTF-8") */
  int code = getCharsetCode(value);
  if (code != -1) {
    return code;
  }

  /* Fall back to decimal integer string (e.g. "1047", "819") */
  int n = 0;
  if (sscanf(value, "%d", &n) == 1 && n >= 1 && n <= 65535) {
    return n;
  }

  return -1;
}

#endif /* z/OS, Linux, AIX */

#ifdef __ZOWE_OS_WINDOWS

/* JOE 1/20/22 */
#include <Windows.h>

/*
   Windows doc 

   int MultiByteToWideChar(
   _In_      UINT   CodePage,          // expected
   _In_      DWORD  dwFlags,           // composite|precomposed|failOnInputError
   _In_      LPCSTR lpMultiByteStr,    // inputString
   _In_      int    cbMultiByte,       // length of inputString or -1 if nullTerminated and feeling adventurous
   _Out_opt_ LPWSTR lpWideCharStr,     // receivingBuffer (or NULL for brave souls who want Windows to allocate)
   _In_      int    cchWideChar        // length in WideChars of output
  );

  IBM resources
    http://www-01.ibm.com/software/globalization/ccsid/ccsid_registered.html

*/

int convertCharset2(char *input, 
                    int inputLength, 
                    int inputCCSID,
                    int outputMode,
                    char **outputArg, 
                    int outputLength, 
                    int outputCCSID,
                    ShortLivedHeap *slh, // optional
                    int *conversionLength,
                    int *reasonCode,
                    char *workArea,
                    int  workAreaLength){
  char *wideTemp = NULL;
  int  tempLength = 2 * inputLength; /* only allocated if needed */
  bool needToFreeTemp = false;
  if (workArea && (workAreaLength >= tempLength)){
    wideTemp = workArea;
  } else {
    wideTemp = safeMalloc(tempLength,"WideTemp");
    needToFreeTemp = true;
  }
  int wideCharCount = 0;
  int required = MultiByteToWideChar(inputCCSID,0,input,inputLength,NULL,0);
  int status = MultiByteToWideChar(inputCCSID,0,input,inputLength,(LPWSTR)wideTemp,inputLength); /* 2nd length is count of wchar */
  if (status == 0){
    *reasonCode = GetLastError();
    if (needToFreeTemp){
      safeFree(wideTemp,tempLength);
    }
    return CHARSET_CONVERSION_ROUTINE_FAILURE;
  } else {
    wideCharCount = status;
  }
  char *outputBuffer = NULL;
  if (outputMode == CHARSET_OUTPUT_USE_BUFFER){
    if (outputLength < 2*inputLength){
      return CHARSET_SHORT_BUFFER;
    }
    outputBuffer = *outputArg;
  } else if (outputMode == CHARSET_OUTPUT_SAFE_MALLOC){
    outputLength = 3*inputLength;
    outputBuffer = safeMalloc(outputLength,"ConversionBuffer");
  } else {
    outputLength = 3*inputLength;
    outputBuffer = SLHAlloc(slh,outputLength);
  }
  
  status = WideCharToMultiByte(outputCCSID,0,(LPWSTR)wideTemp,wideCharCount,outputBuffer,outputLength,NULL,NULL);
  if (needToFreeTemp){
    safeFree(wideTemp,tempLength);
  }
  if (status == 0){
    *reasonCode = GetLastError();
    if (outputMode == CHARSET_OUTPUT_SAFE_MALLOC){
      safeFree((char*)outputBuffer,outputLength);
    }
    return CHARSET_CONVERSION_ROUTINE_FAILURE;
  } else {
    *conversionLength = status;
    if (outputMode == CHARSET_OUTPUT_SAFE_MALLOC){
      char *finalOutput = safeMalloc(*conversionLength,"Conversion Final Buffer");
      memcpy(finalOutput,(char*)outputBuffer,*conversionLength);
      safeFree((char*)outputBuffer,outputLength);
      *outputArg = finalOutput;
      return CHARSET_CONVERSION_SUCCESS;
    } else {
      *outputArg = (char*)outputBuffer;
      return CHARSET_CONVERSION_SUCCESS;
    }
  }
}

int convertCharset(char *input, 
                   int inputLength, 
                   int inputCCSID,
                   int outputMode,
                   char **outputArg, 
                   int outputLength, 
                   int outputCCSID,
                   ShortLivedHeap *slh, // optional
                   int *conversionLength,
                   int *reasonCode){
  return convertCharset2(input,inputLength,inputCCSID,
                         outputMode,outputArg,outputLength,outputCCSID,
                         slh,conversionLength,reasonCode,NULL,0);
}

/* lots o'codes:
   https://docs.microsoft.com/en-us/windows/win32/intl/code-page-identifiers
*/


#elif defined(__ZOWE_OS_ZOS) && !defined(__ZOWE_COMP_XLCLANG)

/*

 The following table represents an EBCDIC-to-ASCII mapping.

 The mapping is taken from "z/OS DFSMS Using Magnetic Tapes
 Appendix D. Equivalent ASCII and EBCDIC codes"

 The characters, which cannot be mapped to ASCII, are mapped to
 the termination character 0x1A.

 The characters, which are notes as irregularities in the document,
 are also mapped to the termination character.

 Additionally, LF (0x25) has been mapped to the termination to
 match the results produced by CUNLCNV.

*/

#define IBM1047_TO_ASCII_TERM 0x1A

static const union {
  char table[256];
  uint64_t alignment;
} IBM1047_TO_ASCII = {

    .table = {
        /*         0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F */
        /* 0 */ 0x00, 0x01, 0x02, 0x03, 0x1A, 0x09, 0x1A, 0x7F, 0x1A, 0x1A, 0x1A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        /* 1 */ 0x10, 0x11, 0x12, 0x13, 0x1A, 0x1A, 0x08, 0x1A, 0x18, 0x19, 0x1A, 0x1A, 0x1C, 0x1D, 0x1E, 0x1F,
        /* 2 */ 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x17, 0x1B, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x05, 0x06, 0x07,
        /* 3 */ 0x1A, 0x1A, 0x16, 0x1A, 0x1A, 0x1A, 0x1A, 0x04, 0x1A, 0x1A, 0x1A, 0x1A, 0x14, 0x15, 0x1A, 0x1A,
        /* 4 */ 0x20, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x2E, 0x3C, 0x28, 0x2B, 0x1A,
        /* 5 */ 0x26, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x24, 0x2A, 0x29, 0x3B, 0x5E,
        /* 6 */ 0x2D, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,
        /* 7 */ 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,
        /* 8 */ 0x1A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A,
        /* 9 */ 0x1A, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A,
        /* A */ 0x1A, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A,
        /* B */ 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A,
        /* C */ 0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A,
        /* D */ 0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A,
        /* E */ 0x5C, 0x1A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A,
        /* F */ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A, 0x1A
    },

};

#ifdef __ZOWE_COMP_XLCLANG
#include "CUNHC.h"
#else 
#include "//'SYS1.SCUNHF(CUNHC)'"
#endif


/* Open XL (ibm-clang64) does not declare the __troo HLASM translate builtin
 * in <builtins.h>  its clang path only wires up __stckf. xlclang provided
 * the full family via #pragma linkage(__troo, builtin). Declare it here so
 * the symbol resolves at link time against LE's C runtime. */
#if defined(__ZOWE_COMP_CLANG)
static int __troo(char *output, char *input, unsigned long inputLength,
                    char *table, unsigned char testChar,
                    unsigned char mask) {
    int cc;
    unsigned long gr0 = testChar;   /* low byte consumed by hardware */
    __asm(
        ASM_PREFIX
        "         LG    0,%1 \n"        /* The test byte */
        "         LG    1,%2 \n"        /* The translation table */
        "         LG    8,%3 \n"        /* output */
        "         LG    9,%5 \n"        /* input length */
        "         LG    2,%4 \n"        /* input */
        "         TROO  8,2,0\n"        /* M3=0: test before xlate */
        "         BRC   1,*-4\n"        /* CC=3 => partial, retry */
        "         IPM   15\n"
        "         SRL   15,28\n"
        "         ST    15,%0\n"
        : "=m"(cc)
        : "m"(gr0),"m"(table),"m"(output),"m"(input),"m"(inputLength)
        : "cc", "memory");
    return cc;
  }
#endif


int convertCharset(char *input, 
                   int inputLength, 
                   int inputCCSID,
                   int outputMode,
                   char **output, 
                   int outputLength, 
                   int outputCCSID,
                   ShortLivedHeap *slh, // optional
                   int *conversionOutputLength, 
                   int *reasonCode){

  /* Check if we can perform the fast path one-to-one conversion,
   * this should cover the majority of all the cases. */
  if (inputCCSID == CCSID_IBM1047 && outputCCSID == CCSID_UTF_8 &&
      outputMode == CHARSET_OUTPUT_USE_BUFFER && inputLength <= outputLength) {

    int cc = __troo(
        *output, input, inputLength,
        (char *)IBM1047_TO_ASCII.table,
        IBM1047_TO_ASCII_TERM,
        0     /* mask 0 to stop if the term char is encountered */
    );

    if (cc == 0) {
      *conversionOutputLength = inputLength;
      return CHARSET_CONVERSION_SUCCESS;
    }
  }

  unsigned char dda [CUNBCPRM_DDA_REQ];
  unsigned char workBuffer [4096];
#ifdef _LP64
  CUN4BCPR parms = { CUNBCPRM_DEFAULT };
#else
  CUNBCPRM parms ={ CUNBCPRM_DEFAULT};
#endif
  int outputAllocLength = 3*inputLength; /* paranoia */
  char *outputBuffer;
  
  parms.Src_Buf_Ptr=input;
  switch (outputMode){
  case CHARSET_OUTPUT_USE_BUFFER:
    if (outputLength < 2*inputLength){
      return CHARSET_SHORT_BUFFER;
    }
    outputBuffer = *output;
    break;
  case CHARSET_OUTPUT_SAFE_MALLOC:
    outputBuffer = safeMalloc(outputAllocLength,"Conversion Buffer");
    outputLength = outputAllocLength;
    break;
  case CHARSET_OUTPUT_USE_SLH:
    outputBuffer = SLHAlloc(slh,outputAllocLength);
    outputLength = outputAllocLength;
    *output = outputBuffer;
    break;
  }
  parms.Targ_Buf_Ptr = outputBuffer;
  parms.Targ_Buf_Len=outputLength;
  parms.Src_Buf_Len=inputLength;
  parms.Src_CCSID=inputCCSID;
  parms.Targ_CCSID=outputCCSID;
  memcpy(parms.Technique,"LMER",4);
  parms.Wrk_Buf_Ptr=workBuffer;
  parms.Wrk_Buf_Len=4096;
  parms.DDA_Buf_Ptr=dda;
  parms.DDA_Buf_Len=CUNBCPRM_DDA_REQ;

  if (TRACE_CHARSET_CONVERSION){
    printf("Before CUNLCNV parms\n");
#ifdef _LP64
    dumpbuffer((char*)&parms,sizeof(CUN4BCPR));
#else
    dumpbuffer((char*)&parms,sizeof(CUNBCPRM));
#endif
    fflush(stdout);
  }

#ifdef _LP64
  CUN4LCNV ( &parms );
#else
  CUNLCNV ( & parms );
#endif

  *conversionOutputLength = (((char*)parms.Targ_Buf_Ptr) - outputBuffer);
  if (TRACE_CHARSET_CONVERSION) {
    printf("inputLen=%d reasonCode = %d src=%d targ=%d\n",inputLength,parms.Reason_Code,parms.Src_CCSID,parms.Targ_CCSID);
    fflush(stdout);
  }

  if (parms.Return_Code){
    if (outputMode == CHARSET_OUTPUT_SAFE_MALLOC){
      safeFree(parms.Targ_Buf_Ptr,outputAllocLength);
    }
    *reasonCode = parms.Return_Code;

    return CHARSET_CONVERSION_ROUTINE_FAILURE;
  } else {
    if (outputMode == CHARSET_OUTPUT_SAFE_MALLOC){
      *output = safeMalloc(*conversionOutputLength,"Converted Buffer");
      memcpy(*output,parms.Targ_Buf_Ptr,*conversionOutputLength);
      safeFree(parms.Targ_Buf_Ptr,outputAllocLength);
    }
    return CHARSET_CONVERSION_SUCCESS;
  }
}



/* End of Traditional METAL and XLC cases, since linkage(OS64_NOSTACK) doesn't work in xlclang and clang 
   some C code goes through here, too.  We should short circuit easy special cases here some day */
#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX) || (defined(__ZOWE_OS_ZOS) && defined(__ZOWE_COMP_XLCLANG))

#include <iconv.h>
#include <errno.h>


/*
  ICONV names

  I have gone with the IBM ccsid numbers to name some iconv_charsets 

  what is "WCHAR_T" (is it UTF16 (BE/LE)?) 

  NOTE: iconv_open is EXPENSIVE. It would make sense to keep a dictionary of
        already-opened converters and reuse them.
 */

static const char *getCharsetName(int ibmCode){
  switch (ibmCode){
  case CCSID_ISO_8859_1:
    return "ISO8859-1"; /* NO dash after ISO: z/OS iconv rejects "ISO-8859-1" (errno 121); glibc accepts both */
  case CCSID_IBM1047:
    return "IBM-1047";
  case CCSID_UTF_8:
    return "UTF-8";
  case CCSID_UTF_16:
    return "UTF-16";
  case CCSID_UTF_16_BE:
    return "UTF-16BE";
  case CCSID_UTF_16_LE:
    return "UTF-16LE";
  default:
    return NULL;
  }
}

int convertCharset(char *input, 
                   int inputLength, 
                   int inputCCSID,
                   int outputMode,
                   char **output, 
                   int outputLength, 
                   int outputCCSID,
                   ShortLivedHeap *slh, // optional
                   int *conversionOutputLength, 
                   int *reasonCode){
  *reasonCode  = 0;

  const char *inputCharset = getCharsetName(inputCCSID);
  const char *outputCharset = getCharsetName(outputCCSID);
  if ((inputCharset == NULL) || (outputCharset == NULL)){
    return CHARSET_UNKNOWN_CCSID;
  }

  iconv_t converter = iconv_open (outputCharset, inputCharset);
  if (converter == (iconv_t) -1){
    *reasonCode = errno;
    return CHARSET_CONVERSION_UNIMPLEMENTED;
  }

  char* inputBuffer = input;
  size_t inputSize = (size_t)inputLength;

  char *outputBuffer = NULL;
  size_t outputSize = 0;

  size_t outputAllocLength = 3*inputSize; /* paranoia (but could ACTUALLY be 4*inputSize!) */

  switch (outputMode){
  case CHARSET_OUTPUT_USE_BUFFER:
    outputBuffer = *output;
    outputSize = (size_t)outputLength;
    outputAllocLength = 0;
    break;
  case CHARSET_OUTPUT_SAFE_MALLOC:
    outputBuffer = safeMalloc(outputAllocLength,"Conversion Buffer");
    outputSize = outputAllocLength;
    break;
  case CHARSET_OUTPUT_USE_SLH:
    outputBuffer = SLHAlloc(slh,outputAllocLength);
    outputSize = outputAllocLength;
    *output = outputBuffer;
    break;
  default:
    return CHARSET_INTERNAL_ERROR;
  }

  /* size_t iconv (iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft) */
  char* outputBufferStart = outputBuffer;
  int result = CHARSET_CONVERSION_SUCCESS;

  size_t iconv_status = iconv(converter, &inputBuffer, &inputSize, &outputBuffer, &outputSize);
  iconv_close(converter);

  if (iconv_status == -1){
    switch(errno) {
    case E2BIG: 
      result = CHARSET_SHORT_BUFFER;
      break;

    case EILSEQ:
    case EINVAL:
      result = CHARSET_CONVERSION_ROUTINE_FAILURE;
      break;

    default:
      result = CHARSET_INTERNAL_ERROR;
      break;
    }
    switch(outputMode) {
    case CHARSET_OUTPUT_USE_BUFFER:
    case CHARSET_OUTPUT_USE_SLH:
      break;
    case CHARSET_OUTPUT_SAFE_MALLOC:
      safeFree(outputBufferStart,outputAllocLength);
      break;
    }
  } else {
    result = CHARSET_CONVERSION_SUCCESS;
    size_t actualOutputLength = outputBuffer-outputBufferStart;
    *conversionOutputLength = (int) actualOutputLength;
    switch(outputMode) {
    case CHARSET_OUTPUT_USE_BUFFER:
    case CHARSET_OUTPUT_USE_SLH:
      *output = outputBufferStart;
      break;
    case CHARSET_OUTPUT_SAFE_MALLOC:
      /* PITA - safeFree has to be called with the size of the buffer,
         so if we didn't use all of (which is unlikely...), we have
         to make yet another copy and return the pointer to that. */
      *output = safeMalloc(*conversionOutputLength,"Converted Buffer");
      memcpy(*output,outputBufferStart,actualOutputLength);
      safeFree(outputBufferStart,outputAllocLength);
      break;
    }
  }
  return result;
}

/* Streaming charset conversion for the file-content path (USE_BUFFER only).
 * Converts as much of [input, inputLength] as forms COMPLETE characters into
 * the caller's output buffer, and reports BOTH how much output was produced
 * (*conversionOutputLength) and how much input was consumed (*inputBytesConsumed).
 *
 * A trailing incomplete multibyte sequence (iconv EINVAL) is intentionally left
 * unconsumed and is NOT an error: the caller carries those leftover bytes
 * (inputLength - *inputBytesConsumed) forward and prepends them to the next read,
 * which is what fixes multi-byte characters split across a read-buffer boundary.
 *
 * Characters the target cannot represent are substituted rather than aborting
 * the whole response (the #828 silent empty body): //TRANSLIT handles
 * unmappable-in-target, and an explicit '?' is emitted for bytes that are
 * invalid in the source encoding, so one bad byte can't stall the stream. */
int convertCharsetStreaming(char *input, int inputLength, int inputCCSID,
                            char *output, int outputLength, int outputCCSID,
                            int *conversionOutputLength, int *inputBytesConsumed,
                            int *reasonCode){
  *reasonCode = 0;
  *conversionOutputLength = 0;
  *inputBytesConsumed = 0;

  const char *inputCharset = getCharsetName(inputCCSID);
  const char *outputCharset = getCharsetName(outputCCSID);
  if ((inputCharset == NULL) || (outputCharset == NULL)){
    return CHARSET_UNKNOWN_CCSID;
  }

  /* "//TRANSLIT" is a glibc transliteration extension: on glibc it gives clean
   * per-character substitution for characters unmappable in the target. z/OS
   * iconv does NOT implement it as a modifier -- it opens "NAME//TRANSLIT" as a
   * distinct, malfunctioning converter (drops data, mishandles incomplete
   * sequences) -- and z/OS iconv already substitutes unmappable characters
   * natively (SUB, 0x1A). So attempt //TRANSLIT only on glibc; everywhere else
   * use the plain converter, whose EILSEQ we substitute for ourselves below. */
  iconv_t converter = (iconv_t) -1;
#if defined(__ZOWE_OS_LINUX)
  char translitCharset[64];
  snprintf(translitCharset, sizeof(translitCharset), "%s//TRANSLIT", outputCharset);
  converter = iconv_open (translitCharset, inputCharset);
#endif
  if (converter == (iconv_t) -1){
    converter = iconv_open (outputCharset, inputCharset);
  }
  if (converter == (iconv_t) -1){
    *reasonCode = errno;
    return CHARSET_CONVERSION_UNIMPLEMENTED;
  }

  char  *inPtr  = input;
  size_t inLeft = (size_t) inputLength;
  char  *outPtr = output;
  size_t outLeft = (size_t) outputLength;
  int result = CHARSET_CONVERSION_SUCCESS;

  while (inLeft > 0){
    size_t status = iconv(converter, &inPtr, &inLeft, &outPtr, &outLeft);
    if (status != (size_t) -1){
      break; /* all remaining input consumed */
    }
    if (errno == EINVAL){
      break; /* incomplete trailing sequence -> caller carries it forward */
    } else if (errno == E2BIG){
      result = CHARSET_SHORT_BUFFER; /* output full; caller has what fit */
      break;
    } else if (errno == EILSEQ){
      /* byte not valid in the source encoding: substitute and skip it so a
         single bad byte cannot stall the whole stream. */
      if (outLeft == 0){ result = CHARSET_SHORT_BUFFER; break; }
      *outPtr++ = 0x3F; /* '?' as a numeric byte: correct in ASCII-family targets
                           (UTF-8, 819). The char literal '?' would be EBCDIC 0x6F
                           under -fexec-charset=IBM-1047 and render 'o' in a 819 stream. */
      outLeft--;
      inPtr++;
      inLeft--;
      continue;
    } else {
      result = CHARSET_INTERNAL_ERROR;
      break;
    }
  }

  iconv_close(converter);
  *conversionOutputLength = (int) ((size_t) outputLength - outLeft);
  *inputBytesConsumed     = (int) ((size_t) inputLength  - inLeft);
  return result;
}

/* This build has a native streaming converter (iconv); suppress the portable
 * fallback defined after the OS switch below. */
#define ZOWE_HAVE_CONVERT_CHARSET_STREAMING 1

#else
#error Unknown OS
#endif

#ifndef ZOWE_HAVE_CONVERT_CHARSET_STREAMING
/* Portable convertCharsetStreaming for platforms whose branch above provides no
 * native streaming converter (Windows, z/OS metal-C). There is no carry-forward
 * here: this platform's convertCharset processes the whole input buffer in one
 * shot, so we report the input fully consumed and leave nothing pending. This
 * exists so httpserver.c's streamTextForFile2 can call one API on every
 * platform; pre-#828 behavior on these platforms is unchanged. */
int convertCharsetStreaming(char *input, int inputLength, int inputCCSID,
                            char *output, int outputLength, int outputCCSID,
                            int *conversionOutputLength, int *inputBytesConsumed,
                            int *reasonCode){
  *reasonCode = 0;
  *conversionOutputLength = 0;
  *inputBytesConsumed = 0;
  char *outputArg = output;
  int rc = convertCharset(input, inputLength, inputCCSID,
                          CHARSET_OUTPUT_USE_BUFFER, &outputArg,
                          outputLength, outputCCSID,
                          NULL, conversionOutputLength, reasonCode);
  if ((outputArg != output) && (*conversionOutputLength > 0) &&
      (*conversionOutputLength <= outputLength)){
    memcpy(output, outputArg, (size_t) *conversionOutputLength);
  }
  *inputBytesConsumed = inputLength;
  return rc;
}
#endif




/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

