

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>
#include <metal/ctype.h>
#include "qsam.h"
#include "metalio.h"
#else
#define _ALL_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifndef _MSC_VER  /* WINDOWS */
#include <sys/time.h>
#else
#include <winsock.h>
#endif  /* WINDOWS */

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "timeutls.h"

static const char daysInAMonth[2][12] = { {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
                                          {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
                                        };

/* must get odd powers of 10  */
static
void writePackedDecimal(void *vbuffer, int place, int placeValue, int value){
  char *buffer = (char*)vbuffer;
  value = value % (placeValue * 10);
  while (placeValue > 1){
    int high = (value / placeValue);
    int low = 0;

    value = value - (placeValue * high);
    placeValue = placeValue/10;
    low = (value / placeValue);
    value = value - (placeValue * low);
    placeValue = placeValue/10;
    /* printf("place=%d high=%d low=%d\n",place,high,low); */
    buffer[place++] = high<<4|low;
  }
}

static
int extractFromPackedDecimal(const void *vbuffer, int place, int placeValue){
  const uint8_t *buffer = (const uint8_t*)vbuffer;
  int result = 0;
  while (placeValue > 1){
    uint8_t byte = buffer[place++];
    int high = byte >> 4;
    int low = byte & 0xf;
    result = (result*100)+(high*10)+low;
    placeValue = placeValue/100;
  }
  return result;
}

#ifdef __ZOWE_OS_ZOS

/* -------------This code was taken from zlrtl.c------------------------------ */


/* Get STCK time - the native Z architecture clock */

void getSTCK(int64 *stckPtr){
  int64 stck = 0;

  __asm(ASM_PREFIX
        " STCK %0 "
        :"=m"(stck));
  *stckPtr = stck;
}

void getSTCKU(uint64 *stckPtr){
  uint64 stck = 0;

  __asm(ASM_PREFIX
        " STCK %0 "
        :"=m"(stck));
  *stckPtr = stck;
}

/* Service routines for 31- and 64-bit modes to use when converting STCK to
 * time stamp text representation and vise versa */
static short pcCallCode31[] =
  { 0x90EC, 0xD00C, /* STM 14,12,12(13) */
    0x5821, 0x0000, /* L   2,0(1)       service routine */
    0x5811, 0x0004, /* L   1,4(1)       param list */
    0xB218, 0x2000, /* PC  0(2)         */
    0x980C, 0xD014, /* LM 0,12,20(13)   */
    0x58E0, 0xD00C, /* L r14, 12(13)    */
    0x07FE};        /* BR 14            */

#define PCCALL64_SIZE 23
static short pcCallCode64[PCCALL64_SIZE] =
  { 0xEB5C, 0x4708, 0x0024, /* STMG    R5,R12,X'00708'(R4)  */
    0xA74B, 0xFF00,         /* AGHI    R4,X'FF00'           */
    0xB904, 0x0081,         /* LGR     R8,R1                */
    0xB904, 0x0012,         /* LGR     R1,R2                */
    0x010D,
    0xB218, 0x8000,         /* PC  0(8)                     */
    0xB904, 0x003F,         /* LGR R3,R15                   */
    0x010E,
    0x4140, 0x4100,         /* LA  R4,X'100'(,R4)           */
    0xEB7C, 0x4718, 0x0004, /* LMG     R7,R12,X'00718'(R4)  */
    0x47F0, 0x7002 };       /* B       X'002'(,R7)          */

typedef int pc_call_os_fn(int, void *);

pc_call_os_fn *pcCall31 = (pc_call_os_fn*)pcCallCode31;

int pcCall(int serviceRoutine, void *parmlist){
  int thingy;
  int wrapper[4];

#ifdef _LP64
  int routineSize = sizeof(unsigned short)*PCCALL64_SIZE;
  unsigned short *routineUnderBar = (unsigned short *)malloc31(routineSize);
  int i;
  for (i=0; i<PCCALL64_SIZE; i++){
    routineUnderBar[i] = pcCallCode64[i];
  }
  wrapper[0] = 0;
  wrapper[1] = 0;
  wrapper[2] = 0;
  wrapper[3] = (int)routineUnderBar;

  int res = 0;

  res = ((pc_call_os_fn*)wrapper)(serviceRoutine,parmlist);
  free31((char*)routineUnderBar,routineSize);
  return res;
#else
  return pcCall31(serviceRoutine,parmlist);
#endif
}

/* Structures for passing parameters to the service routines */
typedef struct STCKCONVPlist_tag{
  char flags1;
  char flags2;
  char dateFlags;  /* 1 is MMDDYYYY, 3 is YYYYMMDD */
  char timeFlags;  /* 33 is bin, 34 is dec, 35 is MIC */
  int  stckvalHigh;
  int  stckvalLow;
  char output[16];
} STCKCONVPlist;

typedef struct CONVTODPlist_tag{
  char flags1;
  char flags2;
  char dateFlags;  /* 1 is MMDDYYYY, 3 is YYYYMMDD */
  char timeFlags;  /* 1 is bin, 2 is dec, 3 is MIC */
  int  offset;
  char input[16];
  int64 stckval;
} CONVTODPlist;

/* This function converts GMT time considering local offset information
 * which is taken from CVT vector. */
void convertTODToLocal(int64 *stck) {
  // Jump to CVT control block to get the information about local offset and leap secs
  unsigned char *cvt_extension_2 = ((unsigned char ***)0)[0x10/4][0x148/4];
  // Subtract leap seconds from GMT time returned from stck
  *stck -= *(unsigned long long *)&cvt_extension_2[0x50]; /* CVTXTNT2.CVTLSO leap seconds */
  // Adding the offset to finally get to the local time
  *stck += *(unsigned long long *)&cvt_extension_2[0x38]; /* CVTXTNT2.CVTLDTO local time offset */
}

/* Converts from STCK into 16-byte text time stamp
 * stck - the TOD value which needs to be converted,
 * output - the pointer to 16-byte output char array
 * The example of the conversion:
 * C9BCB377 65D90D00 - 08271169 83200000 20120618 00000000 ,
 * which is 2012/06/18 08:27:11:69832 */
int stckToTimestamp(int64 stck, char *output)
{
  int res = 0;
  int stckconvListLen = 0;

#ifdef METTLE
  __asm (ASM_PREFIX
         " XGR   15,15                    \n"
         " LHI   %0,STCKCONVSIZE          \n"
         " J     BYPASSSTCKCONVLIST        \n"
         "STCKCONVLIST STCKCONV MF=L      \n"
         "STCKCONVSIZE EQU *-STCKCONVLIST  \n"
         "BYPASSSTCKCONVLIST DS 0H          "
         :"=r"(stckconvListLen)
         :
         :"r15"
  );

  char *workarea = safeMalloc31 (stckconvListLen, "stckconv workarea");
    // according to the description of STCKCONV we should zero AR1
  __asm (ASM_PREFIX
#ifdef _LP64
         " SAM31                          \n"
#endif
         " LAM   1,1,=F'0'                \n"
         " LGR   1,%3                     \n"
         " LGR   2,%2                     \n"
         " STCKCONV STCKVAL=%1,CONVVAL=(2),TIMETYPE=DEC,DATETYPE=YYYYMMDD,MF=(E,(1))   \n"
#ifdef _LP64
         " SAM64                          \n"
#endif
         " LR    %0,15                      "
         :"=r"(res)
         :"m"(stck),"r"(output),"r"(workarea)
         :"r1","r2","r15"
  );
  safeFree(workarea, stckconvListLen);
#else
  /* not METAL */
  STCKCONVPlist * pPlist = (STCKCONVPlist *) safeMalloc31(sizeof(STCKCONVPlist), "STCKCONVPlist");
  int * value = (int *)(&stck);

  int * __ptr32 CVTPTR = (int* __ptr32) ((int* __ptr32) 16);
  int * __ptr32 CVT = (int * __ptr32) CVTPTR[0];
  int * __ptr32 SFT = (int * __ptr32) (CVT[772/4]);
  int serviceRoutine = SFT[304/4];


  pPlist->dateFlags = 0x03;
  pPlist->timeFlags = 0x22;
  pPlist->stckvalHigh = value[0];
  pPlist->stckvalLow = value[1];

  res = pcCall(serviceRoutine, pPlist);
  if (!res){
    memcpy(output,&(pPlist->output),16);
  }
  safeFree((char*)pPlist, sizeof(STCKCONVPlist));
#endif

  return res;
}

/* Converts from 16-byte text timestamp to STCK;
 * *todText - a pointer to 16-byte input text time stamp
 * *stck - a pointer to output TOD value
 * offset - a TOD value which denotes local offset of the given input time stamp
 * and is subtracted from the result TOD in order to get the output in GMT */
int timestampToSTCK(char *todText, int64 *stck, int64 offset){
  int *value = (int*)stck;

  int *CVTPTR = (int*)((int*)16);
  int *CVT = (int*)CVTPTR[0];
  int *SFT = (int*)(CVT[772/4]);
  int serviceRoutine = SFT[352/4];
  int res = 0;
  CONVTODPlist plist;

  memset(&plist,0,sizeof(CONVTODPlist));
  plist.dateFlags = 0x03;
  plist.timeFlags = 0x02;
  memcpy(&(plist.input),todText,16);

  /*
    printf("serviceRoutine = 0x%x\n",serviceRoutine);fflush(stdout);
    dumpbuffer((char*)(&plist),sizeof(CONVTODPlist));
    printf("\n");
    */
  res = pcCall(serviceRoutine, &plist);
  /*
    printf("pcCall res=0x%x\n",res);
    dumpbuffer((char*)(&plist),sizeof(CONVTODPlist));
    printf("stckval %llx\n",stck);
    */
  *stck = (plist.stckval - offset);
  return res;
}

int timeZoneDifferenceFor(int64 theTime)
{
  return -1;
}

#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_WINDOWS)


#include <errno.h>

/*
  The MVS Epoch started on 1 January 1900; the Unix Epoch on 1 January 1970.
  This is the number of seconds between those two dates. It's the same value
  as computed below for epoch1900Gmt, but positive.
 */
static
const uint64 secondsBetween360andUnixEpochs = UINT64_C(2208988800);

/*
  Bit 51 of the TOD clock is the microsecond unit; this many bits have to
  be ditched to convert the 64-bit TOD value to microseconds.
 */
#define STCK_MICRO_SHIFT 12

static
int64 stckFromTimeval(struct timeval* time)
{
  int64 stckSeconds = ((int64) time->tv_sec)+secondsBetween360andUnixEpochs;
  if (stckSeconds <= 0) {
    return 0;
  }
  /*
    TBD: This should check for overflow.
   */
  uint64 justMicroseconds = ((uint64) time->tv_usec);
  uint64 microseconds = (((uint64)stckSeconds)*1000000)+justMicroseconds;
  uint64 rawStck = microseconds << STCK_MICRO_SHIFT;
  return (int64) rawStck;
}

static
void stckToTimeval(struct timeval* time, int64 stck)
{
  uint64 ustck = (uint64)stck;
  uint64 microseconds = ustck >> STCK_MICRO_SHIFT;
  uint64 stckSeconds = microseconds/1000000;
  time->tv_sec = stckSeconds-secondsBetween360andUnixEpochs;
  time->tv_usec =  microseconds%1000000;
}

static uint64 zoneDifference = 0;
static uint64 stckZoneDifference = 0;
static int zoneKnown = 0;
static int localIsWest = 0;

static
uint64 sleazyToSeconds(const struct tm* time)
{
  uint64 result =  time->tm_year;
  result = result * 365 + time->tm_yday;
  result = result * 24  + time->tm_hour;
  result = result * 60  + time->tm_min;
  result = result * 60  + time->tm_sec;
  return result;
}

/* a platform sensitive wrapper */
static void platformGMTime(time_t *time,
                           struct tm *tm){
#ifdef __ZOWE_OS_WINDOWS
  memcpy(tm,gmtime(time),sizeof(struct tm));
#else
  gmtime_r(time,tm);
#endif
}

/* A missing function in Windows -
 * This implementation came from A MS VisStudio Support pag 
 */

#ifdef __ZOWE_OS_WINDOWS

#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
 
struct timezone 
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};

static int gettimeofday(struct timeval *tv, struct timezone *tz)
{
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  static int tzflag;
 
  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);
 
    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;
 
    /*converting file time to unix epoch*/
    tmpres -= DELTA_EPOCH_IN_MICROSECS; 
    tmpres /= 10;  /*convert into microseconds*/
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }
 
  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
    tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }
 
  return 0;
}

