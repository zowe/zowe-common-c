/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZJB_HPP
#define ZJB_HPP

#include <iostream>
#include <vector>
#include <string>
#include "zjbtype.h"

struct ZJob
{
  std::string jobname;
  std::string jobid;
  std::string owner;
  std::string status;
  std::string retcode;
};

struct ZJobDD
{
  std::string jobid;
  std::string ddn;
  std::string dsn;
  std::string stepname;
  std::string procstep;
  int key;
};

int zjb_list_by_owner(ZJB *, std::string, std::vector<ZJob> &jobs);

int zjb_list_dds_by_jobid(std::string, std::vector<ZJobDD> &jobDDs);
int zjb_read_jobs_output_by_jobid_and_key(std::string, int, std::string &);
int zjb_get_job_dsn_by_jobid_and_key(std::string, int, std::string &);
int zjb_read_job_content_by_dsn(std::string, std::string &);
int zjb_submit(std::string, std::string &);
int zjb_delete_by_jobid(std::string);

#endif