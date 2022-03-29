/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include "cppregex.h"
#include <regex>

CPPRegex::CPPRegex()
{
  /* value = start; */
}

int CPPRegex::compile(const char *pattern, int cflags)
{
  try {
    rx.assign(pattern,std::regex::ECMAScript);
  } catch (const std::regex_error& e){
    return e.code();
  }
  return 0;
}

enum reg_errcode_t {
  REG_NOERROR = 0, REG_NOMATCH, REG_BADPAT, REG_ECOLLATE,
  REG_ECTYPE, REG_EESCAPE, REG_ESUBREG, REG_EBRACK,
  REG_EPAREN, REG_EBRACE, REG_BADBR, REG_ERANGE,
  REG_ESPACE, REG_BADRPT, REG_EEND, REG_ESIZE,
  REG_ERPAREN
};
  
bool CPPRegex::exec(const char *first, const char *last, std::cmatch& matchResults){
  try {
    bool status = regex_search(first, last, matchResults, rx);
    return status;
  } catch (const std::regex_error& e){
    return e.code();
  }
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
