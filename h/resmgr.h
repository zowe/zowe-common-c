

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

/**
 * @brief Contains the information representing a resource manager instance.
 */
typedef struct ResourceManagerHandle_tag {
  char eyecatcher[8];
#define RESMGR_HANDLE_EYECATCHER  "RSRCVRMH"
  char token[4];
  void * __ptr32 wrapperParms;
  unsigned short asid;
} ResourceManagerHandle;

/**
 * @brief A user function invoked when the resource manager is triggered.
 *
 * @param managerParmList Address of the resource manager parameter list (see
 * IHARMPL for details).
 * @param userData User data specified on the resmgrAddXXXXResourceManager call.
 * @param reasonCode If the return value is 8, the output value 1 instructs
 * the system to remove the returning resource manager.
 * @return All values except 8 are ignored. 8 tells the system to honor
 * the user provided reason code.
 */
typedef int (ResourceManagerRouinte)(void * __ptr32 managerParmList, void * __ptr32 userData, int * __ptr32 reasonCode);

ZOWE_PRAGMA_PACK_RESET

#ifndef __LONGNAME__
#define resmgrAddTaskResourceManager RMGRATRM
#define resmgrAddAddressSpaceResourceManager RMGRAARM
#define resmgrDeleteTaskResourceManager RMGRDTRM
#define resmgrDeleteAddressSpaceResourceManager RMGRDARM
#endif

/**
 * @brief Adds a task resource manager.
 *
 * @param userRoutine User routine to be called when the resource manager is
 * invoked.
 * @param userData User data passed to the user routine.
 * @param handle Handle containing the resource manager information.
 * @param managerRC Resource manager return code.
 * @return RC_RESMGR_OK is returned when the resource manager has been added.
 * RC_RESMGR_SERVICE_FAILED is returned when an error occurred (see managerRC
 * for details).
 */
int resmgrAddTaskResourceManager(ResourceManagerRouinte * __ptr32 userRoutine, void * __ptr32 userData,
                                 ResourceManagerHandle * __ptr32 handle, int *managerRC);

/**
 * @brief  Adds an address space resource manager.
 *
 * @param asid Address space to be monitored.
 * @param userRoutine User routine to be called when the resource manager is
 * invoked (must be in common storage).
 * @param userData User data passed to the user routine.
 * @param handle Handle containing the resource manager information.
 * @param managerRC Resource manager return code.
 * @return RC_RESMGR_OK is returned when the resource manager has been added.
 * RC_RESMGR_SERVICE_FAILED is returned when an error occurred (see managerRC
 * for details).
 */
int resmgrAddAddressSpaceResourceManager(unsigned short asid, void * __ptr32 userRoutine, void * __ptr32 userData,
                                         ResourceManagerHandle * __ptr32 handle, int *managerRC);

/**
 * @brief Deletes a task resource manager.
 *
 * @param handle Handle containing resource manager information.
 * @param managerRC Resource manager return code.
 * @return RC_RESMGR_OK is returned when the resource manager has been deleted.
 * RC_RESMGR_SERVICE_FAILED is returned when an error occurred (see managerRC
 * for details).
 */
int resmgrDeleteTaskResourceManager(ResourceManagerHandle * __ptr32 handle, int *managerRC);

/**
 * @brief Deletes an address space resource manager.
 *
 * @param handle Handle containing the resource manager information.
 * @param managerRC Resource manager return code.
 * @return RC_RESMGR_OK is returned when the resource manager has been deleted.
 * RC_RESMGR_SERVICE_FAILED is returned when an error occurred (see managerRC
 * for details).
 */
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

