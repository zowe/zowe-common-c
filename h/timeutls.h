

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __TIMEUTLS__
#define __TIMEUTLS__ 1

#include "zowetypes.h"

/* Constants for Unix time conversion */
// Correct number of seconds in a year with 365 days
#define SECS_IN_A_YEAR 31536000
#define SECS_IN_A_DAY 86400
#define DAYS_IN_YEAR 365
#define DAYS_BEFORE_2902 59
#define SECS_IN_HOUR 3600
#define MINS_IN_HOUR 60
#define SECS_IN_MIN 60

/* Here go some common constants for working with days-moths-years */
#define MONTH_NUMBER 12

#define LEAP_YEAR 1
#define COMMON_YEAR 0

#define DAYS_IN_COMMON 365
#define DAYS_IN_LEAP 366

/* The structure for day-month couple representation */
typedef struct dayMonth {

  char day;
  char month;

} dayStamp;

#ifndef __LONGNAME__
#define getSTCK GETSTCK
#define getSTCKU GETSTCKU
#define convertTODToLocal CONVTODL
#define timeZoneDifferenceFor TZDIFFOR
#define stckToTimestamp STCKCONV
#define timestampToSTCK CONVTOD
#define timeFromMidnight MIDNIGHT
#define stckFromYYYYMMDD STCKYYMD
#define elapsedTime ELPSTIME
#define stckToUnix STCKUNIX
#define stckToUnixSecondsAndMicros STCK2USM
#define unixToTimestamp CONVUNIX
#define getDayAndMonth GTDAYMNT
#define snprintLocalTime SNPRNTLT
#endif

/*
  See the Principles of Operation, Chapter 4 for details:

  http://www-01.ibm.com/support/docview.wss?uid=pub1sa22783209

  All 370-and-later machines have a "time of day" (TOD) clock,
  available to problem-state programs via the STCK (Store Clock)
  instruction (and relatives). The clock was originally a 64-bit
  counter, with bit 51 incremented every microsecond. Bits to the
  right (lower order) of bit 51 may be implemented, providing
  fractions of a microsecond. Bit 31 is therefore incremented
  every 2^20 microseconds, or every 1.048576 seconds. The clock
  has a maximum period before overflow of about 143 years.

  Later models expanded the size of the TOD clock on "both ends",
  to be a 128-bit counter; 8 high-order bits (to the "left") and
  56 low-order bits (to the "right) were added. Nonetheless,
  64-bit TOD values will continue to be used as timestamps for
  the immediate future, and this code doesn't (currently) attempt
  to handle that.

  By convention, time 0 (the start of the "360 Epoch") is:

    00:00:00 (0 am), UTC time, January 1, 1900

  This means that the 64-bit TOD clock will roll over around 2043.

  Note that TOD clock values are ALWAYS unsigned; it is impossible to
  represent a timeprior to the start of the 360 Epoch.

  The "Unix Epoch", OTOH, began at:

    00:00:00 (0 am), UTC time, January 1, 1970

  Unix systems generally measure time relative to that date
  using this struct (or a similar one):

    struct timespec {
        time_t   tv_sec;        // seconds
        long     tv_nsec;       // nanoseconds
    };

  time_t is (nowadays) a SIGNED, 64-bit integer, so the range is
  effectively unlimited. And since it is signed, it IS possible to
  represent times PRIOR to the start of the Unix Epoch. This means
  that it's possible to have a Unix time_t that cannot be represented
  as a TOD clock value. At least in the Linux implementations, an
  effort will be made to return TOD values of zero in such cases.

  (Aside: both tv_sec and tv_nsec are signed. It's very unclear from
  the man pages how times prior to the Unix Epoch are handled: is
  tv_usec negative? and if so, what exactly does that mean? I'm
  currently going to just ignore this whole issue and hopefully come
  up with functions that at least work for times AFTER the start of
  the Unix epoch.)

  For consistency's sake, the Linux implementations of functions that
  return a TOD clock value (such as getSTCK) will (of course) return a
  value in the 360 Epoch.

  Someday (prior to 2043...) this will all have to be revisited.

 */

