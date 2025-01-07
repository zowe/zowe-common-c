/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "zssitype.h"
#include "zjbm.h"
#include "zwto.h"
#include "zssi31.h"
#include "zjbm31.h"
#include "zstorage.h"
#include "zjsytype.h"
#include "zmetal.h"

#define LOOP_MAX 100

#define SYMBOL_ENTRIES 3
typedef struct
{
  JSYTABLE jsymbolTable;
  JSYENTRY jsymbolEntry[SYMBOL_ENTRIES];
  unsigned char buffer[SYMBOL_ENTRIES * 16];
} JSYMBOLO;

#pragma prolog(ZJBSYMB, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZJBSYMB(const char *symbol, char *value)
{
  int rc = 0;
  WTO_BUF buf = {0};

  JSYMPARM jsym = {0};

  unsigned char *p = NULL;

  memcpy(jsym.jsymeye, "JSYM", sizeof(jsym.jsymeye));
  jsym.jsymlng = jsymsize;
  jsym.jsymrqop = jsymextr;

#define JSYMVRMC 0x0100;

  jsym.jsymvrm = JSYMVRMC;
  jsym.jsymsnmn = 1;
  jsym.jsymsnml = (int)strlen(symbol);
  jsym.jsymsnma = (void *PTR32)symbol;

  JSYMBOLO jsymbolOutput = {0};

  jsym.jsymouta = &jsymbolOutput;
  jsym.jsymouts = sizeof(JSYMBOLO);

  rc = iazsymbl(&jsym);

  if (0 != rc)
  {
    // TODO(Kelosky): read jsymerad
    buf.len = sprintf(buf.msg, "Error: IAZSYMBL RC was: '%d', JSYMRETN was: '%d', JSYMREAS: %d", rc, jsym.jsymretn, jsym.jsymreas);
    wto(&buf);
    return -1;
  }

  p = (unsigned char *)&jsymbolOutput.jsymbolTable; // --> table
  JSYENTRY *jsymbolEntry = (JSYENTRY *)(p + jsymbolOutput.jsymbolTable.jsytent1); // --> first entry

  p = p + jsymbolEntry->jsyevalo;
  memcpy(value, p, jsymbolEntry->jsyevals);

  return 0;
}

// purge a job
#pragma prolog(ZJBMPRG, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZJBMPRG(const char *PTR64 jobid)
{
  int size = 128000;
  int rc = 0;
  int loopControl = 0;

  // return rc
  SSOB *PTR32 ssobp = NULL;
  SSOB ssob = {0};
  SSIB ssib = {0};
  SSJM ssjm = {0};

  WTO_BUF buf = {0};
  char jobid_name[9] = {0};
  strncpy(jobid_name, jobid, sizeof(jobid_name - 1));

  memcpy(ssob.ssobid, "SSOB", sizeof(ssob.ssobid));
  ssob.ssoblen = sizeof(ssob);
  ssob.ssobssib = &ssib;
  ssob.ssobfunc = 85; // Extended status function call - Modify job function call
  ssob.ssobindv = (int)(SSJM * PTR32)(&ssjm);

  memcpy(ssib.ssibid, "SSIB", sizeof(ssib.ssibid));
  ssib.ssiblen = sizeof(ssib);
  memcpy(ssib.ssibssnm, "JES2", sizeof(ssib.ssibssnm));

  memcpy(ssjm.ssjmeye, "SSJMPL  ", sizeof(ssjm.ssjmeye));
  ssjm.ssjmlen = ssjmsize;
  ssjm.ssjmvrm = ssjmvrmc;

  ssjm.ssjmopt1 = ssjm.ssjmopt1 | ssjmpd64; // 64 bit storage
  ssjm.ssjmopt1 = ssjm.ssjmopt1 | ssjmpsyn; // SYNC

  ssjm.ssjmtype = ssjmprg;                  // purge
  ssjm.ssjmpflg = ssjm.ssjmpflg | ssjmpprt; // prehaps required for purge
  ssjm.ssjmsel1 = ssjm.ssjmsel1 | ssjmsoji;
  ssjm.ssjmsel2 = ssjm.ssjmsel2 | ssjmsjob; // batch jobs
  ssjm.ssjmsel2 = ssjm.ssjmsel2 | ssjmsstc; // stcs
  ssjm.ssjmsel2 = ssjm.ssjmsel2 | ssjmstsu; // time sharing users
  memcpy(ssjm.ssjmojbi, jobid_name, sizeof(ssjm.ssjmojbi));

  ssobp = &ssob;
  ssobp = (SSOB * PTR32)((unsigned int)ssobp | 0x80000000);
  rc = iefssreq(&ssobp); // TODO(Kelosky): recovery, abends if jobid doesnt exist for example

  if (0 != rc || 0 != ssob.ssobretn)
  {
    // https://www.ibm.com/docs/en/zos/3.1.0?topic=85-output-parameters
    buf.len = sprintf(buf.msg, "IEFSSREQ rc was: '%d' SSOBRTN was: '%d' SSJMRETN was: '%d', SSJMRET2 was: '%d'", rc, ssob.ssobretn, ssjm.ssjmretn, ssjm.ssjmret2); // STATREAS contains the reason
    wto(&buf);
    return -1;
  }

  buf.len = sprintf(buf.msg, "IEFSSREQ rc was: '%d' SSOBRTN was: '%d'", rc, ssob.ssobretn); // STATREAS contains the reason
  wto(&buf);

  return 0;
}

