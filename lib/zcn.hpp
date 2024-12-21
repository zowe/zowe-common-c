#ifndef ZCN_HPP
#define ZCN_HPP

#include <iostream>
#include <vector>
#include <string>
#include "zcntype.h"

/**
 * @brief Activate extended console
 *
 * @param zcn extended console returned attributes and error information
 * @param name console name, max of 8 characters e.g. MYCONSOL
 * @return int 0 for success; non zero otherwise
 */
int zcn_activate(ZCN* zcn, std::string name);

/**
 * @brief Deactivate extended console
 *
 * @param zcn extended console returned attributes and error information
 * @return int 0 for success; non zero otherwise
 */
int zcn_deactivate(ZCN *zcn);

/**
 * @brief Write command to extended console
 *
 * @param zcn extended console returned attributes and error information
 * @param command command to run, e.g. D IPLINFO
 * @return int
 */
int zcn_put(ZCN *zcn, std::string command);

/**
 * @brief Obtain data from extended console
 *
 * @param zcn extended console returned attributes and error information
 * @param response command response
 * @return int
 */
int zcn_get(ZCN *zcn, std::string& response);

#endif