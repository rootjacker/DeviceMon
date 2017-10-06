#include "pch.h"
#include "Common.h"
#include "DeviceMon.h"
#include "CyclicBuffer.h"
#include "RemoveLock.h"

void OnDriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS OnDeviceCreateClose(PDEVICE_OBJECT, PIRP);
NTSTATUS OnDeviceIoControl(PDEVICE_OBJECT, PIRP);
NTSTATUS OnGenericDispatch(PDEVICE_OBJECT, PIRP);
NTSTATUS OnPnpDispatch(PDEVICE_OBJECT, PIRP);
NTSTATUS OnPowerDispatch(PDEVICE_OBJECT, PIRP);

Globals globals;

extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING /* RegistryPath */) {
    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, DRIVER_PREFIX "DriverEntry\n"));

    DriverObject->DriverUnload = OnDriverUnload;

    //
    // set generic handler
    //
    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i)
        DriverObject->MajorFunction[i] = OnGenericDispatch;

    // specifics 
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = OnDeviceCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = OnDeviceIoControl;
    DriverObject->MajorFunction[IRP_MJ_PNP] = OnPnpDispatch;
    DriverObject->MajorFunction[IRP_MJ_POWER] = OnPowerDispatch;

    UNICODE_STRING name, symLink;
    RtlInitUnicodeString(&name, DeviceName);
    RtlInitUnicodeString(&symLink, DeviceSymLink);

    PDEVICE_OBJECT DeviceObject;
    auto status = IoCreateDevice(DriverObject, 0, &name, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Error creating device object (0x%08X)\n", status));
        return status;
    }

    //
    // save device object in global state
    //
    globals.MainDeviceObject = DeviceObject;

    status = IoCreateSymbolicLink(&symLink, &name);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Error creating symbolic link (0x%08X)\n", status));
        IoDeleteDevice(DeviceObject);
        return status;
    }

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, DRIVER_PREFIX "DriverEntry completed successfully\n"));

    return status;
}

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR information = 0) {
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

void OnDriverUnload(PDRIVER_OBJECT DriverObject) {
    globals.IsMonitoring = false;
    if (globals.DataBuffer)
        delete globals.DataBuffer;
    if (globals.Event) {
        ObDereferenceObject(globals.Event);
        globals.Event = nullptr;
    }

    UNICODE_STRING symLink;
    RtlInitUnicodeString(&symLink, DeviceSymLink);
    IoDeleteSymbolicLink(&symLink);

    //
    // delete all filter device objects
    //
    for (auto device = DriverObject->DeviceObject; device; ) {
        auto next = device->NextDevice;
        if (device != globals.MainDeviceObject) {
            auto context = static_cast<GenericDeviceData*>(device->DeviceExtension);
            NT_ASSERT(context);
            IoDetachDevice(context->TargetDevice);
            IoDeleteDevice(device);
        }
        device = next;
    }

    //
    // delete main device object
    //
    IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS OnDeviceCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (globals.MainDeviceObject != DeviceObject)
        return OnGenericDispatch(DeviceObject, Irp);

    return CompleteIrp(Irp);
}

NTSTATUS StartMonitoring() {
    if (globals.IsMonitoring)
        return STATUS_SUCCESS;

    if (globals.DataBuffer == nullptr) {
        globals.DataBuffer = new (NonPagedPool) CyclicBuffer;
        if (!globals.DataBuffer)
            return STATUS_INSUFFICIENT_RESOURCES;

        globals.DataBuffer->Init(1 << 14, NonPagedPool, DRIVER_TAG);
    }
    globals.IsMonitoring = true;
    return STATUS_SUCCESS;
}

NTSTATUS OnDeviceIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (globals.MainDeviceObject != DeviceObject)
        return OnGenericDispatch(DeviceObject, Irp);

    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR information = 0;

    auto stack = IoGetCurrentIrpStackLocation(Irp);

    switch (static_cast<DriverMonIoctls>(stack->Parameters.DeviceIoControl.IoControlCode)) {
        case DriverMonIoctls::StartMonitoring:
            status = StartMonitoring();
            break;

        case DriverMonIoctls::StoptMonitoring:
            globals.IsMonitoring = false;
            globals.DataBuffer->Reset();
            break;

        case DriverMonIoctls::AddDeviceToMonitor: {
            auto deviceName = static_cast<PCWSTR>(Irp->AssociatedIrp.SystemBuffer);
            UNICODE_STRING udeviceName;
            RtlInitUnicodeString(&udeviceName, deviceName);
            PFILE_OBJECT fileObject;
            PDEVICE_OBJECT deviceObject;
            status = IoGetDeviceObjectPointer(&udeviceName, FILE_ALL_ACCESS, &fileObject, &deviceObject);
            if (!NT_SUCCESS(status)) {
                break;
            }
            ObDereferenceObject(fileObject);

            auto context = new (NonPagedPool) GenericDeviceData;
            ::wcscpy_s(context->DeviceName, deviceName);
            PDEVICE_OBJECT sourceDevice;
            status = IoCreateDevice(DeviceObject->DriverObject, sizeof(GenericDeviceData), nullptr, deviceObject->DeviceType, deviceObject->Characteristics, FALSE, &sourceDevice);
            if (!NT_SUCCESS(status)) {
                KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Error creating device object (0x%08X)\n", status));
                break;
            }

            auto ext = static_cast<GenericDeviceData*>(sourceDevice->DeviceExtension);
            ext->TargetDevice = IoAttachDeviceToDeviceStack(sourceDevice, deviceObject);
            if (!ext->TargetDevice) {
                KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, DRIVER_PREFIX "Error attaching devices\n"));
                status = STATUS_UNSUCCESSFUL;
                IoDeleteDevice(sourceDevice);
                break;
            }

            IoInitializeRemoveLock(&ext->RemoveLock, DRIVER_TAG, 10, 0);

            sourceDevice->Flags |= ext->TargetDevice->Flags & (DO_DIRECT_IO | DO_BUFFERED_IO);
            sourceDevice->Flags |= DO_POWER_PAGABLE;
            sourceDevice->Flags &= ~DO_DEVICE_INITIALIZING;

            ext->DeviceObject = sourceDevice;
            *(PVOID*)Irp->AssociatedIrp.SystemBuffer = ext->TargetDevice;
            information = sizeof(PVOID);
            break;
        }

        case DriverMonIoctls::RemoveDeviceToMonitor: {
            if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(PVOID)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            auto target = *(PVOID*)Irp->AssociatedIrp.SystemBuffer;

            bool found = false;
            for (auto device = DeviceObject->NextDevice; device; device = device->NextDevice) {
                auto context = static_cast<GenericDeviceData*>(device->DeviceExtension);
                if (context->TargetDevice == target) {
                    IoDetachDevice(context->TargetDevice);
                    IoDeleteDevice(device);
                    found = true;
                    break;
                }
            }
            if (!found) {
                status = STATUS_NOT_FOUND;
            }
            break;
        }

        case DriverMonIoctls::SetEventHandle: {
            if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(HANDLE)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            auto handle = *static_cast<HANDLE*>(Irp->AssociatedIrp.SystemBuffer);
            PKEVENT event;
            status = ObReferenceObjectByHandle(handle, EVENT_ALL_ACCESS, *ExEventObjectType, KernelMode, reinterpret_cast<PVOID*>(&event), nullptr);
            if (!NT_SUCCESS(status))
                break;

            if (globals.Event)
                ObDereferenceObject(globals.Event);
            globals.Event = event;
            break;
        }

        case DriverMonIoctls::GetData: {
            auto size = stack->Parameters.DeviceIoControl.OutputBufferLength;
            if (size < sizeof(IrpArrivedInfo)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }
            auto buffer = static_cast<PBYTE>(MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority));
            if (buffer == nullptr) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }
            information = globals.DataBuffer->Read(buffer, size);
            break;
        }

        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    return CompleteIrp(Irp, status, information);
}

