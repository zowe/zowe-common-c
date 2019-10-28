

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_CHARSETS__
#define __ZOWE_CHARSETS__ 1

/** \file
 *  \brief charsets.h defines a platform independent interface for charset conversion.  
 */

#include "zowetypes.h"
#include "utils.h"

/* debugging switch */

#define TRACE_CHARSET_CONVERSION FALSE

/* COMMON CODE PAGE */

#define CCSID_IBM037            37   /* classic EBCDIC */
#define CCSID_IBM437           437   
#define CCSID_IBM1047         1047   /* "programmer's" EBCID */
#define CCSID_EBCDIC_1047     1047   /* synonym for backward compatiblity */
#define CCSID_UTF_16_BE       1201   /* IBM: #define CCSID UTF_16_BE       1201  */
                                        
#define CCSID_WINDOWS_LATIN_1 1252   /* MS "classic" ASCII variant */


/* ZOS valid CCSID's and for Linux, as base of codes to map to ICONV strings */
#if defined( __ZOWE_OS_ZOS ) || defined (__ZOWE_OS_LINUX)  || defined (__ZOWE_OS_AIX)

#define CCSID_ISO_8859_1       819
#define CCSID_UTF_16          1200    /* is the endian-ness platform-dependent */

#define CCSID_UTF_16_LE       1202
#define CCSID_UTF_8           1208

/* The following two must be hex */
#define CCSID_UNTAGGED        (short)0x0000
#define CCSID_BINARY          (short)0xFFFF

#elif defined(__ZOWE_OS_WINDOWS)
/* WINDOWS CCSID's that are not common 
   see https://msdn.microsoft.com/en-us/library/windows/desktop/dd317756(v=vs.85).aspx
*/

#define CCSID_ISO_8859_1     28591
#define CCSID_UTF_16_LE       1200    /* worried about difference, or is 1200 native */

#define CCSID_SYSTEM_DEFAULT_ANSI CP_ACP
#define CCSID_THREAD_DEFAULT_ANSI CP_THREAD_ACP
#define CCSID_UTF_8               CP_UTF8

#endif


#define CHARSET_OUTPUT_USE_BUFFER  1  /**< an output memory management mode */
#define CHARSET_OUTPUT_SAFE_MALLOC 2  /**< an output memory management mode */
#define CHARSET_OUTPUT_USE_SLH     3  /**< an output memory management mode */

#define CHARSET_CONVERSION_SUCCESS 0  /**< a conversion status code */
#define CHARSET_SHORT_BUFFER 8        /**< a conversion status code */
#define CHARSET_INTERNAL_ERROR 12     /**< a conversion status code */
#define CHARSET_CONVERSION_ROUTINE_FAILURE 16  /**< a conversion status code */
#define CHARSET_CONVERSION_UNIMPLEMENTED   20  /**< a conversion status code */
#define CHARSET_UNKNOWN_CCSID              24  /**< a conversion status code */

/**
 *   convertCharset provides charset conversion with a set of constants defining input and output charsets
 *   and a variety of output memory management options.   IF the output mode is set ot CHARSET_OUTPUT_USE_BUFFER
 *   is used and the output buffer is short, bad things will happen.   Keep in mind that some conversions will produce
 *   output that is 2-3 times longer than the input.  
 */

int convertCharset(char *input, 
                   int inputLength, 
                   int inputCCSID,
                   int outputMode,
                   char **output, 
                   int outputLength, 
                   int outputCCSID,
                   ShortLivedHeap *slh, // optional
                   int *conversionOutputLength, 
                   int *reasonCode);



#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

