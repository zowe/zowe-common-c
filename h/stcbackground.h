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
#define STC_BG_CB_LABEL_LEN 30

struct STCIntervalCallbackData_tag;
typedef int (*STCIntervalCallback)(void* server, 
                                    STCBase* stcBase, 
                                    STCModule* module, 
                                    struct STCIntervalCallbackData_tag* callbackData, 
                                    void* userData
                                   );

typedef struct STCIntervalCallbackData_tag {
  int id;
  char callbackLabel[STC_BG_CB_LABEL_LEN];
  STCIntervalCallback callback;
  void* userData;
  int timeInterval;
  int countInterval;
} STCIntervalCallbackData; 

typedef struct STCCallbackList_tag {
  void* server;
  void* callbackList;
} STCCallbackList;

STCModule* stcInitBackgroundModule(void* server, STCBase *base);
int stcAddIntervalCallback(STCModule *module, STCIntervalCallback callback, char* callbackLabel, int timeInterval, void* userData);
int stcModifyInterval(STCIntervalCallbackData* callback, int newInterval);

#endif 
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which 
  accompanies this distribution, and is available at 
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/