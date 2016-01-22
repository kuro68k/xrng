#include "pch.h"

#include <stdio.h>
#include <conio.h>
#include <time.h>


void PrintLastError(void)
{
	DWORD error = GetLastError();
	if (error == 0)
	{
		printf("No error.\n");
		return;
	}

	LPSTR buffer = 0;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, NULL);
	if (size != 0)
		printf("%s\n", buffer);
	else
		printf("Failed to get error message.\n");
}

bool GetWinUSBDevice(DEVICE_DATA *deviceData, UCHAR *inPipe, UCHAR *outPipe)
{
	HRESULT					hr;
	USB_DEVICE_DESCRIPTOR	deviceDesc;
	BOOL					noDevice;
	BOOL					bResult;
	ULONG					lengthReceived;

	*inPipe = 0;
	*outPipe = 0;

	// Find a device connected to the system that has WinUSB installed using our
	// INF
	hr = OpenDevice(deviceData, &noDevice);

	if (FAILED(hr)) {
		if (noDevice) {
			printf(_T("Device not connected or driver not installed\n"));
		}
		else {
			printf(_T("Failed looking for device, HRESULT 0x%x\n"), hr);
		}
		return false;
	}

	// Get device descriptor
	bResult = WinUsb_GetDescriptor(deviceData->WinusbHandle,
		USB_DEVICE_DESCRIPTOR_TYPE,
		0,
		0,
		(PBYTE)&deviceDesc,
		sizeof(deviceDesc),
		&lengthReceived);

	if (FALSE == bResult || lengthReceived != sizeof(deviceDesc)) {

		printf(_T("Error among LastError %d or lengthReceived %d\n"),
			FALSE == bResult ? GetLastError() : 0,
			lengthReceived);
		CloseDevice(deviceData);
		return false;
	}

	// set alternative setting
	if (!WinUsb_SetCurrentAlternateSetting(deviceData->WinusbHandle, 1))
	{
		printf("Unable to set alternative setting.\n");
		return false;
	}

	// Print a few parts of the device descriptor
	printf(_T("Device found: VID_%04X&PID_%04X; bcdUsb %04X\n"),
		deviceDesc.idVendor,
		deviceDesc.idProduct,
		deviceDesc.bcdUSB);
	printf("Configurations:\t%u\n", deviceDesc.bNumConfigurations);
	printf("\n");

	USB_INTERFACE_DESCRIPTOR intDesc;
	if (!WinUsb_QueryInterfaceSettings(deviceData->WinusbHandle, 1, &intDesc))
	{
		printf("Error calling QueryInterfaceSettings()\n.");
		return false;
	}

	printf("Endoints:\t%u\n", intDesc.bNumEndpoints);
	printf("Class:\t\t%u\n", intDesc.bInterfaceClass);
	printf("Subclass:\t%u\n", intDesc.bInterfaceSubClass);

	for (int index = 0; index < intDesc.bNumEndpoints; index++)
	{
		WINUSB_PIPE_INFORMATION pipe;
		if (WinUsb_QueryPipe(deviceData->WinusbHandle, 1, (UCHAR)index, &pipe))
		{
			printf("\tPipe index:\t%u\n", index);
			printf("\tPipe type:\t%u ", pipe.PipeType);
			if (pipe.PipeType == UsbdPipeTypeInterrupt)
				printf("(interrupt, ");
			else
				printf("(other, ");
			if (USB_ENDPOINT_DIRECTION_IN(pipe.PipeId))
				printf("in)\n");
			else
				printf("out)\n");

			if (pipe.PipeType == UsbdPipeTypeBulk)
			{
				if (USB_ENDPOINT_DIRECTION_IN(pipe.PipeId))
					*inPipe = pipe.PipeId;
				else
					*outPipe = pipe.PipeId;
			}
		}
	}
	printf("\n");

	printf("In pipe:\t%u\n", *inPipe);
	printf("Out pipe:\t%u\n", *outPipe);
	printf("\n");
	if ((*inPipe == 0) || (*outPipe == 0))
	{
		printf("Unable to find in/out pipes.\n");
		return false;
	}

	return true;
}


LONG __cdecl _tmain(LONG Argc, LPTSTR *Argv)
{
	DEVICE_DATA				deviceData;
	UCHAR					inPipe;
	UCHAR					outPipe;

	UNREFERENCED_PARAMETER(Argc);
	UNREFERENCED_PARAMETER(Argv);

	// WinUSB
	if (!GetWinUSBDevice(&deviceData, &inPipe, &outPipe))
	{
		printf("Unable to open WinUSB device.\n");
		return 0;
	}

	UINT8 buffer[64];
	ULONG cbSize = sizeof(buffer);
	/*
	while (!_kbhit())
	//for (int j = 0; j < 30; j++)
	{
		ULONG cbRead = 0;
		if (!WinUsb_ReadPipe(deviceData.WinusbHandle, inPipe, (PUCHAR)buffer, cbSize, &cbRead, 0))
		{
			printf("Read from pipe failed.\n");
			return -1;
		}
		else
		{
			//for (ULONG i = 0; i < cbRead; i++)
			//	printf("%02X ", buffer[i]);
			//printf("\n");
			printf("%u : %02X %02X %02X\n", cbRead, buffer[0], buffer[1], buffer[2]);
		}
	}
	*/

	time_t tick = clock() + CLOCKS_PER_SEC;
	ULONG bytes = 0;
	while (!_kbhit())
	{
		ULONG cbRead = 0;
		if (!WinUsb_ReadPipe(deviceData.WinusbHandle, inPipe, (PUCHAR)buffer, cbSize, &cbRead, 0))
		{
			printf("Read from pipe failed.\n");
			return -1;
		}
		else
		{
			bytes += cbRead;
			time_t now = clock();
			if (now >= tick)
			{
				tick = now + CLOCKS_PER_SEC;
				printf("%lu\n", bytes);
				bytes = 0;
			}
		}
	}

	CloseDevice(&deviceData);
	return 0;
}
