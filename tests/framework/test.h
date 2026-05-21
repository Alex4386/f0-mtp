#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Test statistics
extern int tests_run;
extern int tests_passed;
extern int tests_failed;

// Test macros
#define TEST(name) \
    void test_##name(void)

#define RUN_TEST(name) \
    do { \
        printf("Running test: %s\n", #name); \
        tests_run++; \
        test_##name(); \
    } while(0)

// Assertion macros
#define ASSERT(condition) \
    do { \
        if(!(condition)) { \
            printf("  FAIL: %s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #condition); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if((a) != (b)) { \
            printf("  FAIL: %s:%d: Expected %ld, got %ld\n", __FILE__, __LINE__, (long)(b), (long)(a)); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_NE(a, b) \
    do { \
        if((a) == (b)) { \
            printf("  FAIL: %s:%d: Expected not equal to %ld\n", __FILE__, __LINE__, (long)(b)); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_TRUE(condition) ASSERT(condition)
#define ASSERT_FALSE(condition) ASSERT(!(condition))

#define ASSERT_NULL(ptr) \
    do { \
        if((ptr) != NULL) { \
            printf("  FAIL: %s:%d: Expected NULL, got %p\n", __FILE__, __LINE__, (void*)(ptr)); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if((ptr) == NULL) { \
            printf("  FAIL: %s:%d: Expected non-NULL pointer\n", __FILE__, __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_STR_EQ(a, b) \
    do { \
        if(strcmp((a), (b)) != 0) { \
            printf("  FAIL: %s:%d: Expected '%s', got '%s'\n", __FILE__, __LINE__, (b), (a)); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_MEM_EQ(a, b, n) \
    do { \
        if(memcmp((a), (b), (n)) != 0) { \
            printf("  FAIL: %s:%d: Memory mismatch over %zu bytes\n", __FILE__, __LINE__, (size_t)(n)); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("  PASS\n"); \
        tests_passed++; \
    } while(0)

// Test runner
void test_init(void);
void test_summary(void);

// Exit code helper: returns 0 only when every test ran and passed.
// Use as `return test_exit_code();` from main() instead of `return tests_failed;`
// so that silently-incomplete tests (no failure, no PASS) also fail CI.
static inline int test_exit_code(void) {
    if(tests_failed > 0) return 1;
    if(tests_passed != tests_run) return 2;
    return 0;
}
