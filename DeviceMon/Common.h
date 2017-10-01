#pragma once

// common header for driver and user mode

const WCHAR FileDeviceSymLink[] = L"\\\\.\\DeviceMon0100";

const int DeviceNameLength = 128;

enum class DriverMonIoctls {
    StartMonitoring =       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_NEITHER, FILE_ANY_ACCESS),
    StoptMonitoring =       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_NEITHER, FILE_ANY_ACCESS),
    GetStatus =             CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS),

    AddDeviceToMonitor =    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS),
    RemoveDeviceToMonitor = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_ANY_ACCESS),
    RemoveAllDevices =      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x812, METHOD_BUFFERED, FILE_ANY_ACCESS),

    SetEventHandle =        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x820, METHOD_BUFFERED, FILE_ANY_ACCESS),
    GetData =               CTL_CODE(FILE_DEVICE_UNKNOWN, 0x821, METHOD_OUT_DIRECT, FILE_ANY_ACCESS),
};

struct DeviceMonitorStatus {
    WCHAR DeviceName[DeviceNameLength];
    PVOID TargetDevice;
    bool IsMonitoring;
};

enum class IrpMajorCode : unsigned char {
    CREATE                   = 0x00,
    CREATE_NAMED_PIPE        = 0x01,
    CLOSE                    = 0x02,
    READ                     = 0x03,
    WRITE                    = 0x04,
    QUERY_INFORMATION        = 0x05,
    SET_INFORMATION          = 0x06,
    QUERY_EA                 = 0x07,
    SET_EA                   = 0x08,
    FLUSH_BUFFERS            = 0x09,
    QUERY_VOLUME_INFORMATION = 0x0a,
    SET_VOLUME_INFORMATION   = 0x0b,
    DIRECTORY_CONTROL        = 0x0c,
    FILE_SYSTEM_CONTROL      = 0x0d,
    DEVICE_CONTROL           = 0x0e,
    INTERNAL_DEVICE_CONTROL  = 0x0f,
    SHUTDOWN                 = 0x10,
    LOCK_CONTROL             = 0x11,
    CLEANUP                  = 0x12,
    CREATE_MAILSLOT          = 0x13,
    QUERY_SECURITY           = 0x14,
    SET_SECURITY             = 0x15,
    POWER                    = 0x16,
    SYSTEM_CONTROL           = 0x17,
    DEVICE_CHANGE            = 0x18,
    QUERY_QUOTA              = 0x19,
    SET_QUOTA                = 0x1a,
    PNP                      = 0x1b,
};

enum class IrpMinorCode : unsigned char {
    None =                          0,

    // PNP

    START_DEVICE                 = 0x00,
    QUERY_REMOVE_DEVICE          = 0x01,
    REMOVE_DEVICE                = 0x02,
    CANCEL_REMOVE_DEVICE         = 0x03,
    STOP_DEVICE                  = 0x04,
    QUERY_STOP_DEVICE            = 0x05,
    CANCEL_STOP_DEVICE           = 0x06,
                                 
    QUERY_DEVICE_RELATIONS       = 0x07,
    QUERY_INTERFACE              = 0x08,
    QUERY_CAPABILITIES           = 0x09,
    QUERY_RESOURCES              = 0x0A,
    QUERY_RESOURCE_REQUIREMENTS  = 0x0B,
    QUERY_DEVICE_TEXT            = 0x0C,
    FILTER_RESOURCE_REQUIREMENTS = 0x0D,
                                 
    READ_CONFIG                  = 0x0F,
    WRITE_CONFIG                 = 0x10,
    EJECT                        = 0x11,
    SET_LOCK                     = 0x12,
    QUERY_ID                     = 0x13,
    QUERY_PNP_DEVICE_STATE       = 0x14,
    QUERY_BUS_INFORMATION        = 0x15,
    DEVICE_USAGE_NOTIFICATION    = 0x16,
    SURPRISE_REMOVAL             = 0x17,
                                  
    // Power                      

    WAIT_WAKE                    = 0x00,
    POWER_SEQUENCE               = 0x01,
    SET_POWER                    = 0x02,
    QUERY_POWER                  = 0x03,

    // WMI

    QUERY_ALL_DATA              = 0x00,
    QUERY_SINGLE_INSTANCE       = 0x01,
    CHANGE_SINGLE_INSTANCE      = 0x02,
    CHANGE_SINGLE_ITEM          = 0x03,
    ENABLE_EVENTS               = 0x04,
    DISABLE_EVENTS              = 0x05,
    ENABLE_COLLECTION           = 0x06,
    DISABLE_COLLECTION          = 0x07,
    REGINFO                     = 0x08,
    EXECUTE_METHOD              = 0x09,

};

enum class DataItemType : short {
    IrpArrived,
    IrpCompleted,
};

struct CommonInfoHeader {
    USHORT Size;
    USHORT Version;
    DataItemType Type;
    long long Time;
};

struct IrpArrivedInfo : CommonInfoHeader {
    ULONG ProcessId;
    ULONG ThreadId;
    PVOID DeviceObject;
    PVOID Irp;
    IrpMajorCode MajorFunction;
    IrpMinorCode MinorFunction;
    UCHAR Irql;
    UCHAR _padding;
    union {
        struct {
            ULONG IoControlCode;
            ULONG InputBufferLength;
            ULONG OutputBufferLength;
        } DeviceIoControl;
        struct {
            USHORT FileAttributes;
            USHORT ShareAccess;
        } Create;
        struct {
            ULONG Length;
            long long Offset;
        } Read;
        struct {
            ULONG Length;
            long long Offset;
        } Write;
    };
};

struct IrpCompletedInfo : CommonInfoHeader {
    PVOID DeviceObject;
    PVOID Irp;
    long Status;
    ULONG_PTR Information;
};
