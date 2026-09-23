#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "utils.h"

bool verifyCompareIgnoringCase(char *s1, char *s2, int len, int expected) {
    int actual = compareIgnoringCase(s1, s2, len);
    bool succeeded = (actual == expected);
    printf("[%s] compareIgnoringCase(\"%s\", \"%s\", %d), actual return: %d, expected return %d\n", succeeded ? "GOOD" : " BAD", s1, s2, len, actual, expected);
    return succeeded;
}

bool verifyCompareStringsIgnoringCase(char *s1, char *s2, int expected) {
    int actual = compareStringsIgnoringCase(s1, s2);
    bool succeeded = (actual == expected);
    printf("[%s] compareStringsIgnoringCase(\"%s\", \"%s\"), actual return: %d, expected return %d\n", succeeded ? "GOOD" : " BAD", s1, s2, actual, expected);
    return succeeded;
}

int main(int argc, char *argv[]) {

    bool good = true;

    good &= verifyCompareIgnoringCase(    "",    "", 0, 0);
    good &= verifyCompareIgnoringCase(    "", "ABC", 0, 0);
    good &= verifyCompareIgnoringCase(   "a", "ABC", 1, 0);
    good &= verifyCompareIgnoringCase( "abc", "ABC", 3, 0);
    good &= verifyCompareIgnoringCase("abcd", "ABC", 4, 'D' - '\0');
    good &= verifyCompareIgnoringCase(   "d", "ABC", 1, 'D' - 'A');

    good &= verifyCompareStringsIgnoringCase(    "",    "", 0);
    good &= verifyCompareStringsIgnoringCase( "abc", "ABC", 0);
    good &= verifyCompareStringsIgnoringCase(    "", "ABC", '\0' - 'A');
    good &= verifyCompareStringsIgnoringCase(   "a", "ABC", '\0' - 'B');
    good &= verifyCompareStringsIgnoringCase("abcd", "ABC", 'D' - '\0');
    good &= verifyCompareStringsIgnoringCase(   "d", "ABC", 'D' - 'A');

    return good ? 0 : 1;
}