# tests/Makefile.new
# ------------------------------------------------------------------
# Drop-in replacement for tests/Makefile that builds the new MTP
# modules added in src/mtp/operations and src/mtp/storage in addition
# to the original test_object_index / test_storage_interface /
# test_transport targets.
#
# Once verified, rename this file to `Makefile` (and back up the old
# one) so `make` picks it up by default.
# ------------------------------------------------------------------

CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -g -I../src/mtp -Imocks

# --- Source sets ---------------------------------------------------

# Core: small, dependency-free pieces of the MTP stack.
CORE_BASE := \
    ../src/mtp/core/error.c \
    ../src/mtp/core/object_index.c \
    ../src/mtp/core/mtp_string.c \
    ../src/mtp/core/operation.c \
    ../src/mtp/core/operation_registry.c \
    ../src/mtp/core/context.c \
    ../src/mtp/core/transport.c

CORE_FULL := $(CORE_BASE) ../src/mtp/core/dispatcher.c

OPS_BUILDERS := \
    ../src/mtp/operations/device_info.c \
    ../src/mtp/operations/storage_info.c \
    ../src/mtp/operations/object_info.c \
    ../src/mtp/operations/object_props.c

OP_HANDLERS := \
    ../src/mtp/operations/op_device.c \
    ../src/mtp/operations/op_storage.c \
    ../src/mtp/operations/op_objects.c \
    ../src/mtp/operations/op_properties.c \
    ../src/mtp/operations/op_device_props.c \
    ../src/mtp/operations/op_transfer.c \
    ../src/mtp/operations/register_all.c

TRANSFER_SRCS := ../src/mtp/core/transfer_state.c

FAKE_FLIPPER_SRC := mocks/flipper_api_fake.c

STORAGE_SRCS := ../src/mtp/storage/storage_paths.c

FRAMEWORK := framework/test.c

# --- Targets -------------------------------------------------------

EXISTING_BINS := test_object_index test_storage_interface test_transport

NEW_BINS := \
    test_storage_paths \
    test_device_info \
    test_storage_info \
    test_object_info \
    test_object_props \
    test_mtp_string \
    test_operation_registry \
    test_dispatcher \
    test_op_storage \
    test_op_objects \
    test_op_device_props \
    test_transfer_state \
    test_register_all \
    test_e2e_mtp

ALL_BINS := $(EXISTING_BINS) $(NEW_BINS)

.PHONY: all clean run run-new
all: $(ALL_BINS)

# --- Existing tests (unchanged) ------------------------------------

test_object_index: unit/test_object_index.c $(CORE_BASE) $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_storage_interface: unit/test_storage_interface.c $(CORE_BASE) $(FRAMEWORK) mocks/flipper_api_fake.c
	$(CC) $(CFLAGS) -o $@ $^

test_transport: unit/test_transport.c $(CORE_BASE) $(FRAMEWORK) mocks/usb_fake.c
	$(CC) $(CFLAGS) -o $@ $^

# --- New tests -----------------------------------------------------

test_storage_paths: unit/test_storage_paths.c $(STORAGE_SRCS) $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_device_info: unit/test_device_info.c ../src/mtp/operations/device_info.c \
                  ../src/mtp/core/mtp_string.c $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_storage_info: unit/test_storage_info.c ../src/mtp/operations/storage_info.c \
                   ../src/mtp/core/mtp_string.c $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_object_info: unit/test_object_info.c ../src/mtp/operations/object_info.c \
                  ../src/mtp/core/mtp_string.c $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_object_props: unit/test_object_props.c ../src/mtp/operations/object_props.c \
                   ../src/mtp/core/mtp_string.c $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_mtp_string: unit/test_mtp_string.c ../src/mtp/core/mtp_string.c $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_operation_registry: unit/test_operation_registry.c ../src/mtp/core/operation_registry.c \
                         ../src/mtp/core/operation.c $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_dispatcher: unit/test_dispatcher.c $(CORE_FULL) $(OPS_BUILDERS) ../src/mtp/operations/op_device.c \
                 ../src/mtp/operations/register_all.c ../src/mtp/operations/op_storage.c \
                 ../src/mtp/operations/op_objects.c ../src/mtp/operations/op_properties.c \
                 ../src/mtp/operations/op_device_props.c ../src/mtp/operations/op_transfer.c \
                 $(TRANSFER_SRCS) $(STORAGE_SRCS) $(FAKE_FLIPPER_SRC) \
                 mocks/transport_capture.c $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_op_storage: unit/test_op_storage.c $(CORE_FULL) $(OPS_BUILDERS) $(OP_HANDLERS) \
                 $(STORAGE_SRCS) $(TRANSFER_SRCS) $(FAKE_FLIPPER_SRC) $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_op_objects: unit/test_op_objects.c $(CORE_FULL) $(OPS_BUILDERS) $(OP_HANDLERS) \
                 $(STORAGE_SRCS) $(TRANSFER_SRCS) $(FAKE_FLIPPER_SRC) $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_op_device_props: unit/test_op_device_props.c $(CORE_FULL) $(OPS_BUILDERS) $(OP_HANDLERS) \
                      $(STORAGE_SRCS) $(TRANSFER_SRCS) $(FAKE_FLIPPER_SRC) $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_transfer_state: unit/test_transfer_state.c $(CORE_FULL) $(OPS_BUILDERS) $(OP_HANDLERS) \
                     $(STORAGE_SRCS) $(TRANSFER_SRCS) $(FAKE_FLIPPER_SRC) $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_register_all: unit/test_register_all.c $(CORE_FULL) $(OPS_BUILDERS) $(OP_HANDLERS) \
                   $(STORAGE_SRCS) $(TRANSFER_SRCS) $(FAKE_FLIPPER_SRC) $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

test_e2e_mtp: e2e/test_e2e_mtp.c $(CORE_FULL) $(OPS_BUILDERS) $(OP_HANDLERS) \
              $(STORAGE_SRCS) $(TRANSFER_SRCS) $(FAKE_FLIPPER_SRC) \
              mocks/transport_capture.c $(FRAMEWORK)
	$(CC) $(CFLAGS) -o $@ $^

# --- Convenience ---------------------------------------------------

run: all
	@set -e; for bin in $(ALL_BINS); do \
		echo "=== $$bin ==="; \
		./$$bin; \
	done

# Only the freshly-added suites — useful while iterating on Phase A.
run-new: $(NEW_BINS)
	@set -e; for bin in $(NEW_BINS); do \
		echo "=== $$bin ==="; \
		./$$bin; \
	done

clean:
	rm -f $(ALL_BINS)
	rm -rf *.dSYM
