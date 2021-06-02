/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which 
  accompanies this distribution, and is available at 
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __STCBACKGROUND__
#define __STCBACKGROUND__

#include "stcbase.h"

typedef struct STCIntervalCallbackData_tag STCIntervalCallbackData;

typedef int (*STCIntervalCallback)(STCBase* stcBase, 
                                    STCModule* module, 
                                    STCIntervalCallbackData* callbackData, 
                                    void* userData
                                   );

STCModule* stcInitBackgroundModule(STCBase *base);
int stcAddIntervalCallback(STCModule *module, STCIntervalCallback callback, const char* callbackLabel, int intervalSeconds, void* userData);

#endif 
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which 
  accompanies this distribution, and is available at 
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/