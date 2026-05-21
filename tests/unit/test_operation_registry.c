#include "../framework/test.h"
#include "../../src/mtp/core/operation_registry.h"
#include <string.h>

static int handler_a_calls = 0;
static int handler_b_calls = 0;

static void handler_a(MTPContext* ctx, const MTPRequest* req, MTPResponse* resp) {
    (void)ctx;
    (void)req;
    (void)resp;
    handler_a_calls++;
}

static void handler_b(MTPContext* ctx, const MTPRequest* req, MTPResponse* resp) {
    (void)ctx;
    (void)req;
    (void)resp;
    handler_b_calls++;
}

TEST(create_destroy) {
    MTPOperationRegistry* r = mtp_operation_registry_create();
    ASSERT_NOT_NULL(r);
    ASSERT_EQ(mtp_operation_registry_count(r), (size_t)0);
    mtp_operation_registry_destroy(r);
    TEST_PASS();
}

TEST(add_and_find) {
    MTPOperationRegistry* r = mtp_operation_registry_create();

    const MTPOperationEntry e = {.op_code = 0x1001, .name = "a", .handler = handler_a};
    ASSERT(mtp_operation_registry_add(r, &e));
    ASSERT_EQ(mtp_operation_registry_count(r), (size_t)1);

    MTPOperationHandler h = mtp_operation_registry_find(r, 0x1001);
    ASSERT(h == handler_a);

    ASSERT(mtp_operation_registry_contains(r, 0x1001));
    ASSERT(!mtp_operation_registry_contains(r, 0x1002));

    mtp_operation_registry_destroy(r);
    TEST_PASS();
}

TEST(add_updates_existing) {
    MTPOperationRegistry* r = mtp_operation_registry_create();
    const MTPOperationEntry e1 = {.op_code = 0x1001, .name = "a", .handler = handler_a};
    const MTPOperationEntry e2 = {.op_code = 0x1001, .name = "b", .handler = handler_b};

    ASSERT(mtp_operation_registry_add(r, &e1));
    ASSERT(mtp_operation_registry_add(r, &e2));
    ASSERT_EQ(mtp_operation_registry_count(r), (size_t)1); // not duplicated

    MTPOperationHandler h = mtp_operation_registry_find(r, 0x1001);
    ASSERT(h == handler_b); // updated

    mtp_operation_registry_destroy(r);
    TEST_PASS();
}

TEST(get_name) {
    MTPOperationRegistry* r = mtp_operation_registry_create();
    const MTPOperationEntry e = {.op_code = 0x1001, .name = "alpha", .handler = handler_a};
    mtp_operation_registry_add(r, &e);

    const char* name = mtp_operation_registry_get_name(r, 0x1001);
    ASSERT_NOT_NULL(name);
    ASSERT_STR_EQ(name, "alpha");

    ASSERT_NULL(mtp_operation_registry_get_name(r, 0x9999));

    mtp_operation_registry_destroy(r);
    TEST_PASS();
}

TEST(reject_null_handler) {
    MTPOperationRegistry* r = mtp_operation_registry_create();
    const MTPOperationEntry e = {.op_code = 0x1001, .name = "x", .handler = NULL};
    ASSERT(!mtp_operation_registry_add(r, &e));
    ASSERT_EQ(mtp_operation_registry_count(r), (size_t)0);
    mtp_operation_registry_destroy(r);
    TEST_PASS();
}

TEST(find_in_null_registry) {
    ASSERT_NULL(mtp_operation_registry_find(NULL, 0x1001));
    ASSERT_EQ(mtp_operation_registry_count(NULL), (size_t)0);
    TEST_PASS();
}

static int count_iter_visits = 0;
static void count_iter(const MTPOperationEntry* entry, void* user_data) {
    (void)entry;
    (void)user_data;
    count_iter_visits++;
}

TEST(foreach_iterates_all_entries) {
    MTPOperationRegistry* r = mtp_operation_registry_create();
    const MTPOperationEntry e1 = {.op_code = 0x1001, .name = "a", .handler = handler_a};
    const MTPOperationEntry e2 = {.op_code = 0x1002, .name = "b", .handler = handler_b};
    mtp_operation_registry_add(r, &e1);
    mtp_operation_registry_add(r, &e2);

    count_iter_visits = 0;
    mtp_operation_registry_foreach(r, count_iter, NULL);
    ASSERT_EQ(count_iter_visits, 2);

    mtp_operation_registry_destroy(r);
    TEST_PASS();
}

int main(void) {
    printf("Running Operation Registry Tests\n");
    printf("=================================\n\n");

    RUN_TEST(create_destroy);
    RUN_TEST(add_and_find);
    RUN_TEST(add_updates_existing);
    RUN_TEST(get_name);
    RUN_TEST(reject_null_handler);
    RUN_TEST(find_in_null_registry);
    RUN_TEST(foreach_iterates_all_entries);

    test_summary();
    return test_exit_code();
}