NTSTATUS OnIrpComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID /* context */) {
    IrpCompletedInfo info;
    KeQuerySystemTime(&info.Time);
    info.Type = DataItemType::IrpCompleted;
    info.Size = sizeof(info);
    info.Version = 0x100;
    info.Irp = Irp;
    info.Status = Irp->IoStatus.Status;
    info.DeviceObject = DeviceObject;
    info.Information = Irp->IoStatus.Information;

    globals.DataBuffer->Write(&info, info.Size);
    KeSetEvent(globals.Event, 1, FALSE);

    return STATUS_SUCCESS;
}

NTSTATUS OnGenericDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    if (DeviceObject == globals.MainDeviceObject)
        return CompleteIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);

    auto context = static_cast<GenericDeviceData*>(DeviceObject->DeviceExtension);

    AutoRemoveLock lock(&context->RemoveLock);
    if (!lock)
        return CompleteIrp(Irp, STATUS_DELETE_PENDING);

    if (!globals.IsMonitoring || globals.Event == nullptr) {
        //
        // not monitoring - just send IRP down the devnode
        //
        IoSkipCurrentIrpStackLocation(Irp);
        return IoCallDriver(context->TargetDevice, Irp);
    }

    auto stack = IoGetCurrentIrpStackLocation(Irp);

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, DRIVER_PREFIX "Device: %p, MJ: %d\n", DeviceObject, stack->MajorFunction));

    IrpArrivedInfo info;
    info.Size = sizeof(info);
    info.Version = 0x0100;
    info.Type = DataItemType::IrpArrived;
    KeQuerySystemTime(&info.Time);
    info.MajorFunction = static_cast<IrpMajorCode>(stack->MajorFunction);
    info.MinorFunction = static_cast<IrpMinorCode>(stack->MinorFunction);
    info.ProcessId = HandleToULong(PsGetCurrentProcessId());
    info.ThreadId = HandleToULong(PsGetCurrentThreadId());
    info.Irql = KeGetCurrentIrql();
    info.DeviceObject = context->TargetDevice;
    info.Irp = Irp;

    const auto& parameters = stack->Parameters;

    switch (stack->MajorFunction) {
        case IRP_MJ_DEVICE_CONTROL:
        case IRP_MJ_INTERNAL_DEVICE_CONTROL:
            info.DeviceIoControl.IoControlCode = parameters.DeviceIoControl.IoControlCode;
            info.DeviceIoControl.InputBufferLength = parameters.DeviceIoControl.InputBufferLength;
            info.DeviceIoControl.OutputBufferLength = parameters.DeviceIoControl.OutputBufferLength;
            break;

        case IRP_MJ_READ:
            info.Read.Length = parameters.Read.Length;
            info.Read.Offset = parameters.Read.ByteOffset.QuadPart;
            break;

        case IRP_MJ_WRITE:
            info.Read.Length = parameters.Write.Length;
            info.Read.Offset = parameters.Write.ByteOffset.QuadPart;
            break;

    }

    globals.DataBuffer->Write(&info, info.Size);
    KeSetEvent(globals.Event, 1, FALSE);

