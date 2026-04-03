
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * tests/unit/zosaccounts/zosaccountstest.c
 *
 * Unit tests for zosaccounts.h / zosaccounts.c.
 *
 * All tests operate on the real z/OS RACF/SAF database via the BPX callable
 * services.  No mocking is used; each test drives the functions with values
 * that are verifiably true for any valid UNIX user: the UID and primary GID
 * of the process that runs the binary (obtained via getuid() / getgid()), and
 * the user name obtained by resolving that UID through getUserInfo() itself.
 *
 * Functions explicitly NOT tested
 * --------------------------------
 *   resetZosUserPassword  - deliberately excluded (per test requirements).
 *   userIdGet  (numeric string path) - a bug exists in the implementation:
 *       `userId = atoi(userId)` should be `userId = atoi(string)`.  The
 *       numeric branch is avoided to prevent undefined behaviour.
 *
 * Compile on z/OS (xlclang, lp64) -- see tests/Makefile target
 * "test_zosaccounts".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* getuid(), getgid() */

#include "zowetypes.h"
#include "alloc.h"
#include "zosaccounts.h"
#include "zowetests.h"

/* ===================================================================
 *  Module-wide state
 *
 *  Populated once in main() before the test suites run, using the
 *  identity of the process itself as a known-good anchor.
 * =================================================================== */

static int  s_uid  = -1;  /* current process UID  */
static int  s_gid  = -1;  /* current process GID  */

/*
 * User name and group name retrieved at startup.  The name buffers are
 * intentionally larger than USER_NAME_LEN / GROUP_NAME_LEN because
 * userInfoGetUserName() and groupInfoGetGroupName() do NOT null-terminate;
 * the extra bytes (zeroed by the initializer) act as a guaranteed sentinel.
 */
static char s_userName[USER_NAME_LEN + 8];    /* e.g. "IBMUSER\0\0..." */
static char s_groupName[GROUP_NAME_LEN + 8];

static int  s_setupOk = 0;   /* set to 1 when startup lookup succeeds */

/* ===================================================================
 *  Suite 1 - getUserInfo  (lookup by UID)
 * =================================================================== */

