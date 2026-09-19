#ifndef _COMMON_H_
#define _COMMON_H_

// fields expected in the search result from CSI
extern const int NTESTFIELDS;
extern const char* const TESTFIELDS[];

// types expected in the search result from CSI
extern const int NTYPES;
extern char const TYPES[];

void initCSI();
const char* getCStringName(const char* name44);

int showOutstanding(); // in alloc.c

#endif // _COMMON_H_