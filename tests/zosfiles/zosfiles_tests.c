#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "../../h/unixfile.h"
#include "test_utils.h"

int directoryListEntries_should_return_expected_entry_list(void) {
  int rc;
  int rsn;
  UnixDirectoryEntries entries;
  directoryListEntries("dir2", &entries, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  EXPECT_INTEGER_EQUAL(entries.numEntries, 3);
  EXPECT_STRING_ARRAY_CONTAINS(entries.entryArray, entries.numEntries, "file1");
  EXPECT_STRING_ARRAY_CONTAINS(entries.entryArray, entries.numEntries, "file2");
  EXPECT_STRING_ARRAY_CONTAINS(entries.entryArray, entries.numEntries, "dir2_1");
  RETURN_SUCCEEDED;
}

int fileReadLink2_should_return_expected_type(void) {
  int rc;
  int rsn;
  ReadLinkResult result;
  fileReadLink2("dir1", &result, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  EXPECT_EQUAL(result.type, RL2_TypeRealDir);
  fileReadLink2("dir1/file1", &result, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  EXPECT_INTEGER_EQUAL(result.type, RL2_TypeRealFile);
  fileReadLink2("dir2/file1", &result, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  EXPECT_INTEGER_EQUAL(result.type, RL2_TypeSymlink);
  EXPECT_STRING_EQUAL(result.realPath, "../dir1/file1");
  fileReadLink2("dir2/dir2_1/dir1", &result, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  EXPECT_INTEGER_EQUAL(result.type, RL2_TypeSymlink);
  EXPECT_STRING_EQUAL(result.realPath, "../../dir1");
  RETURN_SUCCEEDED;
}

static int captureDirectoryShape(const char *dirName, char *buffer, size_t bufferSize) {
  char cmd[2048];
  snprintf(cmd, sizeof(cmd), "cd \"%s\" && find . -exec ls -ld {} \\; | awk '{print $9 $10 $11}' | sort", dirName);
  return captureCommandOutput(cmd, buffer, bufferSize);
}

int directoryCopy_should_create_identical_dir3_from_dir2(void) {
  char dir2Shape[4096];
  char dir3Shape[4096];
  int rc;
  int rsn;

  if ((rc = captureCommandOutput("rm -rf dir3", NULL, 0)) != 0) print_failure(__FILE__, __LINE__, "Failed to rm dir3, err: %d\n", errno);
  EXPECT_INTEGER_EQUAL(rc, 0);

  directoryCopy("dir2", "dir3", &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);

  if (captureDirectoryShape("dir2", dir2Shape, sizeof(dir2Shape)) != 0) {
    print_failure(__FILE__, __LINE__, "failed to inspect dir2");
    return 1;
  }
  if (captureDirectoryShape("dir3", dir3Shape, sizeof(dir3Shape)) != 0) {
    print_failure(__FILE__, __LINE__, "failed to inspect dir3");
    return 1;
  }
  EXPECT_STRING_EQUAL(dir3Shape, dir2Shape);

  RETURN_SUCCEEDED;
}

int directoryChangeModeRecursive_should_set_mode(void) {
  int rc;
  int rsn;
  int mode777 = 00777;
  directoryChangeModeRecursive("dir3/dir2_1", 1, mode777, NULL, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  FileInfo info;
  fileInfo("dir3/dir2_1/file3", &info, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  EXPECT_INTEGER_EQUAL(fileUnixMode(&info), mode777);
  fileInfo("dir1", &info, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  EXPECT_INTEGER_EQUAL(fileUnixMode(&info), mode777);
  fileInfo("dir1/file1", &info, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  EXPECT_INTEGER_EQUAL(fileUnixMode(&info), mode777);
  RETURN_SUCCEEDED;
}

int directoryChangeOwnerRecursive_should_change_owner(void) {
  int rc;
  int rsn;

  const char *userIDName = "ZOSFILES_UT_USERID";
  const char *userSID = getenv(userIDName);
  int userID = userSID ? (int) strtol(userSID, NULL, 10) : -1;
  printf("$%s: %s\n", userIDName, userSID ? userSID : "not present, will use -1 by default");

  const char *groupIDName = "ZOSFILES_UT_GRPID";
  const char *groupSID = getenv(groupIDName);
  int groupID = groupSID ? (int) strtol(groupSID, NULL, 10) : -1;
  printf("$%s: %s\n", groupIDName, groupSID ? groupSID : "not present, will use -1 by default");
  
  if (userID < 0 && groupID < 0) {
    puts("Both user ID and group ID are -1, which means no change, change owner test will be skipped");
    return 0;
  }
  directoryChangeOwnerRecursive(NULL, 0, "dir3/dir2_1", userID, groupID, TRUE, NULL, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  FileInfo info;
  fileInfo("dir3/dir2_1/", &info, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  if (userID >= 0) {
    EXPECT_INTEGER_EQUAL(info.ownerUID, userID);
  }
  if (groupID >= 0) {
    EXPECT_INTEGER_EQUAL(info.ownerGID, groupID);
  }
  fileInfo("dir3/dir2_1/file3", &info, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  if (userID >= 0) {
    EXPECT_INTEGER_EQUAL(info.ownerUID, userID);
  }
  if (groupID >= 0) {
    EXPECT_INTEGER_EQUAL(info.ownerGID, groupID);
  }
  RETURN_SUCCEEDED;
}

int directoryDelete_should_delete_dir3(void) {
  int rc;
  int rsn;
  directoryDeleteRecursive("dir3", &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  FileInfo info;
  fileInfo("dir3", &info, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, ENOENT);
  fileInfo("dir1", &info, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  fileInfo("dir1/file1", &info, &rc, &rsn);
  EXPECT_INTEGER_EQUAL(rc, 0);
  EXPECT_INTEGER_EQUAL(rsn, 0);
  RETURN_SUCCEEDED;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    puts("Test directory not specified");
    return 1;
  }
  if (chdir(argv[1]) != 0) {
    perror("Failed to enter the specified directory.");
    return 1;
  }

  BEFORE_TESTS;
  RUN_TEST_CASE(directoryListEntries_should_return_expected_entry_list);
  RUN_TEST_CASE(fileReadLink2_should_return_expected_type);
  RUN_TEST_CASE(directoryCopy_should_create_identical_dir3_from_dir2);
  RUN_TEST_CASE(directoryChangeModeRecursive_should_set_mode);
  RUN_TEST_CASE(directoryChangeOwnerRecursive_should_change_owner);
  RUN_TEST_CASE(directoryDelete_should_delete_dir3);
  FINISH_TESTS;
}