void getSTCK(int64 *stck);
void getSTCKU(uint64 *stck);

/*
  Convert the TOD to local time, using TODAY's differential from
  UTC. Note that this is pretty bogus; it probably ought to be done
  using the differential that applies to the date of the TOD (to
  make sure that the DST adjustment is correct).
 */
void convertTODToLocal(int64 *stck);

/*
  The "timestamp" type for stckToTimestamp and timestampToSTCK
  is a 32-digit packed decimal (aka BCD) number with this form
  (each character representing a single packed digit):

     0  1  2  3 4 5  6 7  8 9  A  B  C  D  E
    HH MM SS UUUUUU 0000 YYYY mm dd 00000000

    HH      Hours (24-hour clock)
    MM      Minutes
    SS      seconds
    UUUUUU  microseconds
    YYYY    4-digit year
    mm      month (1-12)
    dd      day (1-31)

   An example:

    18060758 19300000 20161116 00000000

   This is the format produced by the MVS "STCKCONV" macro. See:

   z/OS MVS Programming: Assembler Services Reference IAR-XCT,
   STCKCONV - Store clock conversion routine

   http://www.ibm.com/support/knowledgecenter/SSLTBW_2.1.0/com.ibm.zos.v2r1.ieaa900/iea3a9_Description14.htm

 */

/*
  Compute the difference, in seconds, between UTC and local time (for
  wherever we happen to be...) for the specified Unix time. This may
  or may not be implemented; if not implemented, -1 will be returned
  (since there's never an offset of -1, anywhere or anytime).
 */
int timeZoneDifferenceFor(int64 theTime);

#define TIMESTAMP_LENGTH 16

/*
  Convert an 8-byte (64-bit) TOD clock value to a timestamp.
 */
int stckToTimestamp(int64 stck, char *output);

/* Converts a 16-byte timestamp to a TOD clock value
 * *todText - a pointer to 16-byte input text time stamp
 * *stck - a pointer to output TOD value
 * offset - a TOD value which denotes local offset of the given input time stamp
 * and is subtracted from the result TOD in order to get the output in GMT
 *
 * If successful, 0 is returned; otherwise some non-zero value will be returned.
 */
int timestampToSTCK(char *todText, int64 *stck, int64 offset);

int timeFromMidnight(int64 stckPtr);
int stckFromYYYYMMDD(int dummy0, int64 *stck,
         int dummy1, int year,
         int dummy2, int month,
         int dummy3, int date,
         int dummy4, int offset);
int elapsedTime(int dummy0, int64 *stck,
    int dummy1, int days,
    int dummy2, int hours,
    int dummy3, int minutes,
    int dummy4, int seconds);

/*
  This converts a TOD value to a Unix time_t value, which is the number of
  seconds relative to the start of the Unix Epoch.
 */
int64 stckToUnix(int64 stck);


/*
  This converts a TOD value to the components of an old Unix struct timeval.
  More recent Unix systems use uint64 for these components (to solve the
  "year 2038 problem").
 */
int stckToUnixSecondsAndMicros(int64 stck,
                               uint32 *out_secs, uint32 *out_micros);

/*
   Hmm. The "timestamp" here is a different format from the one described
   above. This one really is text (not packed decimal), and has this format:

     YYYYMMDDHHMMSS\0\0 (16 characters, the last two of which are nulls)

   Input is a Unix time_t value (seconds since the Unix Epoch).
 */
void unixToTimestamp(uint64 unixTime, char *output);

int getDayAndMonth (dayStamp *stamp,
                    unsigned int dayInAYear,
                    unsigned int year);

#define TZ_FROM_MVS 0
#define TZ_FROM_TZ  1

int snprintLocalTime(char *buffer, int length, int tzSource);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

