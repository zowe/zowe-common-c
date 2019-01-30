

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

IDCAMSCommand *idcamsCreateCommand();
int idcamsAddLineToCommand(IDCAMSCommand *cmd, const char *line);
int idcamsExecuteCommand(const IDCAMSCommand *cmd,
                         IDCAMSCommandOutput **output,
                         int *reasonCode);
void idcamsDeleteCommand(IDCAMSCommand *cmd);

void idcamsPrintCommandOutput(const IDCAMSCommandOutput *commandOutput);
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

