// DevMonLog.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "..\DeviceMon\Common.h"

using namespace std;

int Error(DWORD error = ::GetLastError()) {
    cout << "Error: " << error << endl;
    return 1;
}

void DisplayIrpArrivedDetails(const IrpArrivedInfo& info) {
    cout << "IRP sent: PID: " << info.ProcessId << " TID: " << info.ThreadId << " Major: " << (int)info.MajorFunction;
    cout << " Minor: " << (int)info.MinorFunction << " Device: " << info.DeviceObject;
    cout << endl;

    switch (info.MajorFunction) {
        case IrpMajorCode::DEVICE_CONTROL:
        case IrpMajorCode::INTERNAL_DEVICE_CONTROL:
            cout << "\tIoctl: 0x" << hex << info.DeviceIoControl.IoControlCode << " Input len: " << dec
                << info.DeviceIoControl.InputBufferLength << " Output len: " << info.DeviceIoControl.OutputBufferLength << endl;
            break;
    }
}

void DisplayIrpCompletedDetails(const IrpCompletedInfo& info) {
    cout << "IRP Completed: Device: " << info.DeviceObject << " IRP: 0x" << hex << info.Irp << " Status: " << info.Status;
    cout << endl;
}

void DisplayDetails(const CommonInfoHeader* header) {
    switch (header->Type) {
        case DataItemType::IrpArrived:
            DisplayIrpArrivedDetails(*reinterpret_cast<const IrpArrivedInfo*>(header));
            break;

        case DataItemType::IrpCompleted:
            DisplayIrpCompletedDetails(*reinterpret_cast<const IrpCompletedInfo*>(header));
            break;
    }
}


int main() {
    auto hDevice = ::CreateFile(FileDeviceSymLink, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDevice == INVALID_HANDLE_VALUE) {
        return Error();
    }

    DWORD returned;
    HANDLE hEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!::DeviceIoControl(hDevice, static_cast<DWORD>(DriverMonIoctls::SetEventHandle), &hEvent, sizeof(hEvent), nullptr, 0, &returned, nullptr))
        return Error();

    auto size = 1 << 12;
    auto buffer = make_unique<BYTE[]>(size);

    for (;;) {
        ::WaitForSingleObject(hEvent, INFINITE);
        if (!::DeviceIoControl(hDevice, static_cast<DWORD>(DriverMonIoctls::GetData), nullptr, 0, buffer.get(), size, &returned, nullptr)) {
            Error();
            continue;
        }

        for (DWORD i = 0; i < returned; ) {
            const auto data = reinterpret_cast<CommonInfoHeader*>(buffer.get() + i);
            DisplayDetails(data);
            i += data->Size;
        }
    }

    ::CloseHandle(hEvent);
    ::CloseHandle(hDevice);

    return 0;
}

