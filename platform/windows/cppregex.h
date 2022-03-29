/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __CPPREGEX_H__
#define __CPPREGEX_H__

#include <regex>

class CPPRegex
{
	public:
		CPPRegex();
		int compile(const char *pat, int cflags);
                bool exec(const char *first, const char *last, std::cmatch& results);

	private:
                std::regex rx;
};

#endif // __CPPEGEX_H__

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
