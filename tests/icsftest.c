/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  Test program for ICSF digest (hash) functions.
  Exercises all algorithms defined in icsf.h: MD5, SHA-1,
  SHA-224/256/384/512, and SHA3-224/256/384/512.

  Hashes the ASCII string "TEST STRING" (11 bytes) via the streaming
  API (init/update/finish) and compares the result against reference
  digests computed with openssl and Python hashlib.

  Build on z/OS (64-bit):

    export _C89_L6SYSLIB="CEE.SCEEBND2:SYS1.CSSLIB:CSF.SCSFMOD0"
    xlclang -q64 -I ../h \
      -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 \
      -DSUBPOOL=132 \
      "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
      -o icsftest icsftest.c \
      ../c/icsf.c ../c/alloc.c ../c/utils.c ../c/timeutls.c \
      ../c/collections.c ../c/logging.c ../c/le.c ../c/recovery.c \
      ../c/zos.c ../c/scheduling.c

  Build on z/OS (31-bit):

    export _C89_LSYSLIB="CEE.SCEELKED:SYS1.CSSLIB:CSF.SCSFMOD0"
    xlclang -I ../h \
      -D_OPEN_SYS_FILE_EXT=1 -D_XOPEN_SOURCE=600 -D_OPEN_THREADS=1 \
      -DSUBPOOL=132 \
      "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
      -o icsftest icsftest.c \
      ../c/icsf.c ../c/alloc.c ../c/utils.c ../c/timeutls.c \
      ../c/collections.c ../c/logging.c ../c/le.c ../c/recovery.c \
      ../c/zos.c ../c/scheduling.c

  Run:
    ./icsftest

  Prerequisites:
    - ICSF must be active on the system
    - The user must have READ access to CSNBOWH (31-bit) or CSNEOWH (64-bit)
      in the CSFSERV SAF class
    - SHA-3 algorithms must be enabled in ICSF for SHA-3 tests to pass;
      if unavailable those tests report SKIP rather than FAIL
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "icsf.h"

/*
 * Test input: ASCII "TEST STRING" = 11 bytes.
 * On z/OS (EBCDIC) we must use explicit hex values to get ASCII bytes.
 *
 *   T=0x54 E=0x45 S=0x53 T=0x54 ' '=0x20
 *   S=0x53 T=0x54 R=0x52 I=0x49 N=0x4e G=0x47
 */
static char testInput[] = {
  0x54, 0x45, 0x53, 0x54, 0x20, 0x53, 0x54, 0x52, 0x49, 0x4e, 0x47
};
#define TEST_INPUT_LEN 11

static void bytesToHex(const char *bytes, int len, char *hexOut) {
  static const char hexChars[] = "0123456789abcdef";
  for (int i = 0; i < len; i++) {
    unsigned char b = (unsigned char)bytes[i];
    hexOut[i * 2]     = hexChars[(b >> 4) & 0x0F];
    hexOut[i * 2 + 1] = hexChars[b & 0x0F];
  }
  hexOut[len * 2] = '\0';
}

typedef struct TestVector_tag {
  char *name;
  int   icsfType;
  int   hashLen;
  char *expectedHex;
  int   isSHA3;
} TestVector;

/*
 * Reference digests for "TEST STRING" (ASCII bytes 0x54 0x45 0x53 0x54
 * 0x20 0x53 0x54 0x52 0x49 0x4e 0x47).
 *
 * Verified with:
 *   echo -n "TEST STRING" | openssl dgst -<alg>     (MD5, SHA-1, SHA-2)
 *   python3 -c "import hashlib; print(hashlib.<alg>(b'TEST STRING').hexdigest())"
 */
static TestVector vectors[] = {
  {
    "MD5", ICSF_DIGEST_MD5, 16,
    "2d7d687432758a8eeeca7b7e5d518e7f",
    0
  },
  {
    "SHA-1", ICSF_DIGEST_SHA1, 20,
    "d39d009c05797a93a79720952e99c7054a24e7c4",
    0
  },
  {
    "SHA-224", ICSF_DIGEST_SHA224, 28,
    "0613eac3ea33640ea58f675291f8d078c7dd54c21ae92abeaad8fcbb",
    0
  },
  {
    "SHA-256", ICSF_DIGEST_SHA256, 32,
    "fb6ca29024bd42f1894620ffa45fd976217e72d988b04ee02bb4793ab9d0c862",
    0
  },
  {
    "SHA-384", ICSF_DIGEST_SHA384, 48,
    "2bf1405132e776e685f96cc5f7ba528f92e646f4b3b0ab940f2d18e564f1688a"
    "766cfc2ba65530a9d0d4b5e2d3e572d5",
    0
  },
  {
    "SHA-512", ICSF_DIGEST_SHA512, 64,
    "9e2d93c3d2ab15baf890a6e295e48af31125d4984b9f555b7b82fa9f10a624de"
    "31fdf7501c050cb8a9015d92c860ef8a05c8be44c7257c950b0ee054f90a22eb",
    0
  },
  {
    "SHA3-224", ICSF_DIGEST_SHA3_224, 28,
    "4d191780933cc11abd17a47ad56f46d383fa7dc186383202ee5c6810",
    1
  },
  {
    "SHA3-256", ICSF_DIGEST_SHA3_256, 32,
    "e118f9acaf7ac8b141985029528ea25cd8712187c634b07958406c7646c3ef35",
    1
  },
  {
    "SHA3-384", ICSF_DIGEST_SHA3_384, 48,
    "c9cbd91e5ee01f6b6ee455d411724705be0f8f5558184180dbf504b6789c8a3c"
    "85dd63a8cd068ec167fed4295db109bf",
    1
  },
  {
    "SHA3-512", ICSF_DIGEST_SHA3_512, 64,
    "c313b347e9dcb2638d2f73b9c3f3c52b6987bc07c2ebcd4fc7a95fcc69e91cda"
    "9568cc0483a3a22678e0cf17ce0a5791aacdec4a596b0f92f5c472c339d447a2",
    1
  }
};