#endif /* WINDOWS gettimeofday implementation */

static int tzsetHasBeenCalled = 0;

static inline
void tzsetWrapper()
{
  if (!tzsetHasBeenCalled) {
#ifdef __ZOWE_OS_WINDOWS
    _tzset();
#else
    tzset();
#endif
    tzsetHasBeenCalled = 1;
  }
}

static inline
int timeZoneDifferenceInternal(time_t base, struct tm* local)
{
  /* There has to be a better way... */
  int result = 0;
  tzsetWrapper();
  struct tm utc;
  platformGMTime(&base, &utc);
  uint64 utcBase = sleazyToSeconds(&utc);
  uint64 localBase = sleazyToSeconds(local);
  if (utcBase > localBase) {
    /* local time is west of UTC */
    result = -((int)(utcBase-localBase));
  } else {
    /* local time is east of, or equal to, UTC */
    result = (int)localBase-utcBase;
  }
  return result;
}


/*
  Compute the difference between UTC and local time for the
  specified Unix time (seconds since the Unix Epoch).
 */
int timeZoneDifferenceFor(int64 theTime)
{
  /* There has to be a better way... */
  time_t base = (time_t) theTime;
  struct tm local;
#ifdef __ZOWE_OS_WINDOWS
  struct tm *tempTM = localtime(&base);
  memcpy(&local,tempTM,sizeof(struct tm));
#else
  localtime_r(&base, &local);
#endif
  return timeZoneDifferenceInternal(base, &local);
}

