// DevMonCon.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "..\DeviceMon\Common.h"

using namespace std;

int Usage() {
    cout << "Usage: DevMonCon.exe [status] [add <device>]* [remove <device>]* [start | stop]" << endl;
    return 0;
}

int Error(DWORD error = ::GetLastError()) {
    cout << "Error: " << error << endl;
    return 1;
}

void ShowMonitoredDevices(HANDLE hDevice) {
    DWORD returned;
    DeviceMonitorStatus status[16];
    if (!::DeviceIoControl(hDevice, static_cast<DWORD>(DriverMonIoctls::GetStatus), nullptr, 0, status, sizeof(status), &returned, nullptr)) {
        Error();
        return;
    }

}


int wmain(int argc, wchar_t* argv[]) {
    auto hDevice = ::CreateFile(FileDeviceSymLink, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDevice == INVALID_HANDLE_VALUE) {
        return Error();
    }

    if (argc == 1) {
        ShowMonitoredDevices(hDevice);
        return Usage();
    }

    DWORD returned;
    for (int i = 1; i < argc; i++) {
        if (::_wcsicmp(argv[i], L"help") == 0) {
            return Usage();
        }

        if (::_wcsicmp(argv[i], L"status") == 0) {
            ShowMonitoredDevices(hDevice);
            continue;
        }

        if (::_wcsicmp(argv[i], L"add") == 0) {
            if (!::DeviceIoControl(hDevice, static_cast<DWORD>(DriverMonIoctls::AddDeviceToMonitor), argv[i + 1],
                (1 + static_cast<DWORD>(::wcslen(argv[i + 1]))) * sizeof(WCHAR), nullptr, 0, &returned, nullptr))
                Error();
            i++;
            continue;
        }

        if (::_wcsicmp(argv[i], L"remove") == 0) {
            if (!::DeviceIoControl(hDevice, static_cast<DWORD>(DriverMonIoctls::RemoveDeviceToMonitor), argv[i + 1],
                (1 + static_cast<DWORD>(::wcslen(argv[i + 1]))) * sizeof(WCHAR), nullptr, 0, &returned, nullptr))
                Error();
            i++;
            continue;
        }

        if (::_wcsicmp(argv[i], L"start") == 0) {
            if (!::DeviceIoControl(hDevice, static_cast<DWORD>(DriverMonIoctls::StartMonitoring), nullptr, 0, nullptr, 0, &returned, nullptr))
                Error();
            continue;
        }
        else if (::_wcsicmp(argv[i], L"stop") == 0) {
            if (!::DeviceIoControl(hDevice, static_cast<DWORD>(DriverMonIoctls::StoptMonitoring), nullptr, 0, nullptr, 0, &returned, nullptr))
                Error();
            continue;
        }      
    }

    ::CloseHandle(hDevice);

    return 0;
}