#define VECTOR_COUNT (sizeof(vectors) / sizeof(vectors[0]))

/* Parse a hex string into bytes. Returns number of bytes written. */
static int hexToBytes(const char *hex, char *out, int maxOut) {
  int len = strlen(hex);
  int n = len / 2;
  if (n > maxOut) {
    n = maxOut;
  }
  for (int i = 0; i < n; i++) {
    unsigned int byte;
    char pair[3] = { hex[i*2], hex[i*2+1], '\0' };
    sscanf(pair, "%02x", &byte);
    out[i] = (char)byte;
  }
  return n;
}

/*
 * Test a single algorithm by hashing "TEST STRING" (as ASCII bytes)
 * through the init/update/finish streaming API.
 *
 * Returns:
 *   0 = PASS
 *   1 = FAIL (wrong digest)
 *   2 = SKIP (ICSF returned error, algorithm likely unavailable)
 */
static int testAlgorithm(TestVector *tv) {
  ICSFDigest digest;
  char hash[64];
  int rc;

  rc = icsfDigestInit(&digest, tv->icsfType);
  if (rc != 0) {
    printf("  %-10s SKIP  (icsfDigestInit rc=%d, algorithm may not be available)\n",
           tv->name, rc);
    return 2;
  }

  rc = icsfDigestUpdate(&digest, testInput, TEST_INPUT_LEN);
  if (rc != 0) {
    printf("  %-10s SKIP  (icsfDigestUpdate rc=%d)\n", tv->name, rc);
    return 2;
  }

  rc = icsfDigestFinish(&digest, hash);
  if (rc != 0) {
    printf("  %-10s SKIP  (icsfDigestFinish rc=%d)\n", tv->name, rc);
    return 2;
  }

  /* Compare result against expected */
  char expectedBytes[64];
  int expectedLen = hexToBytes(tv->expectedHex, expectedBytes, sizeof(expectedBytes));

  if (expectedLen != tv->hashLen) {
    printf("  %-10s FAIL  (test vector length mismatch: expected %d, got %d)\n",
           tv->name, tv->hashLen, expectedLen);
    return 1;
  }

  if (memcmp(hash, expectedBytes, tv->hashLen) != 0) {
    char gotHex[129];
    bytesToHex(hash, tv->hashLen, gotHex);
    printf("  %-10s FAIL\n", tv->name);
    printf("    expected: %s\n", tv->expectedHex);
    printf("    got:      %s\n", gotHex);
    return 1;
  }

  printf("  %-10s PASS\n", tv->name);
  return 0;
}

/*
 * Test that hashing "TEST STRING" in multiple chunks produces the same
 * result as hashing in a single call. Uses SHA-256 as the representative.
 */
static int testChunkedHashing(void) {
  ICSFDigest digestSingle, digestChunked;
  char hashSingle[64], hashChunked[64];
  int rc;

  /* Single-shot: all 11 bytes at once */
  rc = icsfDigestInit(&digestSingle, ICSF_DIGEST_SHA256);
  if (rc != 0) {
    printf("  chunked   SKIP  (SHA-256 not available, rc=%d)\n", rc);
    return 2;
  }
  rc = icsfDigestUpdate(&digestSingle, testInput, TEST_INPUT_LEN);
  if (rc != 0) { return 2; }
  rc = icsfDigestFinish(&digestSingle, hashSingle);
  if (rc != 0) { return 2; }

  /* Chunked: one byte at a time */
  rc = icsfDigestInit(&digestChunked, ICSF_DIGEST_SHA256);
  if (rc != 0) { return 2; }
  for (int i = 0; i < TEST_INPUT_LEN; i++) {
    rc = icsfDigestUpdate(&digestChunked, &testInput[i], 1);
    if (rc != 0) { return 2; }
  }
  rc = icsfDigestFinish(&digestChunked, hashChunked);
  if (rc != 0) { return 2; }

  if (memcmp(hashSingle, hashChunked, 32) != 0) {
    char hexSingle[65], hexChunked[65];
    bytesToHex(hashSingle, 32, hexSingle);
    bytesToHex(hashChunked, 32, hexChunked);
    printf("  chunked   FAIL  (single-shot and byte-at-a-time differ)\n");
    printf("    single:  %s\n", hexSingle);
    printf("    chunked: %s\n", hexChunked);
    return 1;
  }

  printf("  chunked   PASS  (SHA-256 single-shot matches byte-at-a-time)\n");
  return 0;
}

