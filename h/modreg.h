/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef H_MODREG_H_
#define H_MODREG_H_   1

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdint.h>
#else
#include <stdint.h>
#endif

#include "lpa.h"
#include "zowetypes.h"

/**
 * This macro marks a module as eligible for the module registry. The macro
 * should be used once in the module code, for example, in the main function.
 */
#define MODREG_MARK_MODULE() \
  __asm( \
      "         MACRO                                                          \n" \
      "&LABEL   GENMMARK                                                       \n" \
      "         LCLA  &MARKVER                                                 \n" \
      "         LCLA  &MARKLEN                                                 \n" \
      "&MARKVER SETA  1                                                        \n" \
      "&MARKLEN SETA  48                                                       \n" \
      "L$GMST   DC    0D                                                       \n" \
      "         DC    CL8'ZWECMOD:'                                            \n" \
      "         DC    H'&MARKVER'                                              \n" \
      "         DC    H'&MARKLEN'                                              \n" \
      "         DC    XL4'00'                                                  \n" \
      "         DC    CL26'&SYSCLOCK'                                          \n" \
      "         DC    XL6'00'                                                  \n" \
      "         MEND  ,                                                        \n" \
      "L$GMMBGN DS    0H                                                       \n" \
      "         J     L$GMMEXT                                                 \n" \
      "         GENMMARK                                                       \n" \
      "L$GMMEXT DS    0H                                                       \n" \
  )

#define MODREG_MAX_REGISTRY_SIZE        32768

#define RC_MODREG_OK                    0
#define RC_MODREG_ALREADY_REGISTERED    1
#define RC_MODREG_MARK_MISSING          2
#define RC_MODREG_ZVT_NULL              3
#define RC_MODREG_OPEN_SAM_ERROR        8
#define RC_MODREG_MODULE_LOAD_ERROR     9
#define RC_MODREG_MODULE_DELETE_ERROR   10
#define RC_MODREG_MARK_MISSING_LPA      11
#define RC_MODREG_CHAIN_NOT_LOCKED      12
#define RC_MODREG_CHAIN_NOT_UNLOCKED    13
#define RC_MODREG_ALLOC_FAILED          14
#define RC_MODREG_LPA_ADD_FAILED        15

#ifndef __LONGNAME__
#define modregRegister MODRRGST
#endif

/**
 * The function attempts to add a module to the module registry and the LPA; if
 * the module is already in the registry, only the existing LPA info is
 * returned and @c RC_MODREG_ALREADY_REGISTERED will be returned; if the module
 * has no "mark" defined by the @c MODREG_MARK_MODULE macro, no action will be
 * taken and @c RC_MODREG_MARK_MISSING will be returned.
 *
 * When checking the registry, two modules are considered the same, if and
 * only if, the modules names and their marks match.
 *
 * @param[in] ddname the DDNAME of the module to be registered.
 * @param[in] module the module to be registered.
 * @param[out] lpaInfo the LPA info of this module.
 * @param[out] rsn an optional reason code variable providing additional
 * information about the failure.
 * @return @c RC_MODREG_OK if the modules has been successfully added,
 * @c RC_MODREG_ALREADY_REGISTERED if the module is already in the registry,
 * and any other RC_MODREG_xxx in case of failure.
 */
int modregRegister(EightCharString ddname, EightCharString module,
                   LPMEA *lpaInfo, uint64_t *rsn);

#endif /* H_MODREG_H_ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
