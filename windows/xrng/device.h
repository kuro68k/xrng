//
// Define below GUIDs
//
#include <initguid.h>

//
// Device Interface GUID.
// Used by all WinUsb devices that this application talks to.
// Must match "DeviceInterfaceGUIDs" registry value specified in the INF file.
// d1ef5aba-506a-4ec6-94af-280f9ead82d5
//
DEFINE_GUID(GUID_DEVINTERFACE_xrng,
    0xd1ef5aba,0x506a,0x4ec6,0x94,0xaf,0x28,0x0f,0x9e,0xad,0x82,0xd5);

typedef struct _DEVICE_DATA {

    BOOL                    HandlesOpen;
    WINUSB_INTERFACE_HANDLE WinusbHandle;
    HANDLE                  DeviceHandle;
    TCHAR                   DevicePath[MAX_PATH];

} DEVICE_DATA, *PDEVICE_DATA;

HRESULT
OpenDevice(
    _Out_     PDEVICE_DATA DeviceData,
    _Out_opt_ PBOOL        FailureDeviceNotFound
    );

VOID
CloseDevice(
    _Inout_ PDEVICE_DATA DeviceData
    );
