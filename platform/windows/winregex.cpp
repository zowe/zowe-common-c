/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <stdlib.h>
#include "winregex.h"
#include "cppregex.h"


regex_t *regexAlloc(){
  regex_t *m;
  CPPRegex *obj;
  
  m      = (decltype(m))malloc(sizeof(*m));
  obj    = new CPPRegex();
  m->obj = obj;
  
  return m;
}

void regexFree(regex_t *m){
  if (m == NULL)
    return;
  delete static_cast<CPPRegex *>(m->obj);
  free(m);
}

int regexComp(regex_t *r, const char *pat, int cflags){
  CPPRegex *obj;
  
  if (r == NULL){
    return REG_E_UNKNOWN;
  }
  
  obj = static_cast<CPPRegex *>(r->obj);
  return obj->compile(pat,cflags);
}

int regexExec(regex_t *r, const char *str, size_t nmatch,
            regmatch_t *pmatch, int eflags){ 
  CPPRegex *obj;
  std::cmatch matchResults;
  
  if (r == NULL){
    return REG_E_UNKNOWN;
  }
  
  obj = static_cast<CPPRegex *>(r->obj);
  // here
  // 1 worry about additional arguments vs. linue DIE
  // 2 get pmatch data structure to fill in.
  bool matched = obj->exec(str,str+strlen(str),matchResults);
  if (matched){
    for (int i=0; i<nmatch ; i++){
      if (i < matchResults.size() && matchResults[i].matched){
        regoff_t start = matchResults.position(i);
        pmatch[i].rm_so = start; 
        pmatch[i].rm_eo = start + matchResults.length(i);
      } else {
        pmatch[i].rm_so = -1;
        pmatch[i].rm_eo = -1;
      }
    }
  }
  return (matched ? 0 : REG_NOMATCH);
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
