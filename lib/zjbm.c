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
#include "zjbtype.h"

#define SYMBOL_ENTRIES 3
typedef struct
{
  JSYTABLE jsymbolTable;
  JSYENTRY jsymbolEntry[SYMBOL_ENTRIES];
  unsigned char buffer[SYMBOL_ENTRIES * 16];
} JSYMBOLO;

static void init_ssib(SSIB *ssib)
{
  memcpy(ssib->ssibid, "SSIB", sizeof(ssib->ssibid));
  ssib->ssiblen = sizeof(SSIB);
  memcpy(ssib->ssibssnm, "JES2", sizeof(ssib->ssibssnm));
}

static void init_ssob(SSOB *PTR32 ssob, SSIB *PTR32 ssib, void *PTR32 function_depenent_area, int function)
{
  memcpy(ssob->ssobid, "SSOB", sizeof(ssob->ssobid));
  ssob->ssoblen = sizeof(SSOB);
  ssob->ssobssib = ssib;
  ssob->ssobindv = (int)function_depenent_area;
  ssob->ssobfunc = function;
}

static void init_stat(STAT *stat)
{
  memcpy(stat->stateye, "STAT", sizeof(stat->stateye));
  stat->statverl = statcvrl;
  stat->statverm = statcvrm;
  stat->statlen = statsize;
}

#pragma prolog(ZJBSYMB, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZJBSYMB(ZJB *zjb, const char *symbol, char *value)
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
    // TODO(Kelosky): read jsymerad for errors
    strcpy(zjb->service_name, "iazsymbl");
    zjb->e_msg_len = sprintf(zjb->e_msg, "Error: IAZSYMBL RC was: '%d', JSYMRETN was: '%d', JSYMREAS: %d", rc, jsym.jsymretn, jsym.jsymreas);
    zjb->detail_rc = ZJB_RTNCD_SERVICE_FAILURE;
    return ZJB_RTNCD_FAILURE;
  }

  p = (unsigned char *)&jsymbolOutput.jsymbolTable; // --> table
  JSYENTRY *jsymbolEntry = (JSYENTRY *)(p + jsymbolOutput.jsymbolTable.jsytent1); // --> first entry

  p = p + jsymbolEntry->jsyevalo;
  memcpy(value, p, jsymbolEntry->jsyevals);

  return ZJB_RTNCD_SUCCESS;
}

// purge a job
#pragma prolog(ZJBMPRG, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZJBMPRG(ZJB *zjb)
{
  int rc = 0;
  int loopControl = 0;

  // return rc
  SSOB *PTR32 ssobp = NULL;
  SSOB ssob = {0};
  SSIB ssib = {0};
  SSJM ssjm = {0};
  SSJF *ssjfp = NULL;

  // https://www.ibm.com/docs/en/zos/3.1.0?topic=sfcd-modify-job-function-call-ssi-function-code-85
  init_ssob(&ssob, &ssib, &ssjm, 85);
  init_ssib(&ssib);

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
  memcpy(ssjm.ssjmojbi, zjb->jobid, sizeof(ssjm.ssjmojbi));

  ssobp = &ssob;
  ssobp = (SSOB * PTR32)((unsigned int)ssobp | 0x80000000);
  rc = iefssreq(&ssobp); // TODO(Kelosky): recovery, abends if jobid doesnt exist for example

  if (0 != rc || 0 != ssob.ssobretn)
  {
    strcpy(zjb->service_name, "IEFSSREQ");
    zjb->service_rc = ssob.ssobretn;
    zjb->service_rsn = ssjm.ssjmretn;
    zjb->service_rsn_secondary = ssjm.ssjmret2;
    zjb->e_msg_len = sprintf(zjb->e_msg, "IEFSSREQ rc was: '%d' SSOBRTN was: '%d', SSJMRETN was: '%d', SSJMRET2 was: '%d'", rc, ssob.ssobretn, ssjm.ssjmretn, ssjm.ssjmret2);
    return ZJB_RTNCD_FAILURE;
  }

  ssjfp = (SSJF *) ssjm.ssjmsjf8; // NOTE(Kelosky): in the future we can return a list of SSJFs, for now, if non-returned, the job was not found

  if (0 == ssjm.ssjmnsjf)
  {
    zjb->e_msg_len = sprintf(zjb->e_msg, "No jobs found matching '%.8s'", zjb->jobid);
    zjb->detail_rc = ZJB_RTNCD_JOB_NOT_FOUND;
    return ZJB_RTNCD_FAILURE;
  }

  return ZJB_RTNCD_SUCCESS;
}

