/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZDS_HPP
#define ZDS_HPP

#include <iostream>
#include <vector>
#include <string>
#include "zds.hpp"
#include "zdstype.h"

struct ZDSMem
{
  std::string name;
  // std::string dsorg;
};

struct ZDSAttributes
{
  std::string name;
  std::string dsorg;
};

int zds_read_dsn(ZDS *zds, std::string dsn, std::string &response);
int zds_read_dd(std::string ddname, std::string &response);
int zdsWrite(std::string, std::string &);
int zdsListMembers(std::string, std::vector<ZDSMem> &);
int zdsList(std::string, std::vector<ZDSAttributes> &);
int zdsReadDynalloc(std::string, std::string, std::string, std::string &); // NOTE(Kelosky): testing only

#endif