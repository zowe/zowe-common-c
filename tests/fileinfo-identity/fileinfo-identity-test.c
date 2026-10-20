/* fileinfo-identity-test.c -- fileGetINode() / fileGetDeviceID() over FileInfo.
 *
 * The (inode, deviceID) pair is how callers establish that two paths name the
 * same file. ZSS keys its upload-session table on exactly that pair, and
 * httpserver.c's makeFileEtag() folds the inode into an ETag. Both accessors
 * are declared in h/unixfile.h for every platform, so the contract this checks
 * is the shared one:
 *
 *   1. a real file has a non-zero inode and device
 *   2. two distinct files differ in inode, and agree on device when they sit
 *      on the same filesystem
 *   3. a hard link is the SAME file -- both halves of the pair match
 *   4. the values are stable across repeated fileInfo() calls
 *
 * Setup uses raw POSIX calls (available on z/OS USS too) so that a failure
 * points at the accessors rather than at some other part of the library.
 *
 * Linux/WSL:  sh tests/fileinfo-identity/run.sh       (adds ASan/UBSan)
 * z/OS:       sh tests/fileinfo-identity/run-zos.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "zowetypes.h"
#include "unixfile.h"

static int failures = 0;
static int checks = 0;

static void check(int condition, const char *what, const char *detail) {
  checks++;
  if (condition) {
    printf("    ok    %s\n", what);
  } else {
    printf("    FAIL  %s\n", what);
    if (detail != NULL) {
      printf("          %s\n", detail);
    }
    failures++;
  }
}

/* Creates a file with one line of content. Returns 0 on success. */
static int makeFile(const char *path) {
  int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
  if (fd < 0) {
    return -1;
  }
  if (write(fd, "zowe", 4) != 4) {
    close(fd);
    return -1;
  }
  return close(fd);
}

static int infoOf(const char *path, FileInfo *info) {
  int returnCode = 0, reasonCode = 0;
  int status = fileInfo(path, info, &returnCode, &reasonCode);
  if (status != 0) {
    printf("    fileInfo(\"%s\") failed: status=%d returnCode=%d reasonCode=0x%x\n",
           path, status, returnCode, reasonCode);
  }
  return status;
}

int main(int argc, char **argv) {
  const char *dir = (argc > 1) ? argv[1] : ".";
  char pathA[1024], pathB[1024], pathLink[1024];
  char detail[256];

  snprintf(pathA,    sizeof(pathA),    "%s/fiid-a.tmp",    dir);
  snprintf(pathB,    sizeof(pathB),    "%s/fiid-b.tmp",    dir);
  snprintf(pathLink, sizeof(pathLink), "%s/fiid-link.tmp", dir);

  /* start clean in case an earlier run died partway */
  unlink(pathLink); unlink(pathB); unlink(pathA);

  printf("fileGetINode / fileGetDeviceID over FileInfo\n");
  printf("  working directory: %s\n\n", dir);

  if (makeFile(pathA) != 0 || makeFile(pathB) != 0) {
    printf("  SETUP FAILED: could not create test files in %s\n", dir);
    printf("  (pass a writable directory as argv[1])\n");
    return 2;
  }
  if (link(pathA, pathLink) != 0) {
    printf("  SETUP FAILED: could not hard-link %s -> %s\n", pathA, pathLink);
    unlink(pathB); unlink(pathA);
    return 2;
  }

  FileInfo infoA = {0}, infoB = {0}, infoLink = {0}, infoAAgain = {0};
  if (infoOf(pathA, &infoA) != 0 || infoOf(pathB, &infoB) != 0 ||
      infoOf(pathLink, &infoLink) != 0 || infoOf(pathA, &infoAAgain) != 0) {
    unlink(pathLink); unlink(pathB); unlink(pathA);
    return 2;
  }

  int inodeA = fileGetINode(&infoA);
  int inodeB = fileGetINode(&infoB);
  int inodeLink = fileGetINode(&infoLink);
  int inodeAAgain = fileGetINode(&infoAAgain);
  int deviceA = fileGetDeviceID(&infoA);
  int deviceB = fileGetDeviceID(&infoB);
  int deviceLink = fileGetDeviceID(&infoLink);

  printf("  %-14s %-12s %-12s\n", "path", "inode", "deviceID");
  printf("  %-14s %-12d %-12d\n", "fiid-a.tmp",    inodeA,    deviceA);
  printf("  %-14s %-12d %-12d\n", "fiid-b.tmp",    inodeB,    deviceB);
  printf("  %-14s %-12d %-12d\n", "fiid-link.tmp", inodeLink, deviceLink);
  printf("\n");

  printf("  case 1: a real file has a non-zero identity\n");
  check(inodeA != 0, "inode of fiid-a.tmp is non-zero", NULL);
  check(deviceA != 0, "deviceID of fiid-a.tmp is non-zero", NULL);

  printf("  case 2: distinct files are distinguishable\n");
  snprintf(detail, sizeof(detail), "both files reported inode %d", inodeA);
  check(inodeA != inodeB, "fiid-a.tmp and fiid-b.tmp have different inodes", detail);
  snprintf(detail, sizeof(detail), "a=%d b=%d, but they share a directory",
           deviceA, deviceB);
  check(deviceA == deviceB, "same-filesystem files share a deviceID", detail);

  printf("  case 3: a hard link is the same file\n");
  snprintf(detail, sizeof(detail), "a=%d link=%d", inodeA, inodeLink);
  check(inodeA == inodeLink, "hard link has the inode of its target", detail);
  snprintf(detail, sizeof(detail), "a=%d link=%d", deviceA, deviceLink);
  check(deviceA == deviceLink, "hard link has the deviceID of its target", detail);

  printf("  case 4: the identity is stable\n");
  snprintf(detail, sizeof(detail), "first=%d second=%d", inodeA, inodeAAgain);
  check(inodeA == inodeAAgain, "a second fileInfo() reports the same inode", detail);

  unlink(pathLink);
  unlink(pathB);
  unlink(pathA);

  printf("\n%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
