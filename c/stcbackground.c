/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which 
  accompanies this distribution, and is available at 
  https://www.eclipse.org/legal/epl-v20.html
  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

#include <time.h>
#include "logging.h"
#include "stcbackground.h"

#define STC_BG_ENTRIES 101
#define STC_BG_CB_DUPLICATE_LABEL_ERR -2
#define STC_BG_CB_LABEL_LENGTH_ERR -3
#define STC_BG_CB_LABEL_LEN 30

struct STCIntervalCallbackData_tag {
  bool isInitialized;
  char callbackLabel[STC_BG_CB_LABEL_LEN];
  STCIntervalCallback callback;
  void* userData;
  int intervalSeconds;
  unsigned int timeOfLastRun;
}; 

typedef struct STCCallbackList_tag {
  STCIntervalCallbackData callbackList[STC_BG_ENTRIES];
} STCCallbackList;


static int processInterval(STCBase *stcBase, STCModule *module, int selectStatus) {

  STCCallbackList* moduleData = (STCCallbackList*)(module->data);
  STCIntervalCallbackData* callbackList = moduleData->callbackList;

  unsigned int currTime = (unsigned)time(NULL);

  for (int i = 0; i < STC_BG_ENTRIES; i++) {
    STCIntervalCallbackData* t = (callbackList + i);
    if (!t->isInitialized) break;

    // giving ability to disable job by making intervalSeconds less than zero
    if (t->intervalSeconds < 0) continue;

    if ((currTime - t->timeOfLastRun) >= t->intervalSeconds) {
      STCIntervalCallback callback = t->callback;
      callback(stcBase, module, t, t->userData);
      t->timeOfLastRun = (unsigned)time(NULL);
    }
  }
  return 0;
};

static STCIntervalCallbackData* findCallback(STCIntervalCallbackData callbackList[], const char* callbackLabel) {
  if(callbackList == NULL || callbackLabel == NULL) return NULL;

  for (int i = 0; i < STC_BG_ENTRIES; i++) {
    STCIntervalCallbackData* callbackData  = (callbackList + i);
    if (!callbackData->isInitialized) break;

    if(!strcmp(callbackData->callbackLabel, callbackLabel)) {
      return callbackData;
    }
  }
  return NULL;
}

static int nextSlot(STCIntervalCallbackData* callbackList) {
  for (int i = 0; i < STC_BG_ENTRIES; i++) {
    if (!callbackList[i].isInitialized)  /* initialise */ {
      return i;
    }
  }
  return -1;
};

STCModule* stcInitBackgroundModule(STCBase *stcBase) {
  STCCallbackList* moduleData = (STCCallbackList*)safeMalloc(sizeof(STCCallbackList),"STCCallbackList");
  if (moduleData == NULL) {
    return NULL;
  }

  STCIntervalCallbackData* callbackList = moduleData->callbackList;
  for (int i = 0; i < STC_BG_ENTRIES; i++) {
    callbackList[i].isInitialized = false;
  }

  STCModule* stcBackgroundModule = stcRegisterModule(
    stcBase,
    STC_MODULE_BACKGROUND,
    moduleData,
    NULL,
    NULL,
    NULL,
    processInterval
  );

  return stcBackgroundModule;
};

int stcAddIntervalCallback(STCModule *module, STCIntervalCallback callback, const char* callbackLabel, int intervalSeconds, void* userData) {
  STCCallbackList* moduleData = (STCCallbackList*)(module->data);
  STCIntervalCallbackData* callbackList = moduleData->callbackList;

  int len;
  int slotId = nextSlot(callbackList);
  if (slotId >= 0 && slotId < STC_BG_ENTRIES) {

    int len = strlen(callbackLabel);
    if (len > STC_BG_CB_LABEL_LEN-1) {
      return STC_BG_CB_LABEL_LENGTH_ERR;
    }

    if(findCallback(callbackList, callbackLabel) != NULL) {
      return STC_BG_CB_DUPLICATE_LABEL_ERR;
    }

    memcpy(callbackList[slotId].callbackLabel, callbackLabel, len+1);

    callbackList[slotId].isInitialized = true;
    callbackList[slotId].callback = callback;
    callbackList[slotId].intervalSeconds = intervalSeconds;
    callbackList[slotId].timeOfLastRun = (unsigned)time(NULL);
    callbackList[slotId].userData = userData;
    return 0;
  }

  return -1;
};


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which 
  accompanies this distribution, and is available at 
  https://www.eclipse.org/legal/epl-v20.html
  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/