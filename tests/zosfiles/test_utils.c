#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include "../../h/zowetypes.h"
#include "test_utils.h"

/*
  Assertion utilities
*/
int valcomp_memcmp(const void *arrayItem, const void *value, int size) {
  return memcmp(arrayItem, value, size);
}

int valcomp_strcmp(const void *arrayItem, const void *value, int size) {
  return strcmp(*((char**)arrayItem), value);
}

int array_contains(const void *array, const void *value, int array_length, int value_size, int (*comp)(const void *, const void *, int)) {
  const char *a = (const char *)array;
  for (int i = 0; i < array_length; i++, a += value_size)
  {
    if (0 == comp(a, value, value_size))
    {
      return i;
    }
  }
  return -1;
}

/*
  Printing utilities
*/

void print_failure(const char *file, int line, const char *msg, ...) {
  printf("[FAILED] %s (%d): ", file, line);
  va_list ap;
  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);
  puts("");
}

void print_results(int total, int failed) {
  printf("Done. Total: %d. Failed:%d.\n", total, failed);
}

void print_case_name(const char *name) {
  printf("Running %s...\n", name);
}

/*
  Shell exec utilities
*/
int captureCommandOutput(const char *cmd, char *buffer, size_t bufferSize) {
  if (buffer && bufferSize > 0) {
    *buffer = '\0';
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
      return -1;
    }
    size_t total = 0, n;
    while (total < bufferSize - 1 &&
          (n = fread(buffer + total, 1, bufferSize - 1 - total, fp)) > 0) {
      total += n;
    }
    buffer[total] = '\0';
    return pclose(fp);
  } else {
    return system(cmd);
  }
}

/*
  Fake stub functions --- they are part of zowe-common-c but unrelated to/unused by the
  current unit tests. These fake stub functions can noticeably simplify the Makefile.
*/
void unixToTimestamp(uint64 unixTime, char *output) {}
void abortIfUnsupportedCAA() {}
char *getCAA(void) { return ""; }