

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

#ifdef __ZOWE_OS_WINDOWS

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

  HERE
  1) what does 1200 really mean
  2) wcTest -> convert to wideNative
  3) 
*/

int charsetToWideNative(char  *input, 
                        int    inputLength, 
                        int    inputCCSID,
                        int    outputMode,
                        char **outputArg,
                        int    outputLength,
                        ShortLivedHeap *slh,
                        int   *conversionLength,
                        int   *reasonCode){
  LPWSTR outputBuffer;
  if (outputMode == CHARSET_OUTPUT_USE_BUFFER){
    if (outputLength < 2*inputLength){
      return CHARSET_SHORT_BUFFER;
    }
    outputBuffer = *((LPWSTR*)outputArg);
  } else if (outputMode == CHARSET_OUTPUT_SAFE_MALLOC){
    outputLength = 3*inputLength;
    outputBuffer = (LPWSTR)safeMalloc(outputLength,"Conversion Temp Buffer");
  } else {
    outputLength = 3*inputLength;
    outputBuffer = (LPWSTR)SLHAlloc(slh,outputLength);
  }
  int status = MultiByteToWideChar(inputCCSID,0,input,inputLength,outputBuffer,outputLength);
  if (status == 0){
    *reasonCode = GetLastError();
    if (outputMode == CHARSET_OUTPUT_SAFE_MALLOC){
      safeFree((char*)outputBuffer,outputLength);
    }
    return CHARSET_CONVERSION_ROUTINE_FAILURE;
  } else {
    *conversionLength = status*2;                      /* 2 because windows returns size in "wide chars" */
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
                   char **output, 
                   int outputLength, 
                   int outputCCSID,
                   ShortLivedHeap *slh, // optional
                   int *conversionOutputLength,
                   int *reasonCode){
  
  return 0;
}

#elif defined(__ZOWE_OS_ZOS)

#include "//'SYS1.SCUNHF(CUNHC)'"

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

#define CHARSETNAME_SIZE 15
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
# undef CHARSETNAME_SIZE
  strcpy( localArray, charsetName);
  strupcase (localArray);

  if ((!strcmp(localArray, "ISO-8859-1"))  ||
      (!strcmp(localArray, "ISO8859-1"))) {
    return CCSID_ISO_8859_1;
  }
  else if (!strcmp(localArray, "IBM-1047")) {
    return CCSID_IBM1047;
  }
  else if (!strcmp(localArray, "UTF-8")) {
    return CCSID_UTF_8;
  }
  else if (!strcmp(localArray, "UTF-16")) {
    return CCSID_UTF_16;
  }
  else if (!strcmp(localArray, "UTF-16BE")) {
    return CCSID_UTF_16_BE;
  }
  else if (!strcmp(localArray, "UTF-16LE")) {
    return CCSID_UTF_16_LE;
  }
  else {
    return -1;
  }
}

#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX) /* end of ZOWE_OS_ZOS */

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
    return "ISO-8859-1";
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
#else  
#error Unknown OS
#endif




/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

