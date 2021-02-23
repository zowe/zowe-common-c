  /* declare the table that maps datasets to semaphores */
  struct sem_table_type {
    char dsn [44];
    char mem [8];
    char usr [8];
    time_t ltime;
    int  cnt;
    int sem_ID;
  } ;
  #define N_SEM_TABLE_ENTRIES 100
  /* declare heartbeat table */
  struct hbt_table_type {
    char usr [8];
    time_t ltime;
    int  cnt;
  } ;
  #define N_HBT_TABLE_ENTRIES 100