// list jobs
#pragma prolog(ZJBMLIST, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZJBMLIST(const char owner[8], STATJQTR **PTR64 jobInfo, int *entries)
{
  int size = 128000;
  int rc = 0;
  int loopControl = 0;

  STATJQTR *statjqtrsp = storageGet64(size); // TODO(Kelosky): dynamic storage based on jobs

  // return rc
  SSOB *PTR32 ssobp = NULL;
  SSOB ssob = {0};
  SSIB ssib = {0};
  STAT stat = {0};
  STATJQ *PTR32 statjqp = NULL;
  STATJQHD *PTR32 statjqhdp = NULL;
  STATJQTR *PTR32 statjqtrp = NULL;
  WTO_BUF buf = {0};

  memcpy(ssob.ssobid, "SSOB", sizeof(ssob.ssobid));
  ssob.ssoblen = sizeof(ssob);
  ssob.ssobssib = &ssib;
  ssob.ssobfunc = 80; // Extended status function call
  ssob.ssobindv = (int)(STAT * PTR32)(&stat);

  memcpy(ssib.ssibid, "SSIB", sizeof(ssib.ssibid));
  ssib.ssiblen = sizeof(ssib);
  memcpy(ssib.ssibssnm, "JES2", sizeof(ssib.ssibssnm));

  memcpy(stat.stateye, "STAT", sizeof(stat.stateye));
  stat.statverl = statcvrl;
  stat.statverm = statcvrm;
  stat.statlen = statsize;
  stat.statsel1 = statsown;
  stat.stattype = statters; // STATMEM to free
  memcpy(stat.statownr, owner, sizeof((stat.statownr)));

  ssobp = &ssob;
  ssobp = (SSOB * PTR32)((unsigned int)ssobp | 0x80000000);
  rc = iefssreq(&ssobp); // TODO(Kelosky): recovery

  if (0 != rc || 0 != ssob.ssobretn)
  {
    buf.len = sprintf(buf.msg, "IEFSSREQ rc was: '%d' SSOBRTN was: '%d', STATREAS was: '%d', STATREA2 was: '%d'", rc, ssob.ssobretn, stat.statreas, stat.statrea2); // STATREAS contains the reason
    wto(&buf);
    // TODO(Kelosky): do we need to call w/statmem here?
    storageFree64(statjqtrsp);
    return -1;
  }

  statjqp = (STATJQ * PTR32) stat.statjobf;
  *jobInfo = statjqtrsp;
  while (statjqp)
  {
    if (loopControl > LOOP_MAX)
      break; // TODO(Kelosky): handle as a condition

    *entries = *entries + 1;

    statjqhdp = (STATJQHD * PTR32)((unsigned char *PTR32)statjqp + statjqp->stjqohdr);
    statjqtrp = (STATJQTR * PTR32)((unsigned char *PTR32)statjqhdp + sizeof(STATJQHD));

    memcpy(statjqtrsp, statjqtrp, sizeof(STATJQTR));
    statjqtrsp++;

    statjqp = (STATJQ * PTR32) statjqp->stjqnext;

    loopControl++;
  }

  stat.stattype = statmem; // free storage
  rc = iefssreq(&ssobp);   // TODO(Kelosky): recovery

  buf.len = sprintf(buf.msg, "IEFSSREQ FREE was: '%d' SSOBRTN was: '%d'", rc, ssob.ssobretn); // STATREAS contains the reason
  wto(&buf);

  return 0;
}

