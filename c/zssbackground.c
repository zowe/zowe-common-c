#include "logging.h"
#include "zssbackground.h"


int nextSlot() {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO,"nextSlot\n");  
  for(int i=1; i < N_TASK_TABLE_ENTRIES; i++) {
    if(task_list[i].id==0)  /* initialise */ {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO,"nextSlot found\n");  
      return i;
    }
  }
  return -1;
}

int addZssBackgroudTask(Task task, char* taskLabel, int timeInterval) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO,"addZssBackgroudTask\n");  
  int len;
  int slotId = nextSlot();
  if(slotId>0) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO,"slotId %d \n", slotId);  
    task_list[slotId].id = slotId;
    len = strlen(taskLabel);
    if(len>7) {
      len = 7;
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
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO,"processZssBackgroundHandler\n");  
  for(int i=1; i < N_TASK_TABLE_ENTRIES; i++) {
    Background_Task* t = &task_list[i];
    if(t->id!=0)  /* initialise */ {
      t->countInterval+=ZSS_BACK_SECONDS;
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO,"background task interval %d %d %d\n",i,t->timeInterval,t->countInterval); 
      if(t->countInterval==0) {
        zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO,"running background task %d %s\n",i, t->taskLabel); 
        Task task = t->task;
        task();
        t->countInterval=-t->timeInterval;
      }
    } else {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO,"break task queue %d",i); 
      break;
    }
  }
  return 0;
};