static inline
void computeCurrentZoneDifference()
{
  if (zoneKnown) return;
  time_t now = time(0);
  /* If there's a problem getting the time, just set no difference */
  if (((time_t)-1) != now) {
    int diff = timeZoneDifferenceFor(now);
    if (diff < 0) {
      /* local time is west of UTC */
      localIsWest = 1;
      zoneDifference = (uint64)(-diff);
    } else {
      /* local time is east of, or equal to, UTC */
      localIsWest = 0;
      zoneDifference = (uint64)diff;
    }
    stckZoneDifference = zoneDifference*1000000 << STCK_MICRO_SHIFT;
  }
  zoneKnown = 1;
}

void getSTCK(int64 *stck)
{
  struct timeval time;
  int64 result = 0;
  if (0 != gettimeofday(&time, 0)) {
    /* ruh-roh - what now? */
    int error = errno;
    printf("gettimeofday failed: errno=%d (%s)\n", error, strerror(errno));
  } else {
    result = stckFromTimeval(&time);
  }
  *stck = result;
}

void getSTCKU(uint64 *stck)
{
  struct timeval time;
  int64 result = 0;
  if (0 != gettimeofday(&time, 0)) {
    /* ruh-roh - what now? */
    int error = errno;
    printf("gettimeofday failed: errno=%d (%s)\n", error, strerror(errno));
  } else {
    result = stckFromTimeval(&time);
  }
  *stck = (uint64)result;
}

