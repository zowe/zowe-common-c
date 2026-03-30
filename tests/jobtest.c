

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  jobtest - CLI test for the job service (SSI 80/79) facility.

  Usage:
    jobtest [options]

  Options:
    -name <pattern>   Filter by job name (supports * wildcard)
    -owner <userid>   Filter by owner
    -id <jobid>       Filter by exact job ID
    -idlow <jobid>    Low end of job ID range
    -idhigh <jobid>   High end of job ID range
    -stc              Select started tasks only
    -tsu              Select TSO users only
    -job              Select batch jobs only
    -phase <code>     Filter by phase (decimal)
    -terse            Terse data only (no sysout info)
    -verbose          Request verbose sysout data (DDNAMEs, step info)
    -dslist           Request full dataset list (incl SYSIN)
    -max <n>          Limit number of jobs returned
    -sysout           Show sysout details for each job
    -datasets         Show per-dataset info (requires -verbose or -dslist)
    -read             Read sysout content (requires -id with -verbose)
    -lines <n>        Max lines per sysout dataset (default 10000)
    -help             Show this help

  Examples:
    jobtest -name JDEVLIN* -sysout
    jobtest -owner JDEVLIN -verbose -datasets
    jobtest -stc -max 20
    jobtest -id JOB12345
    jobtest -id JOB12345 -verbose -read -lines 100
*/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zowetypes.h"
#include "alloc.h"
#include "zos.h"
#include "ssi.h"
#include "jobservice.h"

static const char *phaseToStr(uint8_t phase) {
  switch (phase) {
  /* detailed sub-phases (sttrphaz values 1-26) */
  case JOB_PHASE_NOSUB:   return "NOSUB";
  case JOB_PHASE_FSSCI:   return "FSSCI";
  case JOB_PHASE_PSCBAT:  return "POSTSCAN";
  case JOB_PHASE_PSCDSL:  return "POSTSCAN";
  case JOB_PHASE_FETCH:   return "FETCH";
  case JOB_PHASE_VOLWT:   return "VOLWAIT";
  case JOB_PHASE_SYSSEL:  return "SYSSEL";
  case JOB_PHASE_ALLOC:   return "ALLOC";
  case JOB_PHASE_VOLUAV:  return "VOLUAV";
  case JOB_PHASE_VERIFY:  return "VERIFY";
  case JOB_PHASE_SYSVER:  return "SYSVER";
  case JOB_PHASE_ERROR:   return "ERROR";
  case JOB_PHASE_SELECT:  return "SELECT";
  case JOB_PHASE_ONMAIN:  return "EXEC";
  case JOB_PHASE_BRKDWN:  return "BRKDWN";
  case JOB_PHASE_RESTRT:  return "RESTART";
  case JOB_PHASE_DONE:    return "DONE";
  case JOB_PHASE_OUTPT:   return "OUTPUT";
  case JOB_PHASE_OUTQUE:  return "OUTQUE";
  case JOB_PHASE_OSWAIT:  return "OSWAIT";
  case JOB_PHASE_CMPLT:   return "COMPLETE";
  case JOB_PHASE_DEMSEL:  return "DEMSEL";
  case JOB_PHASE_EFWAIT:  return "EFWAIT";
  case JOB_PHASE_EFBAD:   return "EFBAD";
  /* composite high-level phases (128+) */
  case JOB_PHASE_INPUT:   return "INPUT";
  case JOB_PHASE_WTCONV:  return "WTCONV";
  case JOB_PHASE_CONV:    return "CONV";
  case JOB_PHASE_SETUP:   return "SETUP";
  case JOB_PHASE_SPIN:    return "SPIN";
  case JOB_PHASE_WTBKDN:  return "OUTPUT(W)";
  case JOB_PHASE_WTPURG:  return "WTPURG";
  case JOB_PHASE_PURG:    return "PURGE";
  case JOB_PHASE_RECV:    return "RECV";
  case JOB_PHASE_WTXMIT:  return "WTXMIT";
  case JOB_PHASE_XMIT:    return "XMIT";
  case JOB_PHASE_EXEC:    return "EXEC";
  case JOB_PHASE_POSTEX:  return "POSTEX";
  default:                return "???";
  }
}