// list data sets for a job
#pragma prolog(ZJBMLSDS, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZJBMLSDS(const char *PTR64 jobid, STATSEVB **PTR64 sysoutInfo, int *entries, unsigned char token[8])
{
  int size = 128000;
  int rc = 0;
  int loopControl = 0;

  STATSEVB *statsetrsp = storageGet64(size); // TODO(Kelosky): dynamic storage based on jobs

  // return rc
  SSOB *PTR32 ssobp = NULL;
  SSOB ssob = {0};
  SSIB ssib = {0};
  STAT stat = {0};

  STATJQ *PTR32 statjqp = NULL;
  STATJQHD *PTR32 statjqhdp = NULL;
  STATJQTR *PTR32 statjqtrp = NULL;

  STATVO *PTR32 statvop = NULL;
  STATSVHD *PTR32 statsvhdp = NULL;
  STATSEVB *PTR32 statsevbp = NULL;

  WTO_BUF buf = {0};
  char jobid_name[9] = "         ";
  strncpy(jobid_name, jobid, sizeof(jobid_name - 1));

  memcpy(ssob.ssobid, "SSOB", sizeof(ssob.ssobid));
  ssob.ssoblen = sizeof(ssob);
  ssob.ssobssib = &ssib;
  ssob.ssobfunc = 80; // Extended status function call
  ssob.ssobindv = (int)(STAT * PTR32)(&stat);

  memcpy(ssib.ssibid, "SSIB", sizeof(ssib.ssibid));
  ssib.ssiblen = sizeof(ssib);
  memcpy(ssib.ssibssnm, "JES2", sizeof(ssib.ssibssnm));

  memcpy(stat.stateye, "STAT", sizeof(stat.stateye));
  stat.statverl = statcvrl;
  stat.statverm = statcvrm;
  stat.statlen = statsize;
  stat.statsel1 = statsjbi;
  stat.stattype = statoutv; // STATMEM to free

  memcpy(stat.statjbil, jobid_name, sizeof((stat.statjbil)));
  memcpy(stat.statjbih, jobid_name, sizeof((stat.statjbih)));

  ssobp = &ssob;
  ssobp = (SSOB * PTR32)((unsigned int)ssobp | 0x80000000);
  rc = iefssreq(&ssobp); // TODO(Kelosky): recovery, abends if jobid doesnt exist for example

  if (0 != rc || 0 != ssob.ssobretn)
  {
    buf.len = sprintf(buf.msg, "IEFSSREQ rc was: '%d' SSOBRTN was: '%d', STATREAS was: '%d', STATREA2 was: '%d'", rc, ssob.ssobretn, stat.statreas, stat.statrea2); // STATREAS contains the reason
    wto(&buf);
    return -1;
  }

  statjqp = (STATJQ * PTR32) stat.statjobf;
  statvop = (STATVO * PTR32) statjqp->stjqsvrb;

  while (statjqp)
  {
    if (loopControl > LOOP_MAX)
      break; // TODO(Kelosky): handle as a condition

    statjqhdp = (STATJQHD * PTR32)((unsigned char *PTR32)statjqp + statjqp->stjqohdr);
    statjqtrp = (STATJQTR * PTR32)((unsigned char *PTR32)statjqhdp + sizeof(STATJQHD));

    *sysoutInfo = statsetrsp;
    while (statvop)
    {
      if (loopControl > LOOP_MAX)
        break; // TODO(Kelosky): handle as a condition

      *entries = *entries + 1;

      statsvhdp = (STATSVHD * PTR32)((unsigned char *PTR32)statvop + statvop->stvoohdr);
      statsevbp = (STATSEVB * PTR32)((unsigned char *PTR32)statsvhdp + sizeof(STATSVHD));

      STATSEO2 *PTR32 statseo2 = (STATSEO2 * PTR32)((unsigned char *PTR32)statsevbp + statsevbp->stvslen);

      memcpy(statsetrsp, statsevbp, sizeof(STATSEVB));
      statsetrsp++;

      statvop = (STATVO * PTR32) statvop->stvojnxt;
    }

    statjqp = (STATJQ * PTR32) statjqp->stjqnext;

    loopControl++;
  }

  stat.stattype = statmem; // free storage
  rc = iefssreq(&ssobp);   // TODO(Kelosky): recovery

  buf.len = sprintf(buf.msg, "IEFSSREQ FREE was: '%d' SSOBRTN was: '%d'", rc, ssob.ssobretn); // STATREAS contains the reason
  wto(&buf);

  return 0;
}
