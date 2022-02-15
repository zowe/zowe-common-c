#include "psxregex.h"

regex_t *regexAlloc(){
  return (regex_t*)safeMalloc(sizeof(regex_t),"regex_t");
}

void regexFree(regex_t *r){
  regfree(r);
  safeFree((char*)r,sizeof(regex_t));
}
