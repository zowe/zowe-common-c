

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

#define ERRNO_CCSID_MISMATCH 666888333

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

/**
 * Returns TRUE if the given CCSID uses a multi-byte encoding (e.g. UTF-8,
 * UTF-16, or EBCDIC MIX), and FALSE if it is single-byte.
 * Returns FALSE for unrecognised or special values (0, -1, 0xFFFF).
 */
bool isMultiByteCCSID(int ccsid);

/**
 * Returns TRUE if convertCharsetStreaming can handle the given
 * source->target pair on this build: the identity pair always can (it is
 * streamed without a converter), otherwise both CCSIDs must resolve to
 * converter names and the converter must open (verified by a trial open on
 * iconv builds). Lets callers reject an unusable pair up front - e.g. with an
 * HTTP 400 - instead of discovering it mid-stream, where the response status
 * is already committed. On builds whose converter takes numeric CCSIDs
 * directly (Windows, z/OS metal-C) this is permissively TRUE.
 */
bool isCharsetStreamingPairSupported(int sourceCCSID, int targetCCSID);

/**
 * Parses an encoding value that may be either a charset name string
 * (e.g. "IBM-1047", "UTF-8", "binary") or a decimal CCSID integer
 * string (e.g. "1047", "819", "65535").
 * Returns the CCSID on success, or -1 if the value cannot be parsed
 * or is not in the valid range 1-65535.
 * Note: "binary" / "BINARY" returns CCSID_BINARY (0xFFFF).
 */
int parseEncodingValue(const char *value);

#elif defined(__ZOWE_OS_WINDOWS)
#include <Windows.h>
/* WINDOWS CCSID's that are not common 
   see https://msdn.microsoft.com/en-us/library/windows/desktop/dd317756(v=vs.85).aspx
*/

#define CCSID_ISO_8859_1     28591
#define CCSID_UTF_16_LE       1200    /* worried about difference, or is 1200 native */
#define CCSID_UTF_16          1200    /* worried about difference, or is 1200 native */

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

/**
 * @brief Streaming charset conversion for the file-content path (USE_BUFFER only).
 *
 * Converts as much of @p input (up to @p inputLength bytes) as forms complete
 * characters into the caller's output buffer, reporting both how much output was
 * produced and how much input was consumed.
 *
 * A trailing incomplete multibyte sequence (iconv @c EINVAL) is intentionally
 * left unconsumed and is not treated as an error: the caller carries those
 * leftover bytes (@p inputLength minus @p *inputBytesConsumed) forward and
 * prepends them to the next read, which fixes multibyte characters split across
 * a read-buffer boundary.
 *
 * Characters the target cannot represent are substituted rather than aborting
 * the whole response (the issue #828 silent empty body): the @c //TRANSLIT
 * suffix handles characters unmappable in the target encoding, and a '?' in
 * the TARGET's own encoding (see getSubstituteBytes in charsets.c) is emitted
 * for bytes that are invalid in the source encoding, so a single bad byte
 * cannot stall the stream.
 *
 * @param[in]  input                   Bytes to convert.
 * @param[in]  inputLength             Number of bytes available in @p input.
 * @param[in]  inputCCSID              CCSID of the source encoding.
 * @param[out] output                  Buffer receiving the converted bytes.
 * @param[in]  outputLength            Capacity of @p output in bytes.
 * @param[in]  outputCCSID             CCSID of the target encoding.
 * @param[out] conversionOutputLength  Number of bytes written to @p output.
 * @param[out] inputBytesConsumed      Number of bytes consumed from @p input.
 * @param[out] reasonCode              Receives the underlying errno on converter
 *                                     open failure; 0 otherwise.
 *
 * @retval CHARSET_CONVERSION_SUCCESS       (0)  Conversion progressed; a trailing
 *         incomplete multibyte sequence left unconsumed is reported here, not as
 *         an error.
 * @retval CHARSET_SHORT_BUFFER             (8)  Output buffer was filled before
 *         all input was consumed; drain @p output and call again.
 * @retval CHARSET_INTERNAL_ERROR           (12) iconv reported an unexpected errno.
 * @retval CHARSET_CONVERSION_UNIMPLEMENTED (20) Converter could not be opened;
 *         @p reasonCode holds the errno.
 * @retval CHARSET_UNKNOWN_CCSID            (24) @p inputCCSID or @p outputCCSID
 *         is not recognized.
 *
 * @note On platforms without a native iconv streaming converter, the portable
 *       fallback delegates to convertCharset and may additionally return
 *       CHARSET_INTERNAL_ERROR (12) or CHARSET_CONVERSION_ROUTINE_FAILURE (16).
 *       That fallback reports all input as consumed (no carry-forward).
 *
 * @return One of the CHARSET_* status codes above.
 */
int convertCharsetStreaming(char *input, int inputLength, int inputCCSID,
                            char *output, int outputLength, int outputCCSID,
                            int *conversionOutputLength, int *inputBytesConsumed,
                            int *reasonCode);

/**
   Returns -1 if charsetName is not known.
   
   Returned codes may be platform-specific.
*/
int getCharsetCode(const char *charsetName);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