//    IoCopyCurrentIrpStackLocationToNext(Irp);
//    IoSetCompletionRoutineEx(DeviceObject, Irp, OnIrpComplete, nullptr, TRUE, TRUE, TRUE);
    IoSkipCurrentIrpStackLocation(Irp);

    return IoCallDriver(context->TargetDevice, Irp);
}

NTSTATUS OnPnpDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    auto context = static_cast<GenericDeviceData*>(DeviceObject->DeviceExtension);

    AutoRemoveLock lock(&context->RemoveLock);
    if (!lock)
        return CompleteIrp(Irp, STATUS_DELETE_PENDING);

    auto stack = IoGetCurrentIrpStackLocation(Irp);
    auto minor = stack->MinorFunction;

    IoSkipCurrentIrpStackLocation(Irp);
    auto status = IoCallDriver(context->TargetDevice, Irp);

    if (minor == IRP_MN_REMOVE_DEVICE) {
        auto removeLock = lock.Detach();
        IoReleaseRemoveLockAndWait(removeLock, nullptr);
        IoDetachDevice(context->TargetDevice);
        IoDeleteDevice(DeviceObject);
    }
    return status;
}

NTSTATUS OnPowerDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PoStartNextPowerIrp(Irp);
    auto ext = static_cast<GenericDeviceData*>(DeviceObject->DeviceExtension);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(ext->TargetDevice, Irp);
}

