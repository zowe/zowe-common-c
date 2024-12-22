/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <iostream>
#include <vector>
#include "zcli.hpp"

using namespace std;

int function1(ZCLIResult result)
{
  cout << "Function one called" << endl;
  return 0;
}

int function2(ZCLIResult result)
{
  cout << "Function two called" << endl;
  return 0;
}

int function3(ZCLIResult result)
{
  cout << "Function three called got results " << result.get_option("--console-name").get_value() << endl;

  return 0;
}

int main(int argc, char *argv[])
{
  // CLI
  ZCLI zcli(argv[PROCESS_NAME_ARG]);

  // jobs group
  ZCLIGroup jobs_group("jobs");
  jobs_group.set_description("z/OS job operations");

  // jobs verbs
  ZCLIVerb job_list("list");
  job_list.set_description("list jobs");
  job_list.set_zcli_verb_handler(function1);
  ZCLIOption job_owner("owner");
  job_owner.set_description("filter by owner");
  job_list.get_options().push_back(job_owner);
  jobs_group.get_verbs().push_back(job_list);

  ZCLIVerb job_view("view");
  job_view.set_description("view a job");
  job_view.set_zcli_verb_handler(function2);
  jobs_group.get_verbs().push_back(job_view);

  ZCLIVerb job_submit("submit");
  job_submit.set_description("submit a job");
  job_submit.set_zcli_verb_handler(function2);
  jobs_group.get_verbs().push_back(job_submit);

  // console group
  ZCLIGroup console_group("console");
  console_group.set_description("z/OS console operations");

  // console verbs
  ZCLIVerb console_issue("issue");
  console_issue.set_description("issue a console command");
  console_issue.set_zcli_verb_handler(function3);
  ZCLIOption console_name("console-name");
  console_name.set_required(true);
  console_name.set_description("extended console name");
  ZCLIOption console_data("data");
  console_data.set_required(true);
  console_issue.get_options().push_back(console_name);
  console_issue.get_options().push_back(console_data);
  console_group.get_verbs().push_back(console_issue);

  // add all groups to the CLI
  zcli.get_groups().push_back(console_group);
  zcli.get_groups().push_back(jobs_group);

  // parse
  return zcli.parse(argc, argv);
}