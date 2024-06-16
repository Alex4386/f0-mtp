#pragma once

#define MTP_STANDARD_VERSION 100
#define MTP_VENDOR_EXTENSION_ID 0x6
#define MTP_VENDOR_EXTENSION_VERSION 100
#define MTP_FUNCTIONAL_MODE 0x0

#define MTP_TYPE_COMMAND 0x1
#define MTP_TYPE_DATA 0x2
#define MTP_TYPE_RESPONSE 0x3

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

#define MTP_OP_DELETE_OBJECT 0x100B
#define MTP_OP_SEND_OBJECT_INFO 0x100C
#define MTP_OP_SEND_OBJECT 0x100D
#define MTP_OP_GET_DEVICE_PROP_DESC 0x1014
#define MTP_OP_GET_DEVICE_PROP_VALUE 0x1015

#define MTP_OP_GET_OBJECT_PROPS_SUPPORTED 0x9801
#define MTP_OP_GET_OBJECT_PROP_DESC 0x9802
#define MTP_OP_GET_OBJECT_PROP_VALUE 0x9803
#define MTP_OP_SET_OBJECT_PROP_VALUE 0x9804

// MTP Object Props
#define MTP_PROP_OBJECT_FORMAT 0xDC02
#define MTP_PROP_OBJECT_FILE_NAME 0xDC07

// MTP DataTypes
#define MTP_TYPE_INT8 0x0001
#define MTP_TYPE_UINT8 0x0002
#define MTP_TYPE_INT16 0x0003
#define MTP_TYPE_UINT16 0x0004
#define MTP_TYPE_INT32 0x0005
#define MTP_TYPE_UINT32 0x0006
#define MTP_TYPE_INT64 0x0007
#define MTP_TYPE_UINT64 0x0008
#define MTP_TYPE_INT128 0x0009
#define MTP_TYPE_UINT128 0x000A
#define MTP_TYPE_AINT8 0x4001
#define MTP_TYPE_AUINT8 0x4002
#define MTP_TYPE_AINT16 0x4003
#define MTP_TYPE_AUINT16 0x4004
#define MTP_TYPE_AINT32 0x4005
#define MTP_TYPE_AUINT32 0x4006
#define MTP_TYPE_AINT64 0x4007
#define MTP_TYPE_AUINT64 0x4008
#define MTP_TYPE_AINT128 0x4009
#define MTP_TYPE_AUINT128 0x400A
#define MTP_TYPE_STR 0xFFFF

// MTP Prop Get/Set
#define MTP_PROP_GET 0x00
#define MTP_PROP_GETSET 0x01

// MTP Response Codes
#define MTP_RESP_UNKNOWN 0x2000
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

#define MTP_RESP_INVALID_OBJECT_PROP_CODE 0xA801

// Storage IDs
#define INTERNAL_STORAGE_ID 0x00010001
#define EXTERNAL_STORAGE_ID 0x00020001

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
} MTPStorageInfoHeader;

typedef struct ObjectInfoHeader {
    uint32_t storage_id;
    uint16_t format;
    uint16_t protection_status;
    uint32_t compressed_size;
    uint16_t thumb_format;
    uint32_t thumb_compressed_size;
    uint32_t thumb_pix_width;
    uint32_t thumb_pix_height;
    uint32_t image_pix_width;
    uint32_t image_pix_height;
    uint32_t image_bit_depth;
    uint32_t parent_object;
    uint16_t association_type;
    uint32_t association_desc;
} ObjectInfoHeader;

struct MTPHeader {
    uint32_t len; // 0
    uint16_t type; // 4
    uint16_t op; // 6
    uint32_t transaction_id; // 8
};

typedef struct MTPDataPersistence {
    uint32_t left_bytes;
    uint8_t* global_buffer;
    uint32_t buffer_offset;

    uint16_t op;
    uint32_t transaction_id;
    uint32_t params[5];

    uint32_t prev_handle;
    File* current_file;
} MTPDataPersistence;

struct MTPContainer {
    struct MTPHeader header; // 0
    uint32_t params[5]; // 12
};

extern uint16_t supported_operations[];
extern uint16_t supported_device_properties[];
extern uint16_t supported_playback_formats[];

void OpenSession(AppMTP* mtp, uint32_t session_id);
void CloseSession(AppMTP* mtp);
void GetStorageIDs(AppMTP* mtp, uint32_t* storage_ids, uint32_t* count);
int GetStorageInfo(AppMTP* mtp, uint32_t storage_id, uint8_t* buf);
bool DeleteObject(AppMTP* mtp, uint32_t handle);

void mtp_handle_bulk(AppMTP* mtp, uint8_t* buffer, uint32_t length);
void handle_mtp_command(AppMTP* mtp, struct MTPContainer* container);
void handle_mtp_data_packet(AppMTP* mtp, uint8_t* buffer, int32_t length, bool cont);
void handle_mtp_response(AppMTP* mtp, struct MTPHeader* container);
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
void send_mtp_response_stream(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    void* callback_context,
    int (*callback)(void* ctx, uint8_t* buffer, int length),
    uint32_t length);
int BuildDeviceInfo(uint8_t* buffer);

bool CheckMTPStringHasUnicode(uint8_t* buffer);
char* ReadMTPString(uint8_t* buffer);
void WriteMTPString(uint8_t* buffer, const char* str, uint16_t* length);
void send_device_info(AppMTP* mtp, uint32_t transaction_id);
void send_storage_ids(AppMTP* mtp, uint32_t transaction_id);

void send_device_prop_value(AppMTP* mtp, uint32_t transaction_id, uint32_t prop_code);
void send_device_prop_desc(AppMTP* mtp, uint32_t transaction_id, uint32_t prop_code);
int GetDevicePropValue(uint32_t prop_code, uint8_t* buffer);
int GetDevicePropDesc(uint32_t prop_code, uint8_t* buffer);
int GetObjectHandles(AppMTP* mtp, uint32_t storage_id, uint32_t association, uint8_t* buffer);
int GetObjectInfo(AppMTP* mtp, uint32_t handle, uint8_t* buffer);
void GetObject(AppMTP* mtp, uint32_t transaction_id, uint32_t handle);
char* get_base_path_from_storage_id(uint32_t storage_id);
char* get_path_from_handle(AppMTP* mtp, uint32_t handle);
uint32_t issue_object_handle(AppMTP* mtp, char* path);
void handle_mtp_data_complete(AppMTP* mtp);

struct MTPResponseBufferContext {
    uint8_t* buffer;
    uint32_t size;
    uint32_t sent;
};

struct MTPObjectPropDescHeaderChunk1 {
    uint16_t prop_code;
    uint16_t data_type;
    uint8_t get_set; // 0x00: Get, 0x01: GetSet
};

struct MTPObjectPropDescHeaderChunk2 {
    uint32_t group_code;
    uint8_t form_flag;
};
