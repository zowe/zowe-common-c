/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <iostream>
#include <vector>
#include "zcn.hpp"
#include "zut.hpp"
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

int handle_console_issue(ZCLIResult);
int handle_test_command(ZCLIResult);

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
  test_command.set_zcli_verb_handler(handle_test_command);
  test_group.get_verbs().push_back(test_command);

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
  console_issue.set_zcli_verb_handler(handle_console_issue);
  ZCLIOption console_name("console-name");
  console_name.set_required(true);
  console_name.set_description("extended console name");
  console_issue.get_options().push_back(console_name);
  ZCLIPositional console_command("command");
  console_command.set_required(true);
  console_issue.get_positionals().push_back(console_command);
  console_group.get_verbs().push_back(console_issue);

  // add all groups to the CLI
  zcli.get_groups().push_back(test_group);
  zcli.get_groups().push_back(console_group);
  zcli.get_groups().push_back(jobs_group);

  // parse
  return zcli.parse(argc, argv);
}

int handle_test_console(ZCLIResult result)
{
  cout << "test code called " << endl;

  return 0;
}

int handle_test_command(ZCLIResult result)
{
  int rc = zut_test();
  cout << "test code called " << rc << endl;

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
      cout << "Error: could not activate console: '" << console_name << "' rc: '" << rc << "' service_rc: '" << zcn.service_rc << "'" << endl;
      cout << "  Details: " << zcn.e_msg << endl;
      return -1;
    }

    printf("%.8s", zcn.console_name);

    rc = zcn_put(&zcn, command);
    if (0 != rc)
    {
      cout << "Error: could not write to console: '" << console_name << "' rc: '" << rc << "' service_rc: '" << zcn.service_rc << "'" << endl;
      cout << "  Details: " << zcn.e_msg << endl;
      return -1;
    }

    string response = "";
    rc = zcn_get(&zcn, response);
    if (0 != rc)
    {
      cout << "Error: could not get from console: '" << console_name << "' rc: '" << rc << "' service_rc: '" << zcn.service_rc << "'" << endl;
      cout << "  Details: " << zcn.e_msg << endl;
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
      cout << "Error: could not deactivate rc: '" << rc << "' rsn: '" << zcn.service_rsn << "'" << endl;
      cout << "  Details: " << zcn.e_msg << endl;
      return -1;
    }
    return rc;
}