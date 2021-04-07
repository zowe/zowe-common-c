#include "logging.h"
#include "stcbackground.h"

#define STC_BG_ENTRIES 101
#define STC_BG_INTERVAL 10


static int processInterval(STCBase *stcBase, STCModule *module, int selectStatus) {
  STCCallbackList* moduleData = (STCCallbackList*)(module->data);
  STCIntervalCallbackData* callbackList = moduleData->callbackList;

  for (int i = 0; i < STC_BG_ENTRIES; i++) {
    STCIntervalCallbackData* t = &callbackList[i];
    if (t->id != -1)  /* initialise */ {
      
      // giving ability to disable job by making timeInterval less than zero
      // as we return callbackdata, user can make timeInterval zero
      if(t->timeInterval<0) continue;

      t->countInterval+=STC_BG_INTERVAL;
      if (t->countInterval <= 0) {
        STCIntervalCallback callback = t->callback;
        callback(moduleData->server, stcBase, module, t, t->userData);
        t->countInterval = -t->timeInterval;
      }
    } else {
      break;
    }
  }
  return 0;
};

static int nextSlot(STCIntervalCallbackData* callbackList) {
  for (int i = 0; i < STC_BG_ENTRIES; i++) {
    if (callbackList[i].id == -1)  /* initialise */ {
      return i;
    }
  }
  return -1;
};

STCModule* stcInitBackgroundModule(void* server, STCBase *stcBase) {
  STCCallbackList* moduleData = (STCCallbackList*)safeMalloc(sizeof(STCCallbackList),"STCCallbackList");
  if (moduleData == NULL) {
    return NULL;
  }

  STCIntervalCallbackData* callbackList = (STCIntervalCallbackData*)safeMalloc(sizeof(STCIntervalCallbackData)*STC_BG_ENTRIES,"STCIntervalCallbackData");
  if (callbackList == NULL) {
    return NULL;
  }

  for (int i = 0; i < STC_BG_ENTRIES; i++) {
    callbackList[i].id = -1;
  }
  moduleData->callbackList = callbackList;
  moduleData->server = server;

  STCModule* stcBackgroundModule=stcRegisterModule(
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

int stcAddIntervalCallback(STCModule *module, STCIntervalCallback callback, char* callbackLabel, int timeInterval, void* userData) {
  STCCallbackList* moduleData = (STCCallbackList*)(module->data);
  STCIntervalCallbackData* callbackList = moduleData->callbackList;

  int len;
  int slotId = nextSlot(callbackList);
  if (slotId >= 0 && slotId<STC_BG_ENTRIES) {
    callbackList[slotId].id = slotId;
    len = strlen(callbackLabel);
    if (len > STC_BG_CB_LABEL_LEN-1) {
      len = STC_BG_CB_LABEL_LEN-1;
    }
    memcpy(callbackList[slotId].callbackLabel, callbackLabel, len);
    callbackList[slotId].callback = callback;
    callbackList[slotId].timeInterval = timeInterval;
    callbackList[slotId].countInterval = -timeInterval;
    callbackList[slotId].userData = userData;
    return 0;
  }

  return -1;
};

//if timeInterval < 0, we disable the job
int stcModifyInterval(STCIntervalCallbackData* callback, int newInterval) {
  callback->timeInterval = newInterval;
  callback->countInterval = -newInterval;
}