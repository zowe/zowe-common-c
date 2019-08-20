/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef RS_rsclibc_H_
#define RS_rsclibc_H_

#include "zowetypes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "utils.h"

unsigned long long rsclibc_currentSecondsSinceJan1970(void);
int rsclibc_currentSecondsAndMicrosSince1970(unsigned int *out_seconds,
                                              unsigned int *out_micros);

char *rscDuplicateString(const char *s);
void *rscAlloc(size_t size, const char *label);
void rscFree(void *data, size_t size);


#endif /* RS_rsclibc_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
