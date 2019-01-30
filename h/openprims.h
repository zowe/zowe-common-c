

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_OPENPRIMS_
#define __ZOWE_OPENPRIMS_ 1

/* OPENPRIMS is a set of Windows-and-POSIX-spanning primitives for

   Synchronizing access to an object
   Sleeping on an event and getting woken up by some thread/task


   Windows SelectEx strategy

   NeedEvents EventObject(s)
   Soc
   

*/



#ifdef __ZOWE_OS_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* #include <winsock2.h> */

typedef HANDLE Mutex;
typedef HANDLE Thread;

#define mutexLock(x) WaitForSingleObject(x,INFINITE)
#define mutexUnlock(x) ReleaseMutex(x)
#define mutexCreate(x) (x) = CreateMutexEx(NULL,NULL,0x00000000,MUTEX_ALL_ACCESS);

typedef struct OSThread_tag {
  HANDLE windowsThread;
  int    threadID;
} OSThread;

#define threadCreate(osThreadPtr,mainFunction,data) \
  (((osThreadPtr->windowsThread = CreateThread(NULL,0,mainFunction,data,0,&(osThreadPtr->threadID))) != NULL ) ? 0 : GetLastError())
/* TODO implement detach for Windows */
#define threadDetach(osThread) (0)


#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_ISERIES)

#include <sys/types.h>
#include <pthread.h>

typedef struct OSThread_tag {
  pthread_t threadID;
} OSThread;

typedef pthread_mutex_t Mutex;

#define mutexLock(x) pthread_mutex_lock(&(x))
#define mutexUnlock(x) pthread_mutex_unlock(&(x))
#define mutexCreate(x) pthread_mutex_init(&(x), NULL);

#define threadCreate(osThread,mainFunction,data) pthread_create(&(osThread->threadID), NULL, mainFunction, data)
#define threadDetach(osThread) pthread_detach(&((osThread)->threadID))

#elif defined(__ZOWE_OS_ZOS) && !defined(METTLE)  /* the mainframe POSIX stuff is weird */

#include <sys/types.h>
#include <pthread.h>

typedef struct OSThread_tag {
  pthread_t threadID;
} OSThread;

typedef pthread_mutex_t Mutex;

#define mutexLock(x) pthread_mutex_lock(&(x))
#define mutexUnlock(x) pthread_mutex_unlock(&(x))
#define mutexCreate(x) pthread_mutex_init(&(x), NULL);

#define threadCreate(osThread,mainFunction,data) pthread_create(&(osThread->threadID), NULL, mainFunction, data)
#define threadDetach(osThread) pthread_detach(&((osThread)->threadID))

#elif defined(__ZOWE_OS_ZOS) && defined(METTLE)
/* needed for taskAnchor struct */
typedef int OSThread;

#endif

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

