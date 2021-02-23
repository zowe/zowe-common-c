

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_OS_WINDOWS
#include <time.h>
#endif

#include "semTable.h"
#include <sys/sem.h>
extern struct sem_table_type sem_table_entry [N_SEM_TABLE_ENTRIES];
extern struct hbt_table_type hbt_table_entry [N_HBT_TABLE_ENTRIES];

int addUserToHbt(char user[8]){

  printf("in AddUserToHbt, User: %s\n");
  int rc = srchUserInHbt(user);
  if (rc == -1) {
    rc = findSlotInHbt();
    if (rc >= 0) {
      memcpy(hbt_table_entry[rc].usr, user, 8);
      hbt_table_entry[rc].cnt = 1;
      time(&hbt_table_entry[rc].ltime);
    }
    else {
      printf("Error - no empty entry in HBT.");
    }

  }
  else {
    hbt_table_entry[rc].cnt++;
    time(&hbt_table_entry[rc].ltime);
  }

}

int srchUserInHbt(char user[8]){

  for (int k = 0; k < N_HBT_TABLE_ENTRIES; k++) {
     if (memcmp(hbt_table_entry[k].usr, user,8) == 0) {
       return k;
     }
  }

  return -1;

}

void resetTimeInHbt(char user[8]){

  int ent = srchUserInHbt(user);
  if (ent > -1) {
    time(&hbt_table_entry[ent].ltime);
  }

  return;

}

int findSlotInHbt(){

  for (int k = 0; k < N_HBT_TABLE_ENTRIES; k++) {
    if (hbt_table_entry[k].cnt == -1) {
       return k;
     }
  }

  return -1;

}

int srchUserInSem(char user[8]){
  int rc;
  printf("in srchUserInSem: user is: %s\n", user);
  for (int i = 0; i < N_SEM_TABLE_ENTRIES; i++) {
     if (memcmp(sem_table_entry[i].usr, user,8) == 0) {
       printf("User found in semTable: %d semid: %d\n", i, sem_table_entry[i].sem_ID);
       if (sem_table_entry[i].sem_ID > 0){
          postSemaphore(sem_table_entry[i].sem_ID);
       }
     }
  }

  return 0;

}
