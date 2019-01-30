

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __RESMGR__
#define __RESMGR__

#include "zowetypes.h"

#if !defined(__ZOWE_OS_ZOS)
#error "unsupported platform"
#endif

ZOWE_PRAGMA_PACK

typedef struct ResourceManagerHandle_tag {
  char eyecatcher[8];
#define RESMGR_HANDLE_EYECATCHER  "RSRCVRMH"
  char token[4];
  void * __ptr32 wrapperParms;
  unsigned short asid;
} ResourceManagerHandle;

typedef int (ResourceManagerRouinte)(void * __ptr32 managerParmList, void * __ptr32 userData, int * __ptr32 reasonCode);

ZOWE_PRAGMA_PACK_RESET

#ifndef __LONGNAME__
#define resmgrAddTaskResourceManager RMGRATRM
#define resmgrAddAddressSpaceResourceManager RMGRAARM
#define resmgrDeleteTaskResourceManager RMGRDTRM
#define resmgrDeleteAddressSpaceResourceManager RMGRDARM
#endif

/*****************************************************************************
* Add task resource manager.
*
* Parameters:
*   userRoutine               - user routine to be called when resource manager
*                               is invoked
*   userData                  - user data passed to user routine
*   handle                    - handle containing resource manager information
*   managerRC                 - resource manager return code
*
* Return value:
*   RC_RESMGR_OK is returned when resource manager has been added.
*   RC_RESMGR_SERVICE_FAILED is returned when an error occurred (see managerRC
*   for details).
*****************************************************************************/
int resmgrAddTaskResourceManager(ResourceManagerRouinte * __ptr32 userRoutine, void * __ptr32 userData,
                                 ResourceManagerHandle * __ptr32 handle, int *managerRC);

/*****************************************************************************
* Add address space resource manager.
*
* Parameters:
*   asid                      - address space to be monitored
*   userRoutine               - user routine to be called when resource manager
*                               is invoked (must be in common storage)
*   userData                  - user data passed to user routine
*   handle                    - handle containing resource manager information
*   managerRC                 - resource manager return code
*
* Return value:
*   RC_RESMGR_OK is returned when resource manager has been added.
*   RC_RESMGR_SERVICE_FAILED is returned when an error occurred (see managerRC
*   for details).
*****************************************************************************/
int resmgrAddAddressSpaceResourceManager(unsigned short asid, void * __ptr32 userRoutine, void * __ptr32 userData,
                                         ResourceManagerHandle * __ptr32 handle, int *managerRC);

/*****************************************************************************
* Delete task resource manager.
*
* Parameters:
*   handle                    - handle containing resource manager information
*   managerRC                 - resource manager return code
*
* Return value:
*   RC_RESMGR_OK is returned when resource manager has been deleted.
*   RC_RESMGR_SERVICE_FAILED is returned when an error occurred (see managerRC
*   for details).
*****************************************************************************/
int resmgrDeleteTaskResourceManager(ResourceManagerHandle * __ptr32 handle, int *managerRC);

/*****************************************************************************
* Delete address space resource manager.
*
* Parameters:
*   handle                    - handle containing resource manager information
*   managerRC                 - resource manager return code
*
* Return value:
*   RC_RESMGR_OK is returned when resource manager has been deleted.
*   RC_RESMGR_SERVICE_FAILED is returned when an error occurred (see managerRC
*   for details).
*****************************************************************************/
int resmgrDeleteAddressSpaceResourceManager(ResourceManagerHandle * __ptr32 handle, int *managerRC);

#define RC_RESMGR_OK              0
#define RC_RESMGR_ERROR           8
#define RC_RESMGR_SERVICE_FAILED  9

#endif /* __RESMGR__ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

