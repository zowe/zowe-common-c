

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_SIGNAL_H
#define __ZOWE_SIGNAL_H

#include "zowetypes.h"

#ifdef __ZOWE_OS_ZOS
#define SIGHUP       1
#define SIGINT       2
#define SIGABRT      3
#define SIGILL       4
#define SIGPOLL      5
#define SIGURG       6
#define SIGFPE       8
#define SIGKILL      9
#define SIGBUS      10
#define SIGSEGV     11
#define SIGSYS      12
#define SIGPIPE     13
#define SIGALRM     14
#define SIGTERM     15
#define SIGUSR1     16
#define SIGUSR2     17
#define SIGABND     18
#define SIGQUIT     24
#define SIGTRAP     26
#define SIGXCPU     29
#define SIGXFSZ     30
#define SIGVTALRM   31
#define SIGPROF     32
#define SIGDANGER   33
#define SIGNULL      0
#define SIGCHLD     20
#define SIGIO       23
#define SIGIOER     27
#define SIGWINCH    28
#define SIGTRACE    37
#define SIGDUMP     39
#define SIGSTOP      7
#define SIGTTIN     21
#define SIGTTOU     22
#define SIGTSTP     25
#define SIGTHSTOP   34
#define SIGCONT     19
#define SIGTHCONT   35

#define SA_FLAGS_DFT      0000000
#define SA_NOCLDSTOP      8000000
#define SA_OLD_STYLE      4000000
#define SA_ONSTACK        2000000
#define SA_RESETHAND      1000000
#define SA_RESTART        0800000
#define SA_SIGINFO        0400000
#define SA_NOCLDWAIT      0200000
#define SA_NODEFER        0100000
#define SA_IGNORE         0000000

#define SIG_DFL (SignalHandler) 0
#define SIG_IGN (SignalHandler) 1

#elif defined(__ZOWE_OS_LINUX)
#include <signal.h>
#else
#error Unknown OS
#endif

typedef void (*SignalHandler)(int);
int signalControl(int signum, SignalHandler handler, int *returnCode, int *reasonCode);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

