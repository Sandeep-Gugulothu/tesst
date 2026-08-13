#include <Windows.h>
#include <stdio.h>

#define IOCTL_HIDE_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DRIVER_NAME "HideProc"
#define DRIVER_PATH "d:\\tesst\\driver\\driver.sys"

BOOL LoadDriver() {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) { printf("[-] OpenSCManager failed: %lu\n", GetLastError()); return FALSE; }

    SC_HANDLE hSvc = CreateServiceA(hSCM, DRIVER_NAME, DRIVER_NAME,
        SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        DRIVER_PATH, NULL, NULL, NULL, NULL, NULL);

    if (!hSvc) {
        if (GetLastError() == ERROR_SERVICE_EXISTS)
            hSvc = OpenServiceA(hSCM, DRIVER_NAME, SERVICE_ALL_ACCESS);
        else { printf("[-] CreateService failed: %lu\n", GetLastError()); CloseServiceHandle(hSCM); return FALSE; }
    }

    if (!StartService(hSvc, 0, NULL) && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
        printf("[-] StartService failed: %lu\n", GetLastError());
        CloseServiceHandle(hSvc); CloseServiceHandle(hSCM); return FALSE;
    }

    printf("[+] Driver loaded\n");
    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
    return TRUE;
}

void UnloadDriver() {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    SC_HANDLE hSvc = OpenServiceA(hSCM, DRIVER_NAME, SERVICE_ALL_ACCESS);
    if (hSvc) {
        SERVICE_STATUS ss;
        ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
        DeleteService(hSvc);
        CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
    printf("[+] Driver unloaded\n");
}

int main() {
    if (!LoadDriver()) return 1;

    HANDLE hDevice = CreateFileA("\\\\.\\HideProc",
        GENERIC_READ | GENERIC_WRITE, 0, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open device: %lu\n", GetLastError());
        UnloadDriver(); return 1;
    }

    DWORD bytesReturned;
    if (DeviceIoControl(hDevice, IOCTL_HIDE_PROCESS, NULL, 0, NULL, 0, &bytesReturned, NULL))
        printf("[+] Process hidden\n");
    else
        printf("[-] IOCTL failed: %lu\n", GetLastError());

    CloseHandle(hDevice);
    UnloadDriver();
    return 0;
}
