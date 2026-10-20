#ifndef _COMMON_H_
#define _COMMON_H_

// fields expected in the search result from CSI
extern const int NTESTFIELDS;
extern const char* const TESTFIELDS[];

// types expected in the search result from CSI
extern const int NTYPES;
extern char const TYPES[];

void initCSI();
void initLoggings(int level);
void uninitLoggings();
// returns a null-terminated string of name44. the returned string remains avaiable till the next call.
const char* getCStringName(const char* name44);

/**
 *  prints possibly leaked memory allocations.
 *  returns total number of allocations have been made.
 *  NOTICE: implemented in c/alloc.c
 */
#if ((!METTLE || WRITSTAT) && TRACK_MEMORY)
int showOutstanding();
#endif

#endif // _COMMON_H_
