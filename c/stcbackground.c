#include "logging.h"
#include "stcbackground.h"

#define MAIN_WAIT_MILLIS 10000
#define STC_BG_ENTRIES 101
#define STC_BG_MIN_INTERVAL_SECS MAIN_WAIT_MILLIS/1000
#define STC_BG_CB_DUPLICATE_LABEL_ERR -2
#define STC_BG_CB_LABEL_LEN 30

struct STCIntervalCallbackData_tag {
  int id;
  char callbackLabel[STC_BG_CB_LABEL_LEN];
  STCIntervalCallback callback;
  void* userData;
  int intervalSeconds;
  int countInterval;
}; 

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

    t->countInterval -= STC_BG_MIN_INTERVAL_SECS;
    if (t->countInterval <= 0) {
      STCIntervalCallback callback = t->callback;

      callback(stcBase, module, t, t->userData);
      
      t->countInterval = t->intervalSeconds;
    }
  }
  return 0;
};

static STCIntervalCallbackData* findCallback(STCIntervalCallbackData callbackList[], const char* callbackLabel) {
  if(callbackList == NULL || callbackLabel == NULL) return NULL;

  for (int i = 0; i < STC_BG_ENTRIES; i++) {
    STCIntervalCallbackData* callbackData  = (callbackList + i);
    if (callbackData->id == -1) break;

    if(!strcmp(callbackData->callbackLabel, callbackLabel)) {
      return callbackData;
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

  return stcBackgroundModule;
};

const char* truncateLabel(const char* callbackLabel) {
  int len = strlen(callbackLabel);
  if (len > STC_BG_CB_LABEL_LEN-1) {
    len = STC_BG_CB_LABEL_LEN-1;
  }

  char* labelCopy = (char*)safeMalloc((len+1)*sizeof(char),"char");
  memcpy(labelCopy, callbackLabel, len);
  *(labelCopy + len) = '\0';

  return labelCopy;
}

int stcAddIntervalCallback(STCModule *module, STCIntervalCallback callback, const char* callbackLabel, int intervalSeconds, void* userData) {
  STCCallbackList* moduleData = (STCCallbackList*)(module->data);
  STCIntervalCallbackData* callbackList = moduleData->callbackList;

  int len;
  int slotId = nextSlot(callbackList);
  if (slotId >= 0 && slotId < STC_BG_ENTRIES) {

    const char* labelTruncated = truncateLabel(callbackLabel);
    if(findCallback(callbackList, labelTruncated) != NULL) {
      safeFree((char*)labelTruncated,strlen(labelTruncated)+1);
      return STC_BG_CB_DUPLICATE_LABEL_ERR;
    }

    memcpy(callbackList[slotId].callbackLabel, labelTruncated, strlen(labelTruncated)+1);
    safeFree((char*)labelTruncated,strlen(labelTruncated)+1);

    callbackList[slotId].id = slotId;
    callbackList[slotId].callback = callback;
    callbackList[slotId].intervalSeconds = intervalSeconds;
    callbackList[slotId].countInterval = intervalSeconds;
    callbackList[slotId].userData = userData;
    return 0;
  }

  return -1;
};

//if intervalSeconds < 0, we disable the job
int stcModifyInterval(STCModule *module, const char* callbackLabel, int newIntervalSeconds) {
  STCCallbackList* moduleData = (STCCallbackList*)(module->data);
  STCIntervalCallbackData* callbackList;
  if(moduleData != NULL) {
    callbackList = moduleData->callbackList;
  }

  STCIntervalCallbackData* callbackData = findCallback(callbackList, callbackLabel);
  if(callbackData != NULL) {
    callbackData->intervalSeconds = newIntervalSeconds;
    callbackData->countInterval = newIntervalSeconds;
    return 0;
  }

  return -1;
}