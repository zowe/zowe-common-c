

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __UTILSOPT__
#define __UTILSOPT__   1

#ifndef __LONGNAME__
#define fillToUpperTable FLTOUPPR
#define fillToUpperAndPrintableTable FLTOUAPT
#define fastInsensitiveSearch FSTNSSCH
#define fastSensitiveSearch FSTSSCH
#endif

void fillToUpperTable(char *table);
void fillToUpperAndPrintableTable(char *table);
int fastInsensitiveSearch(char *str, int len, char *searchString, int searchLen, int startPos, char *toUpperTable);
int fastSensitiveSearch(char *str, int len, char *searchString, int searchLen, int startPos);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

