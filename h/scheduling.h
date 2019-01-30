

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __SCHEDULING__
#define __SCHEDULING__ 1

#ifndef __LONGNAME__

#define zosWait SCHZOSWT
#define zosWaitList SCHZOSWL
#define zosPost SCHZOSPT
#define makeRLETask LEMKRLET
#define deleteRLETask LEDLRLET
#define startRLETask SCHSRLET

#endif

int zosWait(void *ecb, int clearFirst);
int zosPost(void *ecb, int completionCode);

RLETask *makeRLETask(RLEAnchor *anchor, 
                     int taskFlags, 
                     int functionPointer(RLETask *task));
void deleteRLETask(RLETask *task);
int startRLETask(RLETask *task, int *completionECB);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

