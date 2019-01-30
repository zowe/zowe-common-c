

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __PDSUTIL__
#define __PDSUTIL__ 1


typedef struct PDSIterator_tag{
#ifdef METTLE
  char *dcb;
#else
  FILE *in;
#endif
  char block[256];
  int blockCount;
  int lastBlock;
  int posInBlock;
  int bytesInBlock;
  int currentPosInBlock;
  int done;
  char memberName[9];
} PDSIterator;

#define startPDSIterator STPDSITR
#define nextPDSEntry NXPDSMEM
#define nextPDSEntry1 NXPDSME1
#define listDirectory LSTPDSDR
#define memberExistsInDDName MXSTDDNM
#define getPDSMembers GETPDSMB

int startPDSIterator(PDSIterator *iterator, char *pdsName);

int nextPDSEntry1(PDSIterator *iterator);

int nextPDSEntry(PDSIterator *iterator);

int endPDSIterator(PDSIterator *iterator);

void listDirectory(char *pdsName);

int memberExistsInDDName(char *ddname);

StringList *getPDSMembers(char *pdsName);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

