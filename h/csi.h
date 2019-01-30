

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __CSI__
#define __CSI__   1

/*
  MVS Catalog search interface. This has no equivalent in Unixland, do not attempt to port.
 */

#define DSN_AVAILABLE 0
#define DSN_MIGRATED  1
#define DSN_NOT_CATALOGED 5
#define DSN_ZERO_LENGTH_DSN 99
#define DSN_CSI_FAILURE 100

#define FIELD_NAME_LENGTH 8

typedef struct CSIRequestParmBlock_tag {

  char *dsnFilter;
  int filterLength;
  char *fieldNames;    // 8-byte char sequence array is expected
  int fieldCount;

} CSIRequestParmBlock;

/* The format of the workarea to be returned:
 * CSIWorkareaHeader
 * CSICatalogData
 * CSIEntryData
 * requested field lengths
 * requested field data
 * http://pic.dhe.ibm.com/infocenter/zos/v1r12/index.jsp?topic=%2Fcom.ibm.zos.r12.idac100%2Fc1057.htm */

ZOWE_PRAGMA_PACK

typedef struct _CSIWorkareaHeader{
  int total_size;
  int required_length; /* if insufficient for 1 cat entry and 1 entry */
  int used_length;
  short number_of_fields;  /* doc says this is actually 1 greater than number of fields */
} CSIWorkareaHeader;

typedef struct _CSICatalogData{
  char flags;
  char type; /* always 0xf0 ?? */
  char name[44];
  char module_id[2];
  char reason_code;
  char return_code;

} CSICatalogData;

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

typedef struct _CSIEntryData{
  char flags;
  char type;
  char name[44];

  union {

    struct {
      char module_id[2];
      char reason_code;
      char return_code;
    };

    struct {
      unsigned short totalDataLength;
      char reserved[2];
    };

  };

} CSIEntryData;

ZOWE_PRAGMA_PACK_RESET

#define dataSetStatus dsstats
int dataSetStatus(char *dsn);

#define getCSIInfo gtcsiif
CSIWorkareaHeader *getCSIInfo(CSIRequestParmBlock *csiRequest);

#define freeCSIResultArea frcsirsa
void freeCSIResultArea(CSIWorkareaHeader *resultArea);

#endif



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