// list jobs
#pragma prolog(ZJBMLIST, "&CCN_MAIN SETB 1 \n MYPROLOG")
int ZJBMLIST(ZJB *zjb, STATJQTR **PTR64 jobInfo, int *entries)
{
  int rc = 0;
  int loopControl = 0;

  STATJQTR *statjqtrsp = storageGet64(zjb->buffer_size); // TODO(Kelosky): dynamic storage based on jobs

  SSOB *PTR32 ssobp = NULL;
  SSOB ssob = {0};
  SSIB ssib = {0};
  STAT stat = {0};
  STATJQ *PTR32 statjqp = NULL;
  STATJQHD *PTR32 statjqhdp = NULL;
  STATJQTR *PTR32 statjqtrp = NULL;
  WTO_BUF buf = {0};

  // https://www.ibm.com/docs/en/zos/3.1.0?topic=sfcd-extended-status-function-call-ssi-function-code-80
  init_ssob(&ssob, &ssib, &stat, 80);
  init_ssib(&ssib);
  init_stat(&stat);
  stat.statsel1 = statsown;
  stat.stattype = statters;
  memcpy(stat.statownr, zjb->owner_name, sizeof((stat.statownr)));

  ssobp = &ssob;
  ssobp = (SSOB * PTR32)((unsigned int)ssobp | 0x80000000);
  rc = iefssreq(&ssobp); // TODO(Kelosky): recovery

  if (0 != rc || 0 != ssob.ssobretn)
  {
    strcpy(zjb->service_name, "IEFSSREQ");
    zjb->service_rc = ssob.ssobretn;
    zjb->service_rsn = stat.statreas;
    zjb->service_rsn_secondary = stat.statrea2;
    zjb->e_msg_len = sprintf(zjb->e_msg, "IEFSSREQ rc was: '%d' SSOBRTN was: '%d', STATREAS was: '%d', STATREA2 was: '%d'", rc, ssob.ssobretn, stat.statreas, stat.statrea2); // STATREAS contains the reason
    storageFree64(statjqtrsp);
    return ZJB_RTNCD_FAILURE;
  }

  statjqp = (STATJQ * PTR32) stat.statjobf;
  *jobInfo = statjqtrsp;

  int total_size = 0;

  while (statjqp)
  {
    if (loopControl > zjb->jobs_max)
    {
      zjb->detail_rc = ZJB_RTNCD_MAX_JOBS_REACHED;
      break;
    }

    total_size += (int)sizeof(STATJQTR);

    if (total_size <= zjb->buffer_size)
    {
      *entries = *entries + 1;

      statjqhdp = (STATJQHD * PTR32)((unsigned char *PTR32)statjqp + statjqp->stjqohdr);
      statjqtrp = (STATJQTR * PTR32)((unsigned char *PTR32)statjqhdp + sizeof(STATJQHD));

      memcpy(statjqtrsp, statjqtrp, sizeof(STATJQTR));
      statjqtrsp++;
    }
    else
    {
      zjb->detail_rc = ZJB_RTNCD_INSUFFICIENT_BUFFER;
    }

    statjqp = (STATJQ * PTR32) statjqp->stjqnext;

    loopControl++;
  }

  zjb->buffer_size_needed = total_size;

  stat.stattype = statmem; // free storage
  rc = iefssreq(&ssobp); // TODO(Kelosky): recovery

  return ZJB_RTNCD_SUCCESS;
}

#define LOOP_MAX 100

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

  // https://www.ibm.com/docs/en/zos/3.1.0?topic=sfcd-extended-status-function-call-ssi-function-code-80
  init_ssib(&ssib);
  init_ssob(&ssob, &ssib, &stat, 80);
  init_stat(&stat);

  stat.statsel1 = statsjbi;
  stat.stattype = statoutv;

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
