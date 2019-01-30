

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __DEBUGTOOL__
#define __DEBUGTOOL__ 1

#ifdef METTLE
#include "metalio.h"
#else
#include <stdio.h>
#include <stdarg.h>
#endif
#include "utils.h"

/* This will only work for c99 standard. */

#ifndef DEBUG
#define DEBGLVL 0 // Turn the debug logging off
#endif

#if defined DEBUG && !defined DEBGLVL
#define DEBGLVL 5 // Default severity level
#endif

#define LOG_FATAL    1
#define LOG_ERR      2
#define LOG_WARN     3
#define LOG_INFO     4
#define LOG_DBG      5
#define LOG_DBG2     6

#define LOG(level, ...) do {  \
                             if (level <= DEBGLVL) { \
                               printf(__VA_ARGS__); \
                           } \
                        } while (0)

#define DUMP(level, ...) do {  \
                             if (level <= DEBGLVL) { \
                               dumpbuffer(__VA_ARGS__); \
                             } \
                         } while (0)


#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

