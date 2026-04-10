/*
 * ztso_pipe - Simple TSO command pipe server
 *
 * Reads one TSO command per line from stdin.
 * Executes via tsocmd.
 * Writes all output to stdout.
 * Writes a sentinel line after each command completes.
 *
 * Sentinel format:  @@ZTSO_END RC=nnn
 *
 * EOF on stdin terminates.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CMD 1024
#define MAX_LINE 4096
#define SENTINEL "@@ZTSO_END"

int main() {
  char cmdBuf[MAX_CMD];
  char lineBuf[MAX_LINE];
  char popenCmd[MAX_CMD + 32];

  setvbuf(stdout, NULL, _IOLBF, 0);

  while (fgets(cmdBuf, sizeof(cmdBuf), stdin)) {
    int len = strlen(cmdBuf);
    while (len > 0 && (cmdBuf[len-1] == '\n' || cmdBuf[len-1] == '\r'))
      cmdBuf[--len] = '\0';

    if (len == 0)
      continue;

    snprintf(popenCmd, sizeof(popenCmd), "tsocmd \"%s\" 2>&1", cmdBuf);

    FILE *p = popen(popenCmd, "r");
    if (!p) {
      printf("%s RC=-1 POPEN_FAILED\n", SENTINEL);
      fflush(stdout);
      continue;
    }

    while (fgets(lineBuf, sizeof(lineBuf), p)) {
      fputs(lineBuf, stdout);
    }

    int rc = pclose(p);
    int exitCode = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;

    printf("%s RC=%d\n", SENTINEL, exitCode);
    fflush(stdout);
  }

  return 0;
}
