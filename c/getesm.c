

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include "stdio.h"
#include "string.h"
#include "zowetypes.h"
#include "zos.h"

int getESMHelp(int error) {
    if (error)
        printf("Wrong parameter(s), see help:\n");
    printf("getesm - gets the External Security Manager name and returns RACF, TSS, ACF2 or NONE\n");
    printf("  Format: getesm [-h]\n");
    printf("  Options:\n    -h  This help\n");
    printf("  Exit values:\n    0 for succesful detection\n    1 otherwise\n");
    return error;
}

int main(int argc, char *argv[]) {
    int rc;

    if ( argc > 1 ){
        if (argc == 2 && strcmp(argv[1], "-h") == 0)
            return getESMHelp(0);
        else
            return getESMHelp(1);    
    }
    
    switch(getExternalSecurityManager()) {
        case ZOS_ESM_RTSS:
            printf("TSS\n");
            return 0;
        case ZOS_ESM_RACF:
            printf("RACF\n");
            return 0;
        case ZOS_ESM_ACF2:
            printf("ACF2\n");
            return 0;
        case ZOS_ESM_NONE:
            printf("NONE\n");
            return 0;
        default:
            printf("Error processing Communications Vector Table (CVT).\n");
            return 1;
    }
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
