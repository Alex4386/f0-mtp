#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// MTP Protocol Constants
#define MTP_STANDARD_VERSION         100
#define MTP_VENDOR_EXTENSION_ID      0x6
#define MTP_VENDOR_EXTENSION_VERSION 100
#define MTP_FUNCTIONAL_MODE          0x0

// Buffer sizes
#define MTP_BUFFER_SIZE       1024
#define MTP_PATH_SIZE         256
#define MTP_NAME_SIZE         256
#define MTP_MAX_PACKET_SIZE   512

// File transfer configuration
#define MTP_FILE_SYNC_INTERVAL (1024 * 1024)  // 1MB

// Block size for storage operations
#define MTP_BLOCK_SIZE 65536

// MTP Packet Types
#define MTP_TYPE_COMMAND  0x1
#define MTP_TYPE_DATA     0x2
#define MTP_TYPE_RESPONSE 0x3

// MTP Operation Codes
#define MTP_OP_GET_DEVICE_INFO          0x1001
#define MTP_OP_OPEN_SESSION             0x1002
#define MTP_OP_CLOSE_SESSION            0x1003
#define MTP_OP_GET_STORAGE_IDS          0x1004
#define MTP_OP_GET_STORAGE_INFO         0x1005
#define MTP_OP_GET_NUM_OBJECTS          0x1006
#define MTP_OP_GET_OBJECT_HANDLES       0x1007
#define MTP_OP_GET_OBJECT_INFO          0x1008
#define MTP_OP_GET_OBJECT               0x1009
#define MTP_OP_DELETE_OBJECT            0x100B
#define MTP_OP_SEND_OBJECT_INFO         0x100C
#define MTP_OP_SEND_OBJECT              0x100D
#define MTP_OP_POWER_DOWN               0x1013
#define MTP_OP_GET_DEVICE_PROP_DESC     0x1014
#define MTP_OP_GET_DEVICE_PROP_VALUE    0x1015
#define MTP_OP_MOVE_OBJECT              0x1019
#define MTP_OP_GET_OBJECT_PROPS_SUPPORTED 0x9801
#define MTP_OP_GET_OBJECT_PROP_DESC     0x9802
#define MTP_OP_GET_OBJECT_PROP_VALUE    0x9803
#define MTP_OP_SET_OBJECT_PROP_VALUE    0x9804

// MTP Response Codes
#define MTP_RESP_UNDEFINED                  0x2000
#define MTP_RESP_OK                         0x2001
#define MTP_RESP_GENERAL_ERROR              0x2002
#define MTP_RESP_SESSION_NOT_OPEN           0x2003
#define MTP_RESP_INVALID_TRANSACTION_ID     0x2004
#define MTP_RESP_OPERATION_NOT_SUPPORTED    0x2005
#define MTP_RESP_PARAMETER_NOT_SUPPORTED    0x2006
#define MTP_RESP_INCOMPLETE_TRANSFER        0x2007
#define MTP_RESP_INVALID_STORAGE_ID         0x2008
#define MTP_RESP_INVALID_OBJECT_HANDLE      0x2009
#define MTP_RESP_DEVICE_PROP_NOT_SUPPORTED  0x200A
#define MTP_RESP_STORE_FULL                 0x200C
#define MTP_RESP_OBJECT_WRITE_PROTECTED     0x200D
#define MTP_RESP_STORE_READ_ONLY            0x200E
#define MTP_RESP_ACCESS_DENIED              0x200F
#define MTP_RESP_SPEC_BY_FORMAT_UNSUPPORTED 0x2014
#define MTP_RESP_NO_VALID_OBJECT_INFO       0x2015
#define MTP_RESP_INVALID_OBJECT_PROP_CODE   0xA801
#define MTP_RESP_DEVICE_BUSY                0x2019

// MTP Object Properties
#define MTP_PROP_STORAGE_ID       0xDC01
#define MTP_PROP_OBJECT_FORMAT    0xDC02
#define MTP_PROP_OBJECT_FILE_NAME 0xDC07

// Storage IDs
#define MTP_STORAGE_ID_INTERNAL 0x00010001
#define MTP_STORAGE_ID_EXTERNAL 0x00020001

// MTP Object Formats
#define MTP_FORMAT_UNDEFINED   0x3000
#define MTP_FORMAT_ASSOCIATION 0x3001

// Device Properties
#define MTP_DEVICE_PROP_DEVICE_FRIENDLY_NAME 0xd402
#define MTP_DEVICE_PROP_BATTERY_LEVEL        0x5001

// MTP Control Requests
#define MTP_REQ_CANCEL             0x64
#define MTP_REQ_GET_EXT_EVENT_DATA 0x65
#define MTP_REQ_RESET              0x66
#define MTP_REQ_GET_DEVICE_STATUS  0x67