void convertTODToLocal(int64 *stck) {
  computeCurrentZoneDifference();
  uint64 ustck = (uint64)*stck;
  if (localIsWest) {
    *stck = (int64)(ustck-stckZoneDifference);
  } else {
    *stck = (int64)(ustck+stckZoneDifference);
  }
}

int stckToTimestamp(int64 stck, char *output)
{
  struct timeval timeval;
  stckToTimeval(&timeval, stck);
  time_t time = (time_t)timeval.tv_sec;
  struct tm split;
  platformGMTime(&time, &split);
  memset(output, 0, TIMESTAMP_LENGTH);

  writePackedDecimal(output,0,10,split.tm_hour);
  writePackedDecimal(output,1,10,split.tm_min);
  writePackedDecimal(output,2,10,split.tm_sec);
  writePackedDecimal(output,3,100000, (int)timeval.tv_usec);
  writePackedDecimal(output,8,1000,split.tm_year+1900);
  writePackedDecimal(output,10,10,split.tm_mon+1);
  writePackedDecimal(output,11,10,split.tm_mday);
  return 0;
}

int timestampToSTCK(char *todText, int64 *stck, int64 offset)
{
  /* This is hideously expensive */
  struct timeval timeval;

  struct tm split;
  int year = extractFromPackedDecimal(todText,8,1000);
  if (year < 1900) {
    return -1;
  }

  split.tm_hour = extractFromPackedDecimal(todText,0,10);
  split.tm_min = extractFromPackedDecimal(todText,1,10);
  split.tm_sec =extractFromPackedDecimal(todText,2,10);

  split.tm_year = year-1900;
  split.tm_mon = extractFromPackedDecimal(todText,10,10)-1;
  split.tm_mday = extractFromPackedDecimal(todText,11,10);
  split.tm_isdst = -1;
  time_t localTime = mktime(&split);
  if (((time_t)-1) == localTime) {
    return 1;
  }

  int adjustment = timeZoneDifferenceInternal(localTime, &split);
  int64 utcTime = ((int64)localTime)+adjustment;
  /*
    TBD: This should check for overflow.
   */
  uint64 stckSeconds = (uint64)(utcTime+(int64)secondsBetween360andUnixEpochs);
  uint64 justMicroseconds = (uint64) extractFromPackedDecimal(todText,3,100000);
  uint64 microseconds = (stckSeconds*1000000)+justMicroseconds;
  uint64 rawStck = microseconds << STCK_MICRO_SHIFT;
  if (offset < 0) {
    rawStck -= ((uint64)(-offset));
  } else {
    rawStck += offset;
  }
  *stck = (int64)rawStck;
  return 0;
}
#else
#error OS unknown
#endif