static void testGetUserInfo(void) {
  DESCRIBE("getUserInfo - look up UserInfo by UID") {

    IT("returns 0 for the current process UID") {
      TEST_COVERS(getUserInfo);
      if (!s_setupOk) { SKIP("startup user lookup failed"); }
      UserInfo info = {0};
      int rc = 0, rsn = 0;
      int status = getUserInfo(s_uid, &info, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
    } IT_END

    IT("returns -1 for a UID that is extremely unlikely to exist") {
      TEST_COVERS(getUserInfo);
      UserInfo info = {0};
      int rc = 0, rsn = 0;
      /* UID 2147483647 (INT_MAX) is vanishingly unlikely to be defined. */
      int status = getUserInfo(2147483647, &info, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, status);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 2 - gidGetUserInfo  (lookup by user name)
 * =================================================================== */

static void testGidGetUserInfo(void) {
  DESCRIBE("gidGetUserInfo - look up UserInfo by user name") {

    IT("returns 0 for the current user's name") {
      TEST_COVERS(gidGetUserInfo);
      if (!s_setupOk) { SKIP("startup user lookup failed"); }
      UserInfo info = {0};
      int rc = 0, rsn = 0;
      int status = gidGetUserInfo(s_userName, &info, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
    } IT_END

    IT("returns -1 for a user name that cannot exist") {
      TEST_COVERS(gidGetUserInfo);
      /* Longer-than-legal name; RACF rejects it. */
      UserInfo info = {0};
      int rc = 0, rsn = 0;
      int status = gidGetUserInfo("NOSUCHUSER99", &info, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, status);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 3 - userInfoGetUserId / userInfoGetUserName
 * =================================================================== */

static void testUserInfoAccessors(void) {
  DESCRIBE("userInfoGetUserId / userInfoGetUserName") {

    IT("userInfoGetUserId returns the same UID as getuid()") {
      TEST_COVERS(userInfoGetUserId);
      if (!s_setupOk) { SKIP("startup user lookup failed"); }
      UserInfo info = {0};
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, getUserInfo(s_uid, &info, &rc, &rsn));
      int extractedUid = userInfoGetUserId(&info);
      ASSERT_EQUAL_INT(s_uid, extractedUid);
    } IT_END

    IT("userInfoGetUserName produces the name used to resolve the same UID") {
      TEST_COVERS(userInfoGetUserName);
      if (!s_setupOk) { SKIP("startup user lookup failed"); }
      /*
       * Look up info by UID, extract the name, then look up info by that
       * name and verify its UID matches.  This round-trips both directions
       * without depending on any external string.
       */
      UserInfo info = {0};
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, getUserInfo(s_uid, &info, &rc, &rsn));

      /* Buffer is zero-initialised; userInfoGetUserName does NOT null-terminate. */
      char nameBuf[USER_NAME_LEN + 8];
      memset(nameBuf, 0, sizeof(nameBuf));
      userInfoGetUserName(&info, nameBuf);

      /* Extracted name must be non-empty. */
      ASSERT_GT_INT((int)strlen(nameBuf), 0);

      /* Round-trip: look up info by name, UID must agree. */
      UserInfo info2 = {0};
      rc = rsn = 0;
      ASSERT_EQUAL_INT(0, gidGetUserInfo(nameBuf, &info2, &rc, &rsn));
      ASSERT_EQUAL_INT(s_uid, userInfoGetUserId(&info2));
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 4 - getGroupInfo  (lookup by GID)
 * =================================================================== */

static void testGetGroupInfo(void) {
  DESCRIBE("getGroupInfo - look up GroupInfo by GID") {

    IT("returns 0 for the current process primary GID") {
      TEST_COVERS(getGroupInfo);
      if (!s_setupOk) { SKIP("startup group lookup failed"); }
      GroupInfo info = {0};
      int rc = 0, rsn = 0;
      int status = getGroupInfo(s_gid, &info, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
    } IT_END

    IT("returns -1 for a GID that is extremely unlikely to exist") {
      TEST_COVERS(getGroupInfo);
      GroupInfo info = {0};
      int rc = 0, rsn = 0;
      int status = getGroupInfo(2147483647, &info, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, status);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 5 - gidGetGroupInfo  (lookup by group name)
 * =================================================================== */

static void testGidGetGroupInfo(void) {
  DESCRIBE("gidGetGroupInfo - look up GroupInfo by group name") {

    IT("returns 0 for the current user's primary group name") {
      TEST_COVERS(gidGetGroupInfo);
      if (!s_setupOk) { SKIP("startup group lookup failed"); }
      GroupInfo info = {0};
      int rc = 0, rsn = 0;
      int status = gidGetGroupInfo(s_groupName, &info, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
    } IT_END

    IT("returns -1 for a group name that cannot exist") {
      TEST_COVERS(gidGetGroupInfo);
      GroupInfo info = {0};
      int rc = 0, rsn = 0;
      int status = gidGetGroupInfo("NOSUCHGRP99X", &info, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, status);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 6 - groupInfoGetGroupId / groupInfoGetGroupName
 * =================================================================== */

static void testGroupInfoAccessors(void) {
  DESCRIBE("groupInfoGetGroupId / groupInfoGetGroupName") {

    IT("groupInfoGetGroupId returns the same GID as getgid()") {
      TEST_COVERS(groupInfoGetGroupId);
      if (!s_setupOk) { SKIP("startup group lookup failed"); }
      GroupInfo info = {0};
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, getGroupInfo(s_gid, &info, &rc, &rsn));
      int extractedGid = groupInfoGetGroupId(&info);
      ASSERT_EQUAL_INT(s_gid, extractedGid);
    } IT_END

    IT("groupInfoGetGroupName produces the name used to resolve the same GID") {
      TEST_COVERS(groupInfoGetGroupName);
      if (!s_setupOk) { SKIP("startup group lookup failed"); }
      GroupInfo info = {0};
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, getGroupInfo(s_gid, &info, &rc, &rsn));

      /* Buffer is zero-initialised; groupInfoGetGroupName does NOT null-terminate. */
      char nameBuf[GROUP_NAME_LEN + 8];
      memset(nameBuf, 0, sizeof(nameBuf));
      groupInfoGetGroupName(&info, nameBuf);

      ASSERT_GT_INT((int)strlen(nameBuf), 0);

      /* Round-trip: look up info by name, GID must agree. */
      GroupInfo info2 = {0};
      rc = rsn = 0;
      ASSERT_EQUAL_INT(0, gidGetGroupInfo(nameBuf, &info2, &rc, &rsn));
      ASSERT_EQUAL_INT(s_gid, groupInfoGetGroupId(&info2));
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 7 - userGetName  (resolve UID → name)
 * =================================================================== */

static void testUserGetName(void) {
  DESCRIBE("userGetName - resolve UID to a user name string") {

    IT("returns 0 and a non-empty name for the current process UID") {
      TEST_COVERS(userGetName);
      if (!s_setupOk) { SKIP("startup user lookup failed"); }
      char nameBuf[USER_NAME_LEN + 8];
      memset(nameBuf, 0, sizeof(nameBuf));
      int rc = 0, rsn = 0;
      int status = userGetName(s_uid, nameBuf, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
      ASSERT_GT_INT((int)strlen(nameBuf), 0);
    } IT_END

    IT("resolved name matches the one obtained via getUserInfo + userInfoGetUserName") {
      TEST_COVERS(userGetName);
      if (!s_setupOk) { SKIP("startup user lookup failed"); }
      char viaGetName[USER_NAME_LEN + 8];
      memset(viaGetName, 0, sizeof(viaGetName));
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, userGetName(s_uid, viaGetName, &rc, &rsn));
      ASSERT_EQUAL_STR(s_userName, viaGetName);
    } IT_END

    IT("returns -1 for a UID that is extremely unlikely to exist") {
      TEST_COVERS(userGetName);
      char nameBuf[USER_NAME_LEN + 8];
      memset(nameBuf, 0, sizeof(nameBuf));
      int rc = 0, rsn = 0;
      int status = userGetName(2147483647, nameBuf, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, status);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 8 - groupGetName  (resolve GID → name)
 * =================================================================== */

static void testGroupGetName(void) {
  DESCRIBE("groupGetName - resolve GID to a group name string") {

    IT("returns 0 and a non-empty name for the current primary GID") {
      TEST_COVERS(groupGetName);
      if (!s_setupOk) { SKIP("startup group lookup failed"); }
      char nameBuf[GROUP_NAME_LEN + 8];
      memset(nameBuf, 0, sizeof(nameBuf));
      int rc = 0, rsn = 0;
      int status = groupGetName(s_gid, nameBuf, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
      ASSERT_GT_INT((int)strlen(nameBuf), 0);
    } IT_END

    IT("resolved name matches the one obtained via getGroupInfo + groupInfoGetGroupName") {
      TEST_COVERS(groupGetName);
      if (!s_setupOk) { SKIP("startup group lookup failed"); }
      char viaGetName[GROUP_NAME_LEN + 8];
      memset(viaGetName, 0, sizeof(viaGetName));
      int rc = 0, rsn = 0;
      ASSERT_EQUAL_INT(0, groupGetName(s_gid, viaGetName, &rc, &rsn));
      ASSERT_EQUAL_STR(s_groupName, viaGetName);
    } IT_END

    IT("returns -1 for a GID that is extremely unlikely to exist") {
      TEST_COVERS(groupGetName);
      char nameBuf[GROUP_NAME_LEN + 8];
      memset(nameBuf, 0, sizeof(nameBuf));
      int rc = 0, rsn = 0;
      int status = groupGetName(2147483647, nameBuf, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, status);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 9 - userIdGet  (name-string → UID conversion)
 *
 *  NOTE: The implementation contains a bug in the numeric-string branch:
 *
 *      userId = atoi(userId);  /* should be atoi(string) */
 *
 *  Passing an int to atoi() as a char* is undefined behaviour.  The numeric
 *  path is therefore not exercised here to avoid a crash.  Only the
 *  name-string and NULL paths are tested.
 * =================================================================== */

static void testUserIdGet(void) {
  DESCRIBE("userIdGet - convert user name to UID") {

    IT("returns -1 for a NULL input") {
      TEST_COVERS(userIdGet);
      int rc = 0, rsn = 0;
      int uid = userIdGet(NULL, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, uid);
    } IT_END

    IT("returns the correct UID for the current user's name") {
      TEST_COVERS(userIdGet);
      if (!s_setupOk) { SKIP("startup user lookup failed"); }
      int rc = 0, rsn = 0;
      int uid = userIdGet(s_userName, &rc, &rsn);
      ASSERT_EQUAL_INT(s_uid, uid);
    } IT_END

    IT("returns -1 for a name that cannot exist") {
      TEST_COVERS(userIdGet);
      int rc = 0, rsn = 0;
      int uid = userIdGet("NOSUCHUSER99", &rc, &rsn);
      ASSERT_EQUAL_INT(-1, uid);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 10 - groupIdGet  (name-string → GID conversion)
 *
 *  As with userIdGet, only the name and NULL paths are tested.  Unlike
 *  userIdGet, groupIdGet's numeric branch correctly uses atoi(string),
 *  but we still skip it to keep the test independent of any specific
 *  numeric GID being defined on the system.
 * =================================================================== */

static void testGroupIdGet(void) {
  DESCRIBE("groupIdGet - convert group name to GID") {

    IT("returns -1 for a NULL input") {
      TEST_COVERS(groupIdGet);
      int rc = 0, rsn = 0;
      int gid = groupIdGet(NULL, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, gid);
    } IT_END

    IT("returns the correct GID for the current primary group name") {
      TEST_COVERS(groupIdGet);
      if (!s_setupOk) { SKIP("startup group lookup failed"); }
      int rc = 0, rsn = 0;
      int gid = groupIdGet(s_groupName, &rc, &rsn);
      ASSERT_EQUAL_INT(s_gid, gid);
    } IT_END

    IT("returns -1 for a group name that cannot exist") {
      TEST_COVERS(groupIdGet);
      int rc = 0, rsn = 0;
      int gid = groupIdGet("NOSUCHGRP99X", &rc, &rsn);
      ASSERT_EQUAL_INT(-1, gid);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Suite 11 - getGroupList
 *
 *  getGroupList(userName, groups, &groupCount, &rc, &rsn) follows a
 *  two-call pattern:
 *    1. Pass groups=NULL and *groupCount=0 to learn how many groups
 *       the user belongs to.
 *    2. Allocate an int array of that size and call again to populate it.
 *
 *  Every UNIX user belongs to at least their primary group, so the count
 *  must be >= 1 and the primary GID must appear in the list.
 * =================================================================== */

static void testGetGroupList(void) {
  DESCRIBE("getGroupList - enumerate all groups for a user") {

    IT("count-inquiry call returns 0 and a positive group count") {
      TEST_COVERS(getGroupList);
      if (!s_setupOk) { SKIP("startup user lookup failed"); }
      int groupCount = 0;
      int rc = 0, rsn = 0;
      int status = getGroupList(s_userName, NULL, &groupCount, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
      ASSERT_GT_INT(groupCount, 0);
    } IT_END

    IT("primary GID appears in the group list returned by the second call") {
      TEST_COVERS(getGroupList);
      if (!s_setupOk) { SKIP("startup user lookup failed"); }

      /* First call: determine count. */
      int groupCount = 0;
      int rc = 0, rsn = 0;
      int status = getGroupList(s_userName, NULL, &groupCount, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);
      ASSERT_GT_INT(groupCount, 0);

      /* Allocate and populate. */
      int *groups = (int *)safeMalloc(sizeof(int) * groupCount, "group list");
      rc = rsn = 0;
      status = getGroupList(s_userName, groups, &groupCount, &rc, &rsn);
      ASSERT_EQUAL_INT(0, status);

      /* Primary GID must appear somewhere in the list. */
      int i;
      int found = 0;
      for (i = 0; i < groupCount; i++) {
        if (groups[i] == s_gid) {
          found = 1;
          break;
        }
      }
      safeFree((void *)groups, sizeof(int) * groupCount);
      ASSERT_TRUE(found);
    } IT_END

    IT("returns -1 for a user name that cannot exist") {
      TEST_COVERS(getGroupList);
      int groupCount = 0;
      int rc = 0, rsn = 0;
      int status = getGroupList("NOSUCHUSER99", NULL, &groupCount, &rc, &rsn);
      ASSERT_EQUAL_INT(-1, status);
    } IT_END

  } DESCRIBE_END
}

/* ===================================================================
 *  Entry point
 * =================================================================== */

int main(void) {
  zoweTestInit();

  /*
   * One-time bootstrap: resolve the current process identity into module-wide
   * globals that every suite can rely on.  Done before any DESCRIBE block so
   * that failures here show up as SKIPs rather than cascading assertion
   * failures throughout every suite.
   */
  s_uid = (int)getuid();
  s_gid = (int)getgid();

  {
    UserInfo ui = {0};
    int rc = 0, rsn = 0;
    if (getUserInfo(s_uid, &ui, &rc, &rsn) == 0) {
      memset(s_userName, 0, sizeof(s_userName));
      userInfoGetUserName(&ui, s_userName);

      GroupInfo gi = {0};
      rc = rsn = 0;
      if (getGroupInfo(s_gid, &gi, &rc, &rsn) == 0) {
        memset(s_groupName, 0, sizeof(s_groupName));
        groupInfoGetGroupName(&gi, s_groupName);
        s_setupOk = 1;
      }
    }
  }

  if (!s_setupOk) {
    printf("WARNING: startup identity lookup failed -- "
           "most tests will be skipped\n");
  }

  testGetUserInfo();
  testGidGetUserInfo();
  testUserInfoAccessors();
  testGetGroupInfo();
  testGidGetGroupInfo();
  testGroupInfoAccessors();
  testUserGetName();
  testGroupGetName();
  testUserIdGet();
  testGroupIdGet();
  testGetGroupList();

  return ZOWE_TEST_REPORT();
}
