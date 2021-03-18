#include "logging.h"
#include "stcbackground.h"

static int processStcBackgroundHandler(STCBase *base, STCModule *module, int selectStatus) {
  BackgroundTask* taskList = (BackgroundTask*)(module->data);
  for (int i = 1; i < N_TASK_TABLE_ENTRIES; i++) {
    BackgroundTask* t = &taskList[i];
    if (t->id != 0)  /* initialise */ {
      t->countInterval+=STC_BACKGROUND_INTERVAL;
      if (t->countInterval == 0) {
        Task task = t->task;
        task(base->httpServer, t->taskInput);
        t->countInterval=-t->timeInterval;
      }
    } else {
      break;
    }
  }
  return 0;
};

static int nextSlot(BackgroundTask* taskList) {
  for (int i = 1; i < N_TASK_TABLE_ENTRIES; i++) {
    if (taskList[i].id == 0)  /* initialise */ {
      return i;
    }
  }
  return -1;
};

STCModule* initBackgroundModule(STCBase *base) {

  BackgroundTask* taskList = (BackgroundTask*)safeMalloc(sizeof(BackgroundTask)*N_TASK_TABLE_ENTRIES,"BackgroundTask");
  STCModule* stcBackgroundModule=stcRegisterModule(
    base,
    STC_MODULE_BACKGROUND,
    taskList,
    NULL,
    NULL,
    NULL,
    processStcBackgroundHandler
  );
  return stcBackgroundModule;
};

int addStcBackgroudTask(STCModule *module, Task task, char* taskLabel, int timeInterval, void* taskInput) {
  BackgroundTask* taskList = (BackgroundTask*)(module->data);
  int len;
  int slotId = nextSlot(taskList);
  if (slotId > 0 && slotId<N_TASK_TABLE_ENTRIES) {
    taskList[slotId].id = slotId;
    len = strlen(taskLabel);
    if (len > LEN_TASK_LABEL-1) {
      len = LEN_TASK_LABEL-1;
    }
    memcpy(taskList[slotId].taskLabel, taskLabel, len);
    taskList[slotId].task = task;
    taskList[slotId].timeInterval = timeInterval;
    taskList[slotId].countInterval = -timeInterval;
    taskList[slotId].taskInput = taskInput;
    return 0;
  }
  return slotId;
};