/*
 * Test that hashing in two uneven chunks produces the correct result.
 * Splits "TEST STRING" as "TEST " (5 bytes) + "STRING" (6 bytes).
 * Expected SHA-256: fb6ca29024bd42f1894620ffa45fd976217e72d988b04ee02bb4793ab9d0c862
 */
static int testTwoChunkHashing(void) {
  ICSFDigest digest;
  char hash[64];
  int rc;
  char *expectedHex = "fb6ca29024bd42f1894620ffa45fd976217e72d988b04ee02bb4793ab9d0c862";
  char expectedBytes[32];

  rc = icsfDigestInit(&digest, ICSF_DIGEST_SHA256);
  if (rc != 0) {
    printf("  2-chunk   SKIP  (SHA-256 not available, rc=%d)\n", rc);
    return 2;
  }
  /* First chunk: "TEST " = bytes 0-4 */
  rc = icsfDigestUpdate(&digest, testInput, 5);
  if (rc != 0) { return 2; }
  /* Second chunk: "STRING" = bytes 5-10 */
  rc = icsfDigestUpdate(&digest, testInput + 5, 6);
  if (rc != 0) { return 2; }
  rc = icsfDigestFinish(&digest, hash);
  if (rc != 0) { return 2; }

  hexToBytes(expectedHex, expectedBytes, 32);
  if (memcmp(hash, expectedBytes, 32) != 0) {
    char gotHex[65];
    bytesToHex(hash, 32, gotHex);
    printf("  2-chunk   FAIL\n");
    printf("    expected: %s\n", expectedHex);
    printf("    got:      %s\n", gotHex);
    return 1;
  }

  printf("  2-chunk   PASS  (SHA-256 of \"TEST \"+\"STRING\" matches full)\n");
  return 0;
}

/*
 * Test hashing an empty input. The expected SHA-256 of "" is:
 *   e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
 * (verified with: echo -n "" | openssl dgst -sha256)
 */
static int testEmptyInput(void) {
  ICSFDigest digest;
  char hash[64];
  int rc;
  char *expectedHex = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
  char expectedBytes[32];

  rc = icsfDigestInit(&digest, ICSF_DIGEST_SHA256);
  if (rc != 0) {
    printf("  empty     SKIP  (SHA-256 not available, rc=%d)\n", rc);
    return 2;
  }
  /* No update call -- finish immediately */
  rc = icsfDigestFinish(&digest, hash);
  if (rc != 0) {
    printf("  empty     SKIP  (icsfDigestFinish rc=%d)\n", rc);
    return 2;
  }

  hexToBytes(expectedHex, expectedBytes, 32);
  if (memcmp(hash, expectedBytes, 32) != 0) {
    char gotHex[65];
    bytesToHex(hash, 32, gotHex);
    printf("  empty     FAIL\n");
    printf("    expected: %s\n", expectedHex);
    printf("    got:      %s\n", gotHex);
    return 1;
  }

  printf("  empty     PASS  (SHA-256 of empty string)\n");
  return 0;
}

int main(int argc, char **argv) {
  int pass = 0;
  int fail = 0;
  int skip = 0;

  printf("ICSF Digest Test\n");
  printf("Input: \"TEST STRING\" (ASCII 0x54 0x45 0x53 0x54 0x20 0x53 0x54 0x52 0x49 0x4e 0x47)\n");
  printf("================================================================\n\n");

  printf("Algorithm tests (all 10 algorithms):\n");
  for (int i = 0; i < VECTOR_COUNT; i++) {
    int result = testAlgorithm(&vectors[i]);
    if (result == 0) {
      pass++;
    } else if (result == 1) {
      fail++;
    } else {
      skip++;
    }
  }

  printf("\nStreaming tests:\n");

  int result = testChunkedHashing();
  if (result == 0) { pass++; } else if (result == 1) { fail++; } else { skip++; }

  result = testTwoChunkHashing();
  if (result == 0) { pass++; } else if (result == 1) { fail++; } else { skip++; }

  result = testEmptyInput();
  if (result == 0) { pass++; } else if (result == 1) { fail++; } else { skip++; }

  printf("\n================================================================\n");
  printf("Results: %d passed, %d failed, %d skipped\n", pass, fail, skip);

  if (fail > 0) {
    printf("OVERALL: FAIL\n");
    return 1;
  }

  if (pass == 0) {
    printf("OVERALL: NO TESTS RAN (is ICSF active?)\n");
    return 2;
  }

  printf("OVERALL: PASS\n");
  return 0;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