/* Resets TOD value to timeFromMidnight time
 * stckPtr - pointer address to the STCK 8-byte value
 * The routine first checks the data for correctness */
int timeFromMidnight(int64 stckPtr){
  int64 *stck = (int64*)INT64_CHARPTR(stckPtr);

  char buffer[16];

  /*
     printf("MIDNIGHT called 0x%x\n",stck);
     dumpbuffer((char*)stck,8);
     fflush(stdout);
     */
  int res = stckToTimestamp(*stck,buffer);

  if (res){
    return res;
  } else{
    int64 offset = 0;
    memset(buffer,0,8); /* blank out the time portion */
    res = timestampToSTCK(buffer,stck,offset);
    return res;
  }
}

/* Converts from year-month-day input to STCK value
 * *stck - a pointer to the output TOD value,
 * year (YYYY), nmonth(MM), day(DD),
 * offsetLow, offsetHigh - parts of a 8-byte TOD offset to
 * adjust the manipulations for local time zone */
int stckFromYYYYMMDD(int dummy0, int64 *stck,
         int dummy1, int year,
         int dummy2, int month,
         int dummy3, int date,
         int offsetHigh, int offsetLow){
  char tod[16];
  memset(tod,0,16);
  int64 offset = 0;
  int *offsetData = (int*)(&offset);
  int res;
  offsetData[0] = offsetHigh;
  offsetData[1] = offsetLow;
  /* printf("stckFromYYYYMMDD y=%d m=%d d=%d off=0x%llx\n",year,month,date,offset); */
  writePackedDecimal(tod,8,1000,year);
  writePackedDecimal(tod,10,10,month);
  writePackedDecimal(tod,11,10,date);
  /*
    printf("tod buffer\n");
    dumpbuffer(tod,16);
    */
  res = timestampToSTCK(tod,stck,offset);
  /* printf("timestampToSTCK res=%d stck=0x%llx\n",res,*stck); */
  return res;
}

/* Calculates the elapsed time of DD:HH:MM:SS and returns that in
 * TOD representation
 * *stck -a pointer to 8-byte output TOD value */
int elapsedTime(int dummy0, int64 *stck,
    int dummy1, int days,
    int dummy2, int hours,
    int dummy3, int minutes,
    int dummy4, int seconds){
  long long result = 0ll;
  long long million = (long long)1000000;
  long long secondsInMicros = (seconds * million);
  long long minutesInMicros = (minutes * (60 * million));
  long long hoursInMicros = (hours * (3600 * million));
  long long daysInMicros = (days * (86400 * million));

  /* printf("seconds = %d, minutes=%d hours=%d days=%d\n",seconds,minutes,hours,days);fflush(stdout); */
  *stck = (secondsInMicros + minutesInMicros + hoursInMicros + daysInMicros)<<12;
  /*
    printf("seconds = %llx, minutes=%llx, hours=%llx, elapsed = %llx\n",
   secondsInMicros,minutesInMicros,hoursInMicros,*stck);
   */
  return 0;
}

/* -----------------------------End of zlrtl.c code--------------------------------- */

/* This function is dedicated to converting from STCK representation to Unix time.
 * In this file it is used to generate the example of the input data for
 * unixToTimestamp routine.
 * stck - a value to be converted
 */

