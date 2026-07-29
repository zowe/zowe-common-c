#ifndef TEST_UTILS_H
#define TEST_UTILS_H
#include <string.h>
int valcomp_memcmp(const void *, const void *, int);
int valcomp_strcmp(const void *, const void *, int);
int array_contains(const void *, const void *, int, int, int (*)(const void *, const void*, int));
void print_failure(const char *, int, const char *, ...);
void print_results(int, int);
void print_case_name(const char *);
int captureCommandOutput(const char *cmd, char *buffer, size_t bufferSize);
#define EXPECT_EQUAL(actual, expect) if ((expect) != (actual)) { print_failure(__FILE__, __LINE__, "EXPECT_EQUAL"); return 1; }
#define EXPECT_INTEGER_EQUAL(actual, expect) if ((expect) != (actual)) { print_failure(__FILE__, __LINE__, "EXPECT_EQUAL expected %d but got %d", (expect), (actual)); return 1; }
#define EXPECT_STRING_EQUAL(actual, expect) if (strcmp((expect), (actual)) != 0) { print_failure(__FILE__, __LINE__, "EXPECT_EQUAL expected %s but got %s", (expect), (actual)); return 1; }
#define EXPECT_ARRAY_CONTAINS(array, length, expect) if (array_contains((array), &(expect), (length), sizeof(expect), valcomp_memcmp) < 0) { print_failure(__FILE__, __LINE__, "EXPECT_ARRAY_CONTAINS"); return 1; }
#define EXPECT_WHOLE_ARRAY_CONTAINS(array, expect) EXPECT_ARRAY_CONTAINS((array), sizeof(array)/sizeof(array[0]), (expect))
#define EXPECT_STRING_ARRAY_CONTAINS(array, length, expect) if (array_contains((array), (expect), (length), sizeof(char*), valcomp_strcmp) < 0) { print_failure(__FILE__, __LINE__, "EXPECT_STRING_ARRAY_CONTAINS %s but not found", (expect)); return 1; }
#define EXPECT_WHOLE_STRING_ARRAY_CONTAINS(array, expect) EXPECT_STRING_ARRAY_CONTAINS((array), sizeof(array)/sizeof(char*), (expect))
#define RETURN_SUCCEEDED return 0
#define BEFORE_TESTS int __total_test_cases = 0, __failed_test_cases = 0;
#define RUN_TEST_CASE(func) __total_test_cases++; print_case_name(#func); if (func() != 0) { __failed_test_cases++; print_failure(__FILE__, __LINE__, "test case "#func); }
#define FINISH_TESTS print_results(__total_test_cases, __failed_test_cases); return __failed_test_cases != 0;
#endif