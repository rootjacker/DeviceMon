#pragma once

#include "Common.h"

// driver only declarations

const WCHAR DeviceSymLink[] = L"\\??\\DeviceMon0100";
const WCHAR DeviceName[] = L"\\Device\\DeviceMon0100";

class CyclicBuffer;

void* operator new(size_t size, POOL_TYPE type, ULONG tag = 0);

void operator delete(void* p, size_t);

#define DRIVER_PREFIX "DeviceMon: "

const ULONG DRIVER_TAG = 'NMVD';

struct GenericDeviceData {
    PDEVICE_OBJECT TargetDevice;
    PDEVICE_OBJECT DeviceObject;
    WCHAR DeviceName[DeviceNameLength];
};

struct Globals {
    PDEVICE_OBJECT MainDeviceObject;
    IO_REMOVE_LOCK RemoveLock;
    CyclicBuffer* DataBuffer;
    PKEVENT Event;
    bool IsMonitoring;
};

