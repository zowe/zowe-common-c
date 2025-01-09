/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <iostream>
#include <vector>
#include <stdlib.h>
#include <string>
#include "zcn.hpp"
#include "zut.hpp"
#include "zcli.hpp"
#include "zjb.hpp"
#include "zds.hpp"

using namespace std;

int handle_job_list(ZCLIResult);
int handle_job_list_files(ZCLIResult);
int handle_job_view_file(ZCLIResult);
int handle_job_submit(ZCLIResult);
int handle_job_delete(ZCLIResult);

int handle_console_issue(ZCLIResult);

int handle_data_set_view_dsn(ZCLIResult);

int handle_test(ZCLIResult);

int main(int argc, char *argv[])
{
  // CLI
  ZCLI zcli(argv[PROCESS_NAME_ARG]);

  // test group
  ZCLIGroup test_group("test");
  test_group.set_description("test other operations");

  // test verbs
  ZCLIVerb test_command("command");
  test_command.set_description("test command");
  test_command.set_zcli_verb_handler(handle_test);
  test_group.get_verbs().push_back(test_command);

  // data set group
  ZCLIGroup data_set_group("data-set");
  data_set_group.set_description("z/OS data set operations");

  // data set verbs
  ZCLIVerb data_set_view("view");
  data_set_view.set_description("view data set");
  data_set_view.set_zcli_verb_handler(handle_data_set_view_dsn);
  ZCLIPositional data_set_dsn("dsn");
  data_set_dsn.set_description("dsn name, e.g. SYS1.MACLIB(ABEND)");
  data_set_dsn.set_required(true);
  data_set_view.get_positionals().push_back(data_set_dsn);
  data_set_group.get_verbs().push_back(data_set_view);

  // jobs group
  ZCLIGroup job_group("job");
  job_group.set_description("z/OS job operations");

  // jobs verbs
  ZCLIVerb job_list("list");
  job_list.set_description("list jobs");
  job_list.set_zcli_verb_handler(handle_job_list);
  ZCLIOption job_owner("owner");
  job_owner.set_description("filter by owner");
  job_list.get_options().push_back(job_owner);
  job_group.get_verbs().push_back(job_list);

  ZCLIVerb job_list_files("list-files");
  job_list_files.set_description("list spool files");
  job_list_files.set_zcli_verb_handler(handle_job_list_files);
  ZCLIPositional job_jobid("jobid");
  job_jobid.set_required(true);
  job_jobid.set_description("valid jobid");
  job_list_files.get_positionals().push_back(job_jobid);
  job_group.get_verbs().push_back(job_list_files);

  ZCLIVerb job_view_file("view-file");
  job_view_file.set_description("view job file output");
  job_view_file.set_zcli_verb_handler(handle_job_view_file);
  job_view_file.get_positionals().push_back(job_jobid);

  ZCLIPositional job_dsn_key("key");
  job_dsn_key.set_required(true);
  job_dsn_key.set_description("valid job dsn key via 'job list-files'");
  job_view_file.get_positionals().push_back(job_dsn_key);
  job_group.get_verbs().push_back(job_view_file);

  ZCLIVerb job_submit("submit");
  job_submit.set_description("submit a job");
  job_submit.set_zcli_verb_handler(handle_job_submit);
  ZCLIPositional job_dsn("dsn");
  job_dsn.set_required(true);
  job_dsn.set_description("dsn containing JCL");
  job_submit.get_positionals().push_back(job_dsn);
  job_group.get_verbs().push_back(job_submit);

  ZCLIVerb job_delete("delete");
  job_delete.set_description("delete a job");
  job_delete.set_zcli_verb_handler(handle_job_delete);
  job_delete.get_positionals().push_back(job_jobid);
  job_group.get_verbs().push_back(job_delete);

  // console group
  ZCLIGroup console_group("console");
  console_group.set_description("z/OS console operations");

  // console verbs
  ZCLIVerb console_issue("issue");
  console_issue.set_description("issue a console command");
  console_issue.set_zcli_verb_handler(handle_console_issue);
  ZCLIOption console_name("console-name");
  console_name.set_required(true);
  console_name.set_description("extended console name");
  console_issue.get_options().push_back(console_name);
  ZCLIPositional console_command("command");
  console_command.set_required(true);
  console_command.set_description("command to run, e.g. 'D IPLINFO'");
  console_issue.get_positionals().push_back(console_command);
  console_group.get_verbs().push_back(console_issue);

  // add all groups to the CLI
  zcli.get_groups().push_back(test_group);
  zcli.get_groups().push_back(data_set_group);
  zcli.get_groups().push_back(console_group);
  zcli.get_groups().push_back(job_group);

  // parse
  return zcli.parse(argc, argv);
}

int handle_job_list(ZCLIResult result)
{
  int rc = 0;
  ZJB zjb = {0};
  string owner_name(result.get_option("--owner").get_value());

  vector<ZJob> jobs;
  rc = zjb_list_by_owner(&zjb, owner_name, jobs);

  if (0 != rc)
  {
    cout << "Error: could not list jobs for: '" << owner_name << "' rc: '" << rc << "'" << endl;
    cout << "  Details: " << zjb.diag.e_msg << endl;
    return -1;
  }

  for (vector<ZJob>::iterator it = jobs.begin(); it != jobs.end(); it++)
  {
      cout << it->jobid << " " << left << setw(10) << it->retcode << " " << it->jobname << " " << it->status << endl;
  }

  return 0;
}

