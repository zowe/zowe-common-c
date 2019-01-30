

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/string.h>
#include <metal/ctype.h>
#else
#include <string.h>
#include <stdlib.h>
#endif

#include "xlate.h"

typedef struct {
  const char* name_;
  const unsigned char* table_;
} TableDefinition;

#include "xlate_tables.c"

static TableDefinition tables[] = {
  {"ibm1047_to_iso88591", ibm1047_to_iso88591},
  {"iso88591_to_ibm1047", iso88591_to_ibm1047},
  {"ibm037_to_iso88591", ibm037_to_iso88591},
  {"iso88591_to_ibm037", iso88591_to_ibm037},
  {"e2atable", ebcdic_to_ascii},
  {"a2etable", ascii_to_ebcdic},
  {0,0}
};

const void* getTranslationTable(const char* name)
{
  char nameCpy[32] = {0};
  int nameLen = 0;
  
  nameLen = strlen(name);
  if (nameLen > 31)
  {
    return 0;
  }

  for (int i = 0; i < nameLen; i++)
  {
    nameCpy[i] = tolower(name[i]);
  }

  TableDefinition* td = tables;
  while (td->name_ != 0) {
    if (0 == strcmp(nameCpy, td->name_)) {
      return td->table_;
    }
    ++td;
  }
  return 0;
}

char* e2a(char *buffer, int len)
{
  translate(buffer, ebcdic_to_ascii, (uint32_t)len);
  return buffer;
}

char* a2e(char *buffer, int len)
{
  translate(buffer, ascii_to_ebcdic, (uint32_t)len);
  return buffer;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

