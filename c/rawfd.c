

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include "zowetypes.h"
#include "rawfd.h"

FileDescriptor stdinDescriptor = 0;
FileDescriptor stdoutDescriptor = 1;
FileDescriptor stderrDescriptor = 2;

#if defined(__ZOWE_OS_ZOS)
#include "bpxrawfd.c"
#elif defined(__ZOWE_OS_LINUX)
#include "psxrawfd.c"
#else
#error No implementation for rawfd.h
#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

