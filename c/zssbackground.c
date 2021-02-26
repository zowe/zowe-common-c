#include "logging.h"
#include "zssbackground.h"

int nextSlot() {
  for(int i=1; i < N_TASK_TABLE_ENTRIES; i++) {
    if(task_list[i].id==0)  /* initialise */ {
      return i;
    }
  }
  return -1;
}

int addZssBackgroudTask(Task task, char* taskLabel, int timeInterval) {
  int len;
  int slotId = nextSlot();
  if(slotId>0) {
    task_list[slotId].id = slotId;
    len = strlen(taskLabel);
    if(len>LEN_TASK_LABEL-1) {
      len = LEN_TASK_LABEL-1;
    }
    memcpy(task_list[slotId].taskLabel, taskLabel, len);
    task_list[slotId].task = task;
    task_list[slotId].timeInterval = timeInterval;
    task_list[slotId].countInterval = -timeInterval;
    return 0;
  }
  return slotId;
}

int processZssBackgroundHandler(STCBase *base, STCModule *module, int selectStatus) {
  for(int i=1; i < N_TASK_TABLE_ENTRIES; i++) {
    Background_Task* t = &task_list[i];
    if(t->id!=0)  /* initialise */ {
      t->countInterval+=ZSS_BACK_SECONDS;
      if(t->countInterval==0) {
        Task task = t->task;
        task(module->data);
        t->countInterval=-t->timeInterval;
      }
    } else {
      break;
    }
  }
  return 0;
};