static const char *jobTypeToStr(uint8_t jtype) {
  switch (jtype) {
  case JOB_TYPE_STC:  return "STC";
  case JOB_TYPE_TSU:  return "TSU";
  case JOB_TYPE_JOB:  return "JOB";
  case JOB_TYPE_APPC: return "APPC";
  default:            return "???";
  }
}

static const char *compTypeToStr(uint8_t ctype) {
  switch (ctype) {
  case JOB_COMP_UNKNOWN:  return "";
  case JOB_COMP_NORMAL:   return "RC";
  case JOB_COMP_CC:       return "CC";
  case JOB_COMP_JCLERR:   return "JCLERR";
  case JOB_COMP_CANCELED: return "CANCEL";
  case JOB_COMP_ABEND:    return "ABEND";
  case JOB_COMP_CNVABEND: return "CNVABN";
  case JOB_COMP_SECERR:   return "SECERR";
  case JOB_COMP_EOM:      return "EOM";
  case JOB_COMP_CNVERR:   return "CNVERR";
  case JOB_COMP_SYSFAIL:  return "SYSFAIL";
  case JOB_COMP_FLUSHED:  return "FLUSH";
  default:                return "???";
  }
}

static const char *holdToStr(uint8_t hold) {
  switch (hold) {
  case JOB_HOLD_NONE: return "";
  case JOB_HOLD_HELD: return "HELD";
  case JOB_HOLD_DUP:  return "DUPHOLD";
  default:            return "???";
  }
}

static int printSysoutRecord(const char *record, int recordLen,
                             int lineNumber, void *userData) {
  /* Print the record, trimming trailing blanks */
  int len = recordLen;
  while (len > 0 && record[len - 1] == ' ') {
    len--;
  }
  printf("        %6d: %.*s\n", lineNumber, len, record);
  return 0;
}

static void printUsage(void) {
  printf("Usage: jobtest [options]\n");
  printf("  -name <pattern>   Filter by job name\n");
  printf("  -owner <userid>   Filter by owner\n");
  printf("  -id <jobid>       Filter by exact job ID\n");
  printf("  -idlow <jobid>    Low end of job ID range\n");
  printf("  -idhigh <jobid>   High end of job ID range\n");
  printf("  -stc              Select started tasks only\n");
  printf("  -tsu              Select TSO users only\n");
  printf("  -job              Select batch jobs only\n");
  printf("  -phase <code>     Filter by phase (decimal)\n");
  printf("  -terse            Terse data only (no sysout)\n");
  printf("  -verbose          Request verbose sysout data\n");
  printf("  -dslist           Request full dataset list\n");
  printf("  -max <n>          Limit number of jobs\n");
  printf("  -sysout           Show sysout details\n");
  printf("  -datasets         Show per-dataset info\n");
  printf("  -read             Read sysout content (use with -id -verbose)\n");
  printf("  -lines <n>        Max lines per sysout (default %d)\n",
         SYSOUT_READ_DEFAULT_MAX);
  printf("  -help             Show this help\n");
}

static void printJob(JobInfo *job) {
  printf("  %-8s %-8s %-4s %-8s %-9s",
         job->jobName, job->jobId, jobTypeToStr(job->jobType),
         job->owner, phaseToStr(job->phase));

  if (job->compType != JOB_COMP_UNKNOWN) {
    if (job->compType == JOB_COMP_ABEND) {
      printf(" %s(S%03X)", compTypeToStr(job->compType), job->compCode >> 12);
      if (job->compCode & 0xFFF) {
        printf(" U%04d", job->compCode & 0xFFF);
      }
    } else if (job->compType == JOB_COMP_NORMAL || job->compType == JOB_COMP_CC) {
      printf(" %s=%04d", compTypeToStr(job->compType), job->compCode);
    } else {
      printf(" %s", compTypeToStr(job->compType));
    }
  }

  if (job->holdState == JOB_HOLD_HELD || job->holdState == JOB_HOLD_DUP) {
    printf(" %s", holdToStr(job->holdState));
  }

  printf("\n");

  if (job->activeSys[0]) {
    printf("    System: %s  Member: %s\n", job->activeSys, job->activeMember);
  }
  if (job->originNode[0]) {
    printf("    Origin: %s  Exec: %s\n", job->originNode, job->execNode);
  }
}

