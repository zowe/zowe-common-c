

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __JCSI__
#define __JCSI__ 1
#include "csi.h"

/*
  MVS Catalog search interface. This has no equivalent in Unixland, do not attempt to port.
 */

#define MAX_REQUEST_FIELDS 100
#define WORK_AREA_SIZE 100000


typedef struct _csi_parmblock{
  char filter_spec[44];
  char catalog_name[44];
  char resume_name[44];
  char types[16];
  char match_on_cluster_name;  /* Y or whatever */
  char is_resume;              /* Y or blank, set by CSI on return */
  char use_one_catalog;        /* Y or whatever, although doc is unclear */
  char use_fullword_offsets;   /* F or whatever */
  short num_fields;            /* I hope short is 16 bits here !! */
  char fields[8*MAX_REQUEST_FIELDS];
} csi_parmblock;

typedef struct _csi_work_area_header{
  int total_size;
  int required_length; /* if insufficient for 1 cat entry and 1 entry */
  int used_length;
  short number_of_fields;  /* doc says this is actually 1 greater than number of fields */
} csi_work_area_header;

typedef struct _csi_catalog_data{
  char flags;
  char type; /* always 0xf0 ?? */
  char name[44];
  char module_id[2];
  char reason_code;
  char return_code;
} csi_catalog_data;

/*
0(0) Bitstring 1 CSIEFLAG Entry flag information.
1... .... CSIPMENT Primary entry.
0... .... This entry associates with the preceding primary entry.
.1.. .... CSIENTER Error indication is set for this entry and error code follows
CSIENAME.
..1. .... CSIEDATA Data is returned for this entry.
...1 1111 Reserved.

1(1) Character 1 CSIETYPE Entry Type.
A Non-VSAM data set
B Generation data group
C Cluster
D Data component
G Alternate index
H Generation data set
I Index component
R Path
X Alias
U User catalog connector entry
L ATL Library entry
W ATL Volume entry
 */

typedef struct _csi_entry_data{
  char flags;
  char type;
  char name[44];
  char module_id[2];
  char reason_code;
  char return_code;
  
} csi_entry_data;

#define CSI_ENTRY_ERROR 0x40

typedef struct EntryData_tag{
  char           flags;
  char           type;
  char           name[44];
  union {
    struct{
      char    moduleID[2];
      char    reasonCode;
      char    returnCode;
    } errorInfo;
    struct{
      unsigned short totalLength;
      unsigned short reserved;
    } fieldInfoHeader;
  } data;
} EntryData;

typedef struct EntryDataSet_tag{
  int length;
  int size;
  EntryData **entries;
} EntryDataSet;

int loadCsi();
char * __ptr32 csi(csi_parmblock* __ptr32 csi_parms, int *workAreaSize);
EntryDataSet *returnEntries(char *dsn, char *typesAllowed, int typeCount, int workAreaSize, char **fields, int fieldCount, char *resumeName, char *resumeCatalogName, csi_parmblock * __ptr32 returnParms);
EntryDataSet *getHLQs(char *typesAllowed, int typeCount, int workAreaSize, char **fields, int fieldCount, csi_parmblock *__ptr32 * __ptr32 returnParmsArray);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

