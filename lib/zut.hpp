/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#ifndef ZUT_HPP
#define ZUT_HPP

#include <iostream>
#include <vector>
#include <string>

int zutTest();
void zutDumpStorage(std::string, const void *, size_t);
int zutHello(std::string);
char zutGetHexChar(int);
int zutGetCurrentUser(std::string &);

#endif