static void printSysout(JobSysout *so) {
  printf("    SYSOUT class=%-8s dest=%-18s recs=%-8u pages=%-6u",
         so->sysoutClass, so->dest, so->recordCount, so->pageCount);
  if (so->holdOpr) printf(" OPR-HOLD");
  if (so->holdSys) printf(" SYS-HOLD");
  if (so->isSpin)  printf(" SPIN");
  printf("\n");
}

static void printDataset(JobSysoutDataset *ds) {
  printf("      DD=%-8s Step=%-8s Proc=%-8s",
         ds->ddName, ds->stepName, ds->procName);
  if (ds->countsValid) {
    printf(" Lines=%-8u Pages=%-6u", ds->lineCount, ds->pageCount);
  }
  if (ds->lrecl) {
    printf(" LRECL=%u", ds->lrecl);
  }
  if (ds->isSystemDS) printf(" [SYS]");
  if (ds->isSpinDS)   printf(" [SPIN]");
  printf("\n");
  if (ds->dsName[0]) {
    printf("        DSN=%s\n", ds->dsName);
  }
}

int main(int argc, char *argv[]) {
  JobServiceFilter filter;
  memset(&filter, 0, sizeof(filter));
  filter.detailLevel = JOB_DETAIL_SYSOUT; /* default */

  int showSysout = 0;
  int showDatasets = 0;
  int readContent = 0;
  int sapiOnly = 0;
  char sapiJobId[9] = {0};
  int maxLines = SYSOUT_READ_DEFAULT_MAX;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-name") == 0 && i + 1 < argc) {
      filter.flags |= JOB_SELECT_BY_NAME;
      strncpy(filter.jobName, argv[++i], 8);
    } else if (strcmp(argv[i], "-owner") == 0 && i + 1 < argc) {
      filter.flags |= JOB_SELECT_BY_OWNER;
      strncpy(filter.owner, argv[++i], 8);
    } else if (strcmp(argv[i], "-id") == 0 && i + 1 < argc) {
      filter.flags |= JOB_SELECT_BY_JOBID;
      strncpy(filter.jobIdLow, argv[++i], 8);
      strncpy(filter.jobIdHigh, filter.jobIdLow, 8);
    } else if (strcmp(argv[i], "-idlow") == 0 && i + 1 < argc) {
      filter.flags |= JOB_SELECT_BY_JOBID;
      strncpy(filter.jobIdLow, argv[++i], 8);
    } else if (strcmp(argv[i], "-idhigh") == 0 && i + 1 < argc) {
      filter.flags |= JOB_SELECT_BY_JOBID;
      strncpy(filter.jobIdHigh, argv[++i], 8);
    } else if (strcmp(argv[i], "-stc") == 0) {
      filter.flags |= JOB_SELECT_STC;
    } else if (strcmp(argv[i], "-tsu") == 0) {
      filter.flags |= JOB_SELECT_TSU;
    } else if (strcmp(argv[i], "-job") == 0) {
      filter.flags |= JOB_SELECT_JOB;
    } else if (strcmp(argv[i], "-phase") == 0 && i + 1 < argc) {
      filter.flags |= JOB_SELECT_BY_PHASE;
      filter.phase = (uint8_t)atoi(argv[++i]);
    } else if (strcmp(argv[i], "-terse") == 0) {
      filter.detailLevel = JOB_DETAIL_TERSE;
    } else if (strcmp(argv[i], "-verbose") == 0) {
      filter.detailLevel = JOB_DETAIL_VERBOSE;
    } else if (strcmp(argv[i], "-dslist") == 0) {
      filter.detailLevel = JOB_DETAIL_DSLIST;
    } else if (strcmp(argv[i], "-max") == 0 && i + 1 < argc) {
      filter.maxJobs = atoi(argv[++i]);
    } else if (strcmp(argv[i], "-sysout") == 0) {
      showSysout = 1;
    } else if (strcmp(argv[i], "-datasets") == 0) {
      showDatasets = 1;
    } else if (strcmp(argv[i], "-read") == 0) {
      readContent = 1;
    } else if (strcmp(argv[i], "-lines") == 0 && i + 1 < argc) {
      maxLines = atoi(argv[++i]);
      if (maxLines <= 0) maxLines = SYSOUT_READ_DEFAULT_MAX;
    } else if (strcmp(argv[i], "-sapi") == 0 && i + 2 < argc) {
      sapiOnly = 1;
      strncpy(sapiJobId, argv[++i], 8);
      filter.flags |= JOB_SELECT_BY_NAME;
      strncpy(filter.jobName, argv[++i], 8);
    } else if (strcmp(argv[i], "-help") == 0) {
      printUsage();
      return 0;
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      printUsage();
      return 1;
    }
  }

  /* -read requires verbose data for per-dataset tokens */
  if (readContent) {
    if (filter.detailLevel < JOB_DETAIL_VERBOSE) {
      filter.detailLevel = JOB_DETAIL_VERBOSE;
    }
    showSysout = 1;
    showDatasets = 1;
  }

  printf("Job Service Test - SSI 80/79\n");
  printf("============================\n");

  JobService *service = NULL;
  int rc = jobServiceInit(&service);
  if (rc != 0) {
    fprintf(stderr, "jobServiceInit failed, rc=%d\n", rc);
    return 1;
  }

  /* SSI 79-only path: -sapi <jobid> */
  if (sapiOnly) {
    printf("  SAPI-only mode: jobid=%s jobname=%s, max %d lines per dataset\n",
           sapiJobId, filter.jobName, maxLines);
    rc = jobServiceSAPIRead(service, sapiJobId, filter.jobName,
                            maxLines, printSysoutRecord, NULL);
    printf("jobServiceSAPIRead rc=%d\n", rc);
    jobServiceTerm(service);
    return rc;
  }

  if (readContent) {
    printf("  (read mode: max %d lines per dataset)\n", maxLines);
  }

  JobInfo *jobs = NULL;
  int jobCount = 0;
  rc = jobServiceGetJobs(service, &filter, &jobs, &jobCount);

  if (rc != 0) {
    fprintf(stderr, "jobServiceGetJobs failed, rc=%d\n", rc);
    jobServiceTerm(service);
    return 1;
  }

  printf("\nFound %d job(s)\n\n", jobCount);
  printf("  %-8s %-8s %-4s %-8s %-9s %s\n",
         "JOBNAME", "JOBID", "TYPE", "OWNER", "PHASE", "STATUS");
  printf("  %-8s %-8s %-4s %-8s %-9s %s\n",
         "--------", "--------", "----", "--------", "---------", "------");

  JobInfo *job = jobs;
  while (job) {
    printJob(job);

    if (showSysout && job->sysouts) {
      JobSysout *so = job->sysouts;
      while (so) {
        printSysout(so);

        if (showDatasets && so->datasets) {
          JobSysoutDataset *ds = so->datasets;
          while (ds) {
            printDataset(ds);

            if (readContent && ds->tokenValid) {
              int linesRead = 0;
              printf("      --- content (%d line max) ---\n", maxLines);
              int readRC = jobServiceReadSysout(service,
                                               ds->clientToken,
                                               maxLines,
                                               printSysoutRecord,
                                               NULL,
                                               &linesRead);
              if (readRC == 0) {
                printf("      --- %d lines read ---\n", linesRead);
              } else {
                printf("      --- read failed rc=%d (%d lines before error) ---\n",
                       readRC, linesRead);
              }
            } else if (readContent && !ds->tokenValid) {
              printf("      --- no valid token, skipping read ---\n");
            }

            ds = ds->next;
          }
        } else if (readContent && so->tokenValid) {
          /* No per-dataset info; try element-level token */
          int linesRead = 0;
          printf("    --- content (%d line max) ---\n", maxLines);
          int readRC = jobServiceReadSysout(service,
                                           so->clientToken,
                                           maxLines,
                                           printSysoutRecord,
                                           NULL,
                                           &linesRead);
          if (readRC == 0) {
            printf("    --- %d lines read ---\n", linesRead);
          } else {
            printf("    --- read failed rc=%d (%d lines before error) ---\n",
                   readRC, linesRead);
          }
        }

        so = so->next;
      }
    }

    job = job->next;
  }

  printf("\n%d jobs listed.\n", jobCount);

  jobServiceFreeJobs(service, jobs);
  jobServiceTerm(service);

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
