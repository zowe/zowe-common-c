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

/**
 * @brief
 *
 * @param zds
 * @param dsn
 * @param response
 * @return int
 */
int zds_read_from_dsn(ZDS *zds, std::string dsn, std::string &response);

/**
 * @brief
 *
 * @param zds
 * @param ddname
 * @param response
 * @return int
 */
int zds_read_from_dd(ZDS *zds, std::string ddname, std::string &response);

/**
 * @brief
 *
 * @param zds
 * @param ddname
 * @param data
 * @return int
 */
int zds_write_to_dd(ZDS *zds, std::string ddname, std::string &data);

/**
 * @brief
 *
 * @param zds
 * @param dsn
 * @param data
 * @return int
 */
int zds_write_to_dsn(ZDS *zds, std::string dsn, std::string &data);

int zdsListMembers(std::string, std::vector<ZDSMem> &);
int zdsList(std::string, std::vector<ZDSAttributes> &);
int zdsReadDynalloc(std::string, std::string, std::string, std::string &); // NOTE(Kelosky): testing only

#endif
