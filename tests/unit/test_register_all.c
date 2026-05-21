#include "../framework/test.h"
#include "../../src/mtp/core/operation_registry.h"
#include "../../src/mtp/operations/register_all.h"
#include <string.h>

TEST(register_all_registers_full_set) {
    MTPOperationRegistry* reg = mtp_operation_registry_create();
    size_t n = mtp_register_all_operations(reg);
    ASSERT(n >= 18); // we register at least 18 ops

    // Spot-check a few critical codes
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_GET_DEVICE_INFO));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_OPEN_SESSION));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_CLOSE_SESSION));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_GET_STORAGE_IDS));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_GET_STORAGE_INFO));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_GET_OBJECT_HANDLES));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_GET_OBJECT_INFO));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_GET_OBJECT));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_DELETE_OBJECT));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_MOVE_OBJECT));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_GET_OBJECT_PROPS_SUPPORTED));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_GET_OBJECT_PROP_VALUE));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_SET_OBJECT_PROP_VALUE));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_GET_DEVICE_PROP_VALUE));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_GET_DEVICE_PROP_DESC));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_SEND_OBJECT_INFO));
    ASSERT(mtp_operation_registry_contains(reg, MTP_OP_SEND_OBJECT));

    mtp_operation_registry_destroy(reg);
    TEST_PASS();
}

TEST(register_all_handles_null_registry) {
    ASSERT_EQ(mtp_register_all_operations(NULL), (size_t)0);
    TEST_PASS();
}

int main(void) {
    printf("Running register_all Tests\n");
    printf("==========================\n\n");

    RUN_TEST(register_all_registers_full_set);
    RUN_TEST(register_all_handles_null_registry);

    test_summary();
    return test_exit_code();
}
