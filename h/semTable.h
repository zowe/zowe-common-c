  /* declare the table that maps datasets to semaphores */
  struct sem_table_type {
    char dsn [44];
    char mem [8];
    int sem_ID;
  } ;
  #define N_SEM_TABLE_ENTRIES 100