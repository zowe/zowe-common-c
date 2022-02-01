

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
#include <metal/ctype.h>  
#include <metal/stdbool.h>  
#include "metalio.h"
#include "qsam.h"

#else

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>  
#include <sys/stat.h>
#include <sys/types.h> /* JOE 1/20/22 */
#include <stdint.h> /* JOE 1/20/22 */
#include <stdbool.h> /* JOE 1/20/22 */
#include <errno.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "bpxnet.h"
#include "unixfile.h"
#include "json.h"
#include "charsets.h"

#ifdef __ZOWE_OS_WINDOWS
typedef int64_t ssize_t;
#endif

/* A configuration manager is a facility providing access to a set of configuration source
   (most likely textual) in many formats in one or more locations whose data definition is
   provided by a set of JSON Schema Definions.

   Formats
     YAML
     JSON
     PARMLIB
     Environment Variables 

   Paths
     Configs and Schemas Live on paths of higher-to-lower precedence directories and libraries
     (PDS/PDSE's).  
 

   References

     http://json-schema.org

     https://datatracker.ietf.org/doc/html/draft-bhutton-json-schema-validation)
     
*/

#define CONFIG_PATH_OMVS_DIR   0x0001
#define CONFIG_PATH_MVS_LIBRAY 0x0002

typedef struct ConfigPathElement_tag {
  int    flags;
  char  *name;
  struct ConfigPathElement_tag *next;
} ConfigPathElement;

typedef struct ConfigManager_tag {
  ShortLivedHeap *slh;
  ConfigPathElement *schemaPath;
  configPathElement *configPath;
  hashtable *schemaCache;
  hashtable *configCache;
} ConfigManager;

#define ZCFG_SUCCESS 0

/* Merging notes
   defaulting and merging requires the same data shape at all levels and sources 
*/

/* all calls return status */

/* zowe.setup.mvs.proclib */

int cfgGetInt(ConfigManager *mgr, int *value, ...){  /* path elements which are strings and ints for array indexing */

}

int cfgGetInt64(ConfigManager *mgr, int64 *value, ...){

}

#define ZCFG_ALLOC_HEAP 1
#define ZCFG_ALLOC_SLH  2
#define ZCFG_ALLOC_MEMCPY 3 

int cfgGetString(ConfigManager *mgr, char *value, int allocOptions, void *mem, ...){
  return ZCFG_SUCCESS;
}

int cfgGetInstanceType(ConfigManager *mgr, int *type, int *subType, ...){

}






