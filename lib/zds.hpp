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

struct ZDSMem
{
  std::string name;
  // std::string dsorg;
};

struct ZDS
{
  std::string name;
  std::string dsorg;
};

int zdsRead(std::string, std::string &);
int zdsWrite(std::string, std::string &);
int zdsListMembers(std::string, std::vector<ZDSMem> &);
int zdsList(std::string, std::vector<ZDS> &);
int zdsReadDynalloc(std::string, std::string, std::string, std::string &); // NOTE(Kelosky): testing only

#endif