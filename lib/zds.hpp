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
 * @brief Read data from a z/OS data set
 *
 * @param zds data set returned attributes and error information
 * @param dsn
 * @param response
 * @return int 0 for success; non zero otherwise
 */
int zds_read_from_dsn(ZDS *zds, std::string dsn, std::string &response);

/**
 * @brief Read data from a DDNAME
 *
 * @param zds data set returned attributes and error information
 * @param ddname ddname from which to read
 * @param response
 * @return int 0 for success; non zero otherwise
 */
int zds_read_from_dd(ZDS *zds, std::string ddname, std::string &response);

/**
 * @brief Write data to a DDNAME
 *
 * @param zds data set returned attributes and error information
 * @param ddname DDNAME to write to
 * @param data data to write
 * @return int 0 for success; non zero otherwise
 */
int zds_write_to_dd(ZDS *zds, std::string ddname, std::string &data);

/**
 * @brief Write data to a z/OS data set name
 *
 * @param zds data set returned attributes and error information
 * @param dsn data set name to write to
 * @param data data to write
 * @return int 0 for success; non zero otherwise
 */
int zds_write_to_dsn(ZDS *zds, std::string dsn, std::string &data);

/**
 * @brief Create a data set
 *
 * @param zds data set returned attributes and error information
 * @param dsn data set name to create
 * @return int 0 for success; non zero otherwise
 */
int zds_create_dsn(ZDS *zds, std::string dsn);

/**
 * @brief Delete a data set
 *
 * @param zds data set returned attributes and error information
 * @param dsn data set name to delete to
 * @return int 0 for success; non zero otherwise
 */
int zds_delete_dsn(ZDS *zds, std::string dsn);

int zdsListMembers(std::string, std::vector<ZDSMem> &);
int zdsList(std::string, std::vector<ZDSAttributes> &);
int zdsReadDynalloc(std::string, std::string, std::string, std::string &); // NOTE(Kelosky): testing only

#endif