int handle_job_list_files(ZCLIResult result)
{
  int rc = 0;
  ZJB zjb = {0};
  string jobid(result.get_positional("jobid").get_value());

  vector<ZJobDD> job_dds;
  rc = zjb_list_dds_by_jobid(&zjb, jobid, job_dds);

  if (0 != rc)
  {
    cout << "Error: could not list jobs for: '" << jobid << "' rc: '" << rc << "'" << endl;
    cout << "  Details: " << zjb.diag.e_msg << endl;
    return -1;
  }

  for (vector<ZJobDD>::iterator it = job_dds.begin(); it != job_dds.end(); ++it)
  {
      cout << left << setw(9) << it->ddn << " " << it->dsn << " " << setw(4) << it->key << " " << it->stepname << " " << it->procstep << endl;
  }

  return 0;
}

int handle_job_view_file(ZCLIResult result)
{
  int rc = 0;
  ZJB zjb = {0};
  string jobid(result.get_positional("jobid").get_value());
  string key(result.get_positional("key").get_value());

  string resp;
  rc = zjb_read_jobs_output_by_jobid_and_key(&zjb, jobid, atoi(key.c_str()), resp);

  if (0 != rc)
  {
    cout << "Error: could not view job file for: '" << jobid << "' with key '" << key << "' rc: '" << rc << "'" << endl;
    cout << "  Details: " << zjb.diag.e_msg << endl;
    return -1;
  }

  cout << resp;

  return 0;
}

int handle_job_submit(ZCLIResult result)
{
  int rc = 0;
  ZJB zjb = {0};
  string dsn(result.get_positional("dsn").get_value());

  vector<ZJob> jobs;
  string jobid;
  rc = zjb_submit(&zjb, dsn, jobid);

  if (0 != rc)
  {
    cout << "Error: could not submit JCL: '" << dsn << "' rc: '" << rc << "'" << endl;
    cout << "  Details: " << zjb.diag.e_msg << endl;
    return -1;
  }

  cout << "Submitted " << dsn << ", " << jobid << endl;

  return 0;
}

int handle_job_delete(ZCLIResult result)
{
  int rc = 0;
  ZJB zjb = {0};
  string jobid(result.get_positional("jobid").get_value());

  rc = zjb_delete_by_jobid(&zjb, jobid);

  if (0 != rc)
  {
    cout << "Error: could not delete job: '" << jobid << "' rc: '" << rc << "'" << endl;
    cout << "  Details: " << zjb.diag.e_msg << endl;
    return -1;
  }

  cout << "Job " << jobid << " deleted " << endl;

  return 0;
}


int handle_console_issue(ZCLIResult result)
{
    int rc = 0;
    ZCN zcn = {0};

    string console_name(result.get_option("--console-name").get_value());
    string command(result.get_positional("command").get_value());

    rc = zcn_activate(&zcn, string(console_name));
    if (0 != rc)
    {
      cout << "Error: could not activate console: '" << console_name << "' rc: '" << rc << "'" << endl;
      cout << "  Details: " << zcn.diag.e_msg << endl;
      return -1;
    }

    printf("%.8s", zcn.console_name);

    rc = zcn_put(&zcn, command);
    if (0 != rc)
    {
      cout << "Error: could not write to console: '" << console_name << "' rc: '" << rc << "'" << endl;
      cout << "  Details: " << zcn.diag.e_msg << endl;
      return -1;
    }

    string response = "";
    rc = zcn_get(&zcn, response);
    if (0 != rc)
    {
      cout << "Error: could not get from console: '" << console_name << "' rc: '" << rc << "'" << endl;
      cout << "  Details: " << zcn.diag.e_msg << endl;
      return -1;
    }

    cout << response << endl;

    // example issuing command which requires a reply
    // e.g. zowexx console issue --console-name DKELOSKX "SL SET,ID=DK00"
    // rc = zcn_get(&zcn, response);
    // cout << response << endl;
    // char reply[24] = {0};
    // sprintf(reply, "R %.*s,CANCEL", zcn.reply_id_len, zcn.reply_id);
    // rc = zcn_put(&zcn, reply.c_str());
    // rc = zcn_get(&zcn, response);
    // cout << response << endl;

    rc = zcn_deactivate(&zcn);
    if (0 != rc)
    {
      cout << "Error: could not deactivate console: '" << console_name << "' rc: '" << rc << "'" << endl;
      cout << "  Details: " << zcn.diag.e_msg << endl;
      return -1;
    }
    return rc;
}

int handle_data_set_view_dsn(ZCLIResult result)
{
  int rc = 0;
  string dsn = result.get_positional("dsn").get_value();
  ZDS zds = {0};
  string response;
  rc = zds_read_dsn(&zds, dsn, response);
    if (0 != rc)
    {
      cout << "Error: could not read data set: '" << dsn << "' rc: '" << rc << "'" << endl;
      cout << "  Details: " << zds.diag.e_msg << endl;
      return -1;
    }
  cout << response;

  return rc;
}

int handle_test(ZCLIResult result)
{
  int rc = 0;
  rc = zut_test();

  cout << "test code called " << rc << endl;

  return 0;
}