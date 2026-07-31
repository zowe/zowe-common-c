#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "zowetypes.h"
#include "impersonation.h"

/*
  64-bits:
  xlclang -q64 -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DSUBPOOL=132 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" "-Wl,ac=1" -I ../h -I ../platform/posix -Wbitwise-op-parentheses -o tlsimpersontest tlsimpersontest.c ../c/impersonation.c ../c/zos.c ../c/utils.c ../c/alloc.c ../c/timeutls.c

  31-bits:
  xlclang -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 -DSUBPOOL=132 "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" "-Wl,ac=1" -I ../h -I ../platform/posix -Wbitwise-op-parentheses -o tlsimpersontest31 tlsimpersontest.c ../c/impersonation.c ../c/zos.c ../c/utils.c ../c/alloc.c ../c/timeutls.c

  Testing: (you need READ access to FACILITY(BPX.FILEATTR.PROGCTL, BPX.FILEATTR.APF))
  
  TOM@LPAR01: extattr +ap tlsimpersontest
  TOM@LPAR01: ./tlsimpersontest -s JERRY HISPASS
  
  In supervisor state
  ------------------------
  Before impersonation:
  ACEE user name: 'TOM' (from address-space)
  ------------------------
  After impersonation:
  ACEE user name: 'JERRY' (from task)
  ------------------------
  After ending impersonation:
  ACEE user name: 'TOM' (from address-space)
  ------------------------
  In supervisor state
  ./
*/

typedef struct {
  int trace;
  int initSupervisor;
  char *user;
  char *password;
} params;

void usage(const char *argv0) {
  fprintf(stderr, "Usage: %s [-t] [-s] user password\n", argv0);
}

params parseArgs(int argc, char* argv[]) {
  params p;
  memset(&p, 0, sizeof(params));
  int opt;
  while ((opt = getopt(argc, argv, "ts")) != -1) {
    switch (opt) {
    case 't': p.trace = 1; break;
    case 's': p.initSupervisor = 1; break;
    default:  usage(argv[0]); exit(1);
    }
  }
  if (optind + 2 > argc) {
    usage(argv[0]);
    exit(1);
  }
  p.user = argv[optind];
  p.password = argv[optind + 1];
  return p;
}

void printAceeUserName(void) {
  int fromTaskAcee = 1;
  ACEE *acee = getTaskAcee();
  if (acee == NULL) {
    fromTaskAcee = 0;
    acee = getCurrentACEE();
  }
  if (acee == NULL) {
    fputs("No ACEE available", stderr);
    return;
  }
  int len = (unsigned char) acee->aceeuser[0];
  printf("ACEE user name: '%.*s' (from %s)\n", len, acee->aceeuser + 1, fromTaskAcee ? "task" : "address-space");
}

#define PROBLEM_STATE 0x00010000
void printState(void) {
    puts(extractPSW() & PROBLEM_STATE ? "In problem state" : "In supervisor state");
}

int main(int argc, char* argv[]) {
  params p = parseArgs(argc, argv);

  if (p.initSupervisor) {
    supervisorMode(TRUE);
  }

  printState();
  puts("------------------------");

  puts("Before impersonation:");
  printAceeUserName();
  puts("------------------------");

  if (tlsImpersonate(p.user, p.password, 1, p.trace) != 0) {
    fputs("Impersonation failed", stderr);
    return 1;
  }

  puts("After impersonation:");
  printAceeUserName();
  puts("------------------------");

  tlsImpersonate(p.user, p.password, 0, p.trace);
  puts("After ending impersonation:");
  printAceeUserName();
  puts("------------------------");

  printState();

  return 0;
}