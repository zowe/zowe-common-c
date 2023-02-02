

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __IDCAMS_H__
#define __IDCAMS_H__ 1

#include "zowetypes.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS targets are supported only
#endif

#define IDCAMS_SYSIN_EXIT_RECORD_SIZE 80

typedef struct IDCAMSCommand_tag IDCAMSCommand;
typedef struct IDCAMSCommandOutput_tag IDCAMSCommandOutput;

#pragma map(idcamsCreateCommand, "RSIDCCCM")
#pragma map(idcamsAddLineToCommand, "RSIDCALN")
#pragma map(idcamsExecuteCommand, "RSIDCEXE")
#pragma map(idcamsDeleteCommand, "RSIDCDCM")
#pragma map(idcamsPrintCommandOutput, "RSIDCPCO")
#pragma map(idcamsDeleteCommandOutput, "RSIDCDCO")

/**
 * @brief Allocates a handle for an IDCAMS command.
 *
 * @return Handle if success, NULL in case of an allocation error.
 */
IDCAMSCommand *idcamsCreateCommand();

/**
 * @brief Adds a command line to an IDCAMS command handle.
 *
 * @param cmd Command handle to be used.
 * @param line IDCAMS command line to be added.
 * @return RC_IDCAMS_OK in case of success, RC_IDCAMS_LINE_TOO_LONG if the line
 * is too long, RC_IDCAMS_ALLOC_FAILED in case of a memory allocation error.
 */
int idcamsAddLineToCommand(IDCAMSCommand *cmd, const char *line);

/**
 * @brief Executes an IDCAMS command.
 *
 * @param cmd Handle with the commands to be executed.
 * @param output Output from IDCAMS. Ignored if NULL. The caller is responsible
 * for removal of this command.
 * @param reasonCode
 * @return RC_IDCAMS_OK in case of success, RC_IDCAMS_ALLOC_FAILED in case of
 * a memory allocation error, RC_IDCAMS_CALL_FAILED if the IDCAMS call failed.
 * If RC_IDCAMS_CALL_FAILED is returned, the reasonCode parameter will return
 * the IDCAMS return code.
 */
int idcamsExecuteCommand(const IDCAMSCommand *cmd,
                         IDCAMSCommandOutput **output,
                         int *reasonCode);

/**
 * @brief Deletes a IDCAMS command handle.
 *
 * @param cmd Command to delete.
 */
void idcamsDeleteCommand(IDCAMSCommand *cmd);

/**
 * @brief Prints the output of the execution of an IDCAMS command to stdout.
 *
 * @param commandOutput Command to print.
 */
void idcamsPrintCommandOutput(const IDCAMSCommandOutput *commandOutput);

/**
 * @brief Deletes the IDCAMS command output.
 *
 * @param commandOutput Command to be deleted.
 */
void idcamsDeleteCommandOutput(IDCAMSCommandOutput *commandOutput);

#define RC_IDCAMS_OK              0
#define RC_IDCAMS_LINE_TOO_LONG   4
#define RC_IDCAMS_CALL_FAILED     8
#define RC_IDCAMS_ALLOC_FAILED    16

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

