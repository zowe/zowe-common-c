

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef H_CELLPOOL_H_
#define H_CELLPOOL_H_

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#include "zowetypes.h"

#ifndef __LONGNAME__

#define cellpoolGetDWordAlignedSize CPASIZE
#define cellpoolBuild CPBUILD
#define cellpoolDelete CPDELETE
#define cellpoolGet CPGET
#define cellpoolFree CPFREE

#endif

ZOWE_PRAGMA_PACK

typedef int32_t CPID;

typedef struct CPHeader_tag {
  char text[24];
} CPHeader;

ZOWE_PRAGMA_PACK_RESET

<<<<<<< HEAD
<<<<<<< HEAD
#define CPID_NULL 0
=======
#define CPID_NULL -1
>>>>>>> 96d076a... Add wrappers for CPOOL.
=======
#define CPID_NULL 0
>>>>>>> 0b15b01... Use 0 for CPID_NULL - less error-prone. Update test build commands.

/**
 * @brief Helper function to convert a size to its next 8-byte aligned value.
 * This function can be used to ensure that the cells are aligned properly.
 * @param size The value to be converted.
 * @return 8-byte aligned value.
 */
unsigned int cellpoolGetDWordAlignedSize(unsigned int size);

/**
 * @brief Build a 31-bit cell pool.
 * @details
 * - When the function is called in SRB or cross-memory mode, the cell
 * pool storage belongs to the TCB from ASCBXTCB, otherwise the storage will be
 * owned by the TCB from PSATOLD.
 * - In SRB and cross-memory modes the call requires elevated privileges
 * (key 0-7, SUP state or APF).
 * - The storage is aligned on the default CPOOL boundary, i.e. if the cell size
 * is not a multiple of 4 or 8, cells do not reside on a particular boundary,
 * otherwise the cells will reside on 4 and 8 byte boundaries respectively.
 * @param pCellCount Primary cell count.
 * @param sCellCount Secondary cell count.
 * @param cellSize Cell size.
 * @param subpool The subpool of the cell pool storage.
 * @param key The key of the cell pool storage.
 * @param header The cell pool header.
 * @return The cell pool ID on success, CPID_NULL on failure.
 */
CPID cellpoolBuild(unsigned int pCellCount,
                   unsigned int sCellCount,
                   unsigned int cellSize,
                   int subpool, int key,
                   const CPHeader *header);

/**
 * @brief Delete a cell pool.
 * @param cellpoolID The ID of the cell pool to be deleted.
 */
void cellpoolDelete(CPID cellpoolID);

/**
 * @brief Get a cell pool cell.
 * @param cellpoolID The ID of the cell pool to get a cell from.
 * @param conditional If true and the cell pool cannon get more storage, NULL is
 * returned, otherwise the request will ABEND.
 */
void *cellpoolGet(CPID cellpoolID, bool conditional);

/**
 * @brief Release a cell pool cell.
 * @param cellpoolID The ID of the cell pool to return the cell to.
 * @param cell The cell to be returned.
 */
void cellpoolFree(CPID cellpoolID, void *cell);

#endif /* H_CELLPOOL_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

