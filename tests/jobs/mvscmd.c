

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  mvscmd.c — Issue MVS system commands via __console2() and collect responses.

  __console2() is an LE C runtime function declared in <sys/console.h>.
  It wraps the EMCS console facility.  No APF authorization is required.

  Flow:
    1. Activate an EMCS console
    2. Issue a command
    3. Poll for async responses (cart-matched messages)
    4. Deactivate when done

  The __console2() API uses a _CONSOLE_MSG structure for both
  commands and responses.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/console.h>
#include <errno.h>

#include "mvscmd.h"

struct MvsConsole_tag {
  int  active;
  char consoleName[9];
};

/*
  Generate a console name from the PID.
*/
static void genConsoleName(char *buf) {
  pid_t pid = getpid();
  snprintf(buf, 9, "ZC%06d", (int)(pid % 1000000));
}

int mvsCmdOpen(MvsConsole **console, const char *consoleName) {
  MvsConsole *con = (MvsConsole *)malloc(sizeof(MvsConsole));
  if (!con) return -1;
  memset(con, 0, sizeof(MvsConsole));

  if (consoleName && consoleName[0]) {
    strncpy(con->consoleName, consoleName, 8);
    con->consoleName[8] = '\0';
  } else {
    genConsoleName(con->consoleName);
  }

  /* Activate the EMCS console */
  struct __cons_msg2 cmsg;
  memset(&cmsg, 0, sizeof(cmsg));
  cmsg.__cm2_format = __CONSOLE_FORMAT_3;
  cmsg.__cm2_msg_length = 0;

  int cmd = _CONSOLE_ACTIVATE;

  /* Set console name */
  memset(cmsg.__cm2_consname, ' ', sizeof(cmsg.__cm2_consname));
  memcpy(cmsg.__cm2_consname, con->consoleName, strlen(con->consoleName));

  int rc = __console2(&cmsg, NULL, &cmd);
  if (rc != 0) {
    free(con);
    return rc;
  }

  con->active = 1;
  *console = con;
  return 0;
}

int mvsCmdIssue(MvsConsole *console, const char *command,
                int timeoutMs,
                MvsCmdResponse **responses, int *responseCount) {
  *responses = NULL;
  *responseCount = 0;

  if (!console || !console->active) return -1;

  /* Issue the command */
  struct __cons_msg2 cmsg;
  memset(&cmsg, 0, sizeof(cmsg));
  cmsg.__cm2_format = __CONSOLE_FORMAT_3;

  /* Copy command text */
  int cmdLen = strlen(command);
  if (cmdLen > (int)sizeof(cmsg.__cm2_msg) - 1) {
    cmdLen = (int)sizeof(cmsg.__cm2_msg) - 1;
  }
  memcpy(cmsg.__cm2_msg, command, cmdLen);
  cmsg.__cm2_msg_length = cmdLen;

  /* Set console name */
  memset(cmsg.__cm2_consname, ' ', sizeof(cmsg.__cm2_consname));
  memcpy(cmsg.__cm2_consname, console->consoleName,
         strlen(console->consoleName));

  /* Generate a CART for response matching */
  memset(cmsg.__cm2_cart, ' ', sizeof(cmsg.__cm2_cart));
  memcpy(cmsg.__cm2_cart, "ZCMD", 4);

  int cmd = _CONSOLE_ISSUE;
  int rc = __console2(&cmsg, NULL, &cmd);
  if (rc != 0) return rc;

  /* Collect responses by polling with GETMSG */
  MvsCmdResponse *head = NULL;
  MvsCmdResponse *tail = NULL;
  int count = 0;

  int elapsed = 0;
  int pollInterval = 100;  /* 100ms between polls */

  while (elapsed < timeoutMs) {
    struct __cons_msg2 rmsg;
    memset(&rmsg, 0, sizeof(rmsg));
    rmsg.__cm2_format = __CONSOLE_FORMAT_3;

    /* Set console name for GETMSG */
    memset(rmsg.__cm2_consname, ' ', sizeof(rmsg.__cm2_consname));
    memcpy(rmsg.__cm2_consname, console->consoleName,
           strlen(console->consoleName));

    /* Match on CART */
    memset(rmsg.__cm2_cart, ' ', sizeof(rmsg.__cm2_cart));
    memcpy(rmsg.__cm2_cart, "ZCMD", 4);

    cmd = _CONSOLE_GET;
    rc = __console2(&rmsg, NULL, &cmd);

    if (rc == 0 && rmsg.__cm2_msg_length > 0) {
      /* Got a response message */
      MvsCmdResponse *resp = (MvsCmdResponse *)malloc(sizeof(MvsCmdResponse));
      if (resp) {
        memset(resp, 0, sizeof(MvsCmdResponse));
        int len = rmsg.__cm2_msg_length;
        if (len > 255) len = 255;
        memcpy(resp->text, rmsg.__cm2_msg, len);
        resp->text[len] = '\0';

        /* Trim trailing spaces */
        for (int i = len - 1; i >= 0 && resp->text[i] == ' '; i--) {
          resp->text[i] = '\0';
        }

        resp->routeCode = rmsg.__cm2_routcde[0];

        if (!head) {
          head = tail = resp;
        } else {
          tail->next = resp;
          tail = resp;
        }
        count++;
      }
      /* Keep polling — more messages might follow */
      continue;
    }

    /* No message available — wait and retry */
    usleep(pollInterval * 1000);
    elapsed += pollInterval;

    /* After getting at least one message, wait less before giving up */
    if (count > 0) {
      pollInterval = 200;
      /* If we've gone 500ms without a new message after getting some,
         assume we have them all */
      if (elapsed > 500 && count > 0) break;
    }
  }

  *responses = head;
  *responseCount = count;
  return 0;
}

void mvsCmdFreeResponses(MvsCmdResponse *responses) {
  while (responses) {
    MvsCmdResponse *next = responses->next;
    free(responses);
    responses = next;
  }
}

void mvsCmdClose(MvsConsole *console) {
  if (!console) return;

  if (console->active) {
    struct __cons_msg2 cmsg;
    memset(&cmsg, 0, sizeof(cmsg));
    cmsg.__cm2_format = __CONSOLE_FORMAT_3;
    cmsg.__cm2_msg_length = 0;

    memset(cmsg.__cm2_consname, ' ', sizeof(cmsg.__cm2_consname));
    memcpy(cmsg.__cm2_consname, console->consoleName,
           strlen(console->consoleName));

    int cmd = _CONSOLE_DEACTIVATE;
    __console2(&cmsg, NULL, &cmd);
    console->active = 0;
  }

  free(console);
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
