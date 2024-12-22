/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZCLI_HPP
#define ZCLI_HPP

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

using namespace std;

#define PROCESS_NAME_ARG 0
#define CLI_GROUP_ARG 1
#define CLI_VERB_ARG 2
#define CLI_REMAIN_ARG_START 3

#define ZCLI_MENU_WIDTH 15
#define ZCLI_MEDU_INDENT "  " // TODO(Kelosky)
#define ZCLI_FLAG_PREFIX "--" // TODO(Kelosky)

class ZCLIName
{
protected:
  string name;

public:
  ZCLIName(string n) : name(n) {}
  string get_name() { return name; }
};

class ZCLIRequired
{
protected:
  bool required;
  bool found;

public:
  ZCLIRequired() { required = false;}
  void set_found(bool f) { found = f; }
  bool get_found() { return found; }

  void set_required(bool r) { required = r; }
  bool get_required() { return required; }
};

class ZCLIDescription
{
protected:
  string description;

public:
  void set_description(string d) { description = d; }
  string get_description() { return description; }
};

class ZCLIFlag : public ZCLIName
{
public:
  ZCLIFlag(string n) : ZCLIName(n) {}
  string get_flag_name() { return "--" + name; };
};

class
    ZCLIOption : public ZCLIFlag,
                 public ZCLIRequired,
                 public ZCLIDescription
{
private:
  string value;

public:
  ZCLIOption(string n) : ZCLIFlag(n) {}
  void help_line() { cerr << "  " << left << setw(ZCLI_MENU_WIDTH) << get_flag_name() << "   " << get_description() << endl; }
  void set_value(string v) { value = v; }
  string get_value() { return value; }
};

class ZCLIOptionProvider
{
protected:
  vector<ZCLIOption> options;

public:
  vector<ZCLIOption> &get_options() { return options; }
  void set_options(vector<ZCLIOption> &o) { options = o; }
};

class ZCLIPositional : public ZCLIRequired
{
private:
  string name;

public:
  ZCLIPositional(string n) : name(n) {}
  string get_name() { return name; }
};

class ZCLIResult
{
private:
  vector<ZCLIOption> options;

public:
  vector<ZCLIOption> &get_options() { return options; }
  ZCLIOption &get_option(string option);
};

typedef ZCLIOption &(*zcli_get_option)(string);

typedef int (*zcli_verb_handler)(ZCLIResult);

class ZCLIVerb : public ZCLIName, public ZCLIDescription, public ZCLIOptionProvider
{
private:
  vector<ZCLIPositional> positionals;
  zcli_verb_handler cb;

public:
  ZCLIVerb(string n) : ZCLIName(n) {}
  vector<ZCLIPositional> &get_positionals() { return positionals; }
  void set_zcli_verb_handler(zcli_verb_handler h) { cb = h; }
  zcli_verb_handler get_zcli_verb_handler() { return cb; }
  ZCLIOption &get_option(string);
  void help_line() { cerr << "  " << left << setw(ZCLI_MENU_WIDTH) << get_name() << " | " << get_description() << endl; }
  void help(string, string);
};

class ZCLIGroup : public ZCLIName, public ZCLIDescription, public ZCLIOptionProvider
{
private:
  vector<ZCLIVerb> verbs;

public:
  ZCLIGroup(string n) : ZCLIName(n) {};
  ZCLIVerb &get_verb(string);
  vector<ZCLIVerb> &get_verbs() { return verbs; }
  void help(string);
  void help_line() { cerr << "  " << left << setw(ZCLI_MENU_WIDTH) << get_name() << " | " << get_description() << endl; }
};

class ZCLI : public ZCLIName, public ZCLIOptionProvider
{
private:
  bool validate();
  vector<ZCLIGroup> groups;

public:
  ZCLI(string n) : ZCLIName(n) {}
  int parse(int, char *[]);
  void init();
  vector<ZCLIGroup> &get_groups() { return groups; };
  ZCLIGroup &get_group(string);
  ZCLIVerb &get_verb(int, char *[]);
  void help();
};

// TOOD(Kelosky): check for duplicates
// TOOD(Kelosky): ensure no unused parms

bool ZCLI::validate()
{
  if (0 == groups.size())
  {
    cerr << "ZCLI Error: must define at least one group" << endl;
    return false;
  }

  for (vector<ZCLIGroup>::iterator it = groups.begin(); it != groups.end(); it++)
  {
    if (0 == it->get_verbs().size())
    {
      cerr << "ZCLI Error: each group must contain at least one verb, " << it->get_name() << " does not" << endl;
      return false;
    }

    for (vector<ZCLIVerb>::iterator iit = it->get_verbs().begin(); iit != it->get_verbs().begin(); iit++)
    {
      if (NULL == iit->get_zcli_verb_handler())
      {
        cerr << "ZCLI Error: each verb must container a handler, " << iit->get_name() << " does not" << endl;
        return false;
      }
      for (vector<ZCLIOption>::iterator iiit = iit->get_options().begin(); iiit != iit->get_options().begin(); iiit++)
      {
        if (string::npos != iiit->get_name().find(" "))
        {
          cerr << "ZCLI Error: option cannot contain a space, '" << iiit->get_name() << "' does" << endl;
          return false;
        }
      }
    }
  }
  return true;
}

