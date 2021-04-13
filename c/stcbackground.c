#include "logging.h"
#include "stcbackground.h"

#define STC_BG_ENTRIES 101
#define STC_BG_INTERVAL_SECS 10
#define STC_BG_CB_DUPLICATE_LABEL_ERR -2

typedef struct STCCallbackList_tag {
  STCIntervalCallbackData callbackList[STC_BG_ENTRIES];
} STCCallbackList;

static int processInterval(STCBase *stcBase, STCModule *module, int selectStatus) {
  STCCallbackList* moduleData = (STCCallbackList*)(module->data);
  STCIntervalCallbackData* callbackList = moduleData->callbackList;

  for (int i = 0; i < STC_BG_ENTRIES; i++) {
    STCIntervalCallbackData* t = (callbackList + i);
    if (t->id == -1) break;

    // giving ability to disable job by making intervalSeconds less than zero
    // as we return callbackdata, user can make intervalSeconds zero
    if (t->intervalSeconds < 0) continue;

    t->countInterval += STC_BG_INTERVAL_SECS;
    if (t->countInterval <= 0) {
      STCIntervalCallback callback = t->callback;
      callback(stcBase, module, t, t->userData);
      t->countInterval = -t->intervalSeconds;
    }
  }
  return 0;
};

static STCIntervalCallbackData* findCallback(STCIntervalCallbackData callbackList[], const char* callbackLabel) {
  for (int i = 0; i < STC_BG_ENTRIES; i++) {
    STCIntervalCallbackData callbackData  = callbackList[i];
    if (callbackData.id == -1) break;

    if(!strcmp(callbackData.callbackLabel, callbackLabel)) {
      return &callbackData;
    }
  }
  return NULL;
}

static int nextSlot(STCIntervalCallbackData* callbackList) {
  for (int i = 0; i < STC_BG_ENTRIES; i++) {
    if (callbackList[i].id == -1)  /* initialise */ {
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
    callbackList[i].id = -1;
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

  stcBaseMainLoop(stcBase, STC_BG_INTERVAL_SECS*1000);
  return stcBackgroundModule;
};

const char* trimLabel(const char* callbackLabel) {
  int len = strlen(callbackLabel);
  if (len > STC_BG_CB_LABEL_LEN-1) {
    len = STC_BG_CB_LABEL_LEN-1;
  }

  char labelCopy[STC_BG_CB_LABEL_LEN] = "";
  strncpy(labelCopy, callbackLabel, len); 
  return labelCopy;
}

int stcAddIntervalCallback(STCModule *module, STCIntervalCallback callback, const char* callbackLabel, int intervalSeconds, void* userData) {
  STCCallbackList* moduleData = (STCCallbackList*)(module->data);
  STCIntervalCallbackData* callbackList = moduleData->callbackList;

  int len;
  int slotId = nextSlot(callbackList);
  if (slotId >= 0 && slotId < STC_BG_ENTRIES) {

    const char* labelTrim = trimLabel(callbackLabel);
    if(findCallback(callbackList, labelTrim) != NULL) {
      return STC_BG_CB_DUPLICATE_LABEL_ERR;
    }

    callbackList[slotId].id = slotId;
    memcpy(callbackList[slotId].callbackLabel, labelTrim, strlen(labelTrim));
    callbackList[slotId].callback = callback;
    callbackList[slotId].intervalSeconds = intervalSeconds;
    callbackList[slotId].countInterval = -intervalSeconds;
    callbackList[slotId].userData = userData;
    return 0;
  }

  return -1;
};

//if intervalSeconds < 0, we disable the job
int stcModifyInterval(STCModule *module, const char* callbackLabel, int newIntervalSeconds) {
  STCCallbackList* moduleData = (STCCallbackList*)(module->data);
  STCIntervalCallbackData* callbackList = moduleData->callbackList;

  STCIntervalCallbackData* callbackData = findCallback(callbackList, callbackLabel);
  callbackData->intervalSeconds = newIntervalSeconds;
  callbackData->countInterval = -newIntervalSeconds;
}