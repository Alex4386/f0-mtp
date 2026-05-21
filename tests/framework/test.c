#include "test.h"

int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

void test_init(void) {
    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;
}

void test_summary(void) {
    printf("\n");
    printf("=====================================\n");
    printf("Test Summary:\n");
    printf("  Total:  %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("=====================================\n");

    if(tests_failed == 0 && tests_passed == tests_run) {
        printf("All tests passed!\n");
    } else if(tests_failed > 0) {
        printf("Some tests failed!\n");
    } else {
        // No failures, but not every test reported PASS.
        // This catches the case where a test body finishes silently
        // without calling TEST_PASS() at the end.
        printf(
            "INCOMPLETE: %d test(s) ran but never called TEST_PASS() or an assertion.\n",
            tests_run - tests_passed);
    }
}