void ZCLI::init()
{
  ZCLIOption help("help");
  help.set_description("CLI help");

  get_options().push_back(help);

  for (vector<ZCLIGroup>::iterator it = groups.begin(); it != groups.end(); it++)
  {
    help.set_description("group help");
    it->get_options().push_back(help);
    for (vector<ZCLIVerb>::iterator iit = it->get_verbs().begin(); iit != it->get_verbs().end(); iit++)
    {
      help.set_description("verb help");
      iit->get_options().push_back(help);
    }
  }
}

void ZCLIVerb::help(string cli_name, string group_name)
{
  cerr << "Usage is '" << cli_name << " " << group_name << " " << get_name() << ":" << endl;

  if (get_options().size() > 0)
  {
    cerr << "Options:" << endl;
    for (vector<ZCLIOption>::iterator it = options.begin(); it != options.end(); it++)
    {
      it->help_line();
    }
  }
}

void ZCLIGroup::help(string cli_name)
{
  cerr << "Usage is '" << cli_name << " " << name << " <verb>' where verb is one of:" << endl;
  for (vector<ZCLIVerb>::iterator it = verbs.begin(); it != verbs.end(); it++)
  {
    it->help_line();
  }

  if (get_options().size() > 0)
  {
    cerr << "Options:" << endl;
    for (vector<ZCLIOption>::iterator it = options.begin(); it != options.end(); it++)
    {
      it->help_line();
    }
  }
}

void ZCLI::help()
{
  cerr << "Usage is '" << name << " <group>' where group is one of:" << endl;
  for (vector<ZCLIGroup>::iterator it = groups.begin(); it != groups.end(); it++)
  {
    it->help_line();
  }

  if (get_options().size() > 0)
  {
    cerr << "Options:" << endl;
    for (vector<ZCLIOption>::iterator it = options.begin(); it != options.end(); it++)
    {
      it->help_line();
    }
  }
}

ZCLIGroup &ZCLI::get_group(string group_name)
{
  for (vector<ZCLIGroup>::iterator it = groups.begin(); it != groups.end(); it++)
  {
    if (group_name == it->get_name())
      return *it;
  }
  ZCLIGroup *not_found = new ZCLIGroup("not found");
  return *not_found;
}

ZCLIVerb &ZCLIGroup::get_verb(string verb_name)
{
  for (vector<ZCLIVerb>::iterator it = verbs.begin(); it != verbs.end(); it++)
  {
    if (verb_name == it->get_name())
      return *it;
  }
  ZCLIVerb *not_found = new ZCLIVerb("not found");
  return *not_found;
}

ZCLIOption &ZCLIVerb::get_option(string option_name)
{
  for (vector<ZCLIOption>::iterator it = options.begin(); it != options.end(); it++)
  {
    if (option_name == it->get_flag_name())
      return *it;
  }
  ZCLIOption *not_found = new ZCLIOption("not found");
  return *not_found;
}

ZCLIOption &ZCLIResult::get_option(string option_name)
{
  for (vector<ZCLIOption>::iterator it = options.begin(); it != options.end(); it++)
  {
    if (option_name == it->get_flag_name())
      return *it;
  }
  ZCLIOption *not_found = new ZCLIOption("not found");
  return *not_found;
}

int ZCLI::parse(int argc, char *argv[])
{
  init();
  bool valid = validate();

  if (!valid)
    return -1;

  if (argc <= CLI_GROUP_ARG || string(argv[CLI_GROUP_ARG]) == "--help")
  {
    help();
    return 0;
  }

  // attempt to get a group
  ZCLIGroup &group = get_group(argv[CLI_GROUP_ARG]);

  // show main help if unknown group
  if (0 == group.get_verbs().size())
  {
    // delete command_group;
    cerr << "Unknown command group: " << argv[CLI_GROUP_ARG] << endl;
    help();
    return 0;
  }

  // show group level help if group only
  if (argc <= CLI_VERB_ARG || string(argv[CLI_VERB_ARG]) == "--help")
  {
    group.help(name);
    return 0;
  }

  // attempt to get a verb
  ZCLIVerb &verb = group.get_verb(argv[CLI_VERB_ARG]);

  // show group level help if unknwon verb
  if (NULL == verb.get_zcli_verb_handler())
  {
    // delete command_group;
    cerr << "Unknown command verb: " << argv[CLI_VERB_ARG] << endl;
    group.help(name);
    return 0;
  }

  // look for help
  for (int i = CLI_REMAIN_ARG_START; i < argc; i++)
  {
    if (string(argv[i]) == "--help")
    {
      verb.help(name, group.get_name());
      return 0;
    }
  }

  ZCLIResult results;

  for (int i = CLI_REMAIN_ARG_START; i < argc; i++)
  {
    ZCLIOption &option = verb.get_option(argv[i]);
    if (string::npos != option.get_name().find(" "))
    {
      cerr << "Unknown option on: " << argv[i] << endl;
      verb.help(name, group.get_name());
      return 1;
    }

    if (i + 1 > argc - 1) // index vs count
    {
      cerr << "Missing required value for: " << argv[i] << endl;
      verb.help(name, group.get_name());
      return -1;
    }

    option.set_found(true);
    option.set_value(argv[i + 1]);
    results.get_options().push_back(option);

    i++; // advance to next parm
  }

  for (vector<ZCLIOption>::iterator it = verb.get_options().begin(); it != verb.get_options().end(); it++)
  {

    if (it->get_required() && !it->get_found())
    {
      cerr << "Required option missing: " << it->get_flag_name() << endl;
      verb.help(name, group.get_name());
      return -1;
    }
  }

  // set values
  return verb.get_zcli_verb_handler()(results);
}

#endif