int64 stckToUnix(int64 stck) {
  uint64 ustck = (uint64) stck; //for shift
    /*
      Under some 32-bit compilation conditions (maybe 32-bit, metal) the compiler does
      not like naked 64-bit constants so we force its creation here:
     */
  int64 epoch1900Gmt = ((int64)0xFFFFFDFD << 32) |  ((int64)0xAE01DC00);
  int clockMicroShift = 0xC;
  int64 microseconds = ustck >> clockMicroShift;
  int64 javaClock = microseconds/1000L;
  int64 unixClockSec = ( javaClock + epoch1900Gmt ) / 1000;

  return unixClockSec;
}

/* This function outputs seconds and microseconds into the Unix epoch,
 * returning 0 on success or -1 on failure.
 */
int stckToUnixSecondsAndMicros(int64 stck,
                               uint32 *out_secs, uint32 *out_micros)
{
  uint64 ustck = (uint64) stck; //for shift
    /*
      Under some 32-bit compilation conditions (maybe 32-bit, metal) the compiler does
      not like naked 64-bit constants so we force its creation here:
     */
  int64 epoch1900GmtMillis = ((int64)0xFFFFFDFD << 32) |  ((int64)0xAE01DC00);
  int clockMicroShift = 0xC;
  int64 microseconds = ustck >> clockMicroShift;
  int64 unixMicros = microseconds + (epoch1900GmtMillis * 1000L);
  int64 unixSeconds = unixMicros / 1000000L;
  *out_secs = (uint32) unixSeconds;
  *out_micros = (uint32) (unixMicros % 1000000);

  return 0;
}

/* This function returns forms the structure with day and month
 * as numbers taking the day number within a year.
 * Leap years are accounted for.
 * *stamp - a pointer to the structure "day + month",
 * dayInAYear - number of the day in the current year,
 * year (can be YYYY or YY, it doesn't matter)
 * An example:
 * 166 day of the year 2012 is 06/15
 */

int getDayAndMonth (dayStamp *stamp,
                    unsigned int dayInAYear,
                    unsigned int year) {

  short int tmp_acc = dayInAYear;
  // printf("Number of day in a year: %u\n", tmp_acc);
  int is_leap = COMMON_YEAR;

  if (year % 4 == 0) {
        is_leap = LEAP_YEAR;
        // printf("This year is leap!\n");
  }
  if ((!is_leap && (tmp_acc >= DAYS_IN_COMMON)) || (is_leap && (tmp_acc >= DAYS_IN_LEAP))) {
    stamp->day = 0;
    stamp->month = 0;
    return -1;
  }

  for (int i = 0; i < MONTH_NUMBER; i++) {

    // printf("In loop: i=%d, tmp_acc = %u\n", i, tmp_acc);
    char currentNumber = daysInAMonth[is_leap][i];
    if (tmp_acc > currentNumber) {
      tmp_acc -= daysInAMonth[is_leap][i];
    }
    else if (tmp_acc <= currentNumber) {
      stamp->day = tmp_acc;
      stamp->month = i + 1;
      return 0;
    }
  }
  return -1; //It can't happen though

}

/* Converting from Unix time to UTC format
 * Returns a time stamp YYYYMMDDHHMMSS00
 * Goes in pair with intToStr function to form
 * the proper time stamp
 * unixTime - input of the current time in seconds elapsed from 1/1/1970,
 * *output - a pointer to 16-byte char array with the time stamp
 * The example of conversion:
 * C9BCB377 65D90D00 - F2F0F1F2 F0F6F1F8 F0F8F2F7 F1F10000 (|20120618082711..|) */

