#pragma once

#define MTP_STANDARD_VERSION 100
#define MTP_VENDOR_EXTENSION_ID 0x6
#define MTP_VENDOR_EXTENSION_VERSION 100
#define MTP_FUNCTIONAL_MODE 0x0

#define MTP_REQ_CANCEL 0x64
#define MTP_REQ_GET_EXT_EVENT_DATA 0x65
#define MTP_REQ_RESET 0x66
#define MTP_REQ_GET_DEVICE_STATUS 0x67

// MTP Operation Codes
#define MTP_OP_GET_DEVICE_INFO 0x1001
#define MTP_OP_OPEN_SESSION 0x1002
#define MTP_OP_CLOSE_SESSION 0x1003
#define MTP_OP_GET_STORAGE_IDS 0x1004
#define MTP_OP_GET_STORAGE_INFO 0x1005
#define MTP_OP_GET_NUM_OBJECTS 0x1006
#define MTP_OP_GET_OBJECT_HANDLES 0x1007
#define MTP_OP_GET_OBJECT_INFO 0x1008
#define MTP_OP_GET_OBJECT 0x1009
#define MTP_OP_SEND_OBJECT_INFO 0x100C
#define MTP_OP_SEND_OBJECT 0x100D
#define MTP_OP_DELETE_OBJECT 0x100B
#define MTP_OP_GET_DEVICE_PROP_DESC 0x1014
#define MTP_OP_GET_DEVICE_PROP_VALUE 0x1015

// MTP Response Codes
#define MTP_RESP_OK 0x2001
#define MTP_RESP_GENERAL_ERROR 0x2002
#define MTP_RESP_SESSION_NOT_OPEN 0x2003
#define MTP_RESP_INVALID_TRANSACTION_ID 0x2004
#define MTP_RESP_OPERATION_NOT_SUPPORTED 0x2005
#define MTP_RESP_PARAMETER_NOT_SUPPORTED 0x2006
#define MTP_RESP_INCOMPLETE_TRANSFER 0x2007
#define MTP_RESP_INVALID_STORAGE_ID 0x2008
#define MTP_RESP_INVALID_OBJECT_HANDLE 0x2009
#define MTP_RESP_DEVICE_PROP_NOT_SUPPORTED 0x200A
#define MTP_RESP_STORE_FULL 0x200C
#define MTP_RESP_OBJECT_WRITE_PROTECTED 0x200D
#define MTP_RESP_STORE_READ_ONLY 0x200E
#define MTP_RESP_ACCESS_DENIED 0x200F
#define MTP_RESP_NO_THUMBNAIL_PRESENT 0x2010
#define MTP_RESP_SELF_TEST_FAILED 0x2011
#define MTP_RESP_PARTIAL_DELETION 0x2012
#define MTP_RESP_STORE_NOT_AVAILABLE 0x2013
#define MTP_RESP_SPEC_BY_FORMAT_UNSUPPORTED 0x2014
#define MTP_RESP_NO_VALID_OBJECT_INFO 0x2015
#define MTP_RESP_INVALID_CODE_FORMAT 0x2016
#define MTP_RESP_UNKNOWN_VENDOR_CODE 0x2017
#define MTP_RESP_CAPTURE_ALREADY_TERMINATED 0x2018
#define MTP_RESP_DEVICE_BUSY 0x2019

// Endpoint Addresses
#define MTP_EP_IN_ADDR 0x01
#define MTP_EP_OUT_ADDR 0x82
#define MTP_EP_INT_IN_ADDR 0x03

// Storage IDs
#define INTERNAL_STORAGE_ID 0x00000001
#define EXTERNAL_STORAGE_ID 0x00000001

// MTP Playback formats
#define MTP_FORMAT_UNDEFINED 0x3000
#define MTP_FORMAT_ASSOCIATION 0x3001

typedef struct {
    uint32_t handle;
    char name[256];
    uint32_t size;
    uint32_t parent_handle;
    bool is_directory;
} MTPObject;

struct mtp_device_status {
    uint16_t wLength;
    uint16_t wCode;
};

// Storage information structure based on MTP specification
typedef struct {
    uint16_t storage_type;
    uint16_t filesystem_type;
    uint16_t access_capability;
    uint64_t max_capacity;
    uint64_t free_space_in_bytes;
    uint32_t free_space_in_objects;
    char storage_description[64];
    char volume_identifier[32];
} MTPStorageInfo;

struct MTPHeader {
    uint32_t len; // 0
    uint16_t type; // 4
    uint16_t op; // 6
    uint32_t transaction_id; // 8
};

struct MTPContainer {
    struct MTPHeader header; // 0
    uint32_t params[5]; // 12
};

void OpenSession(AppMTP* mtp, uint32_t session_id);
void CloseSession(AppMTP* mtp);
void GetStorageIDs(AppMTP* mtp, uint32_t* storage_ids, uint32_t* count);
void GetStorageInfo(AppMTP* mtp, uint32_t storage_id, MTPStorageInfo* info);
uint32_t CreateObject(AppMTP* mtp, uint32_t parent_handle, const char* name, bool is_directory);
uint32_t ReadObject(AppMTP* mtp, uint32_t handle, uint8_t* buffer, uint32_t size);
void WriteObject(AppMTP* mtp, uint32_t handle, uint8_t* data, uint32_t size);
bool DeleteObject(AppMTP* mtp, uint32_t handle);

void mtp_handle_bulk(AppMTP* mtp, uint8_t* buffer, int32_t length);
int mtp_handle_class_control(AppMTP* mtp, usbd_device* dev, usbd_ctlreq* req);
void send_mtp_response(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    uint32_t* params);
void send_mtp_response_buffer(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    uint8_t* buffer,
    uint32_t size);
int BuildDeviceInfo(uint8_t* buffer);
void WriteMTPString(uint8_t* buffer, const char* str, uint16_t* length);
void send_device_info(AppMTP* mtp, uint32_t transaction_id);
