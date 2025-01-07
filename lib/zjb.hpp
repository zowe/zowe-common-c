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

/**
 * @brief Return a list of jobs from an input or default owner
 *
 * @param zjb job returned attributes and error information
 * @param owner_name owner name of the job to query, defaults to currnet user if == "", may use wild cards, i.e. "IBMUS*"
 * @param jobs populated list of job information array
 * @return int 0 for success; non zero otherwise
 */
int zjb_list_by_owner(ZJB *zjb, std::string owner_name, std::vector<ZJob> &jobs);

int zjb_list_dds_by_jobid(ZJB *zjb, std::string, std::vector<ZJobDD> &jobDDs);

int zjb_read_jobs_output_by_jobid_and_key(ZJB *zjb, std::string, int, std::string &);

int zjb_get_job_dsn_by_jobid_and_key(ZJB *zjb, std::string, int, std::string &);

int zjb_read_job_content_by_dsn(ZJB *zjb, std::string, std::string &);

/**
 * @brief Submit a job from a given input data set
 *
 * @param zjb job returned attributes and error information
 * @param dsn data set name containing JCL to submit, i.e. "IBMUSER.JCL(IEFBR14)""
 * @param jobid jobid retuned after successfully submitting JCL
 * @return int 0 for success; non zero otherwise
 */
int zjb_submit(ZJB *zjb, std::string dsn, std::string &jobid);

int zjb_delete_by_jobid(ZJB *zjb, std::string jobid);

#endif