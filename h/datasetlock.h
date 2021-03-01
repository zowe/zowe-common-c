/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __DATASETLOCK__
#define __DATASETLOCK__ 1

#define HEARTBEAT_DEFAULT 30
#define HEARTBEAT_EXPIRE_TIME 30
/* declare the table that maps datasets to semaphores */
#define N_SEM_TABLE_ENTRIES 100

int heartbeat_expiry_time=HEARTBEAT_EXPIRE_TIME;
int heartbeat_loop_time=HEARTBEAT_DEFAULT;

struct sem_table_type {
  char dsn [44];
  char mem [8];
  char usr [8];
  time_t ltime;
  int  cnt;
  int sem_ID;
};

struct sem_table_type sem_table_entry [N_SEM_TABLE_ENTRIES];

/* declare heartbeat table */
#define N_HBT_TABLE_ENTRIES 100
struct hbt_table_type {
  char usr [8];
  time_t ltime;
  int  cnt;
};

struct hbt_table_type hbt_table_entry [N_HBT_TABLE_ENTRIES];       

void initLockResources();
void heartbeatBackgroundHandler(void* server);
#endif
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/