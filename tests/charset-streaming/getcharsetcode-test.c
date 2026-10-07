/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this
  distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0
  Copyright Contributors to the Zowe Project.
*/

/* getcharsetcode-test.c -- regression guard for getCharsetCode()'s name->CCSID
 * contract. getCharsetCode was refactored from an 88-line if/else strcmp chain
 * to a table lookup; equivalence to the prior chain was proven exhaustively by
 * a differential test (every name x case-variants). This file is the DURABLE
 * guard: it pins every table entry (exact + lower-case, since strupcase makes
 * lookups case-insensitive) plus the unknown/empty/oversize/NULL -> -1 edges,
 * so a later accidental edit to the table is caught. Runs under ASan; see run.sh. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "zowetypes.h"
#include "charsets.h"

/* The name->CCSID contract (mirrors the table in charsets.c getCharsetCode). */
static const struct { const char *name; int ccsid; } EXPECT[] = {
  {"ISO-8859-1", CCSID_ISO_8859_1},
  {"ISO8859-1", CCSID_ISO_8859_1},
  {"IBM-819", CCSID_ISO_8859_1},
  {"UTF-8", CCSID_UTF_8},
  {"UTF-16", CCSID_UTF_16},
  {"UTF-16BE", CCSID_UTF_16_BE},
  {"UTF-16LE", CCSID_UTF_16_LE},
  {"IBM-37", 37},
  {"IBM-38", 38},
  {"IBM-259", 259},
  {"IBM-273", 273},
  {"IBM-274", 274},
  {"IBM-275", 275},
  {"IBM-277", 277},
  {"IBM-278", 278},
  {"IBM-280", 280},
  {"IBM-281", 281},
  {"IBM-284", 284},
  {"IBM-285", 285},
  {"IBM-290", 290},
  {"IBM-297", 297},
  {"IBM-367", 367},
  {"IBM-420", 420},
  {"IBM-423", 423},
  {"IBM-424", 424},
  {"IBM-437", 437},
  {"IBM-500", 500},
  {"IBM-775", 775},
  {"IBM-838", 838},
  {"IBM-850", 850},
  {"IBM-851", 851},
  {"IBM-852", 852},
  {"IBM-855", 855},
  {"IBM-857", 857},
  {"IBM-858", 858},
  {"IBM-860", 860},
  {"IBM-861", 861},
  {"IBM-862", 862},
  {"IBM-863", 863},
  {"IBM-864", 864},
  {"IBM-865", 865},
  {"IBM-866", 866},
  {"IBM-868", 868},
  {"IBM-869", 869},
  {"IBM-870", 870},
  {"IBM-871", 871},
  {"IBM-880", 880},
  {"IBM-891", 891},
  {"IBM-903", 903},
  {"IBM-904", 904},
  {"IBM-905", 905},
  {"IBM-918", 918},
  {"IBM-924", 924},
  {"IBM-1026", 1026},
  {"IBM-1047", CCSID_IBM1047},
  {"IBM1047", CCSID_IBM1047},
  {"IBM-1140", 1140},
  {"IBM-1141", 1141},
  {"IBM-1142", 1142},
  {"IBM-1143", 1143},
  {"IBM-1144", 1144},
  {"IBM-1145", 1145},
  {"IBM-1146", 1146},
  {"IBM-1147", 1147},
  {"IBM-1148", 1148},
  {"IBM-1149", 1149},
};

static int fails = 0;
static int checks = 0;
#define CK(cond, ...) do{ checks++; if(!(cond)){ fails++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } }while(0)

int main(void){
  int n = (int)(sizeof(EXPECT) / sizeof(EXPECT[0]));
  char lower[64];

  /* every entry resolves to its CCSID, exact-case and lower-case */
  for (int i = 0; i < n; i++){
    const char *name = EXPECT[i].name;
    int want = EXPECT[i].ccsid;
    CK(getCharsetCode(name) == want, "%s -> %d (got %d)", name, want, getCharsetCode(name));
    int j;
    for (j = 0; name[j]; j++) lower[j] = (char)tolower((unsigned char)name[j]);
    lower[j] = 0;
    CK(getCharsetCode(lower) == want, "case-insensitive %s -> %d", lower, want);
  }

  /* guards and misses all map to -1 */
  CK(getCharsetCode("NOT-A-CHARSET") == -1, "unknown name -> -1");
  CK(getCharsetCode("") == -1, "empty -> -1");
  CK(getCharsetCode("THIS-NAME-IS-TOO-LONG") == -1, "oversize (> CHARSETNAME_SIZE) -> -1");
  CK(getCharsetCode(NULL) == -1, "NULL -> -1");

  printf("\n%s  (%d/%d checks: %d entries x {exact,lower} + 4 edges)\n",
         fails ? "FAILURES" : "ALL PASS", checks - fails, checks, n);
  return fails;
}