void unixToTimestamp(uint64 unixTime, char *output) {
#ifdef __ZOWE_OS_ZOS
  //Convert unix time

  //Determine the number of days passed and seconds passed of the current day
  int dayOfThisYear = (int)(unixTime / SECS_IN_A_DAY);
  int currentSeconds = (int)(unixTime - (dayOfThisYear * SECS_IN_A_DAY));

  //Determine the number of full 4-years passed and day of the current 4-years
  int currentYear = dayOfThisYear / (DAYS_IN_LEAP + (DAYS_IN_YEAR * 3));
  dayOfThisYear -= currentYear * (DAYS_IN_LEAP + (DAYS_IN_YEAR * 3));
  currentYear *= 4;                // Convert this to the number of years

  //Determine the number of additional years and day of the year

  // Within a 4-year interval, starting from 1970 the third year will be a
  // leap year.  The table below shows the offset into each year in days.
  //
  // Year    Year Day Of Year
  // Offset  Days First   Last
  //   +0    365      0 -  364  :  year+=0   day+= 1
  //   +1    365    365 -  729  :  year+=1   day-= 365-1
  //   +2    366    730 - 1095  :  year+=2   day-= 365*2-1
  //   +3    365   1096 - 1460  :  year+=3   day-= 365*2+366-1

  if (dayOfThisYear < DAYS_IN_YEAR)
    dayOfThisYear += 1;
  else if (dayOfThisYear < (DAYS_IN_YEAR*2)) {
    currentYear += 1;
    dayOfThisYear -= (DAYS_IN_YEAR-1);
  } else if (dayOfThisYear < (DAYS_IN_YEAR*2+DAYS_IN_LEAP)) {
    currentYear += 2;
    dayOfThisYear -= (DAYS_IN_YEAR*2-1);
  } else {
    currentYear += 3;
    dayOfThisYear -= (DAYS_IN_YEAR*2+DAYS_IN_LEAP-1);
  }

  currentYear += 1970;             // Add epoch start year

  dayStamp stamp;
  int convRes = getDayAndMonth(&stamp, dayOfThisYear, currentYear);

  // Hours in a current day
  int hours = currentSeconds / SECS_IN_HOUR;
  // Minutes in a current day
  int minutes = (currentSeconds - (hours * SECS_IN_HOUR)) / 60;
  //Seconds left
  int seconds = currentSeconds - (hours * SECS_IN_HOUR) - SECS_IN_MIN * minutes;

/*
  printf("Time: %d:%d:%d Date: %d.%d.%d \n", hours, minutes, seconds, stamp.day, stamp.month, currentYear);
*/

  memset(output, 0x0, 16);
  // Copy date
  convertIntToString(output, 4, currentYear);
  convertIntToString(output + 4, 2, stamp.month);
  convertIntToString(output + 6, 2, stamp.day);

  // Copy time
  convertIntToString(output + 8, 2, hours);
  convertIntToString(output + 10, 2, minutes);
  convertIntToString(output + 12, 2, seconds);

#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_WINDOWS)
  struct tm split;
  platformGMTime((time_t*)&unixTime, &split);

  memset(output, 0x0, 16);
  // Copy date
  convertIntToString(output, 4, split.tm_year+1900);
  convertIntToString(output + 4, 2, split.tm_mon+1);
  convertIntToString(output + 6, 2, split.tm_mday);

  // Copy time
  convertIntToString(output + 8, 2, split.tm_hour);
  convertIntToString(output + 10, 2, split.tm_min);
  convertIntToString(output + 12, 2, split.tm_sec);
#else
#error Unknown OS
#endif
}

int snprintLocalTime(char *buffer, int length, int tzSource) {
#ifdef METTLE
  return 0;
#else
  struct timeval tv_s, *tv = &tv_s;
  int gtod = gettimeofday(tv, NULL);

  time_t tt = tv->tv_sec;
  struct tm result_s, *result = &result_s;
  if (tzSource == TZ_FROM_MVS) {
#ifdef __ZOWE_OS_ZOS
    long time_zone_offset_in_seconds =
        (long) (((long long * __ptr32 * __ptr32 * __ptr32) 0)[0x10 / 4 /*FLCCVT*/][0x148 / 4 /*CVTEXT2*/][0x38 / 8 /*CVTLDTO*/] /
                4096000000LL);
    tt += time_zone_offset_in_seconds;
    gmtime_r(&tt, result);
#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
    localtime_r(&tt, result);
#elif defined(__ZOWE_OS_WINDOWS)
    memcpy(result,localtime(&tt),sizeof(struct tm));
#else
#error Unknown OS
#endif
  } else if (tzSource == TZ_FROM_TZ) {
#ifndef __ZOWE_OS_WINDOWS
    localtime_r(&tt, result);
#else
    memcpy(result,localtime(&tt),sizeof(struct tm));
#endif
  } else {
    return 0;
  }
  return snprintf(buffer, length, "%04d-%02d-%02d-%02d-%02d-%02d.%06d",
                  1900 + result->tm_year, result->tm_mon + 1, result->tm_mday,
                  result->tm_hour, result->tm_min, result->tm_sec,
                  (int)tv->tv_usec);
#endif
}

/* test code moved to COMMON/test/timetest.c */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

